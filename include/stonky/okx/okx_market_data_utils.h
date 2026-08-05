/**
OKX Market Data Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_OKX_MARKET_DATA_UTILS_H
#define INCLUDE_STONKY_OKX_MARKET_DATA_UTILS_H

#include "okx_models.h"
#include <vector>
#include <string>

namespace stonky::okx::utils {

/**
 * Extract first file from ZIP archive stored in memory
 * @param zipData Raw ZIP file bytes
 * @return Decompressed file content
 * @throws std::runtime_error if ZIP extraction fails
 */
[[nodiscard]] std::vector<std::uint8_t> extractZip(const std::vector<std::uint8_t> &zipData);

/**
 * Parse 1-minute candlestick CSV data into Candle structures CSV format: ts,o,h,l,c,vol,volCcy,volCcyQuote,confirm
 * @param csvData Raw CSV bytes (UTF-8 encoded)
 * @param expectedInstrumentName When non-empty, keep only rows whose
 *        `instrument_name` column matches. The bulk archive is keyed by
 *        instrument FAMILY, not by instrument: a "futureschain" file holds
 *        every contract of the family, and families such as BTC-USD carry
 *        eight simultaneous expiries. Without the filter their bars would be
 *        merged into one series by timestamp, silently mixing contracts.
 * @return Vector of Candle structures
 * @throws std::runtime_error if CSV parsing fails
 */
[[nodiscard]] std::vector<Candle> parseCandlesCsv(const std::vector<std::uint8_t> &csvData,
                                                  const std::string &expectedInstrumentName = {});

/**
 * Parse 1-minute candlestick CSV data from string
 * @param csvContent CSV content as string
 * @param expectedInstrumentName See the byte-buffer overload above
 * @return Vector of Candle structures
 */
[[nodiscard]] std::vector<Candle> parseCandlesCsv(const std::string &csvContent,
                                                  const std::string &expectedInstrumentName = {});

/**
 * Parse funding rate CSV data into FundingRate structures CSV format: instId,fundingRate,realizedRate,fundingTime
 * @param csvData Raw CSV bytes (UTF-8 encoded)
 * @param expectedInstrumentName When non-empty, keep only rows whose
 *        instrument column matches — same family-vs-instrument reasoning as
 *        for candles
 * @return ector of FundingRate structures
 */
[[nodiscard]] std::vector<FundingRate> parseFundingRateCsv(const std::vector<std::uint8_t> &csvData,
                                                           const std::string &expectedInstrumentName = {});

} // namespace stonky::okx::utils

#endif // INCLUDE_STONKY_OKX_MARKET_DATA_UTILS_H
