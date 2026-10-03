// StationViz/core/network/NetUtils.cpp
#include "NetUtils.h"

#include <cctype>
#include <cstdlib>

namespace network {
namespace {

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool isSeparator(char c) { return c == ':' || c == '-' || c == '.'; }

} // namespace

std::optional<MacAddr> parseMac(const std::string& macStr) {
    MacAddr out{};
    int i = 0;
    std::size_t pos = 0;

    while (i < 6) {
        if (pos >= macStr.size()) return std::nullopt;

        // Exactly two hex digits, then a separator (or end for the last byte).
        const int hi = hexDigit(macStr[pos]);
        const int lo = (pos + 1 < macStr.size()) ? hexDigit(macStr[pos + 1]) : -1;
        if (hi < 0 || lo < 0) return std::nullopt;
        out.b[i++] = static_cast<uint8_t>(hi * 16 + lo);
        pos += 2;

        if (i < 6) {
            if (pos >= macStr.size() || !isSeparator(macStr[pos])) return std::nullopt;
            ++pos;
        }
    }

    // Nothing may follow the sixth byte.
    if (pos != macStr.size()) return std::nullopt;
    return out;
}

std::optional<uint16_t> parseAppId(const std::string& s) {
    if (s.empty()) return std::nullopt;
    uint16_t v = 0;
    for (char c : s) {
        const int d = hexDigit(c);
        if (d < 0) return std::nullopt;
        // APPID is 16 bits; a wider value means the SCD is malformed.
        if (v > (0xFFFFu >> 4)) return std::nullopt;
        v = static_cast<uint16_t>(v * 16 + d);
    }
    return v;
}

std::optional<uint16_t> parseVlanId(const std::string& s) {
    return parseAppId(s);
}

} // namespace network