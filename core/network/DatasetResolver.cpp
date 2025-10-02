// StationViz/core/network/DatasetResolver.cpp
#include "DatasetResolver.h"

namespace network {

std::optional<DatasetResolver::ResolvedDS>
DatasetResolver::resolveLn0(const scl::SclManager& sm,
                            const std::string& ied,
                            const std::string& ldInst,
                            const std::string& dsName)
{
    auto* iedPtr = sm.getIed(ied);
    if (!iedPtr) return std::nullopt;

    // Cherche LD par ldInst
    const auto* ld = sm.getLogicalDevice(ied, ldInst);
    if (!ld) return std::nullopt;

    // Dans ln0.datasets cherche dsName
    const auto& ln0 = ld->ln0;
    for (const auto& ds : ln0.datasets) {
        if (ds.name == dsName) {
            ResolvedDS out;
            out.dataSetRef = ldInst + std::string("/LLN0.") + ds.name;
            out.members.reserve(ds.members.size());
            for (const auto& fcda : ds.members) {
                out.members.push_back(ObjectRefMapper::toMmsRef(ldInst, fcda));
            }
            return out;
        }
    }
    return std::nullopt;
}

} // namespace network
