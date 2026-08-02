/**
OKX Execution Gateway

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_OKX_EXECUTION_GATEWAY_H
#define INCLUDE_STONKY_OKX_EXECUTION_GATEWAY_H

#include "stonky/interface/i_execution_gateway.h"
#include <memory>
#include <string>

namespace stonky::execution {

/**
 * IExecutionGateway over OKX v5, targeting the X-Perp product family.
 *
 * SYMBOLS ARE BASE COIN NAMES ("AAOI", "XLM"), not instIds. An X-Perp instId
 * carries an expiry suffix (AAOI-USD_UM_XPERP-310711) that CHANGES when the
 * venue relists, so letting it reach the strategy would break every persisted
 * book and open position across a relist. The gateway resolves base -> instId
 * through the stable instFamily and keeps the churn to itself.
 *
 * QUANTITIES ARE BASE UNITS. OKX sizes orders in CONTRACTS, where one contract
 * is ctVal of the base asset (XLM 100, XAU 0.001, AAOI 1), so the gateway
 * converts on the way out and reports specs in base units on the way in. A
 * caller that assumed contracts would be off by a factor of ctVal — silently,
 * and differently per symbol.
 */
class OkxExecutionGateway final : public IExecutionGateway {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    /**
     * @param apiKey / apiSecret / passphrase account credentials (OKX needs all three)
     * @param host REST host — an account belongs to ONE entity, see OKX::API_HOST_*
     * @param wsHost matching WebSocket host, see OKX::WS_HOST_*
     * @param instType instrument type to trade — "FUTURES" for X-Perps, "SWAP" for USDT perps
     */
    OkxExecutionGateway(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &host, const std::string &wsHost,
                        const std::string &instType);

    ~OkxExecutionGateway() override;

    [[nodiscard]] std::string name() const override;

    void start() override;

    InstrumentSpec instrumentSpec(const std::string &symbol) override;

    void refreshInstruments() override;

    void subscribeQuotes(const std::string &symbol) override;

    void unsubscribeQuotes(const std::string &symbol) override;

    std::optional<Quote> lastQuote(const std::string &symbol) override;

    void setOrderUpdateCallback(const onOrderUpdateEvent &cb) override;

    void setFillCallback(const onFillEvent &cb) override;

    void setQuoteCallback(const onQuoteEvent &cb) override;

    void submitPostOnlyLimit(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty, double price, bool reduceOnly) override;

    [[nodiscard]] bool supportsAmend() const override;

    void amendPrice(const std::string &clientOrderId, const std::string &symbol, double price) override;

    bool cancel(const std::string &clientOrderId, const std::string &symbol) override;

    void submitReduceOnlyMarket(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty) override;

    /// Resolve a base symbol to the venue instId, for callers that must talk to
    /// REST directly (position/funding queries). Empty when unknown.
    [[nodiscard]] std::string instIdFor(const std::string &symbol) const;

    /// Contract size in base units, for converting venue position/size fields.
    [[nodiscard]] double contractValue(const std::string &symbol) const;
};
} // namespace stonky::execution

#endif // INCLUDE_STONKY_OKX_EXECUTION_GATEWAY_H
