/**
OKX Private Stream Smoke Test

Connects the authenticated WebSocket against the LIVE venue and reports whether
the login is accepted and the `orders` channel subscribes. Read-only in effect:
it places nothing and only listens, so it is safe to run against a funded
account (an idle account simply reports zero order events).

Its reason to exist: a WS login failure is indistinguishable from a bad key by
the time an execution engine notices it mid-cycle. Prove the handshake first.

Usage:  private_stream_smoke <env-file> [global|eea] [instType]
The env file is KEY=VALUE with API_KEY / API_SECRET / PASSWORD.

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx.h"
#include "stonky/okx/okx_private_stream.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <thread>

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
        spdlog::error("usage: private_stream_smoke <env-file> [global|eea] [instType]");
        return 2;
    }

    const auto env = loadEnv(argv[1]);
    const std::string entity = argc > 2 ? argv[2] : "global";
    const std::string instType = argc > 3 ? argv[3] : "FUTURES";

    if (!env.count("API_KEY") || !env.count("API_SECRET") || !env.count("PASSWORD")) {
        spdlog::error("env file must define API_KEY, API_SECRET and PASSWORD (the passphrase)");
        return 2;
    }

    const auto *wsHost = entity == "eea" ? stonky::okx::WS_HOST_EEA : stonky::okx::WS_HOST_GLOBAL;
    spdlog::info("connecting {} ({}), instType {}", wsHost, entity, instType);

    const stonky::okx::PrivateStream stream(env.at("API_KEY"), env.at("API_SECRET"), env.at("PASSWORD"), wsHost);

    std::atomic<int> orderEvents{0};
    stream.setLoggerCallback([](const stonky::LogSeverity severity, const std::string &message) {
        severity == stonky::LogSeverity::Error ? spdlog::error("  {}", message) : spdlog::info("  {}", message);
    });
    stream.setOrderUpdateCallback([&orderEvents](const stonky::okx::DataEvent &event) {
        ++orderEvents;
        spdlog::info("  orders: {}", event.data.dump());
    });

    stream.subscribeOrders(instType);

    for (int i = 0; i < 15 && !stream.isAuthenticated(); ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    /// Linger briefly so a subscribe error (wrong instType, missing permission)
    /// has time to come back before the verdict.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    if (!stream.isAuthenticated()) {
        spdlog::error("FAILED: no login acknowledgement");
        return 1;
    }

    spdlog::info("OK: authenticated, {} order event(s) seen while listening", orderEvents.load());
    return 0;
}
