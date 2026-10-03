// StationViz/core/network/ObjectRefMapper.cpp
#include "ObjectRefMapper.h"

#include "../scl/SclManager.h"

namespace network {

// Delegates to sclLib. Two implementations of this existed and both produced
// the invalid form "LD/LN.DO.DA[FC]"; one of them would inevitably drift.
std::string ObjectRefMapper::toMmsRef(const std::string& ldInst,
                                      const scl::FcdaRef& f) {
    return scl::SclManager::fcdaToMmsRef(ldInst, f);
}

} // namespace network
