/**
OKX Execution Gateway Smoke Test

Exercises every gateway path that does not place an order: instrument loading,
base-symbol -> instId resolution, the CONTRACT-to-base-unit conversion, the
private order stream and live quotes. Read-only in effect — safe against a
funded account.

The unit conversion is the reason this exists. OKX sizes orders in contracts
(ctVal base units each: XLM 100, XAU 0.001, AAOI 1), so a spec reported in the
wrong unit is wrong by a per-symbol factor and would size every order wrongly
in a way no compiler can catch.

Usage:  execution_gateway_smoke <env-file> [global|eea] [instType] [symbols...]

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx.h"
#include "stonky/okx/okx_execution_gateway.h"
#include "stonky/okx/okx_rest_client.h"
#include <spdlog/spdlog.h>
#include "magic_enum/magic_enum.hpp"
#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {
std::map<std::string, std::string> loadEnv(const std::string &path) {
    std::map<std::string, std::string> kv;
    std::ifstream file(path);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        auto value = line.substr(eq + 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) {
            value.pop_back();
        }
        kv[line.substr(0, eq)] = value;
    }
    return kv;
}
} // namespace

int main(const int argc, char **argv) {
    if (argc < 2) {
        spdlog::error("usage: execution_gateway_smoke <env-file> [global|eea] [instType] [symbols...]");
        return 2;
    }

    const auto env = loadEnv(argv[1]);
    const std::string entity = argc > 2 ? argv[2] : "global";
    const std::string instType = argc > 3 ? argv[3] : "FUTURES";

    std::vector<std::string> symbols;
    for (int i = 4; i < argc; ++i) {
        symbols.emplace_back(argv[i]);
    }
    if (symbols.empty()) {
        symbols = {"AAOI", "XLM", "XAU", "NVDA", "BTC"};
    }

    const auto *host = entity == "eea" ? stonky::okx::API_HOST_EEA : stonky::okx::API_HOST_GLOBAL;
    const auto *wsHost = entity == "eea" ? stonky::okx::WS_HOST_EEA : stonky::okx::WS_HOST_GLOBAL;

    stonky::execution::OkxExecutionGateway gateway(env.at("API_KEY"), env.at("API_SECRET"), env.at("PASSWORD"), host, wsHost, instType);
    gateway.start();

    spdlog::info("--- instrument specs (BASE units) ---");
    for (const auto &symbol: symbols) {
        try {
            const auto spec = gateway.instrumentSpec(symbol);
            spdlog::info("  {:<6} instId {:<30} ctVal {:<8g} qtyStep {:<10g} minQty {:<10g} tick {}", symbol, gateway.instIdFor(symbol), gateway.contractValue(symbol),
                         spec.qtyStep, spec.minQty, spec.tickSize);
        } catch (std::exception &e) {
            spdlog::warn("  {:<6} {}", symbol, e.what());
        }
    }

    spdlog::info("--- live quotes ---");
    for (const auto &symbol: symbols) {
        gateway.subscribeQuotes(symbol);
    }

    int withQuotes = 0;
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        withQuotes = 0;
        for (const auto &symbol: symbols) {
            if (const auto quote = gateway.lastQuote(symbol); quote && quote->sane()) {
                ++withQuotes;
            }
        }
        if (withQuotes == static_cast<int>(symbols.size())) {
            break;
        }
    }

    for (const auto &symbol: symbols) {
        if (const auto quote = gateway.lastQuote(symbol)) {
            spdlog::info("  {:<6} bid {:<12g} ask {:<12g} sane={}", symbol, quote->bid, quote->ask, quote->sane());
        } else {
            spdlog::warn("  {:<6} NO QUOTE", symbol);
        }
    }

    /// The venue-leg data path: funding history drives the ranking, and its
    /// units and sign must survive C++ parsing, not just the HTTP round trip.
    spdlog::info("--- funding history (venue-leg data path) ---");
    const stonky::okx::RESTClient rest(env.at("API_KEY"), env.at("API_SECRET"), env.at("PASSWORD"), host);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto fromMs = nowMs - 30LL * 24 * 3600 * 1000;

    for (const auto &symbol: symbols) {
        const auto instId = gateway.instIdFor(symbol);
        if (instId.empty()) {
            continue;
        }
        try {
            const auto rates = rest.getFundingRates(instId, fromMs, nowMs, 100);
            double cum = 0.0;
            for (const auto &rate: rates) {
                cum += rate.fundingRate.convert_to<double>();
            }
            spdlog::info("  {:<6} {:>3} events, 30d cum {:+.4f}%", symbol, rates.size(), cum * 100.0);
        } catch (std::exception &e) {
            spdlog::warn("  {:<6} funding read failed: {}", symbol, e.what());
        }
    }

    spdlog::info("--- leverage (read path shares the model with setLeverage) ---");
    for (const auto &symbol: symbols) {
        const auto instId = gateway.instIdFor(symbol);
        if (instId.empty()) {
            continue;
        }
        try {
            for (const auto &setting: rest.getLeverage(instId, stonky::okx::MarginMode::cross)) {
                spdlog::info("  {:<6} lever {:g} mgnMode {}", symbol, setting.lever.convert_to<double>(), magic_enum::enum_name(setting.mgnMode));
            }
        } catch (std::exception &e) {
            spdlog::warn("  {:<6} leverage read failed: {}", symbol, e.what());
        }
    }

    try {
        spdlog::info("account equity: {:.4f}", rest.getBalance("USD").totalEq.convert_to<double>());
    } catch (std::exception &e) {
        spdlog::warn("balance read failed: {}", e.what());
    }

    spdlog::info("amend supported: {}", gateway.supportsAmend());
    spdlog::info("RESULT: {}/{} symbols quoting", withQuotes, symbols.size());
    return withQuotes == static_cast<int>(symbols.size()) ? 0 : 1;
}
