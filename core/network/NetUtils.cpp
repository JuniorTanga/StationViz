// StationViz/core/network/NetUtils.cpp
#include "NetUtils.h"
#include <sstream>
#include <iomanip>

namespace network {

std::optional<MacAddr> parseMac(const std::string& macStr) {
    MacAddr out{};
    std::istringstream iss(macStr);
    std::string byteStr;
    int i = 0;
    while (std::getline(iss, byteStr, ':')) {
        if (byteStr.size() > 2 || byteStr.empty() || i >= 6) return std::nullopt;
        unsigned int val;
        std::istringstream bs(byteStr);
        bs >> std::hex >> val;
        if (bs.fail() || val > 0xFF) return std::nullopt;
        out.b[i++] = static_cast<uint8_t>(val);
    }
    if (i != 6) return std::nullopt;
    return out;
}

} // namespace network
