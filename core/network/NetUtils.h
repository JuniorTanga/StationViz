// StationViz/core/network/NetUtils.h
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <optional>

namespace network {

struct MacAddr {
    std::array<uint8_t, 6> b{};
};

// Parses a MAC written with ':', '-' or '.' separators, in either case.
// Every SCD in the repository writes dashes ("01-0C-CD-01-00-01"); the previous
// parser split on ':' only, so it rejected 100% of real data and GOOSE and SV
// failed with no diagnostic.
std::optional<MacAddr> parseMac(const std::string& macStr);

// APPID and VLAN-ID are hexadecimal in SCL. "4001" decimal is 16385, and any
// value containing a letter parsed to 0, so incoming GOOSE was filtered on the
// wrong identifier and every frame was dropped. VLAN-ID 0 is legal, hence the
// separate empty check rather than a sentinel value.
std::optional<uint16_t> parseAppId(const std::string& s);
std::optional<uint16_t> parseVlanId(const std::string& s);

} // namespace network