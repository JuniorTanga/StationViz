#include "SldManager.h"

#include <nlohmann/json.hpp>

namespace sld {

SldManager::SldManager(const scl::SclManager* sclMgr)
    : sclMgr_(sclMgr), builder_(sclMgr ? sclMgr->model() : nullptr) {}

scl::Status SldManager::build() {
    plan_ = SldPlan{};
    return builder_.build(plan_);
}

std::string SldManager::planJson() const {
    using nlohmann::json;
    json J;

    // Flat node list. AppContext reads "graph.nodes" with id/label/kind/eKind and
    // the ss/vl/bay context, so the shape is kept deliberately.
    J["graph"] = json::object();
    J["graph"]["nodes"] = json::array();
    for (const auto& n : plan_.nodes) {
        json jn{{"id", n.id}, {"label", n.label}, {"kind", n.kind},
                {"ss", n.ss}, {"vl", n.vl}, {"bay", n.bay}};
        if (!n.eKind.empty()) jn["eKind"] = n.eKind;
        J["graph"]["nodes"].push_back(std::move(jn));
    }

    J["buses"] = json::array();
    for (const auto& b : plan_.buses) {
        json jb{{"id", b.id}, {"ss", b.ss}, {"vl", b.vl},
                {"label", b.label}, {"bay", b.bay}};
        jb["members"] = b.cnPaths;
        J["buses"].push_back(std::move(jb));
    }

    J["feeders"] = json::array();
    for (const auto& f : plan_.feeders) {
        json jf{{"id", f.id}, {"bus", f.busId}, {"ss", f.ss}, {"vl", f.vl},
                {"bay", f.bay}, {"lane", f.laneIndex},
                {"endpoint", f.endpointType}, {"busLabel", f.busLabel}};
        jf["chain"] = f.chain;
        // Roles resolved by sldLib rather than sniffed from kinds in the UI.
        jf["roles"] = {
            {"busSideDs", f.roles.busSideDisconnector},
            {"breaker", f.roles.breaker},
            {"ct", f.roles.currentTransformer},
            {"vt", f.roles.voltageTransformer},
            {"lineSideDs", f.roles.lineSideDisconnector},
            {"endpoint", f.roles.endpoint},
            {"endpointKind", f.roles.endpointKind},
        };
        J["feeders"].push_back(std::move(jf));
    }

    J["couplers"] = json::array();
    for (const auto& c : plan_.couplers) {
        json jc{{"equip", c.couplerEquipId}, {"busA", c.busA}, {"busB", c.busB},
                {"ss", c.ss}, {"vl", c.vl}, {"bay", c.bay}};
        jc["type"] = c.isBreaker ? "CB" : "DS";
        J["couplers"].push_back(std::move(jc));
    }

    // One entry per winding, so a three-winding transformer is representable.
    J["transformers"] = json::array();
    for (const auto& t : plan_.transformers) {
        for (const auto& w : t.windings) {
            json jt{{"tr", t.transformerId}, {"bus", w.bus}, {"busLabel", w.busLabel},
                    {"winding", w.winding}, {"ss", w.ss}, {"vl", w.vl}};
            J["transformers"].push_back(std::move(jt));
        }
    }

    J["plan_transformers"] = json::array();
    for (const auto& t : plan_.plan_transformers) {
        json jt{{"id", t.id}, {"ss", t.ss}, {"vl", t.vl}, {"label", t.label},
                {"hasTapChanger", t.hasTapChanger}};
        jt["buses"] = t.buses;
        J["plan_transformers"].push_back(std::move(jt));
    }

    if (!plan_.equipmentsFromIEDs.empty()) {
        std::map<std::string, std::map<std::string,
                     std::vector<const scl::EquipmentFromIED*>>> byIed;
        for (const auto& e : plan_.equipmentsFromIEDs)
            byIed[e.iedName][e.ldInst].push_back(&e);

        J["ieds"] = json::array();
        for (const auto& [ied, lds] : byIed) {
            json jIed{{"name", ied}, {"lds", json::array()}};
            for (const auto& [ldInst, eqs] : lds) {
                json jLd{{"inst", ldInst}, {"equipments", json::array()}};
                for (const auto* e : eqs) {
                    json je{{"lnClass", e->lnClass}, {"lnInst", e->lnInst}};
                    if (!e->prefix.empty())        je["prefix"] = e->prefix;
                    if (!e->primaryAnchors.empty()) je["anchors"] = e->primaryAnchors;
                    jLd["equipments"].push_back(std::move(je));
                }
                jIed["lds"].push_back(std::move(jLd));
            }
            J["ieds"].push_back(std::move(jIed));
        }
    }

    J["warnings"] = plan_.warnings;
    return J.dump();
}

} // namespace sld
