/**
OKX Execution Gateway

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx_execution_gateway.h"
#include "stonky/okx/okx.h"
#include "stonky/okx/okx_private_stream.h"
#include "stonky/okx/okx_rest_client.h"
#include "stonky/okx/okx_ws_client.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace stonky::execution {
using namespace stonky::okx;

namespace {
/// Instrument facts the gateway needs, all in BASE units except where noted.
struct InstrumentInfo {
    std::string instId;
    double ctVal{1.0};   ///< base units per contract
    double lotSz{0.0};   ///< contract step
    double minSz{0.0};   ///< min contracts
    double tickSize{0.0};
    double maxLmtSz{0.0}; ///< max contracts per limit order (0 = unknown)
};

double toDouble(const boost::multiprecision::cpp_dec_float_50 &v) { return v.convert_to<double>(); }

/// Round a contract count DOWN to a whole number of lots. Submitting a size
/// that is not a lot multiple is rejected outright (51121).
double floorToLot(const double contracts, const double lotSz) {
    if (lotSz <= 0.0) {
        return contracts;
    }
    return std::floor(contracts / lotSz + 1e-9) * lotSz;
}

/**
 * Map a venue rejection to the chase core's reaction.
 *
 * Codes are the authoritative signal; the message is also scanned because OKX
 * does NOT publish a placement code for "post-only would have taken liquidity"
 * — it accepts and then cancels such an order, reporting the reason
 * asynchronously. Until that path is observed live (see the cancelSource note
 * in the orders handler) a text match is what keeps a routine maker cross from
 * being escalated into a leg-killing hard reject.
 */
RejectKind classifyReject(const std::string &code, const std::string &message) {
    std::string lower = message;
    std::ranges::transform(lower, lower.begin(), [](const unsigned char c) { return std::tolower(c); });

    if (lower.find("post only") != std::string::npos || lower.find("post-only") != std::string::npos || lower.find("post_only") != std::string::npos) {
        return RejectKind::BenignPostOnlyCross;
    }

    /// 51020 below min amount, 51121 not a lot multiple — both mean this size can
    /// never rest, which the core treats as fatal only while nothing has filled.
    if (code == "51020" || code == "51121") {
        return RejectKind::MinNotional;
    }

    /// The order is already gone: a reduce/cancel has achieved its goal.
    if (code == "51400" || code == "51401" || code == "51402" || code == "51410" || code == "51503" || code == "51603") {
        return RejectKind::PositionClosed;
    }

    /// Rate limiting — never reached the book, so back off without counting
    /// toward the fatal cap.
    if (code == "50011" || code == "50013" || code == "50026") {
        return RejectKind::Throttled;
    }

    /// Configuration or permission problems no retry can fix this cycle.
    if (code == "50101" || code == "50103" || code == "50104" || code == "50111" || code == "50113" || code == "50114" || code == "51000" || code == "51001" ||
        code == "51002" || code == "51015") {
        return RejectKind::Permanent;
    }

    return RejectKind::Hard;
}

OrderState parseState(const std::string &state) {
    if (state == "filled") {
        return OrderState::Filled;
    }
    if (state == "partially_filled") {
        return OrderState::PartiallyFilled;
    }
    if (state == "canceled" || state == "mmp_canceled") {
        return OrderState::Cancelled;
    }
    return OrderState::Accepted; /// "live"
}

/**
 * clOrdId translation. OKX allows ONLY case-sensitive alphanumerics (1-32
 * chars) in clOrdId, while the chase core's ids are "fa-<hexms>-<hexctr>" —
 * every order was batch-rejected on the first live cycle because of the
 * hyphens. The core's ids consist of a lowercase prefix and hex digits, so the
 * letter 'Z' can never occur in them: replacing '-' with 'Z' on the way out and
 * back is bijective and needs no lookup table (which would need lifetime
 * management across reconnect replays).
 */
std::string toVenueOrderId(std::string id) {
    std::ranges::replace(id, '-', 'Z');
    return id;
}

std::string fromVenueOrderId(std::string id) {
    std::ranges::replace(id, 'Z', '-');
    return id;
}

double numFromJson(const nlohmann::json &json, const std::string &key) {
    const auto it = json.find(key);
    if (it == json.end() || !it->is_string() || it->get<std::string>().empty()) {
        return 0.0;
    }
    try {
        return std::stod(it->get<std::string>());
    } catch (...) {
        return 0.0;
    }
}
} // namespace

struct OkxExecutionGateway::P {
    std::string instType;
    std::unique_ptr<RESTClient> restClient;
    std::unique_ptr<PrivateStream> privateStream;
    std::unique_ptr<WebSocketClient> quoteClient;

    mutable std::recursive_mutex instrumentsM;
    std::map<std::string, InstrumentInfo> instruments; ///< base symbol -> info
    std::map<std::string, std::string> instIdToSymbol;

    mutable std::recursive_mutex quotesM;
    std::map<std::string, Quote> quotes; ///< base symbol -> quote

    onOrderUpdateEvent orderCB;
    onFillEvent fillCB;
    onQuoteEvent quoteCB;

    /// Fills are deduped on the venue's tradeId: the orders channel re-delivers
    /// an update on reconnect, and crediting a fill twice desynchronises the
    /// strategy's position from the venue's.
    mutable std::recursive_mutex fillsM;
    std::set<std::string> seenFills;

    [[nodiscard]] std::optional<InstrumentInfo> info(const std::string &symbol) const {
        std::lock_guard lk(instrumentsM);
        const auto it = instruments.find(symbol);
        return it == instruments.end() ? std::nullopt : std::optional{it->second};
    }

    void handleOrderEvent(const DataEvent &event) {
        if (!event.data.is_array()) {
            return;
        }

        for (const auto &row: event.data) {
            const auto instId = row.value("instId", "");
            std::string symbol;
            {
                std::lock_guard lk(instrumentsM);
                if (const auto it = instIdToSymbol.find(instId); it != instIdToSymbol.end()) {
                    symbol = it->second;
                }
            }
            if (symbol.empty()) {
                continue; /// not ours (another strategy on the same account)
            }

            const auto clOrdId = fromVenueOrderId(row.value("clOrdId", ""));
            const auto stateStr = row.value("state", "");
            const auto code = row.value("code", "");
            const auto msg = row.value("msg", "");
            const auto cancelSource = row.value("cancelSource", "");

            /// Fill first: an update can be terminal AND carry the last fill.
            const auto fillSz = numFromJson(row, "fillSz");
            const auto tradeId = row.value("tradeId", "");
            if (fillSz > 0.0 && !tradeId.empty() && fillCB) {
                bool fresh = false;
                {
                    std::lock_guard lk(fillsM);
                    fresh = seenFills.insert(tradeId).second;
                }
                if (fresh) {
                    const auto ctVal = info(symbol) ? info(symbol)->ctVal : 1.0;
                    FillEvent fill;
                    fill.clientOrderId = clOrdId;
                    fill.symbol = symbol;
                    fill.fillId = tradeId;
                    fill.qty = fillSz * ctVal; /// contracts -> base units
                    fill.price = numFromJson(row, "fillPx");
                    fill.isMaker = row.value("execType", "") == "M";
                    fillCB(fill);
                }
            }

            if (!orderCB) {
                continue;
            }

            OrderUpdate update;
            update.clientOrderId = clOrdId;
            update.symbol = symbol;
            update.price = numFromJson(row, "px");
            const auto ctVal = info(symbol) ? info(symbol)->ctVal : 1.0;
            update.cumFilledQty = numFromJson(row, "accFillSz") * ctVal;
            update.state = parseState(stateStr);

            /// A non-empty code on an order event is a venue rejection.
            if (!code.empty() && code != "0") {
                update.state = OrderState::Rejected;
                update.reason = fmt::format("code {}: {}", code, msg);
                update.rejectKind = classifyReject(code, msg);
            } else if (update.state == OrderState::Cancelled && !cancelSource.empty()) {
                /// cancelSource is how a post-only cross surfaces: the order is
                /// accepted and then cancelled by the matching engine. The exact
                /// value is carried into the reason so the first live chase
                /// identifies it — classify it explicitly here once observed.
                update.reason = fmt::format("cancelSource {}", cancelSource);
            }

            orderCB(update);
        }
    }

    void handleQuoteEvent(const DataEvent &event) {
        if (!event.data.is_array() || event.data.empty()) {
            return;
        }

        std::string symbol;
        {
            std::lock_guard lk(instrumentsM);
            if (const auto it = instIdToSymbol.find(event.instId); it != instIdToSymbol.end()) {
                symbol = it->second;
            }
        }
        if (symbol.empty()) {
            return;
        }

        const auto &row = event.data.front();
        Quote quote;
        quote.bid = numFromJson(row, "bidPx");
        quote.ask = numFromJson(row, "askPx");
        quote.receivedAt = std::chrono::steady_clock::now();

        if (!quote.sane()) {
            return;
        }

        {
            std::lock_guard lk(quotesM);
            quotes[symbol] = quote;
        }

        if (quoteCB) {
            quoteCB(symbol, quote);
        }
    }
};

OkxExecutionGateway::OkxExecutionGateway(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &host,
                                         const std::string &wsHost, const std::string &instType) : m_p(std::make_unique<P>()) {
    m_p->instType = instType.empty() ? "FUTURES" : instType;
    m_p->restClient = std::make_unique<RESTClient>(apiKey, apiSecret, passphrase, host);
    m_p->privateStream = std::make_unique<PrivateStream>(apiKey, apiSecret, passphrase, wsHost);
    m_p->quoteClient = std::make_unique<WebSocketClient>();
    m_p->quoteClient->setEndpoint(wsHost.empty() ? WS_HOST_GLOBAL : wsHost, "8443");
}

OkxExecutionGateway::~OkxExecutionGateway() = default;

std::string OkxExecutionGateway::name() const { return "OKX"; }

void OkxExecutionGateway::refreshInstruments() {
    const auto type = m_p->instType == "SWAP" ? InstrumentType::SWAP : InstrumentType::FUTURES;
    const auto instruments = m_p->restClient->getInstruments(type, true);

    std::lock_guard lk(m_p->instrumentsM);
    m_p->instruments.clear();
    m_p->instIdToSymbol.clear();

    for (const auto &instrument: instruments) {
        if (instrument.state != InstrumentStatus::live) {
            continue;
        }

        const auto dash = instrument.instId.find('-');
        if (dash == std::string::npos) {
            continue;
        }
        const auto symbol = instrument.instId.substr(0, dash);

        /// X-Perps and dated futures share instType FUTURES. Prefer the X-Perp
        /// (its family carries the _XPERP marker) so a base symbol never
        /// resolves to a contract that actually expires.
        const bool isXPerp = instrument.instFamily.find("XPERP") != std::string::npos;
        if (const auto existing = m_p->instruments.find(symbol); existing != m_p->instruments.end()) {
            const bool existingIsXPerp = existing->second.instId.find("XPERP") != std::string::npos;
            if (existingIsXPerp || !isXPerp) {
                continue;
            }
            m_p->instIdToSymbol.erase(existing->second.instId);
        }

        InstrumentInfo info;
        info.instId = instrument.instId;
        info.ctVal = toDouble(instrument.ctVal) > 0.0 ? toDouble(instrument.ctVal) : 1.0;
        info.lotSz = toDouble(instrument.lotSz);
        info.minSz = toDouble(instrument.minSz);
        info.tickSize = toDouble(instrument.tickSz);

        m_p->instruments[symbol] = info;
        m_p->instIdToSymbol[info.instId] = symbol;
    }

    spdlog::info("OkxExecutionGateway: {} live {} instruments cached", m_p->instruments.size(), m_p->instType);
}

void OkxExecutionGateway::start() {
    refreshInstruments();

    m_p->privateStream->setLoggerCallback([](const LogSeverity severity, const std::string &message) {
        severity == LogSeverity::Error ? spdlog::error("OKX: {}", message) : spdlog::info("OKX: {}", message);
    });
    m_p->privateStream->setOrderUpdateCallback([this](const DataEvent &event) { m_p->handleOrderEvent(event); });
    m_p->privateStream->subscribeOrders(m_p->instType);

    /// House invariant (same as the Bybit gateway): orders must never be placed
    /// before fills can be observed. Bounded rather than indefinite — a venue
    /// hiccup at boot should degrade to a warning, not a dead bot; if auth is
    /// genuinely broken, order placement fails loudly on its own.
    for (int i = 0; i < 100 && !m_p->privateStream->isAuthenticated(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!m_p->privateStream->isAuthenticated()) {
        spdlog::warn("OkxExecutionGateway: private stream not authenticated within 10 s — fills may be missed until it recovers");
    }

    m_p->quoteClient->setLoggerCallback([](const LogSeverity severity, const std::string &message) {
        severity == LogSeverity::Error ? spdlog::error("OKX quotes: {}", message) : spdlog::debug("OKX quotes: {}", message);
    });
    m_p->quoteClient->setDataEventCallback([this](const DataEvent &event) {
        if (event.channel == "tickers") {
            m_p->handleQuoteEvent(event);
        }
    });
}

InstrumentSpec OkxExecutionGateway::instrumentSpec(const std::string &symbol) {
    const auto info = m_p->info(symbol);
    if (!info) {
        throw GatewayError(RejectKind::Permanent, fmt::format("OKX: unknown instrument {}", symbol));
    }

    /// Reported in BASE units: the venue steps in contracts, one contract being
    /// ctVal of the base asset.
    InstrumentSpec spec;
    spec.symbol = symbol;
    spec.tickSize = info->tickSize;
    spec.qtyStep = info->lotSz * info->ctVal;
    spec.minQty = info->minSz * info->ctVal;
    spec.maxQty = info->maxLmtSz > 0.0 ? info->maxLmtSz * info->ctVal : 0.0;
    spec.minNotional = 0.0; /// OKX constrains size, not notional
    return spec;
}

void OkxExecutionGateway::subscribeQuotes(const std::string &symbol) {
    const auto info = m_p->info(symbol);
    if (!info) {
        return;
    }

    nlohmann::json subscription;
    subscription["channel"] = "tickers";
    subscription["instId"] = info->instId;

    /// subscribe() posts the connect work, run() services it — the reverse order
    /// starts an io_context with an empty queue, which exits immediately.
    m_p->quoteClient->subscribe(subscription.dump());
    m_p->quoteClient->run();
}

void OkxExecutionGateway::unsubscribeQuotes(const std::string &) {
    /// The venue caps subscriptions generously and the strategy re-subscribes
    /// the same handful of symbols every cycle; churning them would cost more
    /// than the idle stream.
}

std::optional<Quote> OkxExecutionGateway::lastQuote(const std::string &symbol) {
    std::lock_guard lk(m_p->quotesM);
    const auto it = m_p->quotes.find(symbol);
    return it == m_p->quotes.end() ? std::nullopt : std::optional{it->second};
}

void OkxExecutionGateway::setOrderUpdateCallback(const onOrderUpdateEvent &cb) { m_p->orderCB = cb; }

void OkxExecutionGateway::setFillCallback(const onFillEvent &cb) { m_p->fillCB = cb; }

void OkxExecutionGateway::setQuoteCallback(const onQuoteEvent &cb) { m_p->quoteCB = cb; }

void OkxExecutionGateway::submitPostOnlyLimit(const std::string &clientOrderId, const std::string &symbol, const OrderSide side, const double qty, const double price,
                                              const bool reduceOnly) {
    const auto info = m_p->info(symbol);
    if (!info) {
        throw GatewayError(RejectKind::Permanent, fmt::format("OKX: unknown instrument {}", symbol));
    }

    const auto contracts = floorToLot(qty / info->ctVal, info->lotSz);
    if (contracts <= 0.0 || contracts < info->minSz) {
        throw GatewayError(RejectKind::MinNotional,
                           fmt::format("OKX: qty {} = {} contracts, below min {} (ctVal {})", qty, contracts, info->minSz, info->ctVal));
    }

    Order order;
    order.instId = info->instId;
    order.tdMode = MarginMode::cross;
    order.clOrdId = toVenueOrderId(clientOrderId);
    order.side = side == OrderSide::Buy ? Side::buy : Side::sell;
    order.posSide = PositionSide::_net;
    order.ordType = OrderType::post_only;
    order.sz = contracts;
    order.px = price;
    order.reduceOnly = reduceOnly;

    const auto responses = m_p->restClient->placeOrder(order);
    if (responses.empty()) {
        throw GatewayError(RejectKind::Hard, "OKX: empty placeOrder response");
    }

    if (const auto &response = responses.front(); !response.sCode.empty() && response.sCode != "0") {
        throw GatewayError(classifyReject(response.sCode, response.sMsg), fmt::format("code {}: {}", response.sCode, response.sMsg));
    }
}

bool OkxExecutionGateway::supportsAmend() const {
    /// The REST client has no amend-order call yet; the chase core falls back to
    /// cancel + repost, exactly as it does on Lighter and Hyperliquid.
    return false;
}

void OkxExecutionGateway::amendPrice(const std::string &, const std::string &, double) {
    throw GatewayError(RejectKind::Hard, "OKX: amend not wired (cancel + resubmit)");
}

bool OkxExecutionGateway::cancel(const std::string &clientOrderId, const std::string &symbol) {
    const auto info = m_p->info(symbol);
    if (!info) {
        return false;
    }

    try {
        const auto responses = m_p->restClient->cancelOrder(info->instId, toVenueOrderId(clientOrderId));
        if (responses.empty()) {
            return false;
        }

        const auto &response = responses.front();
        if (!response.sCode.empty() && response.sCode != "0") {
            /// Already gone counts as cancelled — the caller's goal is met.
            return classifyReject(response.sCode, response.sMsg) == RejectKind::PositionClosed;
        }
        return true;
    } catch (std::exception &e) {
        spdlog::debug("OKX: cancel {} failed: {}", clientOrderId, e.what());
        return false;
    }
}

void OkxExecutionGateway::submitReduceOnlyMarket(const std::string &clientOrderId, const std::string &symbol, const OrderSide side, const double qty) {
    const auto info = m_p->info(symbol);
    if (!info) {
        throw GatewayError(RejectKind::Permanent, fmt::format("OKX: unknown instrument {}", symbol));
    }

    const auto contracts = floorToLot(qty / info->ctVal, info->lotSz);
    if (contracts <= 0.0 || contracts < info->minSz) {
        throw GatewayError(RejectKind::MinNotional,
                           fmt::format("OKX: close qty {} = {} contracts, below min {}", qty, contracts, info->minSz));
    }

    Order order;
    order.instId = info->instId;
    order.tdMode = MarginMode::cross;
    order.clOrdId = toVenueOrderId(clientOrderId);
    order.side = side == OrderSide::Buy ? Side::buy : Side::sell;
    order.posSide = PositionSide::_net;
    order.ordType = OrderType::market;
    order.sz = contracts;
    order.reduceOnly = true;

    const auto responses = m_p->restClient->placeOrder(order);
    if (responses.empty()) {
        throw GatewayError(RejectKind::Hard, "OKX: empty placeOrder response");
    }

    if (const auto &response = responses.front(); !response.sCode.empty() && response.sCode != "0") {
        throw GatewayError(classifyReject(response.sCode, response.sMsg), fmt::format("code {}: {}", response.sCode, response.sMsg));
    }
}

void OkxExecutionGateway::ensureOrderStream() { m_p->privateStream->subscribeOrders(m_p->instType); }

std::string OkxExecutionGateway::instIdFor(const std::string &symbol) const {
    const auto info = m_p->info(symbol);
    return info ? info->instId : std::string{};
}

double OkxExecutionGateway::contractValue(const std::string &symbol) const {
    const auto info = m_p->info(symbol);
    return info ? info->ctVal : 0.0;
}
} // namespace stonky::execution
