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
     * @param loginRequest Serialized OKX login op. When non-empty the session is
     *        treated as PRIVATE: the login is written first and subscriptions are
     *        only sent once the venue acknowledges it. A private session also does
     *        NOT self-close while it holds no subscriptions, because it is legitimately
     *        idle between the login ack and the first subscribe.
     */
    void run(const std::string &host, const std::string &port, const std::string &subscriptionRequest, const onDataEvent &dataEventCB,
             const std::string &path = "/ws/v5/public", const std::string &loginRequest = "");

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
};
} // namespace stonky::okx
#endif // INCLUDE_STONKY_OKX_WS_SESSION_H
