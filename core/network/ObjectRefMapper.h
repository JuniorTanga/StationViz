// StationViz/core/network/ObjectRefMapper.h
#pragma once
#include <string>
#include "NetworkTypes.h"
#include "../scl/SclTypes.h"   // scl::FcdaRef

namespace network {

// MMS object reference for an FCDA, delegating to sclLib so there is a single
// implementation: IEC 61850-7-2 spells it LdInst/LNName$FC$DO$DA.
class ObjectRefMapper {
public:
    static std::string toMmsRef(const std::string& ldInst, const scl::FcdaRef& f);
};

} // namespace network
