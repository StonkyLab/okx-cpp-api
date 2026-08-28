/**
OKX WebSocket Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_OKX_WS_SESSION_H
#define INCLUDE_STONKY_OKX_WS_SESSION_H

#include "stonky/utils/log_utils.h"
#include "okx_event_models.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <memory>
#include <functional>

namespace stonky::okx {
using onDataEvent = std::function<void(const DataEvent &event)>;

class WebSocketSession final : public std::enable_shared_from_this<WebSocketSession> {
    struct P;
    std::unique_ptr<P> m_p;

public:
    explicit WebSocketSession(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB);

    ~WebSocketSession();

    /**
     * Run the session.
     * @param host
     * @param port
     * @param subscriptionRequest Must not be empty
     * @param dataEventCB Data Message callback
     * @param path WebSocket path — "/ws/v5/public" (default) or "/ws/v5/private".
     * @param loginProvider Generator of a serialized OKX login op, invoked AT
     *        HANDSHAKE TIME. A generator rather than a string because the login
     *        signature embeds a timestamp the venue rejects after ~30 s — a
     *        reconnect that replayed a stored request would fail authentication
     *        forever. When set the session is PRIVATE: the login is written
     *        first, subscriptions only after the venue acknowledges it, and the
     *        session does not self-close while it holds no subscriptions
     *        (legitimately idle between login ack and first subscribe).
     */
    void run(const std::string &host, const std::string &port, const std::string &subscriptionRequest, const onDataEvent &dataEventCB,
             const std::string &path = "/ws/v5/public", const std::function<std::string()> &loginProvider = {});

    /**
     * Close the session asynchronously
     */
    void close() const;

    /**
     * Subscribe WebSocket according to the subscriptionFilter
     * @param subscriptionRequest
     */
    void subscribe(const std::string &subscriptionRequest) const;

    /**
     * Check if a stream is already subscribed
     * @param subscriptionRequest
     * @return True if subscribed
     */
    [[nodiscard]] bool isSubscribed(const std::string &subscriptionRequest) const;

    /**
     * Seconds since the venue's last pong on THIS connection (from the
     * handshake while none arrived yet; 0.0 before the handshake — the
     * connect phase has its own 30 s expiry). A dead-TCP zombie shows a
     * growing age while the socket still reads open. Thread-safe.
     */
    [[nodiscard]] double secondsSinceLastPong() const;

    /**
     * True when the pong age exceeds the zombie threshold. Such a session
     * hard-closes itself on its next ping tick; owners should treat it as
     * dead immediately (health checks, order-submit guards).
     */
    [[nodiscard]] bool isStale() const;
};
} // namespace stonky::okx
#endif // INCLUDE_STONKY_OKX_WS_SESSION_H
