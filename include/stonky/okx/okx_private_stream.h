/**
OKX Private WebSocket Stream

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_OKX_PRIVATE_STREAM_H
#define INCLUDE_STONKY_OKX_PRIVATE_STREAM_H

#include "stonky/utils/log_utils.h"
#include "okx_ws_session.h"
#include <memory>
#include <string>

namespace stonky::okx {

/**
 * Authenticated WebSocket stream: order lifecycle and fills.
 *
 * Separate from WSStreamManager on purpose. That one is a POLLING cache
 * (readEventTicker blocks until data shows up), which suits a downloader; an
 * execution engine needs the opposite — order updates PUSHED the instant the
 * venue emits them, because a chasing executor reprices against them.
 *
 * The `orders` channel carries both state transitions and fills: a partially or
 * fully filled update also carries fillSz/fillPx/tradeId for that specific
 * fill, so one subscription feeds both an order-update and a fill consumer.
 * Events are delivered RAW (DataEvent::data holds the venue's array) so the
 * consumer decides what a field means — this class does not model orders.
 */
class PrivateStream {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    /**
     * @param wsHost must match the entity the key belongs to — see
     *        OKX::WS_HOST_GLOBAL / WS_HOST_EEA. Empty selects the global host.
     */
    PrivateStream(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &wsHost = "");

    ~PrivateStream();

    PrivateStream(const PrivateStream &) = delete;

    PrivateStream &operator=(const PrivateStream &) = delete;

    void setLoggerCallback(const onLogMessage &onLogMessageCB) const;

    /// Push callback for `orders` events. Set it BEFORE subscribing.
    void setOrderUpdateCallback(const onDataEvent &cb) const;

    /**
     * Connect, authenticate and subscribe the account's order stream.
     * @param instType "FUTURES" (X-Perps live here), "SWAP", "ANY", ...
     */
    void subscribeOrders(const std::string &instType) const;

    /// True once the venue acknowledged the login.
    [[nodiscard]] bool isAuthenticated() const;
};
} // namespace stonky::okx

#endif // INCLUDE_STONKY_OKX_PRIVATE_STREAM_H
