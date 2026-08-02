/**
OKX Event Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx_event_models.h"
#include "stonky/utils/utils.h"
#include "stonky/utils/json_utils.h"
#include "magic_enum/magic_enum.hpp"

namespace stonky::okx {
nlohmann::json WSSubscription::toJson() const {
    nlohmann::json json;
    json["channel"] = channel;

    /// Emit only what this subscription actually keys on — the venue rejects the
    /// whole request (code 60012) when it carries an empty instId, which is what
    /// every private-channel subscription would send.
    if (!instId.empty()) {
        json["instId"] = instId;
    }
    if (!instType.empty()) {
        json["instType"] = instType;
    }

    return json;
}

void WSSubscription::fromJson(const nlohmann::json &json) {
    readValue<std::string>(json, "channel", channel);
    readValue<std::string>(json, "instId", instId);
    readValue<std::string>(json, "instType", instType);
}

nlohmann::json WSRequest::toJson() const {
    nlohmann::json json;
    /// magic_enum, not the raw enum: nlohmann serializes a plain enum as its
    /// UNDERLYING INTEGER, so this went out as {"op":0} and the venue rejected
    /// every subscription — public ones included — with code 60012.
    json["op"] = magic_enum::enum_name(op);

    auto args = nlohmann::json::array();

    for (const auto &subscription: subscriptions) {
        auto subJson = subscription.toJson();
        args.push_back(subJson);
    }

    json["args"] = args;
    return json;
}

void WSRequest::fromJson(const nlohmann::json &json) {
    throw std::runtime_error("Unimplemented: WSRequest::fromJson()");
}

nlohmann::json WSResponse::toJson() const {
    throw std::runtime_error("Unimplemented: WSResponse::toJson()");
}

void WSResponse::fromJson(const nlohmann::json &json) {
    readMagicEnum<EventType>(json, "event", event);

    if (event == EventType::error) {
        readValue<std::string>(json, "code", code);
        readValue<std::string>(json, "msg", msg);
        return;
    }

    /// `arg` is NOT universal: private channels also emit bookkeeping events
    /// such as `channel-conn-count`, which carry no `arg` at all. Reading it
    /// unconditionally through the CONST operator[] aborted the process on the
    /// first such message (nlohmann asserts the key exists).
    if (const auto it = json.find("arg"); it != json.end() && it->is_object()) {
        subscription.fromJson(*it);
    }
}

nlohmann::json DataEvent::toJson() const {
    throw std::runtime_error("Unimplemented: DataEvent::toJson()");
}

void DataEvent::fromJson(const nlohmann::json &json) {
    const auto &arg = json["arg"];
    readValue<std::string>(arg, "channel", channel);
    readValue<std::string>(arg, "instId", instId);
    data = json["data"];
}

nlohmann::json DataEventCandlestick::toJson() const {
    throw std::runtime_error("Unimplemented: DataEventCandlestick::toJson()");
}

void DataEventCandlestick::fromJson(const nlohmann::json &json) {
    for (const auto &el: json.items()) {
        Candle candle;
        candle.fromJson(el.value());
        candles.push_back(candle);
    }
}

nlohmann::json DataEventTicker::toJson() const {
    throw std::runtime_error("Unimplemented: DataEventCandlestick::toJson()");
}

void DataEventTicker::fromJson(const nlohmann::json &json) {
    for (const auto &el: json.items()) {
        Ticker ticker;
        ticker.fromJson(el.value());
        tickers.push_back(ticker);
    }
}
}
