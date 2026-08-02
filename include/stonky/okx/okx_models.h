/**
OKX Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_OKX_MODELS_H
#define INCLUDE_STONKY_OKX_MODELS_H

#include "stonky/interface/i_json.h"
#include "stonky/okx/okx_enums.h"
#include <nlohmann/json.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace stonky::okx {
struct Response : IJson {
    std::string code{};
    std::string msg{};
    nlohmann::json data{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Ticker final : IJson {
    InstrumentType instType{InstrumentType::SPOT};

    /// Instrument ID
    std::string instId{};

    /// Last traded price
    boost::multiprecision::cpp_dec_float_50 last{};

    /// Last traded size
    boost::multiprecision::cpp_dec_float_50 lastSz{};

    /// Best ask price
    boost::multiprecision::cpp_dec_float_50 askPx{};

    /// Best ask size
    boost::multiprecision::cpp_dec_float_50 askSz{};

    /// Best bid price
    boost::multiprecision::cpp_dec_float_50 bidPx{};

    /// Best bid size
    boost::multiprecision::cpp_dec_float_50 bidSz{};

    /// Open price in the past 24 hours
    boost::multiprecision::cpp_dec_float_50 open24h{};

    /// Highest price in the past 24 hours
    boost::multiprecision::cpp_dec_float_50 high24h{};

    /// Lowest price in the past 24 hours
    boost::multiprecision::cpp_dec_float_50 low24h{};

    /// 24h trading volume, with a unit of currency. If it is a derivatives contract, the value is the number
    /// of base currency. If it is SPOT/MARGIN, the value is the quantity in quote currency.
    boost::multiprecision::cpp_dec_float_50 volCcy24h{};

    /// 24h trading volume, with a unit of contract. If it is a derivatives contract, the value is the number of
    /// contracts. If it is SPOT/MARGIN, the value is the quantity in base currency.
    boost::multiprecision::cpp_dec_float_50 vol24h{};

    /// Open price in the UTC 0
    boost::multiprecision::cpp_dec_float_50 sodUtc0{};

    /// Open price in the UTC 8
    boost::multiprecision::cpp_dec_float_50 sodUtc8{};

    /// Ticker data generation time, Unix timestamp format in milliseconds
    std::int64_t ts{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Tickers final : Response {
    std::vector<Ticker> tickers{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Candle final : IJson {
    /// Opening time of the candlestick
    std::int64_t ts{};
    boost::multiprecision::cpp_dec_float_50 o{};
    boost::multiprecision::cpp_dec_float_50 h{};
    boost::multiprecision::cpp_dec_float_50 l{};
    boost::multiprecision::cpp_dec_float_50 c{};
    boost::multiprecision::cpp_dec_float_50 vol{};

    /// Trading volume, with a unit of currency. If it is a derivatives contract, the value is the number of base
    /// currency. If it is SPOT/MARGIN, the value is the quantity in quote currency.
    boost::multiprecision::cpp_dec_float_50 volCcy{};

    /// Trading volume, the value is the quantity in quote currency, e.g. the unit is USDT for BTC-USDT and BTC-USDT-SWAP;
    /// The unit is USD for BTC-USD-SWAP
    boost::multiprecision::cpp_dec_float_50 volCcyQuote{};

    /// The state of candlesticks. False represents that it is uncompleted, True represents that it is completed.
    bool confirm{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Candles final : Response {
    std::vector<Candle> candles{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct FundingRate final : Response {
    InstrumentType instType{InstrumentType::SPOT};
    std::string instId{};
    boost::multiprecision::cpp_dec_float_50 fundingRate{};
    std::int64_t fundingTime{};
    std::int64_t nextFundingTime{};
    boost::multiprecision::cpp_dec_float_50 nextFundingRate{};
    boost::multiprecision::cpp_dec_float_50 interestRate{};
    boost::multiprecision::cpp_dec_float_50 premium{};
    boost::multiprecision::cpp_dec_float_50 maxFundingRate{};
    boost::multiprecision::cpp_dec_float_50 minFundingRate{};
    std::int64_t ts{};
    SettState settState = SettState::processing;
    boost::multiprecision::cpp_dec_float_50 settFundingRate{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct FundingRates final : Response {
    std::vector<FundingRate> rates{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Instrument final : IJson {
    InstrumentType instType{InstrumentType::SPOT};

    /// Instrument ID, e.g. BTC-USD-SWAP
    std::string instId{};

    /// Instrument family, e.g. BTC-USD, only applicable to FUTURES/SWAP/OPTION
    std::string instFamily{};

    /// Underlying, e.g. BTC-USD, only applicable to FUTURES/SWAP/OPTION
    std::string uly{};

    /// Base currency, e.g. BTC inBTC-USDT, only applicable to SPOT/MARGIN
    std::string baseCcy{};

    /// Quote currency, e.g. USDT in BTC-USDT, only applicable to SPOT/MARGIN
    std::string quoteCcy{};

    /// Settlement and margin currency, e.g. BTC, only applicable to FUTURES/SWAP/OPTION
    std::string settleCcy{};

    /// Contract value, only applicable to FUTURES/SWAP/OPTION
    boost::multiprecision::cpp_dec_float_50 ctVal{};

    /// Contract multiplier, only applicable to FUTURES/SWAP/OPTION
    boost::multiprecision::cpp_dec_float_50 ctMult{};

    /// Contract value currency, only applicable to FUTURES/SWAP/OPTIO
    std::string ctValCcy{};

    /// Option type, C: Call P: put, only applicable to OPTION
    OptionType optType{OptionType::C};

    /// Strike price, only applicable to OPTION
    boost::multiprecision::cpp_dec_float_50 stk{};

    /// Listing time, Unix timestamp format in milliseconds
    std::int64_t listTime{};

    /// Expiry time, Unix timestamp format in milliseconds
    std::int64_t expTime{};

    /// Max Leverage, not applicable to SPOT, OPTION
    boost::multiprecision::cpp_dec_float_50 lever{};

    /// Tick size, e.g. 0.0001
    boost::multiprecision::cpp_dec_float_50 tickSz{};

    /// Lot size, e.g. BTC-USDT-SWAP: 1
    boost::multiprecision::cpp_dec_float_50 lotSz{};

    /// Minimum order size. If it is a derivatives contract, the value is the number of contracts. If it is SPOT/MARGIN,
    /// the value is the quantity in base currency
    boost::multiprecision::cpp_dec_float_50 minSz{};

    /// Contract type, linear: linear contract, inverse: inverse contract, only applicable to FUTURES/SWAP
    ContractType ctType{ContractType::linear};

    /// Alias, only applicable to FUTURES
    FuturesAlias alias = {FuturesAlias::this_week};

    /// Instrument status
    InstrumentStatus state = {InstrumentStatus::live};

    /// The maximum order quantity of the contract or spot limit order. If it is a derivatives contract, the value
    /// is the number of contracts. If it is SPOT/MARGIN, the value is the quantity in base currency
    boost::multiprecision::cpp_dec_float_50 maxLmtSz{};

    /// The maximum order quantity of the contract or spot market order. If it is a derivatives contract, the value is
    /// the number of contracts. If it is SPOT/MARGIN, the value is the quantity in "USDT
    boost::multiprecision::cpp_dec_float_50 maxMktSz{};

    /// The maximum order quantity of the contract or spot twap order. If it is a derivatives contract, the value is
    /// the number of contracts. If it is SPOT/MARGIN, the value is the quantity in base currency.
    boost::multiprecision::cpp_dec_float_50 maxTwapSz{};

    /// The maximum order quantity of the contract or spot iceBerg order. If it is a derivatives contract, the value
    /// is the number of contracts. If it is SPOT/MARGIN, the value is the quantity in base currency.
    boost::multiprecision::cpp_dec_float_50 maxIcebergSz{};

    /// The maximum order quantity of the contract or spot trigger order. If it is a derivatives contract, the value is
    /// the number of contracts. If it is SPOT/MARGIN, the value is the quantity in base currency.
    boost::multiprecision::cpp_dec_float_50 maxTriggerSz{};

    /// The maximum order quantity of the contract or spot stop market order. If it is a derivatives contract, the
    /// value is the number of contracts. If it is SPOT/MARGIN, the value is the quantity in "USDT".
    boost::multiprecision::cpp_dec_float_50 maxStopSz{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Instruments final : Response {
    std::vector<Instrument> instruments{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct BalanceDetail final : IJson {
    boost::multiprecision::cpp_dec_float_50 availBal{};
    boost::multiprecision::cpp_dec_float_50 availEq{};
    boost::multiprecision::cpp_dec_float_50 cashBal{};
    std::string ccy{};
    boost::multiprecision::cpp_dec_float_50 crossLiab{};
    boost::multiprecision::cpp_dec_float_50 disEq{};
    boost::multiprecision::cpp_dec_float_50 eq{};
    boost::multiprecision::cpp_dec_float_50 eqUsd{};
    boost::multiprecision::cpp_dec_float_50 frozenBal{};
    boost::multiprecision::cpp_dec_float_50 interest{};
    boost::multiprecision::cpp_dec_float_50 isoEq{};
    boost::multiprecision::cpp_dec_float_50 isoLiab{};
    boost::multiprecision::cpp_dec_float_50 isoUpl{};
    boost::multiprecision::cpp_dec_float_50 liab{};
    boost::multiprecision::cpp_dec_float_50 maxLoan{};
    boost::multiprecision::cpp_dec_float_50 mgnRatio{};
    boost::multiprecision::cpp_dec_float_50 notionalLever{};
    boost::multiprecision::cpp_dec_float_50 ordFrozen{};
    boost::multiprecision::cpp_dec_float_50 twap{};
    boost::multiprecision::cpp_dec_float_50 upl{};
    std::int64_t uTime{};
    boost::multiprecision::cpp_dec_float_50 uplLiab{};
    boost::multiprecision::cpp_dec_float_50 stgyEq{};
    boost::multiprecision::cpp_dec_float_50 spotInUseAmt{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Balance final : Response {
    boost::multiprecision::cpp_dec_float_50 adjEq{};
    boost::multiprecision::cpp_dec_float_50 imr{};
    boost::multiprecision::cpp_dec_float_50 isoEq{};
    boost::multiprecision::cpp_dec_float_50 mgnRatio{};
    boost::multiprecision::cpp_dec_float_50 mmr{};
    boost::multiprecision::cpp_dec_float_50 notionalUsd{};
    boost::multiprecision::cpp_dec_float_50 ordFroz{};
    boost::multiprecision::cpp_dec_float_50 totalEq{};
    std::int64_t uTime{};

    std::vector<BalanceDetail> balanceDetails{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct SystemTime final : Response {
    std::int64_t ts{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Position final : IJson {
    std::int32_t adl{};
    boost::multiprecision::cpp_dec_float_50 availPos{};
    boost::multiprecision::cpp_dec_float_50 avgPx{};
    std::int64_t cTime{};
    std::string ccy{};
    boost::multiprecision::cpp_dec_float_50 imr{};
    std::string instId{};
    InstrumentType instType{InstrumentType::MARGIN};
    boost::multiprecision::cpp_dec_float_50 interest{};
    boost::multiprecision::cpp_dec_float_50 last{};
    boost::multiprecision::cpp_dec_float_50 lever{};
    boost::multiprecision::cpp_dec_float_50 liab{};
    std::string liabCcy{};
    boost::multiprecision::cpp_dec_float_50 liqPx{};
    boost::multiprecision::cpp_dec_float_50 margin{};
    boost::multiprecision::cpp_dec_float_50 markPx{};
    MarginMode mgnMode{MarginMode::cross};
    boost::multiprecision::cpp_dec_float_50 mgnRatio{};
    boost::multiprecision::cpp_dec_float_50 mmr{};
    boost::multiprecision::cpp_dec_float_50 notionalUsd{};
    boost::multiprecision::cpp_dec_float_50 pos{};
    std::string posCcy{};
    std::string posId{};
    PositionSide posSide{PositionSide::_net};
    std::string tradeId{};
    std::int64_t uTime{};
    boost::multiprecision::cpp_dec_float_50 upl{};
    boost::multiprecision::cpp_dec_float_50 uplRatio{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Positions final : Response {
    std::vector<Position> positions{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct Order final : IJson {
    std::string instId{};
    MarginMode tdMode{MarginMode::cross};
    std::string clOrdId{};
    Side side{Side::buy};
    std::string ccy{};
    PositionSide posSide{PositionSide::_net};
    OrderType ordType{OrderType::market};
    boost::multiprecision::cpp_dec_float_50 sz{};
    boost::multiprecision::cpp_dec_float_50 px{};
    /// Close-only order: the venue rejects it if it would open or increase a
    /// position. Required by any hedged strategy whose unwind must never flip a
    /// leg. Serialized only when set — sending it on an unsupported combination
    /// is itself a reject.
    bool reduceOnly{false};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct OrderResponse final : IJson {
    std::string clOrdId{};
    std::string ordId{};
    std::string tag{};
    std::string sCode{};
    std::string sMsg{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct OrderResponses final : Response {
    std::vector<OrderResponse> orderResponses{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct OrderDetail final : IJson {
    InstrumentType instType{InstrumentType::MARGIN};
    std::string instId{};
    std::string ccy{};
    std::string ordId{};
    std::string clOrdId{};
    boost::multiprecision::cpp_dec_float_50 px{};
    boost::multiprecision::cpp_dec_float_50 sz{};
    boost::multiprecision::cpp_dec_float_50 pnl{};
    OrderType ordType{OrderType::market};
    Side side{Side::buy};
    PositionSide posSide{PositionSide::_long};
    MarginMode tdMode = MarginMode::isolated;
    boost::multiprecision::cpp_dec_float_50 accFillSz{};
    boost::multiprecision::cpp_dec_float_50 fillPx{};
    std::string tradeId{};
    boost::multiprecision::cpp_dec_float_50 fillSz{};
    std::int64_t fillTime{};
    OrderState state{OrderState::live};
    boost::multiprecision::cpp_dec_float_50 avgPx{};
    boost::multiprecision::cpp_dec_float_50 lever{};
    std::int64_t uTime{};
    std::int64_t cTime{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

struct OrderDetails final : Response {
    std::vector<OrderDetail> orderDetails{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};
/// Single file info from market data history response
struct MarketDataFileInfo final : IJson {
    /// Data file name, e.g. BTC-USDT-SWAP-trades-2025-05-15.zip
    std::string filename{};

    /// Data date timestamp, Unix timestamp format in milliseconds
    std::int64_t dateTs{};

    /// File size in MB
    std::string sizeMB{};

    /// Download URL
    std::string url{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

/// Group detail containing files for a specific instrument
struct MarketDataGroupDetail final : IJson {
    /// Instrument ID (for SPOT)
    std::string instId{};

    /// Instrument family (for non-SPOT)
    std::string instFamily{};

    /// Instrument type
    InstrumentType instType{InstrumentType::SWAP};

    /// Data range start date, Unix timestamp format in milliseconds (inclusive)
    std::int64_t dateRangeStart{};

    /// Data range end date, Unix timestamp format in milliseconds (inclusive)
    std::int64_t dateRangeEnd{};

    /// Data group size in MB
    std::string groupSizeMB{};

    /// List of downloadable files
    std::vector<MarketDataFileInfo> groupDetails{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};

/// Response from /api/v5/public/market-data-history endpoint
struct MarketDataHistory final : Response {
    /// Response timestamp, Unix timestamp format in milliseconds
    std::int64_t ts{};

    /// Total size of all data files in MB
    std::string totalSizeMB{};

    /// Date aggregation type (daily or monthly)
    DateAggrType dateAggrType{DateAggrType::daily};

    /// List of group details
    std::vector<MarketDataGroupDetail> details{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json &json) override;
};
}

#endif //INCLUDE_STONKY_OKX_MODELS_H
