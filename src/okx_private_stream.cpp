/**
OKX Private WebSocket Stream

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx_private_stream.h"
#include "stonky/okx/okx.h"
#include "stonky/okx/okx_ws_client.h"
#include "base64.h"
#include "nlohmann/json.hpp"
#include <openssl/hmac.h>
#include <atomic>
#include <chrono>

namespace stonky::okx {
namespace {
constexpr auto WS_PORT = "8443";

/**
 * OKX WS login signature: base64(HMAC-SHA256(ts + "GET" + "/users/self/verify")).
 *
 * NOTE the timestamp differs from the REST one — WS wants EPOCH SECONDS, while
 * REST wants an ISO-8601 string. Signing a WS login with the REST timestamp
 * fails with a signature error that reads like a bad key.
 */
std::string loginRequestJson(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase) {
    const auto ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const std::string payload = ts + "GET" + "/users/self/verify";

    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int digestLength = SHA256_DIGEST_LENGTH;
    HMAC(EVP_sha256(), apiSecret.data(), static_cast<int>(apiSecret.size()), reinterpret_cast<const unsigned char *>(payload.data()), payload.length(), digest,
         &digestLength);

    nlohmann::json arg;
    arg["apiKey"] = apiKey;
    arg["passphrase"] = passphrase;
    arg["timestamp"] = ts;
    arg["sign"] = base64_encode(digest, sizeof(digest));

    nlohmann::json request;
    request["op"] = "login";
    request["args"] = nlohmann::json::array({arg});
    return request.dump();
}
} // namespace

struct PrivateStream::P {
    std::unique_ptr<WebSocketClient> wsClient;
    std::atomic<bool> authenticated{false};
    onLogMessage logMessageCB;
    onDataEvent orderCB;
};

PrivateStream::PrivateStream(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &wsHost) :
    m_p(std::make_unique<P>()) {
    m_p->wsClient = std::make_unique<WebSocketClient>();
    m_p->wsClient->setEndpoint(wsHost.empty() ? WS_HOST_GLOBAL : wsHost, WS_PORT);
    m_p->wsClient->setPrivateAuth(loginRequestJson(apiKey, apiSecret, passphrase));

    m_p->wsClient->setDataEventCallback([this](const DataEvent &event) {
        if (event.channel == "orders" && m_p->orderCB) {
            m_p->orderCB(event);
        }
    });
}

PrivateStream::~PrivateStream() = default;

void PrivateStream::setLoggerCallback(const onLogMessage &onLogMessageCB) const {
    m_p->logMessageCB = onLogMessageCB;

    /// The session reports its login result through the logger; mirror it into
    /// `authenticated` so a caller can gate order submission on a real ack
    /// instead of a sleep.
    m_p->wsClient->setLoggerCallback([this](const LogSeverity severity, const std::string &message) {
        if (message.find("WebSocket authenticated") != std::string::npos) {
            m_p->authenticated = true;
        } else if (message.find("login failed") != std::string::npos) {
            m_p->authenticated = false;
        }

        if (m_p->logMessageCB) {
            m_p->logMessageCB(severity, message);
        }
    });
}

void PrivateStream::setOrderUpdateCallback(const onDataEvent &cb) const { m_p->orderCB = cb; }

void PrivateStream::subscribeOrders(const std::string &instType) const {
    nlohmann::json subscription;
    subscription["channel"] = "orders";
    subscription["instType"] = instType;

    /// Order matters: subscribe() creates the session and posts the connect
    /// work, run() then services it. Calling run() first starts an io_context
    /// with nothing queued, which returns immediately and kills the io thread —
    /// the connection is then never made and nothing is ever logged.
    m_p->wsClient->subscribe(subscription.dump());
    m_p->wsClient->run();
}

bool PrivateStream::isAuthenticated() const { return m_p->authenticated; }
} // namespace stonky::okx
