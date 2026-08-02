/**
OKX REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef OKX_REST_CLIENT_H
#define OKX_REST_CLIENT_H

#include "okx_models.h"
#include <string>
#include <memory>

namespace stonky::okx {

using onCandlesDownloaded = std::function<void(const std::vector<Candle>&)>;

class RESTClient {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    /// @param host see HTTPSession — empty selects the global host.
    RESTClient(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &host = "");

    ~RESTClient();

    /**
     * Set credentials to the RESTClient instance, it will reset the underlying HTTP Session
     * @param apiKey
     * @param apiSecret
     * @param passphrase
     */
    void setCredentials(const std::string &apiKey, const std::string &apiSecret, const std::string &passphrase, const std::string &host = "") const;

    /**
     * Retrieve the latest price snapshot, best bid/ask price, and trading volume in the last 24 hours.
     * @param instrumentType
     * @return vector of Ticker structures
     * @see https://www.okx.com/docs-v5/en/#rest-api-market-data-get-tickers
     */
    [[nodiscard]] std::vector<Ticker> getTickers(InstrumentType instrumentType) const;

    /**
     * Retrieve a list of instruments with open contracts.
     * @param instrumentType
     * @param force Reload instruments info from server if true
     * @return vector of Instrument structures
     * @see https://www.okx.com/docs-v5/en/#rest-api-public-data-get-instruments
     */
    [[nodiscard]] std::vector<Instrument> getInstruments(InstrumentType instrumentType, bool force = false) const;

    /**
     * Set Instruments from the outside
     * @param instruments
     */
    void setInstruments(const std::vector<Instrument> &instruments) const;

    /**
     * Download historical candles
     * @param instId instrument Id, e.g. "ETH-USDT-SWAP"
     * @param barSize
     * @param from timestamp in ms, must be smaller than "to"
     * @param to timestamp in ms, must be bigger than "from"
     * @param limit maximum number of returned candles, maximum and also the default value is 100
     * @param writer
     * @return vector of Candle structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://www.okx.com/docs-v5/en/#rest-api-market-data-get-candlesticks-history
     */
    [[nodiscard]] std::vector<Candle>
    getHistoricalPrices(const std::string &instId, BarSize barSize, std::int64_t from, std::int64_t to,
                        std::int32_t limit = -1, const onCandlesDownloaded &writer = {}) const;

    /**
     * Retrieve funding rate.
     * @param instId instrument Id, e.g. "ETH-USDT-SWAP"
     * @return Filled FundingRate structure
     * @see https://www.okx.com/docs-v5/en/#rest-api-public-data-get-funding-rate
     */
    [[nodiscard]] FundingRate getLastFundingRate(const std::string &instId) const;

    /**
     * Retrieve funding rate history. This endpoint can retrieve data from the last 3 months.
     * @param instId instrument Id, e.g. "ETH-USDT-SWAP"
     * @param from timestamp in ms, must be smaller than "to"
     * @param to timestamp in ms, must be bigger than "from"
     * @param limit maximum number of returned records, maximum and also the default value is 100
     * @return vector of FundingRate structures
     */
    [[nodiscard]] std::vector<FundingRate>
    getFundingRates(const std::string &instId, int64_t from, int64_t to, int limit = -1) const;

    /**
     * Retrieve a list of assets (with non-zero balance), remaining balance, and available amount in the trading account.
     * @param ccy Single currency or multiple currencies (no more than 20) separated with comma, e.g. BTC or BTC,ETH.
     * @return filled Balance structure
     */
    [[nodiscard]] Balance getBalance(const std::string &ccy) const;

    /**
     * Retrieve API server time.
     * @return
     */
    [[nodiscard]] std::int64_t getSystemTime() const;

    /**
     * Retrieve information on your positions. When the account is in net mode, net positions will be displayed,
     * and when the account is in long/short mode, long or short positions will be displayed. Return in reverse
     * chronological order using ctime.
     * @param instrumentType
     * @param instId instrument Id, e.g. "ETH-USDT-SWAP"
     * @return vector of Position structures
     */
    [[nodiscard]] std::vector<Position> getPositions(InstrumentType instrumentType, const std::string &instId) const;

    /**
     * Cancel an incomplete order.
     * @param instId instrument Id, e.g. "ETH-USDT-SWAP"
     * @param clientOrderId
     * @param orderId
     * @return vector of OrderResponse structures (one for every order in request, this method allows only one so
     * there will always be one response as well)
     */
    [[nodiscard]] std::vector<OrderResponse>
    cancelOrder(const std::string &instId, const std::string &clientOrderId, const std::string &orderId = "") const;

    /**
     * Place order
     * @param order
     * @return vector of OrderResponse structures (one for every order in request, this method allows only one so
     * there will always be one response as well)
     */
    [[nodiscard]] std::vector<OrderResponse> placeOrder(const Order &order) const;

    /**
     * Retrieve order details.
     * @param instId instrument Id, e.g. "ETH-USDT-SWAP"
     * @param clientOrderId Client Order ID as assigned by the client
     * @param orderId Either ordId or clOrdId is required, if both are passed, ordId will be used
     * @return vector of OrderDetail structures (one for every order in request, this method allows only one so
     * there will always be one response as well)
     */
    [[nodiscard]] std::vector<OrderDetail>
    getOrderDetail(const std::string &instId, const std::string &clientOrderId, const std::string &orderId = "") const;

    /**
     * Get download URLs for historical market data.
     * @param module Data module type (Trades, Candles1m, FundingRate, etc.)
     * @param instType Instrument type (SPOT, SWAP, FUTURES, OPTION)
     * @param instFamilyOrIdList Instrument family (for non-SPOT) or ID list (for SPOT), or "ANY" for all
     * @param dateAggrType Date aggregation type (daily or monthly)
     * @param begin Begin timestamp in ms (inclusive)
     * @param end End timestamp in ms (inclusive), max range: 20 days for daily, 20 months for monthly
     * @return MarketDataHistory with download URLs
     * @throws nlohmann::json::exception, std::exception
     * @see https://www.okx.com/docs-v5/en/#public-data-rest-api-get-historical-market-data
     */
    [[nodiscard]] MarketDataHistory getMarketDataHistory(
        MarketDataModule module,
        InstrumentType instType,
        const std::string &instFamilyOrIdList,
        DateAggrType dateAggrType,
        std::int64_t begin,
        std::int64_t end) const;

    /**
     * Download ZIP file from URL and return raw bytes.
     * @param url Full URL to the ZIP file (from MarketDataHistory response)
     * @return Raw ZIP file bytes
     * @throws std::runtime_error if download fails
     */
    [[nodiscard]] static std::vector<std::uint8_t> downloadMarketDataFile(const std::string &url);

    /**
     * Download, extract and parse historical candlestick data.
     * This is a high-level convenience method that combines getMarketDataHistory,
     * downloadMarketDataFile, ZIP extraction and CSV parsing.
     * @param instType Instrument type (SPOT, SWAP, FUTURES, OPTION)
     * @param instFamily Instrument family (e.g., "BTC-USDT")
     * @param dateAggrType Date aggregation type (daily or monthly)
     * @param begin Begin timestamp in ms
     * @param end End timestamp in ms
     * @return Vector of Candle structures from all downloaded files
     * @throws std::runtime_error if any step fails
     */
    [[nodiscard]] std::vector<Candle> downloadAndParseHistoricalCandles(
        InstrumentType instType,
        const std::string &instFamily,
        DateAggrType dateAggrType,
        std::int64_t begin,
        std::int64_t end) const;
};
}

#endif //OKX_REST_CLIENT_H
