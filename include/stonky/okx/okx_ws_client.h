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
     * the first subscribe: the session logs in on connect and only then sends
     * subscriptions.
     * @param loginRequest serialized OKX `login` op
     */
    void setPrivateAuth(const std::string &loginRequest) const;

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
};
}

#endif //INCLUDE_STONKY_OKX_WS_CLIENT_H
