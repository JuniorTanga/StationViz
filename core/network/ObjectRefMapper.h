// StationViz/core/network/ObjectRefMapper.h
#pragma once
#include <string>
#include "NetworkTypes.h"
#include "../scl/SclTypes.h" // pour scl::FcdaRef

namespace network {

// Génère l'object reference MMS "LD/LN.item(component)[FC]"
// cf. libIEC61850: IedConnection_getDataSetDirectory docs.
class ObjectRefMapper {
public:
    static std::string toMmsRef(const std::string& ldInst, const scl::FcdaRef& f);
};

} // namespace network
