/**
OKX Common Stuff

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_OKX_API_OKX_H
#define INCLUDE_STONKY_OKX_API_OKX_H

#include "stonky/okx/okx_models.h"

namespace stonky::okx {

/**
 * API hosts. OKX runs separate legal ENTITIES on separate hosts and an account
 * exists on exactly one of them — a key issued on the EEA entity answers
 * "API key doesn't exist" (code 50119) against the global host, so the host is
 * part of an account's identity, not a mirror to pick freely.
 *
 * The public instrument catalogs are identical across hosts, but TRADEABILITY is
 * not: on the EEA entity (verified 2026-08-02) GET /api/v5/account/instruments
 * returns 100 live FUTURES, all of them ruleType "xperp", and ZERO SWAP — an EEA
 * account trades USD-settled X-Perps instead of USDT swaps.
 */
constexpr auto API_HOST_GLOBAL = "www.okx.com";
constexpr auto API_HOST_EEA = "eea.okx.com";

/// WebSocket hosts, matching the REST entities above.
constexpr auto WS_HOST_GLOBAL = "ws.okx.com";
constexpr auto WS_HOST_EEA = "wseea.okx.com";

class OKX {
public:
    /**
     * Decimal -> string in FIXED notation, trailing zeros trimmed.
     *
     * The venue parses every numeric field as a plain decimal string. Boost's
     * str(0) ("shortest round-tripping form") switches to SCIENTIFIC notation
     * below 1e-4 — measured: 0.00004123 -> "4.123e-05" — which the venue
     * rejects. No X-Perp trades that low today, but SWAP-universe prices do
     * (SHIB-class), so every order field must go through this instead.
     */
    [[nodiscard]] static std::string decimalToString(const boost::multiprecision::cpp_dec_float_50 &value);

    /**
     * Check if the input resolution in minutes is valid, if so then return corresponding API string
     * @param size Bar size in minutes.
     * @param barSize out: BarSize enum value
     * @return Tru if input resolution is valid
     */
    static bool isValidBarSize(std::int32_t size, BarSize &barSize);

    /**
     * Get number of ms for given Bar size
     * @param size
     * @return
     */
    static int64_t numberOfMsForBarSize(BarSize size);

    /**
     * Helper for converting BarSize, which is in the REST data models, to CandlestickChannel, which is
     * in the WS streams data models.
     * @param size
     * @return CandlestickChannel corresponding to BarSize value
     */
    static CandlestickChannel barSizeToCandlestickChannel(BarSize size);

    /**
     * Helper for converting CandlestickChannel, which is in the WS streams data models, to BarSize, which is in the
     * REST data models
     * @param candlestickChannel
     * @return
     */
    static BarSize candlestickChannelToBarSize(CandlestickChannel candlestickChannel);
};
}
#endif //INCLUDE_STONKY_OKX_API_OKX_H
