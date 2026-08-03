/**
OKX REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx_rest_client.h"
#include "stonky/okx/okx_http_session.h"
#include "stonky/okx/okx.h"
#include "stonky/okx/okx_market_data_utils.h"
#include "stonky/utils/utils.h"
#include "stonky/utils/magic_enum_wrapper.hpp"
#include <mutex>
#include <thread>
#include <deque>
#include <spdlog/spdlog.h>

namespace stonky::okx {
template<typename ValueType>
ValueType handleOKXResponse(const http::response<http::string_body> &response) {
    ValueType retVal;
    retVal.fromJson(nlohmann::json::parse(response.body()));

    if (std::stoi(retVal.code) != 0) {
        throw std::runtime_error(fmt::format("OKX API error, code: {}, msg: {}", retVal.code, retVal.msg).c_str());
    }

    return retVal;
}

struct RateLimiter {
    std::mutex m_mutex;
    std::deque<std::int64_t> m_requestTimes;
    const size_t m_limit;
    const std::int64_t m_windowSizeMs;

    RateLimiter(const size_t limit, const std::int64_t windowMs) : m_limit(limit), m_windowSizeMs(windowMs) {}

    void wait() {
        std::unique_lock lock(m_mutex);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // Remove old requests
        while (!m_requestTimes.empty() && (now - m_requestTimes.front() > m_windowSizeMs)) {
            m_requestTimes.pop_front();
        }

        if (m_requestTimes.size() >= m_limit) {
            const auto oldest = m_requestTimes.front();

            if (auto waitTime = (oldest + m_windowSizeMs) - now + 10; waitTime > 0) {
#ifdef VERBOSE_LOG
                spdlog::info("Rate limit reached (Local). Waiting for {} ms", waitTime);
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));

                // Update now after sleep
                now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                while (!m_requestTimes.empty() && (now - m_requestTimes.front() > m_windowSizeMs)) {
                    m_requestTimes.pop_front();
                }
            }
        }
        m_requestTimes.push_back(now);
    }
};

struct RESTClient::P {
private:
    Instruments m_instruments;
    mutable std::recursive_mutex m_locker;

public:
    mutable RateLimiter klineLimiter{20, 2000};
    mutable RateLimiter marketDataHistoryLimiter{1, 1000}; // 1 request per second for market-data-history (conservative)
    RESTClient *parent = nullptr;
    std::shared_ptr<HTTPSession> httpSession;

    explicit P(RESTClient *parent) { this->parent = parent; }

    [[nodiscard]] Instruments getInstruments() const {
        std::lock_guard lk(m_locker);
        return m_instruments;
    }

    void setInstruments(const Instruments &instruments) {
        std::lock_guard lk(m_locker);
        m_instruments = instruments;
    }

    void setInstruments(const std::vector<Instrument> &instruments) {
        std::lock_guard lk(m_locker);
        m_instruments.instruments = instruments;
    }

    static http::response<http::string_body> checkResponse(const http::response<http::string_body> &response) {
        if (response.result() != http::status::ok) {
            throw std::runtime_error(fmt::format("Bad response, code {}, msg: {}", response.result_int(), response.body()).c_str());
        }
        return response;
    }

    std::vector<Candle> getHistoricalPrices(const std::string &instId, BarSize barSize, std::int64_t from, std::int64_t to, std::int32_t limit) const;

    std::vector<FundingRate> getFundingRates(const std::string &instId, int64_t from, int64_t to, int limit) const;
};

RESTClient::RESTClient(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &host) : m_p(std::make_unique<P>(this)) {
    m_p->httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret, passphrase, host);
}

RESTClient::~RESTClient() = default;

void RESTClient::setCredentials(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &host) const {
    m_p->httpSession.reset();
    m_p->httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret, passphrase, host);
}

std::vector<Ticker> RESTClient::getTickers(const InstrumentType instrumentType) const {
    const std::string path = "/api/v5/market/tickers";
    std::map<std::string, std::string> parameters;

    parameters.insert_or_assign("instType", magic_enum::enum_name(instrumentType));

    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters));
    return handleOKXResponse<Tickers>(response).tickers;
}

std::vector<Instrument> RESTClient::getInstruments(const InstrumentType instrumentType, const bool force) const {
    if (m_p->getInstruments().instruments.empty() || force) {
        const std::string path = "/api/v5/public/instruments";
        std::map<std::string, std::string> parameters;

        parameters.insert_or_assign("instType", magic_enum::enum_name(instrumentType));

        const auto response = P::checkResponse(m_p->httpSession->get(path, parameters));
        m_p->setInstruments(handleOKXResponse<Instruments>(response));
    }

    return m_p->getInstruments().instruments;
}

void RESTClient::setInstruments(const std::vector<Instrument> &instruments) const { m_p->setInstruments(instruments); }

std::vector<Candle> RESTClient::P::getHistoricalPrices(const std::string &instId, const BarSize barSize, const std::int64_t from, const std::int64_t to,
                                                       const std::int32_t limit) const {
    const std::string path = "/api/v5/market/history-candles";
    std::map<std::string, std::string> parameters;

    parameters.insert_or_assign("instId", instId);
    parameters.insert_or_assign("bar", magic_enum::enum_name(barSize));

    if (from != -1) {
        parameters.insert_or_assign("after", std::to_string(to));
    }

    if (to != -1) {
        parameters.insert_or_assign("before", std::to_string(from));
    }

    if (limit != -1) {
        parameters.insert_or_assign("limit", std::to_string(limit));
    }

    klineLimiter.wait();
    const auto response = checkResponse(httpSession->get(path, parameters));
    return handleOKXResponse<Candles>(response).candles;
}

std::vector<Candle> RESTClient::getHistoricalPrices(const std::string &instId, const BarSize barSize, const std::int64_t from, const std::int64_t to, const std::int32_t limit,
                                                    const onCandlesDownloaded &writer) const {
    std::vector<Candle> retVal;
    std::vector<Candle> candles;

    if (from < to) {
        candles = m_p->getHistoricalPrices(instId, barSize, from, to, limit);
    }

    while (!candles.empty()) {
        retVal.insert(retVal.end(), candles.begin(), candles.end());
        const std::int64_t lastToTime = candles.back().ts;

        if (writer) {
            if (!candles.back().confirm) {
                candles.pop_back();
            }

            writer(candles);
        }

        candles.clear();

        if (from < lastToTime) {
            candles = m_p->getHistoricalPrices(instId, barSize, from, lastToTime, limit);
        }
    }

    /// Remove last candle if it is not valid
    if (!retVal.empty()) {
        if (!retVal.back().confirm) {
            retVal.pop_back();
        }
    }

    std::ranges::reverse(retVal);
    return retVal;
}

FundingRate RESTClient::getLastFundingRate(const std::string &instId) const {
    const std::string path = "/api/v5/public/funding-rate";
    std::map<std::string, std::string> parameters;
    parameters.insert_or_assign("instId", instId);

    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters));
    return handleOKXResponse<FundingRate>(response);
}

std::vector<FundingRate> RESTClient::P::getFundingRates(const std::string &instId, const int64_t from, const int64_t to, const int limit) const {
    const std::string path = "/api/v5/public/funding-rate-history";
    std::map<std::string, std::string> parameters;

    parameters.insert_or_assign("instId", instId);

    if (from != -1) {
        parameters.insert_or_assign("after", std::to_string(to));
    }

    if (to != -1) {
        parameters.insert_or_assign("before", std::to_string(from));
    }

    if (limit != -1) {
        parameters.insert_or_assign("limit", std::to_string(limit));
    }

    const auto response = checkResponse(httpSession->get(path, parameters));
    return handleOKXResponse<FundingRates>(response).rates;
}

std::vector<FundingRate> RESTClient::getFundingRates(const std::string &instId, const int64_t from, const int64_t to, const int limit) const {
    std::vector<FundingRate> retVal;
    std::vector<FundingRate> rates;

    if (from < to) {
        rates = m_p->getFundingRates(instId, from, to, limit);
    }

    while (!rates.empty()) {
        retVal.insert(retVal.end(), rates.begin(), rates.end());
        const std::int64_t lastToTime = rates.back().fundingTime;
        rates.clear();

        if (from < lastToTime) {
            rates = m_p->getFundingRates(instId, from, lastToTime, limit);
        }
    }

    std::ranges::reverse(retVal);
    return retVal;
}

Balance RESTClient::getBalance(const std::string &ccy) const {
    const std::string path = "/api/v5/account/balance";
    std::map<std::string, std::string> parameters;

    if (!ccy.empty()) {
        parameters.insert_or_assign("ccy", ccy);
    }

    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters, false));
    return handleOKXResponse<Balance>(response);
}

std::int64_t RESTClient::getSystemTime() const {
    const std::string path = "/api/v5/public/time";
    const std::map<std::string, std::string> parameters;

    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters));
    return handleOKXResponse<SystemTime>(response).ts;
}

std::vector<Position> RESTClient::getPositions(const InstrumentType instrumentType, const std::string &instId) const {
    const std::string path = "/api/v5/account/positions";
    std::map<std::string, std::string> parameters;

    parameters.insert_or_assign("instType", magic_enum::enum_name(instrumentType));

    if (!instId.empty()) {
        parameters.insert_or_assign("instId", instId);
    }

    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters, false));
    return handleOKXResponse<Positions>(response).positions;
}

std::vector<OrderResponse> RESTClient::cancelOrder(const std::string &instId, const std::string &clientOrderId, const std::string &orderId) const {
    const std::string path = "/api/v5/trade/cancel-order";

    nlohmann::json json;
    json["instId"] = instId;
    json["clOrdId"] = clientOrderId;
    json["ordId"] = orderId;

    const auto response = P::checkResponse(m_p->httpSession->post(path, json, false));
    return handleOKXResponse<OrderResponses>(response).orderResponses;
}

std::vector<OrderResponse> RESTClient::placeOrder(const Order &order) const {
    const std::string path = "/api/v5/trade/order";
    const auto response = P::checkResponse(m_p->httpSession->post(path, order.toJson(), false));
    return handleOKXResponse<OrderResponses>(response).orderResponses;
}

std::vector<LeverageSetting> RESTClient::setLeverage(const std::string &instId, const double leverage, const MarginMode marginMode,
                                                      const PositionSide positionSide) const {
    const std::string path = "/api/v5/account/set-leverage";

    nlohmann::json body;
    body["instId"] = instId;
    /// The venue takes every numeric field as a string.
    body["lever"] = fmt::format("{:g}", leverage);
    body["mgnMode"] = magic_enum::enum_name(marginMode);

    /// posSide applies to long/short position mode only — sending it in net
    /// mode is itself a parameter error. The enum's leading underscore is a
    /// C++ keyword workaround and must be stripped for the venue.
    if (positionSide != PositionSide::_net) {
        body["posSide"] = std::string(magic_enum::enum_name(positionSide)).substr(1);
    }

    const auto response = P::checkResponse(m_p->httpSession->post(path, body, false));
    return handleOKXResponse<LeverageSettings>(response).settings;
}

std::vector<LeverageSetting> RESTClient::getLeverage(const std::string &instId, const MarginMode marginMode) const {
    const std::string path = "/api/v5/account/leverage-info";
    std::map<std::string, std::string> parameters;

    parameters.insert_or_assign("instId", instId);
    parameters.insert_or_assign("mgnMode", std::string(magic_enum::enum_name(marginMode)));

    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters, false));
    return handleOKXResponse<LeverageSettings>(response).settings;
}

std::vector<OrderDetail> RESTClient::getOrderDetail(const std::string &instId, const std::string &clientOrderId, const std::string &orderId) const {
    const std::string path = "/api/v5/trade/order";
    std::map<std::string, std::string> parameters;

    parameters.insert_or_assign("instId", instId);
    parameters.insert_or_assign("clOrdId", clientOrderId);
    parameters.insert_or_assign("ordId", orderId);

    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters, false));
    return handleOKXResponse<OrderDetails>(response).orderDetails;
}

MarketDataHistory RESTClient::getMarketDataHistory(
    const MarketDataModule module,
    const InstrumentType instType,
    const std::string &instFamilyOrIdList,
    const DateAggrType dateAggrType,
    const std::int64_t begin,
    const std::int64_t end) const {

    const std::string path = "/api/v5/public/market-data-history";
    std::map<std::string, std::string> parameters;

    // Module as number string
    parameters.insert_or_assign("module", std::to_string(static_cast<std::int32_t>(module)));

    // Instrument type
    parameters.insert_or_assign("instType", std::string(magic_enum::enum_name(instType)));

    // For SPOT use instIdList, for others use instFamilyList
    if (instType == InstrumentType::SPOT) {
        parameters.insert_or_assign("instIdList", instFamilyOrIdList);
    } else {
        parameters.insert_or_assign("instFamilyList", instFamilyOrIdList);
    }

    // Date aggregation type
    parameters.insert_or_assign("dateAggrType", std::string(magic_enum::enum_name(dateAggrType)));

    // Timestamps
    parameters.insert_or_assign("begin", std::to_string(begin));
    parameters.insert_or_assign("end", std::to_string(end));

    m_p->marketDataHistoryLimiter.wait();
    const auto response = P::checkResponse(m_p->httpSession->get(path, parameters));
    return handleOKXResponse<MarketDataHistory>(response);
}

std::vector<std::uint8_t> RESTClient::downloadMarketDataFile(const std::string &url) {
    return HTTPSession::downloadBinary(url);
}

std::vector<Candle> RESTClient::downloadAndParseHistoricalCandles(
    const InstrumentType instType,
    const std::string &instFamily,
    const DateAggrType dateAggrType,
    const std::int64_t begin,
    const std::int64_t end) const {

    // Get download URLs
    const auto history = getMarketDataHistory(
        MarketDataModule::Candles1m,
        instType,
        instFamily,
        dateAggrType,
        begin,
        end);

    std::vector<Candle> allCandles;

    // Process each group detail
    for (const auto &detail: history.details) {
        // Download and parse each file
        for (const auto &fileInfo: detail.groupDetails) {
            // Download ZIP file
            const auto zipData = downloadMarketDataFile(fileInfo.url);

            // Extract CSV from ZIP
            const auto csvData = utils::extractZip(zipData);

            // Parse CSV to candles
            auto candles = utils::parseCandlesCsv(csvData);

            // Append to result
            allCandles.insert(allCandles.end(), candles.begin(), candles.end());
        }
    }

    // Sort by timestamp
    std::ranges::sort(allCandles, [](const Candle &a, const Candle &b) {
        return a.ts < b.ts;
    });

    return allCandles;
}
} // namespace stonky::okx
