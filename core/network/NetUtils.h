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

std::optional<MacAddr> parseMac(const std::string& macStr); // "01:0C:CD:01:00:01"

} // namespace network
