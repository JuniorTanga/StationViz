// StationViz/core/network/NetworkMapBuilder.h
#pragma once
#include "../scl/SclManager.h"
#include "NetworkTypes.h"

namespace network {

class NetworkMapBuilder {
public:
    static NetworkMap fromScl(const scl::SclManager& sm);
};

} // namespace network
