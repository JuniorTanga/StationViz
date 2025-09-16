#include "SclManager.h"
#include "SclParser.h"
#include "JsonWriter.h"
#include <iostream>
#include <sstream>

using namespace scl;

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

    if (!model_)
        return;
    for (const auto &i : model_->ieds) {
        iedByName_[i.name] = &i;
    }

    // Index des ConnectivityNode par path
    for (const auto &ss : model_->substations) {
        for (const auto &vl : ss.vlevels) {
            for (const auto &bay : vl.bays) {
                for (const auto &cn : bay.connectivityNodes) {
                    std::string path =
                        !cn.pathName.empty()
                                           ? cn.pathName
                                           : (ss.name + "/" + vl.name + "/" + bay.name + "/" + cn.name);
                    cnByPath_[path] = &cn;
                }
            }
        }
    }
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

std::string SclManager::toJsonSubstations() const {
    JsonWriter w;
    w.beginObject();
    w.key("substations").beginArray();

    if (model_) {
        for (const auto& ss : model_->substations) {
            w.beginObject();
            w.key("name").value(ss.name);

            // VoltageLevels
            w.key("vlevels").beginArray();
            for (const auto& vl : ss.vlevels) {
                w.beginObject();
                w.key("name").value(vl.name);

                if (vl.voltage) {
                    w.key("voltage").beginObject();
                    w.key("value").value(vl.voltage->value);
                    w.key("unit").value(vl.voltage->unit);
                    w.key("mult").value(vl.voltage->multiplier);
                    w.endObject();
                }

                // Bays
                w.key("bays").beginArray();
                for (const auto& bay : vl.bays) {
                    w.beginObject();
                    w.key("name").value(bay.name);

                    // ConnectivityNodes (pratique pour le SLD)
                    w.key("connectivityNodes").beginArray();
                    for (const auto& cn : bay.connectivityNodes) {
                        w.beginObject();
                        w.key("name").value(cn.name);
                        if (!cn.pathName.empty())
                            w.key("path").value(cn.pathName);
                        w.endObject();
                    }
                    w.endArray();

                    // Equipements
                    w.key("equipments").beginArray();
                    for (const auto& ce : bay.equipments) {
                        w.beginObject();
                        w.key("name").value(ce.name);
                        w.key("type").value(ce.type);

                        // Terminals -> CN
                        w.key("terminals").beginArray();
                        for (const auto& t : ce.terminals) {
                            w.beginObject();
                            w.key("name").value(t.name);
                            // Préfère la ref complète si présente, sinon le cNodeName
                            const std::string cnRef = !t.connectivityNodeRef.empty()
                                                          ? t.connectivityNodeRef
                                                          : t.cNodeName;
                            if (!cnRef.empty())
                                w.key("cn").value(cnRef);
                            w.endObject();
                        }
                        w.endArray(); // terminals

                        // LNodeRefs (utile pour relier primaire ↔ IED/LN)
                        if (!ce.lnodes.empty()) {
                            w.key("lnodes").beginArray();
                            for (const auto& lr : ce.lnodes) {
                                w.beginObject();
                                if (!lr.iedName.empty()) w.key("ied").value(lr.iedName);
                                if (!lr.ldInst.empty())  w.key("ld").value(lr.ldInst);
                                if (!lr.prefix.empty())  w.key("prefix").value(lr.prefix);
                                if (!lr.lnClass.empty()) w.key("lnClass").value(lr.lnClass);
                                if (!lr.lnInst.empty())  w.key("lnInst").value(lr.lnInst);
                                w.endObject();
                            }
                            w.endArray();
                        }

                        w.endObject(); // equipment
                    }
                    w.endArray(); // equipments

                    w.endObject(); // bay
                }
                w.endArray(); // bays

                w.endObject(); // vlevel
            }
            w.endArray(); // vlevels

            w.endObject(); // substation
        }
    }

    w.endArray(); // substations
    w.endObject();
    return w.str();
}

std::string SclManager::toJsonNetwork() const {
    JsonWriter w;
    w.beginObject();
    w.key("subnetworks").beginArray();

    if (model_) {
        for (const auto& sn : model_->communication.subNetworks) {
            w.beginObject();
            w.key("name").value(sn.name);
            w.key("type").value(sn.type);

            // (optionnel) propriétés réseau sous SubNetwork (si tu les renseignes dans SclTypes)
            if (!sn.props.empty()) {
                w.key("props").beginObject();
                for (const auto& kv : sn.props) {
                    w.key(kv.first).value(kv.second);
                }
                w.endObject();
            }

            // ConnectedAP
            w.key("connectedAPs").beginArray();
            for (const auto& cap : sn.connectedAPs) {
                w.beginObject();
                w.key("ied").value(cap.iedName);
                w.key("ap").value(cap.apName);

                // Address du ConnectedAP (P fields)
                if (!cap.address.empty()) {
                    w.key("address").beginObject();
                    for (const auto& kv : cap.address) {
                        w.key(kv.first).value(kv.second);
                    }
                    w.endObject();
                }

                // GSE (GOOSE)
                w.key("gses").beginArray();
                for (const auto& g : cap.gses) {
                    w.beginObject();
                    w.key("ld").value(g.ldInst);
                    w.key("cb").value(g.cbName);
                    if (!g.address.empty()) {
                        w.key("address").beginObject();
                        for (const auto& kv : g.address) {
                            w.key(kv.first).value(kv.second);
                        }
                        w.endObject();
                    }
                    w.endObject();
                }
                w.endArray();

                // SMV (Sampled Values)
                w.key("smvs").beginArray();
                for (const auto& v : cap.smvs) {
                    w.beginObject();
                    w.key("ld").value(v.ldInst);
                    w.key("cb").value(v.cbName);
                    if (!v.address.empty()) {
                        w.key("address").beginObject();
                        for (const auto& kv : v.address) {
                            w.key(kv.first).value(kv.second);
                        }
                        w.endObject();
                    }
                    w.endObject();
                }
                w.endArray();

                w.endObject(); // ConnectedAP
            }
            w.endArray(); // connectedAPs

            w.endObject(); // SubNetwork
        }
    }

    w.endArray(); // subnetworks
    w.endObject();
    return w.str();
}
