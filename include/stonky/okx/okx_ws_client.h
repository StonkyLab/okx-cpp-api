/**
OKX WebSocket Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_OKX_WS_CLIENT_H
#define INCLUDE_STONKY_OKX_WS_CLIENT_H

#include <stonky/utils/log_utils.h>
#include "okx_ws_session.h"
#include <string>
#include <functional>

namespace stonky::okx {
class WebSocketClient {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    WebSocketClient(const WebSocketClient &) = delete;

    WebSocketClient &operator=(const WebSocketClient &) = delete;

    WebSocketClient(WebSocketClient &&) noexcept = default;

    WebSocketClient &operator=(WebSocketClient &&) noexcept = default;

    WebSocketClient();

    ~WebSocketClient();

    /**
     * Run the WebSocket IO Context asynchronously and returns immediately without blocking the thread execution
     */
    void run() const;

    /**
     * Set logger callback, if no set then all errors are writen to the stderr stream only
     * @param onLogMessageCB
     */
    void setLoggerCallback(const onLogMessage &onLogMessageCB) const;

    /**
     * Set Data Message callback
     * @param onDataEventCB
     */
    void setDataEventCallback(const onDataEvent &onDataEventCB) const;

    /**
     * Override the endpoint. An OKX account belongs to ONE entity, and the WS
     * host must match it — see OKX::WS_HOST_GLOBAL / WS_HOST_EEA. Must be called
     * before the first subscribe.
     */
    void setEndpoint(const std::string &host, const std::string &port) const;

    /**
     * Turn this client into a PRIVATE (authenticated) one. Must be called before
     * the first subscribe: every (re)connected session logs in first and only
     * then sends subscriptions.
     * @param loginProvider invoked at each handshake — the login signature
     *        embeds a timestamp the venue rejects after ~30 s, so a stored
     *        request cannot survive a reconnect
     */
    void setPrivateAuth(const std::function<std::string()> &loginProvider) const;

    /**
     * Subscribe WebSocket according to the subscriptionRequest
     * @param subscriptionRequest
     */
    void subscribe(const std::string &subscriptionRequest) const;

    /**
     * Check if a stream is already subscribed
     * @param subscriptionRequest subscription request
     * @return True if subscribed
     */
    [[nodiscard]] bool isSubscribed(const std::string &subscriptionRequest) const;

    /**
     * Health of the CURRENT session: false when none exists or when its pong
     * age crossed the zombie threshold (the session then hard-closes itself
     * and the next subscribe() builds a replacement). Order-submit guards
     * consult this so an engine never trades against a dead event stream.
     */
    [[nodiscard]] bool isSessionAlive() const;

    /**
     * Invoked whenever subscribe() creates a NEW session — the first connect
     * and every rebuild after a death — before the session runs. Lets owners
     * reset per-connection state (e.g. a private stream's auth flag).
     */
    void setSessionRebuiltCallback(const std::function<void()> &cb) const;
};
}

#endif //INCLUDE_STONKY_OKX_WS_CLIENT_H
