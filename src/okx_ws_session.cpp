/**
OKX WebSocket Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx_ws_session.h"
#include <deque>
#include "stonky/utils/log_utils.h"
#include "stonky/utils/json_utils.h"
#include <nlohmann/json.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <utility>

namespace stonky::okx {
static constexpr int PING_INTERVAL_IN_S = 20;

struct WebSocketSession::P {
    boost::asio::ip::tcp::resolver resolver;
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws;
    boost::beast::multi_buffer buffer;
    std::string host;
    std::string wsPath{"/ws/v5/public"};
    /// Set => private session: login first, subscribe only after the ack. A
    /// PROVIDER, not a stored request — the login signature embeds a timestamp
    /// the venue rejects after ~30 s, so each (re)connect must sign afresh.
    std::function<std::string()> loginProvider;
    std::string loginRequest; ///< the request written on THIS connection
    bool loginSent{false};
    bool authenticated{false};
    std::vector<std::string> subscriptions;
    /// PENDING subscriptions, in arrival order. This was a single slot, so
    /// subscribing to several symbols in a row silently dropped all but the
    /// last: each call overwrote the previous before the io loop could send it.
    std::deque<std::string> pendingSubscriptions;
    onLogMessage logMessageCB;
    onDataEvent dataEventCB;
    boost::asio::steady_timer pingTimer;
    std::chrono::time_point<std::chrono::system_clock> lastPingTime{};
    std::chrono::time_point<std::chrono::system_clock> lastPongTime{};
    mutable std::recursive_mutex subscriptionLocker;

    P(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, onLogMessage onLogMessageCB) :
        resolver(make_strand(ioc)), ws(make_strand(ioc), ctx), logMessageCB(std::move(onLogMessageCB)), pingTimer(ioc, boost::asio::chrono::seconds(PING_INTERVAL_IN_S)) {}

    void writeSubscriptionRequest(const std::string &request) {
        std::lock_guard lk(subscriptionLocker);

        for (const auto &rq: subscriptions) {
            if (rq == request) {
                return;
            }
        }

        for (const auto &rq: pendingSubscriptions) {
            if (rq == request) {
                return;
            }
        }

        pendingSubscriptions.push_back(request);
    }

    std::string readSubscriptionRequest() {
        std::lock_guard lk(subscriptionLocker);

        if (pendingSubscriptions.empty()) {
            return "";
        }

        const auto request = pendingSubscriptions.front();
        pendingSubscriptions.pop_front();

        WSRequest wsRequest;
        WSSubscription wsSubscription;
        wsSubscription.fromJson(nlohmann::json::parse(request));
        wsRequest.subscriptions.push_back(wsSubscription);
        return wsRequest.toJson().dump();
    }

    static bool isControlEvent(const nlohmann::json &json) { return json.contains("event"); }

    void handleControlEvent(const nlohmann::json &json) {
        std::lock_guard lk(subscriptionLocker);

        /// Handled on the raw json: EventType has no `login` member, and letting
        /// WSResponse parse an unknown event would throw.
        /// Bookkeeping events carry neither a subscription nor an error; they
        /// must not be mistaken for a subscribe ack (EventType has no member for
        /// them, so readMagicEnum would leave the default and mis-record one).
        if (const auto event = json.value("event", ""); event == "channel-conn-count" || event == "channel-conn-count-error" || event == "notice") {
            logMessageCB(LogSeverity::Info, fmt::format("OKX WebSocket notice: {}", json.dump()));
            return;
        }

        if (json.value("event", "") == "login") {
            const auto code = json.value("code", "");
            authenticated = code == "0";
            if (authenticated) {
                logMessageCB(LogSeverity::Info, "OKX WebSocket authenticated");
            } else {
                logMessageCB(LogSeverity::Error, fmt::format("OKX WebSocket login failed, code: {}, message: {}", code, json.value("msg", "")));
            }
            return;
        }

        WSResponse wsResponse;
        wsResponse.fromJson(json);

        if (wsResponse.event == EventType::error) {
            logMessageCB(LogSeverity::Error, fmt::format("OKX Error Event, code: {}, message: {}", wsResponse.code, wsResponse.msg));
        } else if (wsResponse.event == EventType::subscribe) {
            subscriptions.push_back(wsResponse.subscription.toJson().dump());
        } else if (wsResponse.event == EventType::unsubscribe) {
            if (const auto it = std::ranges::find(subscriptions, wsResponse.subscription.toJson().dump()); it != subscriptions.end()) {
                subscriptions.erase(it);
            }
        }

#ifdef VERBOSE_LOG
        logMessageCB(LogSeverity::Info, fmt::format("OKX API control msg: {}", json.dump()));
#endif
    }

    void onResolve(const std::shared_ptr<WebSocketSession> &self, const boost::system::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

        get_lowest_layer(ws).async_connect(
                results, [this, self](const boost::system::error_code &e, const boost::asio::ip::tcp::resolver::results_type::endpoint_type &ep) { onConnect(self, e, ep); });
    }

    void onConnect(const std::shared_ptr<WebSocketSession> &self, boost::system::error_code ec, const boost::asio::ip::tcp::resolver::results_type::endpoint_type &ep) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

        // verify_peer on the shared context validates the certificate chain;
        // bind that certificate to the endpoint requested for this session as
        // well. The callback must be installed before the TLS handshake and
        // before `host` is extended with the port for the WebSocket Host field.
        ws.next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(host));

        if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
            ec = boost::system::error_code(static_cast<int>(ERR_get_error()), boost::asio::error::get_ssl_category());
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        host += ':' + std::to_string(ep.port());

        ws.next_layer().async_handshake(boost::asio::ssl::stream_base::client, [this, self](const boost::system::error_code &e) { onSSLHandshake(self, e); });
    }

    void onSSLHandshake(const std::shared_ptr<WebSocketSession> &self, const boost::system::error_code &ec) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        ws.control_callback([this](boost::beast::websocket::frame_type kind, boost::beast::string_view payload) {
            boost::ignore_unused(kind, payload);

            if (kind == boost::beast::websocket::frame_type::pong) {
                lastPongTime = std::chrono::system_clock::now();
            }
        });

        get_lowest_layer(ws).expires_never();

        ws.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

        ws.set_option(boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::request_type &req) { req.set(boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " okx-client"); }));

        ws.async_handshake(host, wsPath, [this, self](const boost::system::error_code &e) { onHandshake(self, e); });
    }

    void onHandshake(const std::shared_ptr<WebSocketSession> &self, const boost::system::error_code &ec) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        pingTimer.async_wait([this, self](const boost::system::error_code &e) { onPingTimer(self, e); });

        /// A private session must authenticate before the venue accepts any
        /// subscription; the queued subscription is written once the login is
        /// acknowledged (see handleControlEvent / onRead).
        if (loginProvider && !loginSent) {
            loginSent = true;
            loginRequest = loginProvider();
            ws.async_write(boost::asio::buffer(loginRequest),
                             [this, self](const boost::system::error_code &e, const std::size_t bytesTransferred) { onWrite(self, e, bytesTransferred); });
            return;
        }

        ws.async_write(boost::asio::buffer(readSubscriptionRequest()),
                         [this, self](const boost::system::error_code &e, const std::size_t bytesTransferred) { onWrite(self, e, bytesTransferred); });
    }

    void onWrite(const std::shared_ptr<WebSocketSession> &self, const boost::system::error_code &ec, std::size_t bytesTransferred) {
        boost::ignore_unused(bytesTransferred);

        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        ws.async_read(buffer, [this, self](const boost::system::error_code &e, const std::size_t transferred) { onRead(self, e, transferred); });
    }

    void onRead(const std::shared_ptr<WebSocketSession> &self, const boost::system::error_code &ec, std::size_t bytesTransferred) {
        boost::ignore_unused(bytesTransferred);

        if (ec) {
            pingTimer.cancel();
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        try {
            const auto size = buffer.size();
            std::string strBuffer;
            strBuffer.reserve(size);

            for (const auto &it: buffer.data()) {
                strBuffer.append(static_cast<const char *>(it.data()), it.size());
            }

            buffer.consume(buffer.size());

            if (const nlohmann::json json = nlohmann::json::parse(strBuffer); json.is_object()) {
                if (isControlEvent(json)) {
                    handleControlEvent(json);
                } else {
                    try {
                        DataEvent dataEvent;
                        dataEvent.fromJson(json);

                        if (dataEventCB) {
                            dataEventCB(dataEvent);
                        }
                    } catch (std::exception &e) {
                        logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                    }
                }
            }

            if (const auto subscription = readSubscriptionRequest(); !subscription.empty()) {
                ws.async_write(boost::asio::buffer(subscription),
                                 [this, self](const boost::system::error_code &e, const std::size_t transferred) { onWrite(self, e, transferred); });
            } else {
                std::lock_guard lk(subscriptionLocker);
                /// A private session is legitimately subscription-less between the
                /// login ack and its first subscribe, and its owner may keep it open
                /// with none at all — only public sessions quit when idle.
                if (subscriptions.empty() && pendingSubscriptions.empty() && !loginProvider) {
                    logMessageCB(LogSeverity::Warning, fmt::format("No subscriptions, WebSocketSession quit: {}", MAKE_FILELINE));
                    closeWs();
                }
                ws.async_read(buffer, [this, self](const boost::system::error_code &e, const std::size_t transferred) { onRead(self, e, transferred); });
            }
        } catch (nlohmann::json::exception &exc) {
            logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, exc.what()));
            ws.async_close(boost::beast::websocket::close_code::normal, [this](const boost::system::error_code &e) { onClose(e); });
        }
    }

    void ping() {
        if (const std::chrono::duration<double> elapsed = lastPingTime - lastPongTime; elapsed.count() > PING_INTERVAL_IN_S) {
            logMessageCB(LogSeverity::Warning, fmt::format("{}: {}", MAKE_FILELINE, "ping expired"));
        }

        if (ws.is_open()) {
            const boost::beast::websocket::ping_data pingWebSocketFrame;
            ws.async_ping(pingWebSocketFrame, [this](const boost::system::error_code &ec) {
                if (ec) {
                    logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
                } else {
                    lastPingTime = std::chrono::system_clock::now();
                }
            });
        }
    }

    void closeWs() {
        ws.async_close(boost::beast::websocket::close_code::normal, [this](const boost::system::error_code &ec) { onClose(ec); });
    }

    void onClose(const boost::system::error_code &ec) {
        pingTimer.cancel();

        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }
    }

    void onPingTimer(const std::shared_ptr<WebSocketSession> &self, const boost::system::error_code &ec) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        ping();
        pingTimer.expires_after(boost::asio::chrono::seconds(PING_INTERVAL_IN_S));
        pingTimer.async_wait([this, self](const boost::system::error_code &e) { onPingTimer(self, e); });
    }

    [[nodiscard]] bool isSubscribed(const std::string &request) const {
        std::lock_guard lk(subscriptionLocker);

        if (const auto it = std::ranges::find(subscriptions, request); it != subscriptions.end()) {
            return true;
        }

        return false;
    }
};

WebSocketSession::WebSocketSession(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB) :
    m_p(std::make_unique<P>(ioc, ctx, onLogMessageCB)) {}

WebSocketSession::~WebSocketSession() {
    m_p->pingTimer.cancel();

#ifdef VERBOSE_LOG
    m_p->logMessageCB(LogSeverity::Info, "WebSocketSession destroyed");
#endif
}

void WebSocketSession::subscribe(const std::string &subscriptionRequest) const { m_p->writeSubscriptionRequest(subscriptionRequest); }

bool WebSocketSession::isSubscribed(const std::string &subscriptionRequest) const { return m_p->isSubscribed(subscriptionRequest); }

void WebSocketSession::run(const std::string &host, const std::string &port, const std::string &subscriptionRequest, const onDataEvent &dataEventCB,
                           const std::string &path, const std::function<std::string()> &loginProvider) {
    if (subscriptionRequest.empty()) {
        throw std::runtime_error("SubscriptionRequest cannot be empty");
    }

    m_p->host = host;
    m_p->wsPath = path;
    m_p->loginProvider = loginProvider;
    m_p->writeSubscriptionRequest(subscriptionRequest);
    m_p->dataEventCB = dataEventCB;

    auto self = shared_from_this();
    m_p->resolver.async_resolve(
            host, port, [this, self](const boost::system::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results) { m_p->onResolve(self, ec, results); });
}

void WebSocketSession::close() const { m_p->closeWs(); }
} // namespace stonky::okx
