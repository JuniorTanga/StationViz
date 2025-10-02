// StationViz/core/network/NetworkMapBuilder.cpp
#include "NetworkMapBuilder.h"
#include "DatasetResolver.h"
#include <cstdlib>

namespace network {

static uint16_t to_u16(const std::string& s, uint16_t dflt=0) {
    if (s.empty()) return dflt;
    return static_cast<uint16_t>(std::strtoul(s.c_str(), nullptr, 10));
}
static int to_i(const std::string& s, int dflt=-1) {
    if (s.empty()) return dflt;
    return static_cast<int>(std::strtol(s.c_str(), nullptr, 10));
}

NetworkMap NetworkMapBuilder::fromScl(const scl::SclManager& sm) {
    NetworkMap nm;

    // MMS endpoints
    for (auto& kv : sm.mmsEndpoints()) {
        const auto& me = kv.second;
        nm.mms.push_back({me.iedName, me.ip, to_u16(me.port, 102), me.apName});
    }

    // GOOSE endpoints, résolution DataSet → object refs
    for (auto& kv : sm.gseEndpoints()) {
        const auto& ge = kv.second;
        EndpointGse e{ge.iedName, ge.ldInst, ge.cbName, ge.mac,
                      to_u16(ge.appid), to_i(ge.vlanId), to_i(ge.vlanPrio), {}};
        if (!ge.datasetRef.empty()) {
            // datasetRef = nom local sur LLN0; besoin dsName = datasetRef
            if (auto ds = DatasetResolver::resolveLn0(sm, ge.iedName, ge.ldInst, ge.datasetRef)) {
                e.objectRefs = std::move(ds->members);
                // Préparer aussi RCB côté IED (un exemple typique, on affinera si nécessaire)
                nm.rcbByIed[ge.iedName].push_back({ge.ldInst + "/LLN0.RP" + ge.name,  // hypothèse nommage
                                                   ds->dataSetRef, false, true, e.objectRefs});
            }
        }
        nm.gse.push_back(std::move(e));
    }

    // SV endpoints (idem)
    for (auto& kv : sm.svEndpoints()) {
        const auto& se = kv.second;
        EndpointSv e{se.iedName, se.ldInst, se.cbName, se.mac,
                     to_u16(se.appid), to_i(se.vlanId), to_i(se.vlanPrio), {}};
        if (!se.datasetRef.empty()) {
            if (auto ds = DatasetResolver::resolveLn0(sm, se.iedName, se.ldInst, se.datasetRef)) {
                e.objectRefs = std::move(ds->members);
            }
        }
        nm.sv.push_back(std::move(e));
    }

    return nm;
}

} // namespace network
