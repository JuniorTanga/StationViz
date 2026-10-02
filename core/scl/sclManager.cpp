#include "SclManager.h"
#include "SclParser.h"
#include "EquipmentClassifier.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace scl;
using nlohmann::json;

//======== Helpers ==========//
static std::string keyGse(const std::string& ied, const std::string& ld, const std::string& cb){
    return ied + "|" + ld + "|" + cb;
}
static std::string keyMms(const std::string& ied, const std::string& ap){
    return ied + "|" + ap;
}
// SCL uses '/' as the path separator ("S1/VL1/BAY1/CN1") while our logical
// keys use ':' ("S1:VL1:BAY1:CN1"). Split on both so lastSegment() behaves the
// same for either form.
static std::string lastSegment(const std::string& path) {
    auto pos = path.find_last_of("/:");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}



std::string SclManager::logicalCNKey(std::string_view ss, std::string_view vl,
                                     std::string_view bay, std::string_view cn) {
    std::string out; out.reserve(ss.size()+vl.size()+bay.size()+cn.size()+3);
    out.append(ss); out.push_back(':');
    out.append(vl); out.push_back(':');
    out.append(bay); out.push_back(':');
    out.append(cn);
    return out;
}

// DatasetKey hashing / equality
bool operator==(SclManager::DatasetKey const& a, SclManager::DatasetKey const& b) noexcept {
    return a.ied == b.ied && a.ld == b.ld && a.name == b.name;
}
size_t SclManager::DatasetKeyHash::operator()(DatasetKey const& k) const noexcept {
    std::hash<std::string> H;
    size_t h = H(k.ied);
    h ^= (H(k.ld)   << 1);
    h ^= (H(k.name) << 2);
    return h;
}

//===========================//

SclManager::SclManager() = default;

void SclManager::validate_() {
    if (!model_) return;

    // 1) Doublons d’IED
    {
        std::unordered_set<std::string> seen;
        for (const auto& ied : model_->ieds) {
            if (!seen.insert(ied.name).second) {
                diags_.push_back({
                    ErrorCode::DuplicateIEDName,
                    "IED",
                    "Nom d’IED en doublon: " + ied.name,
                    "Assurez l’unicité de IED@name",
                    Severity::Error
                });
            }
        }
    }

    // 1b) Doublons de ConnectivityNode@pathName. cnByPath_ keeps only the last
    //     of them, so without this diagnostic the topology silently loses a
    //     node with no indication of why.
    {
        std::unordered_set<std::string> seen;
        for (const auto& ss : model_->substations) {
            for (const auto& vl : ss.vlevels) {
                for (const auto& bay : vl.bays) {
                    for (const auto& cn : bay.connectivityNodes) {
                        if (cn.pathName.empty()) continue;
                        if (!seen.insert(cn.pathName).second) {
                            diags_.push_back({
                                ErrorCode::DuplicateConnectivityNode,
                                ss.name + "/" + vl.name + "/" + bay.name + "/" + cn.name,
                                "pathName de ConnectivityNode en doublon: " + cn.pathName,
                                "Chaque ConnectivityNode doit avoir un pathName unique",
                                Severity::Error
                            });
                        }
                    }
                }
            }
        }
    }

    // 1c) Doublons de DataSet@name dans un même LN. datasets_ is an
    //     unordered_map, so emplace() silently kept the first of the two.
    {
        for (const auto& ied : model_->ieds) {
            // Scoped per IED: a DataSet name must be unique within one LN of
            // one IED. Two IEDs sharing a DataSet name is legal, and two IEDs
            // sharing a *name* is already reported as DuplicateIEDName.
            std::unordered_set<std::string> seen;
            auto check = [&](const std::string& ldInst, const LogicalDevice& ld) {
                for (const auto& ds : ld.ln0.datasets) {
                    if (!seen.insert(ldInst + "/" + ds.name).second) {
                        diags_.push_back({
                            ErrorCode::DuplicateDataSetName,
                            ied.name + "/" + ldInst + "/LN0/" + ds.name,
                            "Nom de DataSet en doublon dans LN0: " + ds.name,
                            "Chaque DataSet d’un LN doit avoir un nom unique",
                            Severity::Error
                        });
                    }
                }
            };
            for (const auto& ld : ied.ldevices) check(ld.inst, ld);
            for (const auto& ap : ied.accessPoints)
                for (const auto& ld : ap.ldevices) check(ld.inst, ld);
        }
    }

    // 2) Terminal -> ConnectivityNode (référence cassée)
    //    On vérifie uniquement les @connectivityNode explicites (forme canonique).
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                for (const auto& ce : bay.equipments) {
                    for (const auto& t : ce.terminals) {
                        if (!t.connectivityNodeRef.empty()) {
                            if (cnByPath_.find(t.connectivityNodeRef) == cnByPath_.end()) {
                                diags_.push_back({
                                    ErrorCode::BrokenConnectivityNode,
                                    ss.name + "/" + vl.name + "/" + bay.name + "/" + ce.name,
                                    "ConnectivityNode introuvable: " + t.connectivityNodeRef,
                                    "Vérifiez le chemin @connectivityNode ou déclarez le CN",
                                    Severity::Error
                                });
                            }
                        }
                    }
                }
            }
        }
    }

    // 2b) ReportControl validation. These are the control blocks a client
    // subscribes to for B-reports, and they are independent of Communication,
    // so they are checked directly on the IED side.
    auto existsDataset = [&](const std::string& ied, const std::string& ld, const std::string& ds){
        DatasetKey dk{ied, ld, ds};
        return datasets_.find(dk) != datasets_.end();
    };

    for (const auto& ied : model_->ieds) {
        auto checkRpts = [&](const std::string& ldInst, const LogicalDevice& ld) {
            for (const auto& rc : ld.ln0.rptCtrls) {
                const std::string where =
                    ied.name + "/" + ldInst + "/LN0/" + rc.name;

                if (rc.name.empty()) {
                    diags_.push_back({
                        ErrorCode::MissingMandatoryField, where,
                        "ReportControl sans @name",
                        "Chaque ReportControl doit avoir un nom", Severity::Error
                    });
                    continue;
                }
                // An unbuffered RCB uses an implicit DataSet named after the
                // control block, so @datSet may legitimately be absent.
                const std::string dsName =
                    rc.datSet.empty() ? rc.name : rc.datSet;
                if (existsDataset(ied.name, ldInst, dsName)) continue;

                if (rc.datSet.empty()) {
                    // No @datSet and no matching <DataSet>: the RCB relies on a
                    // DataSet the SCL never declares. This is widespread in real
                    // vendor files (the Siemens SIEDIG export declares 55 RCB
                    // names and a single DataSet), and the block is still
                    // subscribable at runtime, so report it as a warning rather
                    // than failing the document.
                    diags_.push_back({
                        ErrorCode::DatasetNotFound, where,
                        "ReportControl sans DataSet déclaré (implicite): " + dsName,
                        "Un RCB non bufferisé utilise un DataSet implicite portant "
                        "le nom du ReportControl; il n'est pas déclaré dans ce SCL. "
                        "Vérifiez auprès de l'IED.",
                        Severity::Warning
                    });
                } else {
                    // An explicit @datSet that resolves to nothing is an
                    // unambiguous authoring error.
                    diags_.push_back({
                        ErrorCode::DatasetNotFound, where,
                        "DataSet introuvable pour ReportControl: " + dsName,
                        "Déclarez le DataSet sous LN0 ou corrigez @datSet",
                        Severity::Error
                    });
                }
            }
        };
        for (const auto& ld : ied.ldevices) checkRpts(ld.inst, ld);
        for (const auto& ap : ied.accessPoints)
            for (const auto& ld : ap.ldevices) checkRpts(ld.inst, ld);
    }

    auto findLDByName = [&](const std::string& ied, const std::string& ld)->const LogicalDevice* {
        auto it = iedByName_.find(ied);
        if (it == iedByName_.end()) return nullptr;
        return findLD_(*it->second, ld);
    };

    // 3a) GSE
    for (const auto& kv : gseEndpoints_) {
        const auto& e = kv.second;
        if (e.datasetRef.empty()) {
            // If the LDevice itself is missing, InvalidLdRef already reports the
            // root cause. Reporting ControlBlockNotFound on top of it is a
            // cascade: the control block was never looked up.
            if (findLDByName(e.iedName, e.ldInst) != nullptr) {
                diags_.push_back({
                    ErrorCode::ControlBlockNotFound,
                    "LN0.GSEControl",
                    "Contrôle GOOSE introuvable ou sans DataSet: " + e.cbName,
                    "Vérifiez LN0/GSEControl@name et @datSet",
                    Severity::Error
                });
            }
        } else if (!existsDataset(e.iedName, e.ldInst, e.datasetRef)) {
            diags_.push_back({
                ErrorCode::DatasetNotFound,
                "LN0.DataSet",
                "DataSet introuvable pour GSE: " + e.datasetRef,
                "Déclarez le DataSet sous LN0 ou corrigez @datSet",
                Severity::Error
            });
        }
    }

    // 3b) SV
    for (const auto& kv : svEndpoints_) {
        const auto& e = kv.second;
        if (e.datasetRef.empty()) {
            if (findLDByName(e.iedName, e.ldInst) != nullptr) {
                diags_.push_back({
                    ErrorCode::ControlBlockNotFound,
                    "LN0.SampledValueControl",
                    "Contrôle SV introuvable ou sans DataSet: " + e.cbName,
                    "Vérifiez LN0/SampledValueControl@name et @datSet",
                    Severity::Error
                });
            }
        } else if (!existsDataset(e.iedName, e.ldInst, e.datasetRef)) {
            diags_.push_back({
                ErrorCode::DatasetNotFound,
                "LN0.DataSet",
                "DataSet introuvable pour SV: " + e.datasetRef,
                "Déclarez le DataSet sous LN0 ou corrigez @datSet",
                Severity::Error
            });
        }
        if (e.smpRate.empty()) {
            diags_.push_back({
                ErrorCode::MissingSmpRate,
                "ConnectedAP.SMV",
                "Sampled Values sans SmpRate (P[type=\"SmpRate\"])",
                "Ajoutez P[type=\"SmpRate\"] côté Address",
                Severity::Warning
            });
        }
    }

    // 4) ConnectedAP -> ldInst. Single source of truth for this diagnostic:
    //    buildIndexes_() no longer emits an InvalidPath for the same fact.
    for (const auto& sn : model_->communication.subNetworks) {
        for (const auto& cap : sn.connectedAPs) {
            auto it = iedByName_.find(cap.iedName);
            if (it == iedByName_.end()) {
                diags_.push_back({
                    ErrorCode::InvalidIedRef,
                    "ConnectedAP",
                    "IED introuvable: " + cap.iedName,
                    "Contrôlez ConnectedAP@iedName vs les IED déclarés",
                    Severity::Error
                });
                continue;
            }
            const IED& ied = *it->second;
            for (const auto& g : cap.gses) {
                if (findLD_(ied, g.ldInst) == nullptr) {
                    diags_.push_back({
                        ErrorCode::InvalidLdRef,
                        "ConnectedAP.GSE",
                        "LDevice introuvable: " + g.ldInst + " sur IED " + cap.iedName,
                        "Contrôlez ldInst vs IED/Server/LDevice",
                        Severity::Error
                    });
                }
            }
            for (const auto& v : cap.smvs) {
                if (findLD_(ied, v.ldInst) == nullptr) {
                    diags_.push_back({
                        ErrorCode::InvalidLdRef,
                        "ConnectedAP.SMV",
                        "LDevice introuvable: " + v.ldInst + " sur IED " + cap.iedName,
                        "Contrôlez ldInst vs IED/Server/LDevice",
                        Severity::Error
                    });
                }
            }

            // A Communication entry for a control block the IED does not
            // declare. Endpoints are now built from LN0, so nothing else would
            // notice this and the address mapping would be silently ignored.
            // tri-state: nullopt = do not report (LD missing, already reported
            // as InvalidLdRef), true = control block present.
            auto hasCb = [&](const std::string& ldInst, const std::string& cbName)
                            -> std::optional<bool> {
                const LogicalDevice* ld = findLD_(ied, ldInst);
                if (!ld) return std::nullopt;
                for (const auto& g : ld->ln0.gseCtrls) if (g.name == cbName) return true;
                for (const auto& s : ld->ln0.smvCtrls) if (s.name == cbName) return true;
                return false;
            };
            for (const auto& g : cap.gses) {
                if (hasCb(g.ldInst, g.cbName) == false) {
                    diags_.push_back({
                        ErrorCode::ControlBlockNotFound,
                        "ConnectedAP.GSE",
                        "GSE sans GSEControl correspondant: " + g.cbName
                            + " sur " + cap.iedName + "/" + g.ldInst,
                        "Déclarez <GSEControl name=\"" + g.cbName
                            + "\"> sous LN0, ou retirez le GSE côté Communication",
                        Severity::Error
                    });
                }
            }
            for (const auto& v : cap.smvs) {
                if (hasCb(v.ldInst, v.cbName) == false) {
                    diags_.push_back({
                        ErrorCode::ControlBlockNotFound,
                        "ConnectedAP.SMV",
                        "SMV sans SampledValueControl correspondant: " + v.cbName
                            + " sur " + cap.iedName + "/" + v.ldInst,
                        "Déclarez <SampledValueControl name=\"" + v.cbName
                            + "\"> sous LN0, ou retirez le SMV côté Communication",
                        Severity::Error
                    });
                }
            }
        }
    }
}

Status SclManager::loadScl(const std::string &filepath) {
    SclParser parser;
    auto res = parser.parseFile(filepath);
    if (!res) {
        return Status(Error{res.error().code,
                            std::string("loadScl: ") + res.error().message});
    }
    adopt_(std::move(res.value()));
    return Status::Ok();
}

Status SclManager::loadSclString(const std::string& xml) {
    SclParser parser;
    auto res = parser.parseString(xml);
    if (!res) {
        return Status(Error{res.error().code,
                            std::string("loadSclString: ") + res.error().message});
    }
    adopt_(std::move(res.value()));
    return Status::Ok();
}

void SclManager::adopt_(SclModel&& m) {
    model_ = std::make_unique<SclModel>(std::move(m));
    buildIndexes_();
    validate_();

    // Snapshot the callbacks: a callback is allowed to register another one,
    // which would reallocate the vector mid-iteration.
    const auto cbs = reloadCbs_;
    for (const auto& cb : cbs) cb(*this);
}

bool SclManager::hasErrors() const {
    for (const auto& d : diags_)
        if (d.severity == Severity::Error) return true;
    return false;
}

bool SclManager::hasWarnings() const {
    for (const auto& d : diags_)
        if (d.severity == Severity::Warning) return true;
    return false;
}

void SclManager::buildIndexes_() {
    iedByName_.clear();
    cnByPath_.clear();
    mapCNByLogical_.clear();
    mapCNByFullToLogical_.clear();
    mapCNSuffix_.clear();
    lnodesByPrimary_.clear();
    primaryByLref_.clear();
    gseEndpoints_.clear();
    svEndpoints_.clear();
    mmsEndpoints_.clear();
    rptCtrls_.clear();
    diags_.clear();
    datasets_.clear();
    fcdaToDatasets_.clear();

    if (!model_) return;

    // --- IED index
    for (const auto &i : model_->ieds) iedByName_[i.name] = &i;

    // --- CN indexes (logique, full, suffix) + mapping LNodeRefs au « primaire »
    for (const auto &ss : model_->substations) {
        auto ss_i = interner_.intern(ss.name);
        for (const auto &vl : ss.vlevels) {
            auto vl_i = interner_.intern(vl.name);
            for (const auto &bay : vl.bays) {
                auto bay_i = interner_.intern(bay.name);
                for (const auto &cn : bay.connectivityNodes) {
                    const std::string full =
                        !cn.pathName.empty()
                            ? cn.pathName
                            : (std::string(ss_i) + "/" + std::string(vl_i) + "/" + std::string(bay_i) + "/" + cn.name);
                    const std::string logical = logicalCNKey(ss_i, vl_i, bay_i, cn.name);

                    cnByPath_[full] = &cn;
                    mapCNByLogical_[logical] = full;
                    mapCNByFullToLogical_[full] = logical;

                    auto suffix = lastSegment(full);
                    mapCNSuffix_[suffix].push_back(full);
                }

                // LNode sous Bay
                for (const auto& lr : bay.lnodes) {
                    const std::string primaryKey = logicalCNKey(ss_i, vl_i, bay_i, "<BAY>");
                    lnodesByPrimary_[primaryKey].push_back(lr);
                    std::string lrefKey = lr.iedName + "|" + lr.ldInst + "|" + lr.prefix + lr.lnClass + lr.lnInst;
                    primaryByLref_[lrefKey].push_back(primaryKey);
                }
                // LNodes sous CE
                for (const auto& ce : bay.equipments) {
                    const std::string primaryKey = logicalCNKey(ss_i, vl_i, bay_i, std::string("CE:")+ce.name);
                    for (const auto& lr : ce.lnodes) {
                        lnodesByPrimary_[primaryKey].push_back(lr);
                        std::string lrefKey = lr.iedName + "|" + lr.ldInst + "|" + lr.prefix + lr.lnClass + lr.lnInst;
                        primaryByLref_[lrefKey].push_back(primaryKey);
                    }
                }
            }
            // LNode sous VoltageLevel
            for (const auto& lr : vl.lnodes) {
                const std::string pk = logicalCNKey(ss_i, vl_i, "<VL>", "<VL>");
                lnodesByPrimary_[pk].push_back(lr);
                std::string lrefKey = lr.iedName + "|" + lr.ldInst + "|" + lr.prefix + lr.lnClass + lr.lnInst;
                primaryByLref_[lrefKey].push_back(pk);
            }
        }
        // LNode sous Substation
        for (const auto& lr : ss.lnodes) {
            const std::string pk = logicalCNKey(ss_i, "<SS>", "<SS>", "<SS>");
            lnodesByPrimary_[pk].push_back(lr);
            std::string lrefKey = lr.iedName + "|" + lr.ldInst + "|" + lr.prefix + lr.lnClass + lr.lnInst;
            primaryByLref_[lrefKey].push_back(pk);
        }
    }

    // --- Endpoints MMS.
    //
    // An AccessPoint address is a declaration in the IED; a ConnectedAP address
    // is the Address actually used on the wire. The previous code read only
    // ConnectedAP, so an SCD that carried the IP under AccessPoint/Address (or
    // Server/Address, which vendor tools emit freely) yielded no endpoint, and
    // a ConnectedAP with no @apName produced the unusable key "IED1|".
    auto ipOf = [](const std::unordered_map<std::string,std::string>& a)->std::string {
        auto it = a.find("IP"); return it==a.end()? "" : it->second;
    };
    auto portOf = [](const std::unordered_map<std::string,std::string>& a)->std::string {
        auto it = a.find("Port");
        return (it != a.end() && !it->second.empty()) ? it->second : "102";
    };

    // IED side first, so the declaration exists even without Communication.
    for (const auto& ied : model_->ieds) {
        for (const auto& ap : ied.accessPoints) {
            MmsEndpoint me{};
            me.iedName = ied.name;
            me.apName  = ap.name;
            me.ip      = ipOf(ap.address);
            me.port    = portOf(ap.address);
            me.addressSource = MmsEndpoint::AddressSource::AccessPoint;
            if (!me.ip.empty())
                mmsEndpoints_[keyMms(me.iedName, me.apName)] = me;

            for (std::size_t i = 0; i < ap.serverAddresses.size(); ++i) {
                const auto& sa = ap.serverAddresses[i];
                const std::string ip = ipOf(sa);
                if (ip.empty()) continue;
                MmsEndpoint se{};
                se.iedName = ied.name;
                se.apName  = ap.name;
                se.ip      = ip;
                se.port    = portOf(sa);
                se.serverName = std::to_string(i);
                se.addressSource = MmsEndpoint::AddressSource::Server;
                mmsEndpoints_[keyMms(se.iedName, se.apName + "#" + se.serverName)] = se;
            }
        }
    }

    // Communication side overrides: it is the effective configuration.
    for (const auto& sn : model_->communication.subNetworks) {
        for (const auto& cap : sn.connectedAPs) {
            MmsEndpoint me{};
            me.iedName = cap.iedName;
            // An absent @apName must not produce the key "IED1|", which no
            // lookup can match. Fall back to the IED's only AccessPoint when
            // there is exactly one, otherwise keep the empty name visible.
            me.apName  = cap.apName;
            if (me.apName.empty()) {
                auto it = iedByName_.find(cap.iedName);
                if (it != iedByName_.end() && it->second->accessPoints.size() == 1)
                    me.apName = it->second->accessPoints.front().name;
            }
            me.ip      = ipOf(cap.address);
            me.port    = portOf(cap.address);
            me.subNetwork = sn.name;
            me.addressSource = MmsEndpoint::AddressSource::ConnectedAP;
            if (!me.ip.empty())
                mmsEndpoints_[keyMms(me.iedName, me.apName)] = std::move(me);
        }
    }

    // --- Endpoints GOOSE/SV.
    //
    // The control block declared in LN0 is the source of truth: an endpoint
    // exists because the IED publishes it. The Communication section only
    // supplies the MAC/APPID needed to find that stream on the wire, and is
    // joined in afterwards. Iterating Communication instead (the previous
    // behaviour) meant an SCD with control blocks but no <Communication> block
    // produced zero endpoints and no diagnostic at all.
    auto getP = [](const std::unordered_map<std::string,std::string>& a,
                   const char* k)->std::string {
        auto it = a.find(k); return it==a.end()? "" : it->second;
    };

    // Communication side: (ied|ld|cb) -> address. Multiple ConnectedAP entries
    // for the same control block would be a modelling error, so first one wins.
    struct CommAddress { std::string mac, appid, vlanId, vlanPrio, smpRate, subNetwork; };
    std::unordered_map<std::string, CommAddress> gseAddr, svAddr;
    // Control-block APPID vs the Communication APPID, when they disagree.
    std::vector<std::string> appIdConflicts;

    for (const auto& sn : model_->communication.subNetworks) {
        for (const auto& cap : sn.connectedAPs) {
            for (const auto& g : cap.gses) {
                CommAddress a{g.address.count("MAC-Address") ? getP(g.address,"MAC-Address") : "",
                              getP(g.address, "APPID"),
                              getP(g.address, "VLAN-ID"),
                              getP(g.address, "VLAN-PRIORITY"),
                              "",
                              sn.name};
                gseAddr.emplace(keyGse(cap.iedName, g.ldInst, g.cbName), std::move(a));
            }
            for (const auto& v : cap.smvs) {
                CommAddress a{v.address.count("MAC-Address") ? getP(v.address,"MAC-Address") : "",
                              getP(v.address, "APPID"),
                              getP(v.address, "VLAN-ID"),
                              getP(v.address, "VLAN-PRIORITY"),
                              getP(v.address, "SmpRate"),
                              sn.name};
                svAddr.emplace(keyGse(cap.iedName, v.ldInst, v.cbName), std::move(a));
            }
        }
    }

    // IED side: every GSEControl / SampledValueControl declared in LN0.
    for (const auto& ied : model_->ieds) {
        auto addLd = [&](const LogicalDevice& ld) {
            for (const auto& cb : ld.ln0.gseCtrls) {
                GseEndpoint e{};
                e.iedName    = ied.name;
                e.ldInst     = ld.inst;
                e.cbName     = cb.name;
                e.datasetRef = cb.datSet;
                // @appID on the control block is authoritative when present;
                // Communication may carry a conflicting value.
                e.appid      = cb.appID;
                if (const auto it = gseAddr.find(keyGse(ied.name, ld.inst, cb.name));
                    it != gseAddr.end()) {
                    e.mac = it->second.mac;
                    e.vlanId = it->second.vlanId;
                    e.vlanPrio = it->second.vlanPrio;
                    e.subNetwork = it->second.subNetwork;
                    if (e.appid.empty()) e.appid = it->second.appid;
                    e.addressDeclared = true;
                    // GSEControl@appID is authoritative; a Communication APPID
                    // that disagrees is an interoperability hazard (we filter
                    // incoming GOOSE by APPID, so the wrong one loses every
                    // frame), so report it rather than silently pick a winner.
                    if (!cb.appID.empty() && !it->second.appid.empty()
                        && cb.appID != it->second.appid) {
                        appIdConflicts.push_back(
                            ied.name + "/" + ld.inst + "/" + cb.name
                            + ": LN0 appID=\"" + cb.appID
                            + "\" vs Communication APPID=\"" + it->second.appid
                            + "\" (utilisé: " + e.appid + ")");
                    }
                }
                gseEndpoints_[keyGse(ied.name, ld.inst, cb.name)] = std::move(e);
            }
            for (const auto& cb : ld.ln0.smvCtrls) {
                SvEndpoint e{};
                e.iedName    = ied.name;
                e.ldInst     = ld.inst;
                e.cbName     = cb.name;
                e.datasetRef = cb.datSet;
                e.appid      = cb.appID;
                if (const auto it = svAddr.find(keyGse(ied.name, ld.inst, cb.name));
                    it != svAddr.end()) {
                    e.mac = it->second.mac;
                    e.vlanId = it->second.vlanId;
                    e.vlanPrio = it->second.vlanPrio;
                    e.smpRate = it->second.smpRate;
                    e.subNetwork = it->second.subNetwork;
                    if (e.appid.empty()) e.appid = it->second.appid;
                    e.addressDeclared = true;
                }
                svEndpoints_[keyGse(ied.name, ld.inst, cb.name)] = std::move(e);
            }
        };
        for (const auto& ld : ied.ldevices) addLd(ld);
        for (const auto& ap : ied.accessPoints)
            for (const auto& ld : ap.ldevices) addLd(ld);
    }

    for (const auto& c : appIdConflicts) {
        diags_.push_back({
            ErrorCode::AppIdMismatch,
            "LN0.ControlBlocks",
            "APPID incohérent pour un bloc de contrôle: " + c,
            "LN0/@appID fait foi (IEC 61850-7-4). Corrigez le P[type=\"APPID\"] "
            "côté Communication ou le @appID du bloc de contrôle.",
            Severity::Warning
        });
    }

    // --- Index des DataSets & mapping FCDA -> datasets
    auto addDatasetsOf = [&](const std::string& iedName, const LogicalDevice& ld) {
        for (const auto& ds : ld.ln0.datasets) {
            DatasetKey dk{iedName, ld.inst, ds.name};
            datasets_.emplace(dk, &ds);
            // FCDA mapping (clé texte compacte)
            for (const auto& f : ds.members) {
                const std::string fkey = (f.ldInst.empty() ? ld.inst : f.ldInst)
                                       + "|" + f.lnClass + f.lnInst
                                       + "|" + f.doName + "|" + f.daName + "|" + f.fc;
                fcdaToDatasets_.emplace(fkey, dk);
            }
        }
    };
    for (const auto& ied : model_->ieds) {
        for (const auto& ld : ied.ldevices) addDatasetsOf(ied.name, ld);
        for (const auto& ap : ied.accessPoints)
            for (const auto& ld : ap.ldevices) addDatasetsOf(ied.name, ld);
    }

    // --- Index des ReportControlBlocks
    auto addRctlsOf = [this](const std::string& iedName, const LogicalDevice& ld) {
        for (const auto& rc : ld.ln0.rptCtrls) {
            if (rc.name.empty()) continue;
            rptCtrls_.emplace(iedName + "|" + ld.inst + "|" + rc.name, &rc);
        }
    };
    for (const auto& ied : model_->ieds) {
        for (const auto& ld : ied.ldevices) addRctlsOf(ied.name, ld);
        for (const auto& ap : ied.accessPoints)
            for (const auto& ld : ap.ldevices) addRctlsOf(ied.name, ld);
    }
}

std::vector<const ReportControlMeta*>
SclManager::reportControlsOf(const std::string& iedName) const {
    std::vector<const ReportControlMeta*> out;
    const std::string prefix = iedName + "|";
    for (const auto& kv : rptCtrls_) {
        if (kv.first.compare(0, prefix.size(), prefix) == 0) out.push_back(kv.second);
    }
    // rptCtrls_ is an unordered_map, so restore document order for a stable UI.
    std::sort(out.begin(), out.end(),
              [](const ReportControlMeta* a, const ReportControlMeta* b) {
                  if (a->name != b->name) return a->name < b->name;
                  return a->rptID < b->rptID;
              });
    return out;
}

std::string SclManager::rcbReference(const std::string& ldInst,
                                     const ReportControlMeta& rc) {
    // The SCL's rptID is the authoritative name: it is what the server reports
    // and what libiec61850 matches rptId against. Only fall back to building the
    // reference from the parts when the file omits it.
    if (!rc.rptID.empty()) return rc.rptID;
    const std::string dsName = rc.datSet.empty() ? rc.name : rc.datSet;
    // Unbuffered blocks live under $RP$, buffered ones under $BR$.
    return ldInst + "/LLN0$" + (rc.buffered ? "BR$" : "RP$") + dsName;
}

bool SclManager::matchCN(const std::string& a, const std::string& b) const {
    if (a == b) return true;
    auto la = lastSegment(a);
    auto lb = lastSegment(b);
    if (la == lb) return true;

    auto itA = mapCNByFullToLogical_.find(a);
    auto itB = mapCNByFullToLogical_.find(b);
    if (itA != mapCNByFullToLogical_.end() && itB != mapCNByFullToLogical_.end())
        return itA->second == itB->second;

    return false;
}

Status SclManager::printSubstations() const {
    if (!model_)
        return Status(Error{ErrorCode::LogicError, "No SCL loaded"});

    std::cout << "SCL version=" << model_->version
              << ", revision=" << model_->revision << "\n";
    for (const auto &ss : model_->substations) {
        std::cout << "Substation: " << ss.name << "\n";
        for (const auto &vl : ss.vlevels) {
            std::cout << "  VoltageLevel: " << vl.name;
            if (vl.voltage) {
                std::cout << "  <Voltage=" << vl.voltage->value
                          << vl.voltage->multiplier << vl.voltage->unit << ">";
            }
            if (!vl.nomFreq.empty())
                std::cout << "  (nomFreq=" << vl.nomFreq << ")";
            std::cout << "\n";
            for (const auto &bay : vl.bays) {
                std::cout << "    Bay: " << bay.name << "\n";
                for (const auto &cn : bay.connectivityNodes) {
                    std::cout << "      CN: name=" << cn.name << ", path=" << cn.pathName
                              << "\n";
                }
                for (const auto &ce : bay.equipments) {
                    std::cout << "      CE: name=" << ce.name << ", type=" << ce.type
                              << "\n";
                    for (const auto &t : ce.terminals) {
                        std::cout << "  Terminal: name=" << t.name
                                  << ", connectivityNode=" << t.connectivityNodeRef
                                  << ", cNodeName=" << t.cNodeName << "\n";
                    }
                    for (const auto &ln : ce.lnodes) {
                        std::cout << "        LNode: ied=" << ln.iedName
                                  << ", ldInst=" << ln.ldInst << ", " << ln.prefix
                                  << ln.lnClass << ln.lnInst << "\n";
                    }
                }
            }
        }
    }
    return Status::Ok();
}

Status SclManager::printIEDs() const {
    if (!model_)
        return Status(Error{ErrorCode::LogicError, "No SCL loaded"});
    for (const auto &ied : model_->ieds) {
        std::cout << "IED: " << ied.name << ", mfg=" << ied.manufacturer
                  << ", type=" << ied.type << "\n";
        for (const auto &ap : ied.accessPoints) {
            std::cout << "  AccessPoint: " << ap.name << "  Address(P): ";
            bool first = true;
            for (const auto &kv : ap.address) {
                if (!first) std::cout << ", ";
                first = false;
                std::cout << kv.first << "=" << kv.second;
            }
            std::cout << "\n";
            for (const auto &ld : ap.ldevices) {
                std::cout << "    LDevice: inst=" << ld.inst << "\n";
                for (const auto &ln : ld.lns) {
                    std::cout << "      LN: " << ln.prefix << ln.lnClass << ln.inst << "\n";
                }
            }
        }
        for (const auto &ld : ied.ldevices) {
            std::cout << "  (Direct) LDevice: inst=" << ld.inst << "\n";
            for (const auto &ln : ld.lns) {
                std::cout << "      LN: " << ln.prefix << ln.lnClass << ln.inst << "\n";
            }
        }
    }
    return Status::Ok();
}

Status SclManager::printEquipmentFromIEDs() const {
    if (!model_)
        return Status(Error{ErrorCode::LogicError, "No SCL loaded"});

    auto eqs = collectEquipmentFromIEDs(true);
    if (eqs.empty()) {
        std::cout << "EquipmentFromIEDs: (none)\n";
        return Status::Ok();
    }

    std::cout << "EquipmentFromIEDs (physical LN classes, logical excluded: LLN0/LPHD/MMXU):\n";
    for (const auto& e : eqs) {
        std::cout << "  IED=" << e.iedName
                  << "  LD=" << e.ldInst
                  << "  LN=" << e.prefix << e.lnClass << e.lnInst;

        // description combinée LN + LD si présente
        auto desc = e.combinedDesc();
        if (!desc.empty()) std::cout << "  desc=\"" << desc << "\"";

        if (!e.primaryAnchors.empty()) {
            std::cout << "  anchors=[";
            for (size_t i=0;i<e.primaryAnchors.size();++i) {
                if (i) std::cout << ", ";
                std::cout << e.primaryAnchors[i];
            }
            std::cout << "]";
        }
        std::cout << "\n";
    }
    return Status::Ok();
}


Status SclManager::printCommunication() const {
    if (!model_)
        return Status(Error{ErrorCode::LogicError, "No SCL loaded"});
    const auto &C = model_->communication;
    for (const auto &sn : C.subNetworks) {
        std::cout << "SubNetwork: " << sn.name << " (type=" << sn.type << ")";
        if (!sn.props.empty()) {
            std::cout << "  Props: ";
            bool first = true;
            for (const auto &kv : sn.props) {
                if (!first) std::cout << ", ";
                first = false;
                std::cout << kv.first << "=" << kv.second;
            }
            std::cout << "\n";
        }
        for (const auto &cap : sn.connectedAPs) {
            std::cout << "  ConnectedAP: ied=" << cap.iedName << ", ap=" << cap.apName << "\n";
            if (!cap.address.empty()) {
                std::cout << "    Address: ";
                bool first = true;
                for (const auto &kv : cap.address) {
                    if (!first) std::cout << ", ";
                    first = false;
                    std::cout << kv.first << "=" << kv.second;
                }
                std::cout << "\n";
            }
            for (const auto &g : cap.gses) {
                std::cout << "    GSE: ldInst=" << g.ldInst << ", cbName=" << g.cbName << "  P{";
                bool first = true;
                for (const auto &kv : g.address) {
                    if (!first) std::cout << ", ";
                    first = false;
                    std::cout << kv.first << "=" << kv.second;
                }
                std::cout << "}\n";
            }
            for (const auto &v : cap.smvs) {
                std::cout << "    SMV: ldInst=" << v.ldInst << ", cbName=" << v.cbName << "  P{";
                bool first = true;
                for (const auto &kv : v.address) {
                    if (!first) std::cout << ", ";
                    first = false;
                    std::cout << kv.first << "=" << kv.second;
                }
                std::cout << "}\n";
            }
        }
    }
    return Status::Ok();
}

Status SclManager::printTopology() const {
    auto edges = collectSldEdges();
    std::cout << "Topology edges (CE -> CN):\n";
    for (const auto &e : edges) {
        std::cout << "  [" << e.ssName << "/" << e.vlName << "/" << e.bayName
                  << "] " << e.ceName << " -> " << e.cnPath << "\n";
    }
    return Status::Ok();
}

Result<const Substation *>
SclManager::findSubstation(const std::string &name) const {
    if (!model_)
        return Result<const Substation *>({ErrorCode::LogicError, "No SCL loaded"});
    for (const auto &ss : model_->substations) {
        if (ss.name == name)
            return Result<const Substation *>(&ss);
    }
    return Result<const Substation *>({ErrorCode::InvalidPath, "Substation not found: " + name});
}

Result<const IED *> SclManager::findIED(const std::string &name) const {
    if (!model_)
        return Result<const IED *>({ErrorCode::LogicError, "No SCL loaded"});
    auto it = iedByName_.find(name);
    if (it != iedByName_.end()) return Result<const IED *>(it->second);
    return Result<const IED *>({ErrorCode::InvalidPath, "IED not found: " + name});
}

const LogicalDevice *SclManager::findLD_(const IED &ied,
                                         const std::string &ldInst) const {
    for (const auto &ld : ied.ldevices)
        if (ld.inst == ldInst) return &ld;
    for (const auto &ap : ied.accessPoints) {
        for (const auto &ld : ap.ldevices)
            if (ld.inst == ldInst) return &ld;
    }
    return nullptr;
}

const LogicalNode *SclManager::findLN_(const LogicalDevice &ld,
                                       const std::string &lnClass,
                                       const std::string &lnInst,
                                       const std::string &prefix) const {
    for (const auto &ln : ld.lns) {
        if (ln.lnClass == lnClass && ln.inst == lnInst && ln.prefix == prefix)
            return &ln;
    }
    return nullptr;
}

Result<ResolvedLNode> SclManager::resolveLNodeRef(const LNodeRef &ref) const {
    auto it = iedByName_.find(ref.iedName);
    if (it == iedByName_.end())
        return Result<ResolvedLNode>({ErrorCode::InvalidPath, "Unknown IED: " + ref.iedName});

    const IED *ied = it->second;
    const LogicalDevice *ld = findLD_(*ied, ref.ldInst);
    if (!ld)
        return Result<ResolvedLNode>({ErrorCode::InvalidPath, "Unknown LDevice: " + ref.ldInst});

    const LogicalNode *ln = findLN_(*ld, ref.lnClass, ref.lnInst, ref.prefix);
    if (!ln)
        return Result<ResolvedLNode>({ErrorCode::InvalidPath, "Unknown LN: " + ref.lnClass + ref.lnInst});

    ResolvedLNode r{ied, ld, ln};
    return Result<ResolvedLNode>(r);
}

std::vector<EdgeCEtoCN> SclManager::collectSldEdges() const {
    std::vector<EdgeCEtoCN> edges;
    if (!model_) return edges;

    for (const auto &ss : model_->substations) {
        for (const auto &vl : ss.vlevels) {
            for (const auto &bay : vl.bays) {
                for (const auto &ce : bay.equipments) {
                    for (const auto &t : ce.terminals) {
                        std::string cnPath =
                            !t.connectivityNodeRef.empty()
                                ? t.connectivityNodeRef
                                : (!t.cNodeName.empty()
                                       ? (ss.name + "/" + vl.name + "/" + bay.name + "/" + t.cNodeName)
                                       : "");
                        if (!cnPath.empty()) {
                            // Normalise en logique si possible
                            auto it = mapCNByFullToLogical_.find(cnPath);
                            if (it != mapCNByFullToLogical_.end()) cnPath = it->second;
                            edges.push_back({ss.name, vl.name, bay.name, ce.name, cnPath});
                        }
                    }
                }
            }
        }
    }
    return edges;
}

std::vector<ConnectivityNode>
SclManager::getConnectivityNodes(const std::string &ss, const std::string &vl,
                                 const std::string &bay) const {
    std::vector<ConnectivityNode> out;
    if (!model_) return out;

    for (const auto &S : model_->substations)
        if (S.name == ss) {
            for (const auto &V : S.vlevels)
                if (V.name == vl) {
                    for (const auto &B : V.bays)
                        if (B.name == bay) {
                            out = B.connectivityNodes;
                            return out;
                        }
                }
        }
    return out;
}

// === DataSet helpers =================================================

namespace {
static std::string makeLnName(const std::string& lnClass, const std::string& lnInst) {
    return (lnClass == "LLN0") ? "LLN0" : (lnClass + lnInst);
}
}

const DataSet* SclManager::getLn0Dataset(const std::string& ied,
                                         const std::string& ldInst,
                                         const std::string& dsName) const
{
    auto itIed = iedByName_.find(ied);
    if (itIed == iedByName_.end()) return nullptr;
    const IED* I = itIed->second;

    auto findIn = [&](const std::vector<LogicalDevice>& lds)->const DataSet* {
        for (const auto& ld : lds) {
            if (ld.inst != ldInst) continue;
            for (const auto& ds : ld.ln0.datasets) {
                if (ds.name == dsName) return &ds;
            }
        }
        return nullptr;
    };

    if (const DataSet* p = findIn(I->ldevices)) return p;
    for (const auto& ap : I->accessPoints)
        if (const DataSet* p = findIn(ap.ldevices)) return p;

    return nullptr;
}

std::string SclManager::fcdaToMmsRef(const std::string& ldInst, const FcdaRef& f)
{
    // "LD/LN.DO(.DA)[FC]"
    std::string ref;
    ref.reserve(ldInst.size() + 1 + f.lnClass.size() + f.lnInst.size()
                + 1 + f.doName.size() + 1 + f.daName.size() + 3 + f.fc.size());

    ref += ldInst;
    ref += '/';
    ref += makeLnName(f.lnClass, f.lnInst);
    ref += '.';
    ref += f.doName;
    if (!f.daName.empty()) {
        ref += '.';
        ref += f.daName;
    }
    if (!f.fc.empty()) {
        ref += '[';
        ref += f.fc;
        ref += ']';
    }
    return ref;
}

std::vector<std::string> SclManager::resolveDatasetMembers(const std::string& ied,
                                                           const std::string& ldInst,
                                                           const std::string& dsName) const
{
    std::vector<std::string> out;
    if (auto* ds = getLn0Dataset(ied, ldInst, dsName)) {
        out.reserve(ds->members.size());
        for (const auto& f : ds->members) {
            // ldInst implicite dans les FCDAs LN0 -> on force ldInst du contrôle
            const std::string effLd = f.ldInst.empty() ? ldInst : f.ldInst;
            out.push_back(fcdaToMmsRef(effLd, f));
        }
    }
    return out;
}


// ---------- NEW: équipements physiques vus depuis les IEDs ----------
std::vector<EquipmentFromIED> SclManager::collectEquipmentFromIEDs(bool skipLogical) const {
    std::vector<EquipmentFromIED> out;
    if (!model_) return out;

    auto push = [&](const std::string& iedName, const LogicalDevice& ld, const LogicalNode& ln){
        if (skipLogical && is_excluded_ln(ln.lnClass)) return;
        if (!is_physical_equipment_ln(ln.lnClass)) return;

        EquipmentFromIED e{iedName, ld.inst, ln.prefix, ln.lnClass, ln.inst};
        e.lnDesc = ln.desc;     // NEW
        e.ldDesc = ld.desc;     // NEW

        // recoller aux ancrages primaires (si la topologie existe)
        std::string lrefKey = iedName + "|" + ld.inst + "|" + ln.prefix + ln.lnClass + ln.inst;
        if (auto it = primaryByLref_.find(lrefKey); it != primaryByLref_.end())
            e.primaryAnchors = it->second;

        out.push_back(std::move(e));
    };

    for (const auto& ied : model_->ieds) {
        // LDevice sous IED
        for (const auto& ld : ied.ldevices)
            for (const auto& ln : ld.lns) push(ied.name, ld, ln);

        // LDevice sous AccessPoint/Server
        for (const auto& ap : ied.accessPoints)
            for (const auto& ld : ap.ldevices)
                for (const auto& ln : ld.lns) push(ied.name, ld, ln);
    }
    return out;
}

// ---------- JSON ----------

std::string SclManager::toJsonSubstations() const {
    json root;
    root["substations"] = json::array();

    if (model_) {
        for (const auto& ss : model_->substations) {
            json jss;
            jss["name"] = ss.name;
            jss["vlevels"] = json::array();
            for (const auto& vl : ss.vlevels) {
                json jvl;
                jvl["name"] = vl.name;
                if (vl.voltage) {
                    jvl["voltage"] = {{"value", vl.voltage->value},
                                      {"unit", vl.voltage->unit},
                                      {"mult", vl.voltage->multiplier}};
                }
                jvl["bays"] = json::array();
                for (const auto& bay : vl.bays) {
                    json jbay; jbay["name"] = bay.name;
                    jbay["connectivityNodes"] = json::array();
                    for (const auto& cn : bay.connectivityNodes) {
                        json jcn;
                        jcn["name"] = cn.name;
                        if (!cn.pathName.empty()) jcn["path"] = cn.pathName;

                        // expose la forme logique (si mappée)
                        std::string full = !cn.pathName.empty()
                                               ? cn.pathName
                                               : (ss.name + "/" + vl.name + "/" + bay.name + "/" + cn.name);
                        auto it = mapCNByFullToLogical_.find(full);
                        if (it != mapCNByFullToLogical_.end()) jcn["logical"] = it->second;

                        jbay["connectivityNodes"].push_back(std::move(jcn));
                    }
                    jbay["equipments"] = json::array();
                    for (const auto& ce : bay.equipments) {
                        json je;
                        je["name"] = ce.name; je["type"] = ce.type;
                        je["terminals"] = json::array();
                        for (const auto& t : ce.terminals) {
                            json jt;
                            jt["name"] = t.name;
                            const std::string cnRef = !t.connectivityNodeRef.empty()
                                                          ? t.connectivityNodeRef
                                                          : (ss.name + "/" + vl.name + "/" + bay.name + "/" + t.cNodeName);
                            jt["cn"] = cnRef;
                            je["terminals"].push_back(std::move(jt));
                        }
                        if (!ce.lnodes.empty()) {
                            je["lnodes"] = json::array();
                            for (const auto& lr : ce.lnodes) {
                                json jl;
                                if (!lr.iedName.empty()) jl["ied"] = lr.iedName;
                                if (!lr.ldInst.empty())  jl["ld"]  = lr.ldInst;
                                if (!lr.prefix.empty())  jl["prefix"] = lr.prefix;
                                if (!lr.lnClass.empty()) jl["lnClass"] = lr.lnClass;
                                if (!lr.lnInst.empty())  jl["lnInst"]  = lr.lnInst;
                                je["lnodes"].push_back(std::move(jl));
                            }
                        }
                        jbay["equipments"].push_back(std::move(je));
                    }
                    jvl["bays"].push_back(std::move(jbay));
                }
                jss["vlevels"].push_back(std::move(jvl));
            }
            root["substations"].push_back(std::move(jss));
        }
    }
    return root.dump();
}

std::string SclManager::toJsonNetworkMap() const
{
    using nlohmann::json;
    json root;
    root["mms"] = json::array();
    root["gse"] = json::array();
    root["sv"]  = json::array();

    // MMS endpoints
    for (const auto& kv : mmsEndpoints_) {
        const auto& m = kv.second;
        json j;
        j["ied"]  = m.iedName;
        j["ap"]   = m.apName;
        j["ip"]   = m.ip;
        j["port"] = m.port;
        root["mms"].push_back(std::move(j));
    }

    // GOOSE endpoints + membres résolus
    for (const auto& kv : gseEndpoints_) {
        const auto& e = kv.second;
        json j;
        j["ied"]      = e.iedName;
        j["ld"]       = e.ldInst;
        j["cb"]       = e.cbName;
        j["mac"]      = e.mac;
        j["appid"]    = e.appid;
        j["vlanId"]   = e.vlanId;
        j["vlanPrio"] = e.vlanPrio;
        j["dataset"]  = e.datasetRef;

        // Résolution des membres (objectRefs MMS)
        j["members"]  = resolveDatasetMembers(e.iedName, e.ldInst, e.datasetRef);
        root["gse"].push_back(std::move(j));
    }

    // SV endpoints + membres résolus
    for (const auto& kv : svEndpoints_) {
        const auto& e = kv.second;
        json j;
        j["ied"]      = e.iedName;
        j["ld"]       = e.ldInst;
        j["cb"]       = e.cbName;
        j["mac"]      = e.mac;
        j["appid"]    = e.appid;
        j["vlanId"]   = e.vlanId;
        j["vlanPrio"] = e.vlanPrio;
        j["smpRate"]  = e.smpRate;
        j["dataset"]  = e.datasetRef;

        j["members"]  = resolveDatasetMembers(e.iedName, e.ldInst, e.datasetRef);
        root["sv"].push_back(std::move(j));
    }

    // Report control blocks. Unlike GOOSE/SV these need no Communication
    // mapping, so they are emitted for every IED that declares one. This is
    // what the COMMUNICATION tab and the MMS supervisor both read.
    root["rcb"] = json::array();
    for (const auto& ied : model_ ? model_->ieds : std::vector<IED>{}) {
        for (const auto& rc : reportControlsOf(ied.name)) {
            json j;
            j["ied"]      = ied.name;
            j["name"]     = rc->name;
            j["rptID"]    = rc->rptID;
            j["ref"]      = rcbReference(ldInstOf(ied, rc), *rc);
            j["confRev"]  = rc->confRev;
            j["buffered"] = rc->buffered;
            j["intgPd"]   = rc->intgPd;
            j["desc"]     = rc->desc;
            const std::string dsName = rc->datSet.empty() ? rc->name : rc->datSet;
            j["dataset"]  = rc->datSet.empty() ? std::string{} : rc->datSet;
            j["datasetImplicit"] = rc->datSet.empty();
            j["datasetDeclared"] = getLn0Dataset(ied.name, ldInstOf(ied, rc), dsName) != nullptr;
            j["members"]  = resolveDatasetMembers(ied.name, ldInstOf(ied, rc), dsName);
            j["trgOps"] = {
                {"dchg", rc->trgOps.dataChange},
                {"qchg", rc->trgOps.qualityChange},
                {"dupd", rc->trgOps.dataUpdate},
                {"intg", rc->trgOps.integrity},
                {"gi",   rc->trgOps.generalInterrogation},
            };
            j["optFields"] = {
                {"seqNum", rc->optFields.seqNum},
                {"timeStamp", rc->optFields.timeStamp},
                {"reasonForInclusion", rc->optFields.reasonForInclusion},
                {"dataSet", rc->optFields.dataSet},
                {"confRev", rc->optFields.confRev},
                {"entryID", rc->optFields.entryID},
                {"bufOvfl", rc->optFields.bufOvfl},
                {"configRef", rc->optFields.configRef},
                {"subSeqNum", rc->optFields.subSeqNum},
            };
            root["rcb"].push_back(std::move(j));
        }
    }

    return root.dump();
}

std::string SclManager::ldInstOf(const IED& ied, const ReportControlMeta* rc) const {
    // Find the LDevice that owns this control block. rptCtrls_ is keyed by
    // ied|ld|rcbName, so scan the two possible containers.
    for (const auto& ld : ied.ldevices)
        for (const auto& cand : ld.ln0.rptCtrls)
            if (&cand == rc) return ld.inst;
    for (const auto& ap : ied.accessPoints)
        for (const auto& ld : ap.ldevices)
            for (const auto& cand : ld.ln0.rptCtrls)
                if (&cand == rc) return ld.inst;
    return {};
}


std::string SclManager::toJsonNetwork() const {
    json root;
    root["subnetworks"] = json::array();

    if (model_) {
        for (const auto& sn : model_->communication.subNetworks) {
            json jsn; jsn["name"] = sn.name; jsn["type"] = sn.type;

            if (!sn.props.empty()) {
                jsn["props"] = json::object();
                for (const auto& kv : sn.props) jsn["props"][kv.first] = kv.second;
            }

            jsn["connectedAPs"] = json::array();
            for (const auto& cap : sn.connectedAPs) {
                json jcap; jcap["ied"] = cap.iedName; jcap["ap"] = cap.apName;

                if (!cap.address.empty()) {
                    jcap["address"] = json::object();
                    for (const auto& kv : cap.address) jcap["address"][kv.first] = kv.second;
                }

                // GSE
                jcap["gses"] = json::array();
                for (const auto& g : cap.gses) {
                    json jg;
                    jg["ld"] = g.ldInst; jg["cb"] = g.cbName;
                    auto it = gseEndpoints_.find(keyGse(cap.iedName, g.ldInst, g.cbName));
                    if (it != gseEndpoints_.end()) {
                        jg["endpoint"] = {
                            {"mac", it->second.mac}, {"appid", it->second.appid},
                            {"vlanId", it->second.vlanId}, {"vlanPrio", it->second.vlanPrio},
                            {"dataset", it->second.datasetRef}
                        };
                    }
                    if (!g.address.empty()) {
                        jg["address"] = json::object();
                        for (const auto& kv : g.address) jg["address"][kv.first] = kv.second;
                    }
                    jcap["gses"].push_back(std::move(jg));
                }

                // SMV
                jcap["smvs"] = json::array();
                for (const auto& v : cap.smvs) {
                    json jv;
                    jv["ld"] = v.ldInst; jv["cb"] = v.cbName;
                    auto it = svEndpoints_.find(keyGse(cap.iedName, v.ldInst, v.cbName));
                    if (it != svEndpoints_.end()) {
                        jv["endpoint"] = {
                            {"mac", it->second.mac}, {"appid", it->second.appid},
                            {"vlanId", it->second.vlanId}, {"vlanPrio", it->second.vlanPrio},
                            {"smpRate", it->second.smpRate}, {"dataset", it->second.datasetRef}
                        };
                    }
                    if (!v.address.empty()) {
                        jv["address"] = json::object();
                        for (const auto& kv : v.address) jv["address"][kv.first] = kv.second;
                    }
                    jcap["smvs"].push_back(std::move(jv));
                }

                jsn["connectedAPs"].push_back(std::move(jcap));
            }
            root["subnetworks"].push_back(std::move(jsn));
        }
    }
    return root.dump();
}

// NEW: Vue IEDs (groupée LD -> équipements depuis LN)
std::string SclManager::toJsonIEDs() const {
    json root; root["ieds"] = json::array();
    auto eqs = collectEquipmentFromIEDs(true);

    // ied -> ld -> [eq...]
    std::map<std::string, std::map<std::string, std::vector<EquipmentFromIED>>> byIed;
    for (auto& e : eqs) byIed[e.iedName][e.ldInst].push_back(std::move(e));

    for (auto& [ied, lds] : byIed) {
        json jIed; jIed["name"] = ied; jIed["lds"] = json::array();
        for (auto& [ld, eqv] : lds) {
            json jLd; jLd["inst"] = ld; jLd["equipments"] = json::array();
            for (auto& e : eqv) {
                json je{
                    {"prefix", e.prefix}, {"lnClass", e.lnClass}, {"lnInst", e.lnInst}
                };

                // NEW: description combinée
                auto desc = e.combinedDesc();
                if (!desc.empty()) je["desc"] = desc;

                // (optionnel) exposer séparément LN/LD pour les afficher distinctement :
                // if (!e.lnDesc.empty()) je["lnDesc"] = e.lnDesc;
                // if (!e.ldDesc.empty()) je["ldDesc"] = e.ldDesc;

                if (!e.primaryAnchors.empty()) je["anchors"] = e.primaryAnchors;
                jLd["equipments"].push_back(std::move(je));
            }
            jIed["lds"].push_back(std::move(jLd));
        }
        root["ieds"].push_back(std::move(jIed));
    }
    return root.dump();
}
