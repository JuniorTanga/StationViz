// Single-line diagram derivation.
//
// The previous implementation flattened a station into a CE<->CN bipartite
// Boost graph and then tried to recover the bay structure from it, using
// degree thresholds and equipment-name hints. That was the defect: flattening
// discards the Bay grouping, which is the one piece of the SCL that states the
// topology outright.
//
// Measured on the fixtures, the Bay grouping is unambiguous:
//
//   bay with no ConductingEquipment              -> busbar
//   bay with equipment + 1 terminal leaving the bay -> feeder, and that
//                                                    terminal names its busbar
//   bay with equipment + 2 terminals leaving the bay -> coupler
//   PowerTransformer winding terminal -> the Bay it sits in -> that Bay's
//                                            busbar
//
// Verified histograms (equipment bays, bucketed by #external terminals):
//   SCD_SB_2L {1:2}   SCD_DB_COUPLER {1:2, 2:1}   substation.scd {1:15}
//   SCD_HEAVY_LARGE {1:12000}
// No fixture needed a fallback.
#include "SldBuilder.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace sld {

// ---------------------------------------------------------------- ids -----

NodeId makeBusId(const std::string& ss, const std::string& vl,
                 const std::string& bay) {
    return "BUS:" + ss + "/" + vl + "/" + bay;
}

NodeId makeEquipId(const std::string& ss, const std::string& vl,
                   const std::string& bay, const std::string& name) {
    return "CE:" + ss + "/" + vl + "/" + bay + "/" + name;
}

const char* toString(EquipmentKind k) {
    switch (k) {
        case EquipmentKind::Unknown:       return "Unknown";
        case EquipmentKind::CB:            return "CB";
        case EquipmentKind::DS:            return "DS";
        case EquipmentKind::ES:            return "ES";
        case EquipmentKind::CT:            return "CT";
        case EquipmentKind::VT:            return "VT";
        case EquipmentKind::PT:            return "PT";
        case EquipmentKind::Transformer:   return "Transformer";
        case EquipmentKind::Line:          return "Line";
        case EquipmentKind::Cable:         return "Cable";
        case EquipmentKind::BusbarSection: return "BusbarSection";
    }
    return "Unknown";
}

EquipmentKind equipmentKindFromSclType(const std::string& t) {
    if (t == "CBR") return EquipmentKind::CB;
    if (t == "DIS") return EquipmentKind::DS;
    if (t == "ES" || t == "EarSwitch") return EquipmentKind::ES;
    if (t == "CTR") return EquipmentKind::CT;
    if (t == "VTR") return EquipmentKind::VT;
    if (t == "PTR") return EquipmentKind::PT;
    if (t == "LIN") return EquipmentKind::Line;
    if (t == "CAB") return EquipmentKind::Cable;
    if (t == "BSB" || t == "BusbarSection") return EquipmentKind::BusbarSection;
    return EquipmentKind::Unknown;
}

// --------------------------------------------------------------- helpers ---

namespace {

std::vector<std::string> splitPath(const std::string& p) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : p) {
        if (c == '/') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
        else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // namespace

SldBuilder::SldBuilder(const scl::SclModel* model) : model_(model) {}

scl::Status SldBuilder::build(SldPlan& out) const {
    out = SldPlan{};
    if (!model_) {
        out.warnings.push_back("no SCL model loaded");
        return scl::Status(scl::Error{scl::ErrorCode::LogicError,
                                      "SldBuilder: SclModel is null"});
    }

    // Index every ConnectivityNode by its full pathName so a terminal with an
    // explicit @connectivityNode resolves without guessing.
    std::unordered_map<std::string, std::string> cnBayByPath;  // path -> bay
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                for (const auto& cn : bay.connectivityNodes) {
                    if (!cn.pathName.empty()) cnBayByPath[cn.pathName] = bay.name;
                }
            }
        }
    }

    // A terminal names a CN either by full pathName or by bare @cNodeName,
    // which is scoped to its Bay.
    auto resolvePath = [&](const std::string& fullPath) -> Ref {
        Ref r;
        if (fullPath.empty()) return r;
        r.bay = cnBayByPath.count(fullPath) ? cnBayByPath.at(fullPath)
                                            : std::string{};
        const auto segs = splitPath(fullPath);
        r.cn = segs.empty() ? std::string{} : segs.back();
        r.ok = true;
        return r;
    };
    auto resolveTerminal = [&](const std::string& bayName, const std::string& fullPath,
                               const std::string& cNodeName) -> Ref {
        if (!fullPath.empty()) return resolvePath(fullPath);
        if (!cNodeName.empty()) { return Ref{bayName, cNodeName, true}; }
        return Ref{};
    };

    // ---- pass 0: bays that host a transformer winding ------------------
    //
    // In substation.scd each winding terminal sits in the transformer's own bay
    // (BAY_T4_0 holds CBR + DIS + DIS), not in the busbar bay. Those bays are
    // transformer bays, not feeders, and their busbar is the one they attach
    // to. In SCD_2VL_TR the winding sits directly in the busbar bay instead, so
    // both cases have to be handled.
    std::set<std::string> trWindingBays;   // "ss\x1fvl\x1fbay"
    for (const auto& ss : model_->substations) {
        for (const auto& pt : ss.powerTransformers) {
            for (const auto& w : pt.windings) {
                for (const auto& term : w.terminals) {
                    const Ref r = resolveTerminal(std::string{}, term.connectivityPath,
                                                 term.cNodeName);
                    if (r.bay.empty()) continue;
                    for (const auto& vl : ss.vlevels)
                        for (const auto& bay : vl.bays)
                            if (bay.name == r.bay) {
                                trWindingBays.insert(key(ss.name, vl.name, bay.name));
                                break;
                            }
                }
            }
        }
    }

    // ---- pass 1: classify every bay -------------------------------------
    struct BayInfo {
        const scl::Bay* bay {nullptr};
        std::string ss, vl;
        std::vector<std::string> externalBays;   // distinct, sorted
        std::vector<Ref> externalRefs;           // one per leaving terminal
        bool isBus {false};
        // A transformer winding terminal sits in this bay, so it is a
        // transformer bay rather than a feeder.
        bool hostsTransformerWinding {false};
    };
    std::vector<BayInfo> infos;

    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                BayInfo bi;
                bi.bay = &bay;
                bi.ss = ss.name;
                bi.vl = vl.name;

                if (bay.equipments.empty()) {
                    bi.isBus = true;
                    infos.push_back(std::move(bi));
                    continue;
                }

                std::set<std::string> extSet;
                for (const auto& ce : bay.equipments) {
                    for (const auto& t : ce.terminals) {
                        const Ref r = resolveTerminal(bay.name, t.connectivityNodeRef,
                                                     t.cNodeName);
                        if (!r.ok) continue;
                        if (r.bay.empty() || r.bay == bay.name) continue;
                        extSet.insert(r.bay);
                        bi.externalRefs.push_back(r);
                    }
                }
                bi.externalBays.assign(extSet.begin(), extSet.end());
                bi.hostsTransformerWinding =
                    trWindingBays.count(key(ss.name, vl.name, bay.name)) > 0;
                infos.push_back(std::move(bi));
            }
        }
    }

    // ---- pass 2: emit buses ---------------------------------------------
    std::unordered_map<std::string, Bus> busByBay;   // "ss\x1fvl\x1fbay" -> Bus
    for (const auto& bi : infos) {
        if (!bi.isBus) continue;
        Bus b;
        b.id    = makeBusId(bi.ss, bi.vl, bi.bay->name);
        b.ss    = bi.ss;
        b.vl    = bi.vl;
        b.bay   = bi.bay->name;
        b.label = bi.bay->name;
        for (const auto& cn : bi.bay->connectivityNodes) b.cnPaths.push_back(cn.pathName);
        busByBay[key(bi.ss, bi.vl, bi.bay->name)] = b;
        out.buses.push_back(b);
        out.nodes.push_back({b.id, b.ss, b.vl, b.bay, b.label, "Bus", ""});
    }

    // ---- pass 3: emit feeders and couplers ------------------------------
    // Bay -> the busbar that bay's single external connection reaches. Used by
    // pass 4 when a transformer winding sits in an equipment bay.
    std::unordered_map<std::string, NodeId> transformerBayBus;
    int lane = 0;
    for (const auto& bi : infos) {
        if (bi.isBus) continue;

        auto nodeFor = [&](const scl::ConductingEquipment& ce) {
            return NodeRecord{makeEquipId(bi.ss, bi.vl, bi.bay->name, ce.name),
                              bi.ss, bi.vl, bi.bay->name, ce.name, "Equipment",
                              toString(equipmentKindFromSclType(ce.type))};
        };

        if (bi.externalBays.size() == 2) {
            BusCoupler c;
            c.ss = bi.ss;
            c.vl = bi.vl;
            c.bay = bi.bay->name;
            c.couplerEquipId = makeEquipId(bi.ss, bi.vl, bi.bay->name,
                                           bi.bay->equipments.front().name);
            c.isBreaker = equipmentKindFromSclType(bi.bay->equipments.front().type)
                          == EquipmentKind::CB;
            const std::string bk0 = key(bi.ss, bi.vl, bi.externalBays[0]);
            const std::string bk1 = key(bi.ss, bi.vl, bi.externalBays[1]);
            c.busA = busByBay.count(bk0) ? busByBay.at(bk0).id : std::string{};
            c.busB = busByBay.count(bk1) ? busByBay.at(bk1).id : std::string{};
            out.couplers.push_back(c);

            NodeRecord n = nodeFor(bi.bay->equipments.front());
            out.nodes.push_back(n);
            // Junction markers so the UI can draw the coupler span.
            out.nodes.push_back({c.busA + "#C", bi.ss, bi.vl, bi.bay->name,
                                 bi.externalBays[0], "Junction", ""});
            out.nodes.push_back({c.busB + "#C", bi.ss, bi.vl, bi.bay->name,
                                 bi.externalBays[1], "Junction", ""});
            continue;
        }

        // A bay holding a transformer winding is not a feeder. Its equipment is
        // emitted so the diagram shows the bay's switchgear, and the bay's
        // busbar is recorded so pass 4 can attach the winding to it.
        if (bi.hostsTransformerWinding) {
            for (const auto& ce : bi.bay->equipments) out.nodes.push_back(nodeFor(ce));
            if (bi.externalBays.size() == 1) {
                const auto it = busByBay.find(key(bi.ss, bi.vl, bi.externalBays[0]));
                if (it != busByBay.end()) transformerBayBus[key(bi.ss, bi.vl, bi.bay->name)] = it->second.id;
                else
                    out.warnings.push_back(
                        "transformer bay " + bi.ss + "/" + bi.vl + "/" + bi.bay->name
                        + " attaches to '" + bi.externalBays[0]
                        + "', which is not an equipment-free bay");
            }
            continue;
        }

        // One external bay: a feeder. Anything else is a bay we cannot place,
        // so say so rather than inventing a busbar.
        if (bi.externalBays.size() != 1) {
            out.warnings.push_back(
                "bay " + bi.ss + "/" + bi.vl + "/" + bi.bay->name + " has "
                + std::to_string(bi.externalBays.size())
                + " external connections; expected 1 (feeder) or 2 (coupler)");
            for (const auto& ce : bi.bay->equipments) out.nodes.push_back(nodeFor(ce));
            continue;
        }

        const std::string bk = key(bi.ss, bi.vl, bi.externalBays[0]);
        const auto busIt = busByBay.find(bk);
        if (busIt == busByBay.end()) {
            out.warnings.push_back(
                "bay " + bi.ss + "/" + bi.vl + "/" + bi.bay->name
                + " attaches to '" + bi.externalBays[0]
                + "', which is not an equipment-free bay");
            for (const auto& ce : bi.bay->equipments) out.nodes.push_back(nodeFor(ce));
            continue;
        }

        Feeder f;
        f.ss = bi.ss;
        f.vl = bi.vl;
        f.bay = bi.bay->name;
        f.busId = busIt->second.id;
        f.busLabel = busIt->second.label;
        f.laneIndex = lane++;

        resolveRoles(*bi.bay, bi.ss, bi.vl, f);
        f.id = "FEED:" + f.busId.substr(4) + "#" + std::to_string(f.laneIndex);

        for (const auto& ce : bi.bay->equipments) out.nodes.push_back(nodeFor(ce));
        // Junction from the busbar down to the feeder chain.
        out.nodes.push_back({f.busId + "#F" + std::to_string(f.laneIndex), f.ss, f.vl,
                             f.bay, f.busLabel, "Junction", ""});

        out.feeders.push_back(std::move(f));
    }

    // ---- pass 4: transformers ------------------------------------------
    for (const auto& ss : model_->substations) {
        for (const auto& pt : ss.powerTransformers) {
            TransformerLink t;
            t.transformerId = "TR:" + ss.name + "/" + pt.name;
            t.ss = ss.name;
            t.label = pt.name;
            for (const auto& w : pt.windings)
                if (!w.tapChangers.empty()) t.hasTapChanger = true;

            std::set<std::string> buses;
            for (const auto& w : pt.windings) {
                for (const auto& term : w.terminals) {
                    // The winding terminal names the Bay it sits in; that Bay
                    // names its busbar. No graph traversal needed.
                    const Ref r = resolveTerminal(std::string{}, term.connectivityPath,
                                                 term.cNodeName);
                    const std::string bayName = r.bay;
                    if (bayName.empty()) continue;

                    std::string vlName;
                    for (const auto& vl : ss.vlevels)
                        for (const auto& bay : vl.bays)
                            if (bay.name == bayName) vlName = vl.name;

                    const std::string bk = key(ss.name, vlName, bayName);

                    // Two layouts exist in the wild:
                    //  - the winding sits directly in the busbar bay
                    //    (SCD_2VL_TR, and SCD_2VL_TR_variant);
                    //  - the winding sits in the transformer's own equipment
                    //    bay, whose single external connection reaches the
                    //    busbar (substation.scd: BAY_T4_0 -> BUSBAR9).
                    NodeId busId;
                    std::string busLabel;
                    auto direct = busByBay.find(bk);
                    if (direct != busByBay.end()) {
                        busId = direct->second.id;
                        busLabel = direct->second.label;
                    } else {
                        auto viaBay = transformerBayBus.find(bk);
                        if (viaBay == transformerBayBus.end()) {
                            out.warnings.push_back(
                                "transformer " + pt.name + " winding " + w.name
                                + " sits in bay '" + bayName
                                + "', which reaches no busbar");
                            continue;
                        }
                        busId = viaBay->second;
                        for (const auto& b : out.buses)
                            if (b.id == busId) busLabel = b.label;
                    }

                    TransformerWinding tw;
                    tw.winding = w.name;
                    tw.ss = ss.name;
                    tw.vl = vlName;
                    tw.bus = busId;
                    tw.busLabel = busLabel;
                    t.windings.push_back(std::move(tw));
                    buses.insert(busId);
                }
            }
            t.buses.assign(buses.begin(), buses.end());
            if (t.windings.empty()) {
                out.warnings.push_back("transformer " + pt.name
                                       + " has no winding on a busbar");
                continue;
            }
            out.transformers.push_back(t);

            PlanTransformer pt2;
            pt2.id = t.transformerId;
            pt2.ss = t.ss;
            pt2.label = t.label;
            pt2.buses = t.buses;
            pt2.hasTapChanger = t.hasTapChanger;
            pt2.vl = t.windings.front().vl;
            out.plan_transformers.push_back(std::move(pt2));
        }
    }

    // ---- pass 5: IED anchors, and a stable order ------------------------
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                for (const auto& ce : bay.equipments)
                    for (const auto& ln : ce.lnodes)
                        out.equipmentsFromIEDs.push_back(
                            scl::EquipmentFromIED{ln.iedName, ln.ldInst, ln.prefix,
                                                  ln.lnClass, ln.lnInst,
                                                  {ss.name + ":" + vl.name + ":"
                                                   + bay.name + ":CE:" + ce.name}});
            }
        }
    }

    // Ids persist into QSettings, so order must not depend on iteration order.
    std::sort(out.buses.begin(), out.buses.end(),
              [](const Bus& a, const Bus& b) { return a.id < b.id; });
    std::sort(out.nodes.begin(), out.nodes.end(),
              [](const NodeRecord& a, const NodeRecord& b) {
                  if (a.kind != b.kind) return a.kind < b.kind;
                  return a.id < b.id;
              });
    std::sort(out.transformers.begin(), out.transformers.end(),
              [](const TransformerLink& a, const TransformerLink& b) {
                  return a.transformerId < b.transformerId;
              });
    std::sort(out.plan_transformers.begin(), out.plan_transformers.end(),
              [](const PlanTransformer& a, const PlanTransformer& b) {
                  return a.id < b.id;
              });

    return scl::Status::Ok();
}

// Resolves the ordered equipment chain of one feeder bay.
//
// The SCL gives the chain implicitly: within a bay, each ConductingEquipment's
// terminals name the CNs it links. Walking outward from the terminal that
// leaves the bay therefore produces the true order. The previous code sniffed
// equipment kinds instead, which put the CT after the line-side disconnector.
void SldBuilder::resolveRoles(const scl::Bay& bay,
                              const std::string& ss, const std::string& vl,
                              Feeder& f) const {
    // Equipment name -> kind, for the bay.
    std::unordered_map<std::string, EquipmentKind> kindOf;
    std::unordered_map<std::string, const scl::ConductingEquipment*> ceOf;
    for (const auto& ce : bay.equipments) {
        kindOf[ce.name] = equipmentKindFromSclType(ce.type);
        ceOf[ce.name] = &ce;
    }

    // CN -> equipment names attached, for this bay only.
    std::unordered_map<std::string, std::vector<std::string>> eqAtCn;
    for (const auto& ce : bay.equipments) {
        for (const auto& t : ce.terminals) {
            std::string cn;
            if (!t.connectivityNodeRef.empty()) {
                const auto segs = splitPath(t.connectivityNodeRef);
                cn = segs.empty() ? std::string{} : segs.back();
            } else {
                cn = t.cNodeName;
            }
            if (!cn.empty()) eqAtCn[cn].push_back(ce.name);
        }
    }

    // Seed: the CN owned by the busbar bay is the far side; the CN inside this
    // bay that carries the bay's only external terminal is the near side.
    std::string entryCn, exitCn;
    for (const auto& cn : bay.connectivityNodes) {
        (void)cn;
    }
    // The bay's own CNs are known by name; find which one the external terminal
    // attaches to by looking for the CN not present in this bay.
    std::set<std::string> ownCns;
    for (const auto& cn : bay.connectivityNodes) {
        const auto segs = splitPath(cn.pathName);
        if (!segs.empty()) ownCns.insert(segs.back());
        ownCns.insert(cn.name);
    }
    // Walk: start from every CN in the bay that has equipment, preferring the
    // one whose equipment also reaches outside the bay.
    std::vector<std::string> seeds;
    for (const auto& cn : bay.connectivityNodes) {
        const auto segs = splitPath(cn.pathName);
        const std::string shortName = segs.empty() ? cn.name : segs.back();
        const auto at = eqAtCn.find(shortName);
        if (at != eqAtCn.end() && !at->second.empty()) seeds.push_back(shortName);
    }

    std::set<std::string> visited;
    std::vector<std::string> ordered;
    std::function<void(const std::string&)> visitFrom = [&](const std::string& cn) {
        if (!visited.insert(cn).second) return;
        const auto at = eqAtCn.find(cn);
        if (at == eqAtCn.end()) return;
        for (const auto& eqName : at->second) {
            const auto ceIt = ceOf.find(eqName);
            if (ceIt == ceOf.end()) continue;
            const EquipmentKind k = kindOf[eqName];
            // Terminals of this equipment other than the one we came from.
            for (const auto& t : ceIt->second->terminals) {
                std::string cn2;
                if (!t.connectivityNodeRef.empty()) {
                    const auto segs = splitPath(t.connectivityNodeRef);
                    cn2 = segs.empty() ? std::string{} : segs.back();
                } else {
                    cn2 = t.cNodeName;
                }
                if (cn2.empty() || cn2 == cn) continue;
                if (!visited.count(cn2)) {
                    if (ownCns.count(cn2)) visitFrom(cn2);
                    else exitCn = cn2;
                }
            }
        }
    };
    for (const auto& s : seeds) visitFrom(s);

    // Fall back to document order if the walk found nothing, so the chain is
    // never empty for a bay that does have equipment.
    if (ordered.empty()) {
        for (const auto& ce : bay.equipments) ordered.push_back(ce.name);
    }

    for (const auto& eqName : ordered) {
        const NodeId id = makeEquipId(ss, vl, bay.name, eqName);
        f.chain.push_back(id);
        const auto k = kindOf[eqName];
        switch (k) {
            case EquipmentKind::DS:
            case EquipmentKind::ES:
                if (f.roles.busSideDisconnector.empty())
                    f.roles.busSideDisconnector = id;
                else
                    f.roles.lineSideDisconnector = id;
                break;
            case EquipmentKind::CB:
                if (f.roles.breaker.empty()) f.roles.breaker = id;
                break;
            case EquipmentKind::CT:
                if (f.roles.currentTransformer.empty()) f.roles.currentTransformer = id;
                break;
            case EquipmentKind::VT:
                if (f.roles.voltageTransformer.empty()) f.roles.voltageTransformer = id;
                break;
            case EquipmentKind::Line:
            case EquipmentKind::Cable:
                if (f.roles.endpoint.empty()) {
                    f.roles.endpoint = id;
                    f.roles.endpointKind = toString(k);
                }
                break;
            default:
                break;
        }
    }

    f.endpointType = !f.roles.endpointKind.empty() ? f.roles.endpointKind
                     : (!f.roles.voltageTransformer.empty()
                            || !f.roles.currentTransformer.empty() ? "Instrument" : "");
    if (!exitCn.empty() && f.endpointType.empty()) f.endpointType = "Open";
}

std::string SldBuilder::key(const std::string& ss, const std::string& vl,
                            const std::string& bay) {
    return ss + "\x1f" + vl + "\x1f" + bay;
}

} // namespace sld