// StationViz/core/network/DatasetResolver.cpp
#include "DatasetResolver.h"

namespace network {

std::optional<DatasetResolver::ResolvedDS>
DatasetResolver::resolveLn0(const scl::SclManager& sm,
                            const std::string& ied,
                            const std::string& ldInst,
                            const std::string& dsName)
{
    auto iedRes = sm.findIED(ied);
    if (!iedRes) return std::nullopt;
    const scl::IED* iedPtr = iedRes.value();

    const auto* ld = sm.findLogicalDevice(*iedPtr, ldInst);
    if (!ld) return std::nullopt;

    // Dans ln0.datasets cherche dsName
    const auto& ln0 = ld->ln0;
    for (const auto& ds : ln0.datasets) {
        if (ds.name == dsName) {
            ResolvedDS out;
            // MMS wants the '$' form: "LD0/LLN0$DS1". The old "/" and "."
            // spelling was never accepted by a server.
            out.dataSetRef = ldInst + "/LLN0$" + ds.name;
            out.members.reserve(ds.members.size());
            for (const auto& fcda : ds.members) {
                // A member may name a different LD; ObjectRefMapper takes the
                // FCDA's own ldInst when it has one.
                const std::string& mld = fcda.ldInst.empty() ? ldInst : fcda.ldInst;
                out.members.push_back(ObjectRefMapper::toMmsRef(mld, fcda));
            }
            return out;
        }
    }
    return std::nullopt;
}

} // namespace network
