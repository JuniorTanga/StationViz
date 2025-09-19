#include "SclManager.h"
#include "SclParser.h"
//#include "JsonWriter.h" //remplacer par nlohmannJson
#include "nlohmannJson/json.hpp"
#include <iostream>
#include <sstream>

using namespace scl;
using nlohmann::json;

//=======HELPERS=========//
static std::string keyGse(const std::string& ied, const std::string& ld, const std::string& cb){
    return ied + "|" + ld + "|" + cb;
}
static std::string keyMms(const std::string& ied, const std::string& ap){
    return ied + "|" + ap;
}

static std::string logicalCNKey(std::string_view ss, std::string_view vl,
                                std::string_view bay, std::string_view cn) {
    std::string out; out.reserve(ss.size()+vl.size()+bay.size()+cn.size()+3);
    out.append(ss); out.push_back(':');
    out.append(vl); out.push_back(':');
    out.append(bay); out.push_back(':');
    out.append(cn);
    return out;
}

// Récupère le dernier segment d'un chemin "A/B/C"
static std::string lastSegment(const std::string &path) {
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return path;
    return path.substr(pos + 1);
}

//========================//

SclManager::SclManager() = default;

Status SclManager::loadScl(const std::string &filepath) {
    SclParser parser;
    auto res = parser.parseFile(filepath);
    if (!res) {
        return Status(Error{res.error().code,
                            std::string("loadScl: ") + res.error().message});
    }
    model_ = std::make_unique<SclModel>(std::move(res.value()));
    buildIndexes_();
    return Status::Ok();
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
    diags_.clear();

    if (!model_) return;

    // --- IED index
    for (const auto &i : model_->ieds) iedByName_[i.name] = &i;

    // --- CN indexes (logique, full, suffix)
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

                // LNode sous Bay (et idem sous CE/VL/SS) -> mapping primaire
                for (const auto& lr : bay.lnodes) {
                    const std::string primaryKey = logicalCNKey(ss_i, vl_i, bay_i, "<BAY>");
                    lnodesByPrimary_[primaryKey].push_back(lr);

                    // clé lref : ied|ld|prefix+class+inst
                    std::string lrefKey = lr.iedName + "|" + lr.ldInst + "|" + lr.prefix + lr.lnClass + lr.lnInst;
                    primaryByLref_[lrefKey].push_back(primaryKey);
                }
                for (const auto& ce : bay.equipments) {
                    const std::string primaryKey = logicalCNKey(ss_i, vl_i, bay_i, "CE:"+ce.name);
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

    // --- Endpoints MMS (ConnectedAP Address)
    for (const auto& sn : model_->communication.subNetworks) {
        for (const auto& cap : sn.connectedAPs) {
            MmsEndpoint me{};
            me.iedName = cap.iedName; me.apName = cap.apName;
            auto it_ip = cap.address.find("IP");
            if (it_ip != cap.address.end()) me.ip = it_ip->second;
            auto it_pt = cap.address.find("Port");
            me.port = (it_pt != cap.address.end()) ? it_pt->second : "102";
            if (!me.ip.empty()) {
                mmsEndpoints_[keyMms(me.iedName, me.apName)] = std::move(me);
            }
        }
    }

    // --- Endpoints GSE/SMV = (ConnectedAP.GSE/SMV) + LN0 ControlBlocks -> DataSet ref
    // mapping (ied, ldInst) -> LD (pour retrouver ln0 metas)
    auto findLD = [&](const std::string& iedName, const std::string& ldInst)->const LogicalDevice* {
        auto it = iedByName_.find(iedName);
        if (it == iedByName_.end()) return nullptr;
        return findLD_(*it->second, ldInst);
    };
    // helpers address
    auto getP = [](const std::unordered_map<std::string,std::string>& a, const char* k)->std::string{
        auto it = a.find(k); return it==a.end()? "" : it->second;
    };

    for (const auto& sn : model_->communication.subNetworks) {
        for (const auto& cap : sn.connectedAPs) {
            // GOOSE
            for (const auto& g : cap.gses) {
                GseEndpoint e{};
                e.iedName = cap.iedName; e.ldInst = g.ldInst; e.cbName = g.cbName;
                e.mac     = getP(g.address, "MAC-Address");
                e.appid   = getP(g.address, "APPID");
                e.vlanId  = getP(g.address, "VLAN-ID");
                e.vlanPrio= getP(g.address, "VLAN-PRIORITY");

                // datasetRef depuis LN0.GSEControl[name=cbName]
                if (auto ld = findLD(e.iedName, e.ldInst)) {
                    for (const auto& cb : ld->ln0.gseCtrls) {
                        if (cb.name == e.cbName) { e.datasetRef = cb.datSet; break; }
                    }
                    if (e.datasetRef.empty()) {
                        diags_.push_back({ErrorCode::InvalidPath,
                                          "LN0.GSEControl",
                                          "Dataset introuvable pour GSEControl: " + e.cbName,
                                          "Vérifie LN0/GSEControl@name et @datSet"});
                    }
                } else {
                    diags_.push_back({ErrorCode::InvalidPath,
                                      "ConnectedAP.GSE",
                                      "LDevice introuvable: " + e.ldInst + " sur IED " + e.iedName,
                                      "Contrôle ldInst côté Communication vs IED/Server/LDevice"});
                }
                gseEndpoints_[keyGse(e.iedName, e.ldInst, e.cbName)] = std::move(e);
            }

            // SMV
            for (const auto& v : cap.smvs) {
                SvEndpoint e{};
                e.iedName = cap.iedName; e.ldInst = v.ldInst; e.cbName = v.cbName;
                e.mac     = getP(v.address, "MAC-Address");
                e.appid   = getP(v.address, "APPID");
                e.vlanId  = getP(v.address, "VLAN-ID");
                e.vlanPrio= getP(v.address, "VLAN-PRIORITY");
                e.smpRate = getP(v.address, "SmpRate"); // si présent dans Address

                if (auto ld = findLD(e.iedName, e.ldInst)) {
                    for (const auto& cb : ld->ln0.smvCtrls) {
                        if (cb.name == e.cbName) { e.datasetRef = cb.datSet; break; }
                    }
                    if (e.datasetRef.empty()) {
                        diags_.push_back({ErrorCode::InvalidPath,
                                          "LN0.SampledValueControl",
                                          "Dataset introuvable pour SMV Control: " + e.cbName,
                                          "Vérifie LN0/SampledValueControl@name et @datSet"});
                    }
                } else {
                    diags_.push_back({ErrorCode::InvalidPath,
                                      "ConnectedAP.SMV",
                                      "LDevice introuvable: " + e.ldInst + " sur IED " + e.iedName,
                                      "Contrôle ldInst côté Communication vs IED/Server/LDevice"});
                }
                svEndpoints_[keyGse(e.iedName, e.ldInst, e.cbName)] = std::move(e);
            }
        }
    }
}

bool SclManager::matchCN(const std::string& a, const std::string& b) const {
    if (a == b) return true;
    // tolérant: compare le dernier segment / suffixes
    auto la = lastSegment(a);
    auto lb = lastSegment(b);
    if (la == lb) return true;

    // si l'un est logique, l'autre full -> normaliser
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
                if (!first)
                    std::cout << ", ";
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
                if (!first)
                    std::cout << ", ";
                first = false;
                std::cout << kv.first << "=" << kv.second;
            }
            std::cout << "\n";
        }
        for (const auto &cap : sn.connectedAPs) {
            std::cout << "  ConnectedAP: ied=" << cap.iedName << ", ap=" << cap.apName
                      << "\n";
            if (!cap.address.empty()) {
                std::cout << "    Address: ";
                bool first = true;
                for (const auto &kv : cap.address) {
                    if (!first)
                        std::cout << ", ";
                    first = false;
                    std::cout << kv.first << "=" << kv.second;
                }
                std::cout << "\n";
            }
            for (const auto &g : cap.gses) {
                std::cout << "    GSE: ldInst=" << g.ldInst << ", cbName=" << g.cbName
                          << "  P{";
                bool first = true;
                for (const auto &kv : g.address) {
                    if (!first)
                        std::cout << ", ";
                    first = false;
                    std::cout << kv.first << "=" << kv.second;
                }
                std::cout << "}";
            }
            for (const auto &v : cap.smvs) {
                std::cout << "    SMV: ldInst=" << v.ldInst << ", cbName=" << v.cbName
                          << "  P{";
                bool first = true;
                for (const auto &kv : v.address) {
                    if (!first)
                        std::cout << ", ";
                    first = false;
                    std::cout << kv.first << "=" << kv.second;
                }
                std::cout << "}";
            }
        }
    }
    return Status::Ok();
}

Status SclManager::printTopology() const {
    auto edges = collectSldEdges();
    std::cout << "Topology edges (CE -> CN):";
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
    return Result<const Substation *>(
        {ErrorCode::InvalidPath, "Substation not found: " + name});
}

Result<const IED *> SclManager::findIED(const std::string &name) const {
    if (!model_)
        return Result<const IED *>({ErrorCode::LogicError, "No SCL loaded"});
    auto it = iedByName_.find(name);
    if (it != iedByName_.end())
        return Result<const IED *>(it->second);
    return Result<const IED *>(
        {ErrorCode::InvalidPath, "IED not found: " + name});
}

const LogicalDevice *SclManager::findLD_(const IED &ied,
                                         const std::string &ldInst) const {
    for (const auto &ld : ied.ldevices)
        if (ld.inst == ldInst)
            return &ld;
    for (const auto &ap : ied.accessPoints) {
        for (const auto &ld : ap.ldevices)
            if (ld.inst == ldInst)
                return &ld;
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
        return Result<ResolvedLNode>(
            {ErrorCode::InvalidPath, "Unknown IED: " + ref.iedName});

    const IED *ied = it->second;
    const LogicalDevice *ld = findLD_(*ied, ref.ldInst);
    if (!ld)
        return Result<ResolvedLNode>(
            {ErrorCode::InvalidPath, "Unknown LDevice: " + ref.ldInst});

    const LogicalNode *ln = findLN_(*ld, ref.lnClass, ref.lnInst, ref.prefix);
    if (!ln)
        return Result<ResolvedLNode>(
            {ErrorCode::InvalidPath, "Unknown LN: " + ref.lnClass + ref.lnInst});

    ResolvedLNode r{ied, ld, ln};
    return Result<ResolvedLNode>(r);
}

std::vector<EdgeCEtoCN> SclManager::collectSldEdges() const {
    std::vector<EdgeCEtoCN> edges;
    if (!model_)
        return edges;
    for (const auto &ss : model_->substations) {
        for (const auto &vl : ss.vlevels) {
            for (const auto &bay : vl.bays) {
                for (const auto &ce : bay.equipments) {
                    for (const auto &t : ce.terminals) {
                        std::string cnPath =
                            !t.connectivityNodeRef.empty()
                                                 ? t.connectivityNodeRef
                                                 : (!t.cNodeName.empty() ? (ss.name + "/" + vl.name + "/" +
                                                                            bay.name + "/" + t.cNodeName)
                                                                         : "\n");
                        if (!cnPath.empty()) {
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
    if (!model_)
        return out;
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

// --- JSON (très simple, sans échappement poussé — suffisant pour debug)
static std::string jsonEscape(const std::string &s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
      if (c == '"' || c == '\\') {
          o.push_back('\\');
          o.push_back(c);
      } else if (c == '\0') {
          o += "\n";
      } else
          o.push_back(c);
  }
  return o;
}



//=========JSON API=========//

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
                    jvl["voltage"] = {
                        {"value", vl.voltage->value},
                        {"unit", vl.voltage->unit},
                        {"mult", vl.voltage->multiplier}
                    };
                }
                jvl["bays"] = json::array();
                for (const auto& bay : vl.bays) {
                    json jbay; jbay["name"] = bay.name;
                    jbay["connectivityNodes"] = json::array();
                    for (const auto& cn : bay.connectivityNodes) {
                        json jcn;
                        jcn["name"] = cn.name;
                        if (!cn.pathName.empty()) jcn["path"] = cn.pathName;
                        // expose aussi la forme logique
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
