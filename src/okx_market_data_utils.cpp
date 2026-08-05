/**
OKX Market Data Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx_market_data_utils.h"
#include <stdexcept>
#include <sstream>
#include <charconv>
#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#include <set>
#include <spdlog/spdlog.h>

namespace stonky::okx::utils {

std::vector<std::uint8_t> extractZip(const std::vector<std::uint8_t> &zipData) {
    // Create memory stream from input data
    void *memStream = mz_stream_mem_create();

    if (!memStream) {
        throw std::runtime_error("Failed to create memory stream");
    }

    mz_stream_mem_set_buffer(memStream, const_cast<void *>(static_cast<const void *>(zipData.data())), static_cast<int32_t>(zipData.size()));
    mz_stream_open(memStream, nullptr, MZ_OPEN_MODE_READ);

    // Create zip reader
    void *zipReader = mz_zip_reader_create();

    if (!zipReader) {
        mz_stream_mem_delete(&memStream);
        throw std::runtime_error("Failed to create ZIP reader");
    }

    if (mz_zip_reader_open(zipReader, memStream) != MZ_OK) {
        mz_zip_reader_delete(&zipReader);
        mz_stream_mem_delete(&memStream);
        throw std::runtime_error("Failed to open ZIP archive");
    }

    // Go to first entry
    if (mz_zip_reader_goto_first_entry(zipReader) != MZ_OK) {
        mz_zip_reader_close(zipReader);
        mz_zip_reader_delete(&zipReader);
        mz_stream_mem_delete(&memStream);
        throw std::runtime_error("ZIP archive is empty");
    }

    // Get file info
    mz_zip_file *fileInfo = nullptr;

    if (mz_zip_reader_entry_get_info(zipReader, &fileInfo) != MZ_OK) {
        mz_zip_reader_close(zipReader);
        mz_zip_reader_delete(&zipReader);
        mz_stream_mem_delete(&memStream);
        throw std::runtime_error("Failed to get file info from ZIP");
    }

    // Allocate buffer for decompressed data
    std::vector<std::uint8_t> result(static_cast<size_t>(fileInfo->uncompressed_size));

    // Open and read entry
    if (mz_zip_reader_entry_open(zipReader) != MZ_OK) {
        mz_zip_reader_close(zipReader);
        mz_zip_reader_delete(&zipReader);
        mz_stream_mem_delete(&memStream);
        throw std::runtime_error("Failed to open ZIP entry");
    }

    // mz_zip_reader_entry_read() is a STREAM read: it returns what the
    // decompressor has ready, not the whole entry, so it has to be looped.
    // Treating a single call's return value as the complete file truncated
    // every archive member past the first ~116 KB — a monthly OKX candle file
    // decompresses to several MB, so all but its first day was silently lost,
    // and the short buffer still parsed cleanly as a valid, shorter CSV.
    const auto expectedSize = static_cast<std::size_t>(fileInfo->uncompressed_size);
    std::size_t totalRead = 0;

    while (true) {
        if (totalRead == result.size()) {
            if (expectedSize > 0) {
                break; // entry fully read
            }
            result.resize(result.empty() ? 1u << 20 : result.size() * 2); // size unknown, grow
        }

        const int32_t chunk = mz_zip_reader_entry_read(zipReader, result.data() + totalRead,
                                                       static_cast<int32_t>(result.size() - totalRead));
        if (chunk < 0) {
            mz_zip_reader_entry_close(zipReader);
            mz_zip_reader_close(zipReader);
            mz_zip_reader_delete(&zipReader);
            mz_stream_mem_delete(&memStream);
            throw std::runtime_error("Failed to extract file from ZIP");
        }
        if (chunk == 0) {
            break; // stream exhausted
        }
        totalRead += static_cast<std::size_t>(chunk);
    }

    result.resize(totalRead);

    // A short read means truncated market data. Failing loudly is the point:
    // the silent version of this cost the dataset months of history.
    if (expectedSize > 0 && totalRead != expectedSize) {
        mz_zip_reader_entry_close(zipReader);
        mz_zip_reader_close(zipReader);
        mz_zip_reader_delete(&zipReader);
        mz_stream_mem_delete(&memStream);
        throw std::runtime_error(fmt::format("Truncated ZIP entry: read {} of {} bytes", totalRead, expectedSize));
    }

    // Cleanup
    mz_zip_reader_entry_close(zipReader);
    mz_zip_reader_close(zipReader);
    mz_zip_reader_delete(&zipReader);
    mz_stream_mem_delete(&memStream);

    return result;
}

std::vector<Candle> parseCandlesCsv(const std::vector<std::uint8_t> &csvData,
                                    const std::string &expectedInstrumentName) {
    const std::string csvContent(reinterpret_cast<const char *>(csvData.data()), csvData.size());
    return parseCandlesCsv(csvContent, expectedInstrumentName);
}

std::vector<Candle> parseCandlesCsv(const std::string &csvContent, const std::string &expectedInstrumentName) {
    std::vector<Candle> candles;
    std::set<std::int64_t> seenTimestamps;
    std::istringstream stream(csvContent);
    std::string line;
    bool isFirstLine = true;
    int linesProcessed = 0;
    int linesSkipped = 0;
    int linesOtherInstrument = 0;

    while (std::getline(stream, line)) {
        // Handle Windows line endings (remove trailing \r)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Skip header line (may contain Chinese characters in legacy data)
        if (isFirstLine) {
            isFirstLine = false;
            // Check if this looks like a header (contains non-digit first character or known headers)
            if (!line.empty() && !std::isdigit(static_cast<unsigned char>(line[0]))) {
                continue;
            }
        }

        // Parse CSV line: ts,o,h,l,c,vol,volCcy,volCcyQuote,confirm
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;

        while (std::getline(lineStream, field, ',')) {
            fields.push_back(field);
        }

        // OKX market data history CSV format (10 fields):
        // instrument_name,open,high,low,close,vol,vol_ccy,vol_quote,open_time,confirm
        if (fields.size() < 10) {
            if (linesSkipped < 5) {
                spdlog::warn("CSV line has {} fields (expected 10): {}", fields.size(), line);
            }
            linesSkipped++;
            continue; // Skip malformed lines
        }

        // A "futureschain" archive file carries every contract of the
        // instrument family, so rows of sibling expiries must be dropped
        // before the timestamp de-duplication below merges them into one
        // series.
        if (!expectedInstrumentName.empty() && fields[0] != expectedInstrumentName) {
            linesOtherInstrument++;
            continue;
        }

        linesProcessed++;

        try {
            Candle candle;

            // fields[0] = instrument_name (skip)
            // fields[1] = open
            // fields[2] = high
            // fields[3] = low
            // fields[4] = close
            // fields[5] = vol
            // fields[6] = vol_ccy (may be "None")
            // fields[7] = vol_quote (may be "None")
            // fields[8] = open_time (timestamp)
            // fields[9] = confirm

            // Parse timestamp from open_time field
            std::int64_t ts = 0;
            if (auto [ptr, ec] = std::from_chars(fields[8].data(), fields[8].data() + fields[8].size(), ts); ec == std::errc()) {
                candle.ts = ts;
            }

            // Parse OHLC values
            candle.o = boost::multiprecision::cpp_dec_float_50(fields[1]);
            candle.h = boost::multiprecision::cpp_dec_float_50(fields[2]);
            candle.l = boost::multiprecision::cpp_dec_float_50(fields[3]);
            candle.c = boost::multiprecision::cpp_dec_float_50(fields[4]);

            // Parse volume - always present
            candle.vol = boost::multiprecision::cpp_dec_float_50(fields[5]);

            // Parse vol_ccy and vol_quote - may be "None"
            if (fields[6] != "None" && !fields[6].empty()) {
                candle.volCcy = boost::multiprecision::cpp_dec_float_50(fields[6]);
            } else {
                candle.volCcy = 0;
            }

            if (fields[7] != "None" && !fields[7].empty()) {
                candle.volCcyQuote = boost::multiprecision::cpp_dec_float_50(fields[7]);
            } else {
                candle.volCcyQuote = 0;
            }

            // Parse confirm flag
            candle.confirm = (fields[9] == "1" || fields[9] == "true" || fields[9] == "True");

            // NOT used to filter here. On the REST endpoint confirm=0 means the
            // candle is still forming, but a bulk archive file contains nothing
            // but long-closed bars, and OKX writes 0 in it for entire stretches
            // of history anyway — CRV-USDT-SWAP carries 0 from 2023-09-01 to
            // 2024-07-18, as does every other symbol in that window. Dropping
            // those rows silently deleted ten months of the dataset. The REST
            // path removes its own trailing unconfirmed candle separately, in
            // RESTClient::getHistoricalPrices().

            // OKX ZIP files contain duplicate rows per candle - deduplicate by timestamp
            if (seenTimestamps.contains(candle.ts)) {
                continue;
            }
            seenTimestamps.insert(candle.ts);

            candles.push_back(candle);
        } catch (const std::exception &e) {
            // Log first few parse errors
            if (linesSkipped < 5) {
                spdlog::warn("Failed to parse CSV line: {} - error: {}", line, e.what());
            }
            linesSkipped++;
        }
    }

    // Everything filtered out means the file holds only other instruments —
    // either the archive changed how it names them, or the wrong family was
    // requested. Silence here would look exactly like an empty month.
    if (candles.empty() && linesOtherInstrument > 0) {
        spdlog::warn("parseCandlesCsv: none of the {} rows belong to {}", linesOtherInstrument,
                     expectedInstrumentName);
    }

    return candles;
}

std::vector<FundingRate> parseFundingRateCsv(const std::vector<std::uint8_t> &csvData,
                                             const std::string &expectedInstrumentName) {
    const std::string csvContent(reinterpret_cast<const char *>(csvData.data()), csvData.size());

    std::vector<FundingRate> rates;
    int linesOtherInstrument = 0;
    std::set<std::int64_t> seenTimestamps;
    std::istringstream stream(csvContent);
    std::string line;
    bool isFirstLine = true;

    while (std::getline(stream, line)) {
        // Handle Windows line endings
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        if (isFirstLine) {
            isFirstLine = false;
            if (!line.empty() && !std::isdigit(static_cast<unsigned char>(line[0]))) {
                continue;
            }
        }

        // Parse CSV line - two formats supported:
        // Bulk download (3 fields): instrument_name,funding_rate,funding_time
        // REST API     (4 fields): instId,fundingRate,realizedRate,fundingTime
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;

        while (std::getline(lineStream, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() < 3) {
            continue;
        }

        // Same family-vs-instrument reasoning as in parseCandlesCsv
        if (!expectedInstrumentName.empty() && fields[0] != expectedInstrumentName) {
            linesOtherInstrument++;
            continue;
        }

        try {
            if (fields[1] == "None" || fields[1].empty()) {
                continue;
            }
            FundingRate rate;
            rate.instId = fields[0];
            rate.fundingRate = boost::multiprecision::cpp_dec_float_50(fields[1]);

            // Determine funding_time field position based on number of fields
            const auto &timeField = (fields.size() >= 4) ? fields[3] : fields[2];

            std::int64_t fundingTime = 0;
            if (auto [ptr, ec] = std::from_chars(timeField.data(), timeField.data() + timeField.size(), fundingTime); ec == std::errc()) {
                rate.fundingTime = fundingTime;
            }

            // Deduplicate by funding_time
            if (seenTimestamps.contains(rate.fundingTime)) {
                continue;
            }
            seenTimestamps.insert(rate.fundingTime);

            rates.push_back(rate);
        } catch (const std::exception &e) {
            spdlog::warn("Exception in parseFundingRateCsv: {}", e.what());
        }
    }

    if (rates.empty() && linesOtherInstrument > 0) {
        spdlog::warn("parseFundingRateCsv: none of the {} rows belong to {}", linesOtherInstrument,
                     expectedInstrumentName);
    }

    return rates;
}

} // namespace stonky::okx::utils
