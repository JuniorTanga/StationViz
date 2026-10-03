// StationViz/core/network/NetworkMapBuilder.cpp
#include "NetworkMapBuilder.h"
#include "DatasetResolver.h"
#include <cstdlib>

namespace network {

static uint16_t to_u16(const std::string& s, uint16_t dflt=0) {
    if (s.empty()) return dflt;
    return static_cast<uint16_t>(std::strtoul(s.c_str(), nullptr, 10));
}
// LDevice that owns a given ReportControl, found by identity.
static std::string ldInstOfRcb(const scl::IED& ied,
                               const scl::ReportControlMeta* rc) {
    for (const auto& ld : ied.ldevices)
        for (const auto& c : ld.ln0.rptCtrls)
            if (&c == rc) return ld.inst;
    for (const auto& ap : ied.accessPoints)
        for (const auto& ld : ap.ldevices)
            for (const auto& c : ld.ln0.rptCtrls)
                if (&c == rc) return ld.inst;
    return {};
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
            if (auto ds = DatasetResolver::resolveLn0(sm, ge.iedName, ge.ldInst,
                                                      ge.datasetRef)) {
                e.objectRefs = std::move(ds->members);
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

    // Report control blocks, taken from the SCL's <ReportControl> elements.
    //
    // These were previously fabricated from the GOOSE control blocks, which was
    // wrong in both directions: a GSEControl has nothing to do with reporting,
    // so every GOOSE endpoint invented an RCB (an IED with 5 GOOSE blocks and 3
    // real report controls produced 5 entries, 3 of them bogus, and missed the 2
    // real ones). The fabricated reference never matched anything a server
    // exposes either, so getRCBValues silently failed and reporting was dead.
    // Bind by reference. Writing this as
    // `sm.model() ? sm.model()->ieds : std::vector<scl::IED>{}` yields a
    // prvalue, so the loop iterated a *copy* of the vector and ldInstOfRcb(),
    // which matches by pointer identity, could never find the LDevice. Every
    // reference came out as "/LLN0$..." with no device prefix.
    static const std::vector<scl::IED> empty;
    const std::vector<scl::IED>& ieds =
        (sm.model() && sm.model()->ieds.empty()) ? empty : sm.model()->ieds;

    for (const auto& ied : ieds) {
        for (const auto* rc : sm.reportControlsOf(ied.name)) {
            const std::string ldInst = ldInstOfRcb(ied, rc);
            RcbConfig r;
            r.rcbRef = scl::SclManager::rcbReference(ldInst, *rc);
            r.buffered = rc->buffered;
            // Ask for a general interrogation at startup: an IED shipped with
            // TrgOps=0 delivers nothing until an integrity period fires, and
            // IntgPd often defaults to 0.
            r.giAtStartup = rc->trgOps.generalInterrogation;

            // An unbuffered RCB addresses an implicit DataSet named after the
            // control block, so @datSet may legitimately be absent.
            const std::string dsName = rc->datSet.empty() ? rc->name : rc->datSet;
            if (auto ds = DatasetResolver::resolveLn0(sm, ied.name, ldInst, dsName)) {
                r.dataSetRef = std::move(ds->dataSetRef);
                r.dsMembers  = std::move(ds->members);
            }
            nm.rcbByIed[ied.name].push_back(std::move(r));
        }
    }

    return nm;
}

} // namespace network
