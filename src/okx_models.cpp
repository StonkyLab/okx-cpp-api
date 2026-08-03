/**
OKX Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx_models.h"
#include "stonky/okx/okx.h"
#include "stonky/utils/utils.h"
#include "stonky/utils/json_utils.h"
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <utility>

namespace stonky::okx {
bool
readDecimalValue(const nlohmann::json &json, const std::string &key, boost::multiprecision::cpp_dec_float_50 &value,
                 boost::multiprecision::cpp_dec_float_50 defaultVal = boost::multiprecision::cpp_dec_float_50("0")) {
    if (const auto it = json.find(key); it != json.end()) {
        if (!it.value().is_null() && it->is_string() && !it->get<std::string>().empty()) {
            value.assign(it->get<std::string>());
            return true;
        }
        value = std::move(defaultVal);
    }

    return false;
}

nlohmann::json Response::toJson() const {
    nlohmann::json json;
    json["code"] = code;
    json["msg"] = msg;
    return json;
}

void Response::fromJson(const nlohmann::json &json) {
    readValue<std::string>(json, "code", code);
    readValue<std::string>(json, "msg", msg);
    data = json["data"];
}

nlohmann::json Ticker::toJson() const {
    throw std::runtime_error("Unimplemented: Ticker::toJson()");
}

void Ticker::fromJson(const nlohmann::json &json) {
    readMagicEnum<InstrumentType>(json, "instType", instType);
    readValue<std::string>(json, "instId", instId);
    readDecimalValue(json, "last", last);
    readDecimalValue(json, "lastSz", lastSz);
    readDecimalValue(json, "askPx", askPx);
    readDecimalValue(json, "askSz", askSz);
    readDecimalValue(json, "bidPx", bidPx);
    readDecimalValue(json, "bidSz", bidSz);
    readDecimalValue(json, "open24h", open24h);
    readDecimalValue(json, "high24h", high24h);
    readDecimalValue(json, "low24h", low24h);
    readDecimalValue(json, "volCcy24h", volCcy24h);
    readDecimalValue(json, "vol24h", vol24h);
    readDecimalValue(json, "sodUtc0", sodUtc0);
    readDecimalValue(json, "sodUtc8", sodUtc8);
    readDecimalValue(json, "vol24h", vol24h);
    ts = readStringAsInt64(json, "ts");
}

nlohmann::json Tickers::toJson() const {
    throw std::runtime_error("Unimplemented: Tickers::toJson()");
}

void Tickers::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        Ticker ticker;
        ticker.fromJson(el.value());
        tickers.push_back(ticker);
    }
}

nlohmann::json Candle::toJson() const {
    throw std::runtime_error("Unimplemented: Candle::toJson()");
}

void Candle::fromJson(const nlohmann::json &json) {
    ts = stoll(json[0].get<std::string>());
    o = boost::multiprecision::cpp_dec_float_50(json[1].get<std::string>());
    h = boost::multiprecision::cpp_dec_float_50(json[2].get<std::string>());
    l = boost::multiprecision::cpp_dec_float_50(json[3].get<std::string>());
    c = boost::multiprecision::cpp_dec_float_50(json[4].get<std::string>());
    vol = boost::multiprecision::cpp_dec_float_50(json[5].get<std::string>());
    volCcy = boost::multiprecision::cpp_dec_float_50(json[6].get<std::string>());
    volCcyQuote = boost::multiprecision::cpp_dec_float_50(json[7].get<std::string>());
    confirm = string2bool(json[8].get<std::string>());
}

nlohmann::json Candles::toJson() const {
    throw std::runtime_error("Unimplemented: Candles::toJson()");
}

void Candles::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        Candle candle;
        candle.fromJson(el.value());
        candles.push_back(candle);
    }
}

nlohmann::json FundingRate::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRate::toJson()");
}

void FundingRate::fromJson(const nlohmann::json &json) {
    if (json.contains("data")) {
        Response::fromJson(json);
        if (!data.empty()) {
            data = data[0];
        }
    } else {
        data = json;
    }

    if (!data.empty()) {
        readMagicEnum<InstrumentType>(data, "instType", instType);
        readValue<std::string>(data, "instId", instId);
        readDecimalValue(data, "fundingRate", fundingRate);
        fundingTime = readStringAsInt64(data, "fundingTime");
        nextFundingTime = readStringAsInt64(data, "nextFundingTime");
        readDecimalValue(data, "nextFundingRate", nextFundingRate);
        readDecimalValue(data, "interestRate", interestRate);
        readDecimalValue(data, "premium", premium);
        readDecimalValue(data, "maxFundingRate", maxFundingRate);
        readDecimalValue(data, "minFundingRate", minFundingRate);
        ts = readStringAsInt64(data, "ts");;
        readMagicEnum<SettState>(data, "settState", settState);
        readDecimalValue(data, "settFundingRate", settFundingRate);
    }
}

nlohmann::json FundingRates::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRates::toJson()");
}

void FundingRates::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        FundingRate fundingRate;
        fundingRate.fromJson(el.value());
        rates.push_back(fundingRate);
    }
}

nlohmann::json Instrument::toJson() const {
    throw std::runtime_error("Unimplemented: Instrument::toJson()");
}

void Instrument::fromJson(const nlohmann::json &json) {
    readMagicEnum<InstrumentType>(json, "instType", instType);
    readValue<std::string>(json, "instId", instId);
    readValue<std::string>(json, "instFamily", instFamily);
    readValue<std::string>(json, "uly", uly);
    readValue<std::string>(json, "baseCcy", baseCcy);
    readValue<std::string>(json, "quoteCcy", quoteCcy);
    readValue<std::string>(json, "settleCcy", settleCcy);
    readDecimalValue(json, "ctVal", ctVal);
    readDecimalValue(json, "ctMult", ctMult);
    readValue<std::string>(json, "ctValCcy", ctValCcy);
    readMagicEnum<OptionType>(json, "optType", optType);
    readDecimalValue(json, "stk", stk);
    listTime = readStringAsInt64(json, "listTime");
    expTime = readStringAsInt64(json, "expTime");
    readDecimalValue(json, "lever", lever);
    readDecimalValue(json, "tickSz", tickSz);
    readDecimalValue(json, "lotSz", lotSz);
    readDecimalValue(json, "minSz", minSz);
    readMagicEnum<ContractType>(json, "ctType", ctType);
    readMagicEnum<FuturesAlias>(json, "alias", alias);
    readMagicEnum<InstrumentStatus>(json, "state", state);
    readDecimalValue(json, "maxLmtSz", maxLmtSz);
    readDecimalValue(json, "maxMktSz", maxMktSz);
    readDecimalValue(json, "maxTwapSz", maxTwapSz);
    readDecimalValue(json, "maxIcebergSz", maxIcebergSz);
    readDecimalValue(json, "maxTriggerSz", maxTriggerSz);
    readDecimalValue(json, "maxStopSz", maxStopSz);
}

nlohmann::json Instruments::toJson() const {
    throw std::runtime_error("Unimplemented: Instruments::toJson()");
}

void Instruments::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        Instrument instrument;
        instrument.fromJson(el.value());
        instruments.push_back(instrument);
    }
}

nlohmann::json BalanceDetail::toJson() const {
    throw std::runtime_error("Unimplemented: Balance::toJson()");
}

void BalanceDetail::fromJson(const nlohmann::json &json) {
    readDecimalValue(json, "availBal", availBal);
    readDecimalValue(json, "availEq", availEq);
    readDecimalValue(json, "cashBal", cashBal);
    readValue<std::string>(json, "ccy", ccy);
    readDecimalValue(json, "crossLiab", crossLiab);
    readDecimalValue(json, "disEq", disEq);
    readDecimalValue(json, "eq", eq);
    readDecimalValue(json, "eqUsd", eqUsd);
    readDecimalValue(json, "frozenBal", frozenBal);
    readDecimalValue(json, "interest", interest);
    readDecimalValue(json, "isoEq", isoEq);
    readDecimalValue(json, "isoLiab", isoLiab);
    readDecimalValue(json, "isoUpl", isoUpl);
    readDecimalValue(json, "liab", liab);
    readDecimalValue(json, "maxLoan", maxLoan);
    readDecimalValue(json, "mgnRatio", mgnRatio);
    readDecimalValue(json, "notionalLever", notionalLever);
    readDecimalValue(json, "ordFrozen", ordFrozen);
    readDecimalValue(json, "twap", twap);
    readDecimalValue(json, "upl", upl);
    uTime = readStringAsInt64(json, "uTime");
    readDecimalValue(json, "uplLiab", uplLiab);
    readDecimalValue(json, "stgyEq", stgyEq);
    readDecimalValue(json, "spotInUseAmt", spotInUseAmt);
}

nlohmann::json Balance::toJson() const {
    throw std::runtime_error("Unimplemented: Balance::toJson()");
}

void Balance::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        readDecimalValue(el.value(), "adjEq", adjEq);
        readDecimalValue(el.value(), "imr", imr);
        readDecimalValue(el.value(), "isoEq", isoEq);
        readDecimalValue(el.value(), "mgnRatio", mgnRatio);
        readDecimalValue(el.value(), "mmr", mmr);
        readDecimalValue(el.value(), "notionalUsd", notionalUsd);
        readDecimalValue(el.value(), "ordFroz", ordFroz);
        readDecimalValue(el.value(), "totalEq", totalEq);
        uTime = readStringAsInt64(el.value(), "uTime");

        for (const auto &elData: el.value()["details"].items()) {
            BalanceDetail balanceDetail;
            balanceDetail.fromJson(elData.value());
            balanceDetails.push_back(balanceDetail);
        }
    }
}

nlohmann::json SystemTime::toJson() const {
    throw std::runtime_error("Unimplemented: SystemTime::toJson()");
}

void SystemTime::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        ts = readStringAsInt64(el.value(), "ts");
    }
}

nlohmann::json Position::toJson() const {
    throw std::runtime_error("Unimplemented: Position::toJson()");
}

void Position::fromJson(const nlohmann::json &json) {
    adl = readStringAsInt(json, "adl");
    readDecimalValue(json, "availPos", availPos);
    readDecimalValue(json, "avgPx", avgPx);
    cTime = readStringAsInt64(json, "cTime");
    readValue<std::string>(json, "ccy", ccy);
    readDecimalValue(json, "imr", imr);
    readValue<std::string>(json, "instId", instId);
    readMagicEnum<InstrumentType>(json, "instType", instType);
    readDecimalValue(json, "interest", interest);
    readDecimalValue(json, "last", last);
    readDecimalValue(json, "lever", lever);
    readDecimalValue(json, "liab", liab);
    readValue<std::string>(json, "liabCcy", liabCcy);
    readDecimalValue(json, "liqPx", liqPx);
    readDecimalValue(json, "margin", margin);
    readDecimalValue(json, "markPx", markPx);
    readMagicEnum<MarginMode>(json, "mgnMode", mgnMode);
    readDecimalValue(json, "mgnRatio", mgnRatio);
    readDecimalValue(json, "mmr", mmr);
    readDecimalValue(json, "notionalUsd", notionalUsd);
    readDecimalValue(json, "pos", pos);
    readValue<std::string>(json, "posCcy", posCcy);
    readValue<std::string>(json, "posId", posId);

    std::string side;
    readValue<std::string>(json, "posSide", side);

    /// The venue spells these "net"/"long"/"short" while the enum members carry a
    /// leading underscore (to dodge the `long` keyword), so a direct enum_cast
    /// never matched and posSide silently stayed at its default.
    if (const auto posSideVal = magic_enum::enum_cast<PositionSide>("_" + side)) {
        posSide = *posSideVal;
    }

    readValue<std::string>(json, "tradeId", tradeId);

    uTime = readStringAsInt64(json, "uTime");
    readDecimalValue(json, "upl", upl);
    readDecimalValue(json, "uplRatio", uplRatio);
}

nlohmann::json Positions::toJson() const {
    throw std::runtime_error("Unimplemented: Positions::toJson()");
}

void Positions::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        Position position;
        position.fromJson(el.value());
        positions.push_back(position);
    }
}

nlohmann::json Order::toJson() const {
    nlohmann::json json;
    json["instId"] = instId;
    /// Every enum must go out as its NAME. A plain enum serializes to its
    /// underlying integer, which the venue rejects — tdMode/side/ordType were
    /// all doing that, so no order could ever have been accepted.
    json["tdMode"] = magic_enum::enum_name(tdMode);

    /// Optional fields are OMITTED when empty — the venue validates present
    /// keys, and an empty string is a parameter error, not an absence.
    if (!clOrdId.empty()) {
        json["clOrdId"] = clOrdId;
    }
    if (!ccy.empty()) {
        json["ccy"] = ccy;
    }
    json["side"] = magic_enum::enum_name(side);
    /// PositionSide members carry a leading underscore to dodge the `long`
    /// keyword; the venue wants them without it.
    json["posSide"] = std::string(magic_enum::enum_name(posSide)).substr(1);
    json["ordType"] = magic_enum::enum_name(ordType);
    // OKX::decimalToString, never Boost's str(): str(N>0) means N SIGNIFICANT
    // DIGITS (63860.4 -> "6.39e+04", 96.4123 -> "96.4"), and even str(0) goes
    // SCIENTIFIC below 1e-4 (0.00004123 -> "4.123e-05") — both venue-rejected.
    // Rounding to the instrument's tick/lot stays the CALLER's job; this
    // serializer reproduces the value exactly, in fixed notation.
    json["sz"] = OKX::decimalToString(sz);
    json["px"] = OKX::decimalToString(px);

    if (reduceOnly) {
        json["reduceOnly"] = true;
    }

    return json;
}

void Order::fromJson(const nlohmann::json &json) {
    throw std::runtime_error("Unimplemented: Order::fromJson()");
}

nlohmann::json OrderResponse::toJson() const {
    throw std::runtime_error("Unimplemented: OrderResponse::toJson()");
}

void OrderResponse::fromJson(const nlohmann::json &json) {
    auto prd = json.dump();

    readValue<std::string>(json, "clOrdId", clOrdId);
    readValue<std::string>(json, "ordId", ordId);
    readValue<std::string>(json, "tag", tag);
    readValue<std::string>(json, "sCode", sCode);
    readValue<std::string>(json, "sMsg", sMsg);
}

nlohmann::json OrderResponses::toJson() const {
    throw std::runtime_error("Unimplemented: OrderResponses::toJson()");
}

void OrderResponses::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        OrderResponse orderResponse;
        orderResponse.fromJson(el.value());
        orderResponses.push_back(orderResponse);
    }
}

nlohmann::json LeverageSetting::toJson() const { throw std::runtime_error("Unimplemented: LeverageSetting::toJson()"); }

void LeverageSetting::fromJson(const nlohmann::json &json) {
    readValue<std::string>(json, "instId", instId);
    readValue<std::string>(json, "ccy", ccy);
    readDecimalValue(json, "lever", lever);
    readMagicEnum<MarginMode>(json, "mgnMode", mgnMode);

    std::string side;
    readValue<std::string>(json, "posSide", side);

    /// The venue spells these "net"/"long"/"short"; the enum members carry a
    /// leading underscore to dodge the `long` keyword.
    if (const auto posSideVal = magic_enum::enum_cast<PositionSide>("_" + side)) {
        posSide = *posSideVal;
    }
}

nlohmann::json LeverageSettings::toJson() const { throw std::runtime_error("Unimplemented: LeverageSettings::toJson()"); }

void LeverageSettings::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        LeverageSetting setting;
        setting.fromJson(el.value());
        settings.push_back(setting);
    }
}

nlohmann::json OrderDetail::toJson() const {
    throw std::runtime_error("Unimplemented: OrderDetail::toJson()");
}

void OrderDetail::fromJson(const nlohmann::json &json) {
    readMagicEnum<InstrumentType>(json, "instType", instType);
    readValue<std::string>(json, "instId", instId);
    readValue<std::string>(json, "ccy", ccy);
    readValue<std::string>(json, "ordId", ordId);
    readValue<std::string>(json, "clOrdId", clOrdId);
    readDecimalValue(json, "px", px);
    readDecimalValue(json, "sz", sz);
    readDecimalValue(json, "pnl", pnl);
    readMagicEnum<OrderType>(json, "ordType", ordType);
    readMagicEnum<Side>(json, "side", side);

    std::string side;
    readValue<std::string>(json, "posSide", side);

    if (const auto posSideVal = magic_enum::enum_cast<PositionSide>(side)) {
        posSide = *posSideVal;
    }

    readMagicEnum<MarginMode>(json, "tdMode", tdMode);
    readDecimalValue(json, "accFillSz", accFillSz);
    readDecimalValue(json, "fillPx", fillPx);
    readValue<std::string>(json, "tradeId", tradeId);
    readDecimalValue(json, "fillSz", fillSz);
    fillTime = readStringAsInt64(json, "fillTime");
    readMagicEnum<OrderState>(json, "state", state);
    readDecimalValue(json, "avgPx", avgPx);
    readDecimalValue(json, "lever", lever);
    uTime = readStringAsInt64(json, "uTime");
    cTime = readStringAsInt64(json, "cTime");
}

nlohmann::json OrderDetails::toJson() const {
    throw std::runtime_error("Unimplemented: OrderDetails::toJson()");
}

void OrderDetails::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: data.items()) {
        OrderDetail orderDetail;
        orderDetail.fromJson(el.value());
        orderDetails.push_back(orderDetail);
    }
}

nlohmann::json MarketDataFileInfo::toJson() const {
    nlohmann::json json;
    json["filename"] = filename;
    json["dateTs"] = std::to_string(dateTs);
    json["sizeMB"] = sizeMB;
    json["url"] = url;
    return json;
}

void MarketDataFileInfo::fromJson(const nlohmann::json &json) {
    readValue<std::string>(json, "filename", filename);
    dateTs = readStringAsInt64(json, "dateTs");
    readValue<std::string>(json, "sizeMB", sizeMB);
    readValue<std::string>(json, "url", url);
}

nlohmann::json MarketDataGroupDetail::toJson() const {
    nlohmann::json json;
    json["instId"] = instId;
    json["instFamily"] = instFamily;
    json["instType"] = magic_enum::enum_name(instType);
    json["dateRangeStart"] = std::to_string(dateRangeStart);
    json["dateRangeEnd"] = std::to_string(dateRangeEnd);
    json["groupSizeMB"] = groupSizeMB;

    nlohmann::json groupDetailsJson = nlohmann::json::array();
    for (const auto &fileInfo: groupDetails) {
        groupDetailsJson.push_back(fileInfo.toJson());
    }
    json["groupDetails"] = groupDetailsJson;

    return json;
}

void MarketDataGroupDetail::fromJson(const nlohmann::json &json) {
    readValue<std::string>(json, "instId", instId);
    readValue<std::string>(json, "instFamily", instFamily);
    readMagicEnum<InstrumentType>(json, "instType", instType);
    dateRangeStart = readStringAsInt64(json, "dateRangeStart");
    dateRangeEnd = readStringAsInt64(json, "dateRangeEnd");
    readValue<std::string>(json, "groupSizeMB", groupSizeMB);

    if (json.contains("groupDetails") && json["groupDetails"].is_array()) {
        for (const auto &el: json["groupDetails"].items()) {
            MarketDataFileInfo fileInfo;
            fileInfo.fromJson(el.value());
            groupDetails.push_back(fileInfo);
        }
    }
}

nlohmann::json MarketDataHistory::toJson() const {
    nlohmann::json json;
    json["ts"] = std::to_string(ts);
    json["totalSizeMB"] = totalSizeMB;
    json["dateAggrType"] = magic_enum::enum_name(dateAggrType);

    nlohmann::json detailsJson = nlohmann::json::array();
    for (const auto &detail: details) {
        detailsJson.push_back(detail.toJson());
    }
    json["details"] = detailsJson;

    return json;
}

void MarketDataHistory::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    // The response has data as array with one element containing the actual data
    if (!data.empty() && data.is_array()) {
        const auto &dataElement = data[0];

        ts = readStringAsInt64(dataElement, "ts");
        readValue<std::string>(dataElement, "totalSizeMB", totalSizeMB);
        readMagicEnum<DateAggrType>(dataElement, "dateAggrType", dateAggrType);

        if (dataElement.contains("details") && dataElement["details"].is_array()) {
            for (const auto &el: dataElement["details"].items()) {
                MarketDataGroupDetail detail;
                detail.fromJson(el.value());
                details.push_back(detail);
            }
        }
    }
}
}
