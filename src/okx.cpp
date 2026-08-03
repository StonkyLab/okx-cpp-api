/**
OKX Common Stuff

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2025 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/okx/okx.h"

#include <iomanip>
#include <sstream>

namespace stonky::okx {
std::string OKX::decimalToString(const boost::multiprecision::cpp_dec_float_50 &value) {
    std::ostringstream stream;
    /// 12 fixed decimals cover every tick the venue lists (finest observed:
    /// 1e-5 on X-Perps, 1e-8 class on SWAP long tail) with room to spare.
    stream << std::fixed << std::setprecision(12) << value;
    auto out = stream.str();

    if (out.find('.') != std::string::npos) {
        out.erase(out.find_last_not_of('0') + 1);
        if (!out.empty() && out.back() == '.') {
            out.pop_back();
        }
    }
    return out;
}

int64_t OKX::numberOfMsForBarSize(const BarSize size) {
    switch (size) {
        case BarSize::_1m:
            return 60000;
        case BarSize::_3m:
            return 60000 * 3;
        case BarSize::_5m:
            return 60000 * 5;
        case BarSize::_15m:
            return 60000 * 15;
        case BarSize::_30m:
            return 60000 * 30;
        case BarSize::_1H:
            return 60000 * 60;
        case BarSize::_2H:
            return 60000 * 120;
        case BarSize::_4H:
            return 60000 * 240;
        case BarSize::_6H:
            return 60000 * 360;
        case BarSize::_12H:
            return 60000 * 720;
        case BarSize::_1D:
            return 86400000;
        case BarSize::_2D:
            return 86400000 * 2;
        case BarSize::_3D:
            return 86400000 * 3;
        case BarSize::_1W:
            return 86400000 * 7;
        case BarSize::_1M:
            return static_cast<int64_t>(86400000) * 30;
        case BarSize::_3M:
            return static_cast<int64_t>(86400000) * 30 * 3;
        default:
            return 0;
    }
}

bool OKX::isValidBarSize(const std::int32_t size, BarSize &barSize) {
    switch (size) {
        case 1:
            barSize = BarSize::_1m;
            return true;
        case 3:
            barSize = BarSize::_3m;
            return true;
        case 5:
            barSize = BarSize::_5m;
            return true;
        case 15:
            barSize = BarSize::_15m;
            return true;
        case 30:
            barSize = BarSize::_30m;
            return true;
        case 60:
            barSize = BarSize::_1H;
            return true;
        case 120:
            barSize = BarSize::_2H;
            return true;
        case 240:
            barSize = BarSize::_4H;
            return true;
        case 360:
            barSize = BarSize::_6H;
            return true;
        case 720:
            barSize = BarSize::_12H;
            return true;
        case 1440:
            barSize = BarSize::_1D;
            return true;
        case 2880:
            barSize = BarSize::_2D;
            return true;
        case 4320:
            barSize = BarSize::_3D;
            return true;
        case 10080:
            barSize = BarSize::_1W;
            return true;
        case 40320:
            barSize = BarSize::_1M;
            return true;
        case 120960:
            barSize = BarSize::_3M;
            return true;
        default:
            return false;
    }
}

CandlestickChannel OKX::barSizeToCandlestickChannel(const BarSize size) {
    switch (size) {
        case BarSize::_1m:
            return CandlestickChannel::candle1m;
        case BarSize::_3m:
            return CandlestickChannel::candle3m;
        case BarSize::_5m:
            return CandlestickChannel::candle5m;
        case BarSize::_15m:
            return CandlestickChannel::candle15m;
        case BarSize::_30m:
            return CandlestickChannel::candle30m;
        case BarSize::_1H:
            return CandlestickChannel::candle1H;
        case BarSize::_2H:
            return CandlestickChannel::candle2H;
        case BarSize::_4H:
            return CandlestickChannel::candle4H;
        case BarSize::_6H:
            return CandlestickChannel::candle6H;
        case BarSize::_12H:
            return CandlestickChannel::candle12H;
        case BarSize::_1D:
            return CandlestickChannel::candle1D;
        case BarSize::_2D:
            return CandlestickChannel::candle2D;
        case BarSize::_3D:
            return CandlestickChannel::candle3D;
        case BarSize::_1W:
            return CandlestickChannel::candle1W;
        case BarSize::_1M:
            return CandlestickChannel::candle1M;
        case BarSize::_3M:
            return CandlestickChannel::candle3M;
        default:
            return CandlestickChannel::candle1H;
    }
}

BarSize OKX::candlestickChannelToBarSize(const CandlestickChannel candlestickChannel) {
    switch (candlestickChannel) {
        case CandlestickChannel::candle1m:
            return BarSize::_1m;
        case CandlestickChannel::candle3m:
            return BarSize::_3m;
        case CandlestickChannel::candle5m:
            return BarSize::_5m;
        case CandlestickChannel::candle15m:
            return BarSize::_15m;
        case CandlestickChannel::candle30m:
            return BarSize::_30m;
        case CandlestickChannel::candle1H:
            return BarSize::_1H;
        case CandlestickChannel::candle2H:
            return BarSize::_2H;
        case CandlestickChannel::candle4H:
            return BarSize::_4H;
        case CandlestickChannel::candle6H:
            return BarSize::_6H;
        case CandlestickChannel::candle12H:
            return BarSize::_12H;
        case CandlestickChannel::candle1D:
            return BarSize::_1D;
        case CandlestickChannel::candle2D:
            return BarSize::_2D;
        case CandlestickChannel::candle3D:
            return BarSize::_3D;
        case CandlestickChannel::candle1W:
            return BarSize::_1W;
        case CandlestickChannel::candle1M:
            return BarSize::_1M;
        case CandlestickChannel::candle3M:
            return BarSize::_3M;
        default:
            return BarSize::_1H;
    }
}
}
