#include "SclParser.h"
#include "pugixml/pugixml.hpp"
#include <sstream>

using namespace scl;

// scl/PathUtils.h
inline std::optional<CNAddress> parseConnectivityPath(const std::string& path) {
    // format attendu: ".../<VL>/<BAY>/<CONNECTIVITY_NODEXX>"
    // On prend les 3 derniers segments non vides
    std::vector<std::string> segs;
    std::string cur;
    for (char c : path) {
        if (c == '/') { if (!cur.empty()) { segs.push_back(cur); cur.clear(); } }
        else cur.push_back(c);
    }
    if (!cur.empty()) segs.push_back(cur);
    if (segs.size() < 3) return std::nullopt;

    CNAddress a;
    a.cn  = segs.back();
    a.bay = segs[segs.size()-2];
    a.vl  = segs[segs.size()-3];
    // SS : on tente via préfixe ou l’attribut substationName du Terminal
    // Ici on ne peut pas inférer directement, on le remplira au call-site.
    return a;
}


namespace {

static std::unordered_map<std::string, std::string>
readAddress(const pugi::xml_node &parent) {
    std::unordered_map<std::string, std::string> res;
    if (auto addr = parent.child("Address")) {
        for (auto p : addr.children("P")) {
            std::string key = p.attribute("type").as_string("");
            std::string val = p.text().as_string("");
            if (!key.empty())
                res[key] = val;
        }
    }
    return res;
}

static void readLNodes(const pugi::xml_node &parent,
                       std::vector<LNodeRef> &out) {
    for (auto ln : parent.children("LNode")) {
        LNodeRef r{};
        r.iedName = ln.attribute("iedName").as_string("");
        r.ldInst = ln.attribute("ldInst").as_string("");
        r.prefix = ln.attribute("prefix").as_string("");
        r.lnClass = ln.attribute("lnClass").as_string("");
        r.lnInst = ln.attribute("lnInst").as_string("");
        out.push_back(std::move(r));
    }
}

static void readTerminals(const pugi::xml_node &ceNode,
                          std::vector<Terminal> &out) {
    for (auto t : ceNode.children("Terminal")) {
        Terminal term{};
        term.name = t.attribute("name").as_string("");
        term.connectivityNodeRef = t.attribute("connectivityNode").as_string("");
        term.cNodeName = t.attribute("cNodeName").as_string("");
        out.push_back(std::move(term));
    }
}

static void readConnectivityNodes(const pugi::xml_node &parent,
                                  std::vector<ConnectivityNode> &out) {
    for (auto cn : parent.children("ConnectivityNode")) {
        ConnectivityNode c{};
        c.name = cn.attribute("name").as_string("");
        c.pathName = cn.attribute("pathName").as_string("");
        out.push_back(std::move(c));
    }
}

static void readConductingEquipments(const pugi::xml_node &parent,
                                     std::vector<ConductingEquipment> &out) {
    for (auto ce : parent.children("ConductingEquipment")) {
        ConductingEquipment e{};
        e.name = ce.attribute("name").as_string("");
        e.type = ce.attribute("type").as_string("");
        readTerminals(ce, e.terminals);
        readLNodes(ce, e.lnodes);
        out.push_back(std::move(e));
    }
}

static std::optional<ScalarWithUnit> readVoltageNode(const pugi::xml_node &vl) {
    if (auto volt = vl.child("Voltage")) {
        ScalarWithUnit sv{};
        sv.value = std::stod(std::string(volt.text().as_string("0")));
        sv.unit = volt.attribute("unit").as_string("");
        sv.multiplier = volt.attribute("multiplier").as_string("");
        return sv;
    }
    return std::nullopt;
}

static void readLogicalNodes(const pugi::xml_node &ldNode,
                             std::vector<LogicalNode> &out) {
    // LN0
    if (auto ln0 = ldNode.child("LN0")) {
        LogicalNode ln{};
        ln.prefix = ln0.attribute("prefix").as_string("");
        ln.lnClass = ln0.attribute("lnClass").as_string("");
        ln.inst = ""; // LN0
        out.push_back(std::move(ln));
    }
    // LN*
    for (auto ln : ldNode.children("LN")) {
        LogicalNode l{};
        l.prefix = ln.attribute("prefix").as_string("");
        l.lnClass = ln.attribute("lnClass").as_string("");
        l.inst = ln.attribute("inst").as_string("");
        out.push_back(std::move(l));
    }
}

static void readLDevicesUnder(const pugi::xml_node &parent,
                              std::vector<LogicalDevice> &out) {
    for (auto ld : parent.children("LDevice")) {
        LogicalDevice d{};
        d.inst = ld.attribute("inst").as_string("");
        readLogicalNodes(ld, d.lns);
        out.push_back(std::move(d));
    }
}

static void readIEDs(const pugi::xml_node &root, std::vector<IED> &out) {
    for (auto ied : root.children("IED")) {
        IED I{};
        I.name = ied.attribute("name").as_string("");
        I.manufacturer = ied.attribute("manufacturer").as_string("");
        I.type = ied.attribute("type").as_string("");

        // 1) LDevice directement sous IED (peu fréquent mais toléré par certains
        // outils)
        readLDevicesUnder(ied, I.ldevices);

        // 2) AccessPoint/Server/LDevice (forme canonique)
        for (auto ap : ied.children("AccessPoint")) {
            AccessPoint A{};
            A.name = ap.attribute("name").as_string("");
            A.address = readAddress(ap);
            if (auto server = ap.child("Server")) {
                readLDevicesUnder(server, A.ldevices);
            }
            I.accessPoints.push_back(std::move(A));
        }

        out.push_back(std::move(I));
    }
}

static Communication readCommunication(const pugi::xml_node &root) {
    Communication C{};
    if (auto comm = root.child("Communication")) {
        for (auto sn : comm.children("SubNetwork")) {
            SubNetwork S{};
            S.name = sn.attribute("name").as_string("");
            S.type = sn.attribute("type").as_string("");
            // propriétés diverses
            for (auto p : sn.children("Text")) {
                (void)p; // placeholder si besoin
            }
            // parfois des P directement sous SubNetwork (BitRate, etc.)
            for (auto p : sn.children("P")) {
                std::string key = p.attribute("type").as_string("");
                if (!key.empty())
                    S.props[key] = p.text().as_string("");
            }

            for (auto cap : sn.children("ConnectedAP")) {
                ConnectedAP CAP{};
                CAP.iedName = cap.attribute("iedName").as_string("");
                CAP.apName = cap.attribute("apName").as_string("");
                CAP.address = readAddress(cap);

                // GSE / SMV
                for (auto g : cap.children("GSE")) {
                    GSE G{};
                    G.ldInst = g.attribute("ldInst").as_string("");
                    G.cbName = g.attribute("cbName").as_string("");
                    G.address = readAddress(g);
                    CAP.gses.push_back(std::move(G));
                }
                for (auto s : cap.children("SMV")) {
                    SMV V{};
                    V.ldInst = s.attribute("ldInst").as_string("");
                    V.cbName = s.attribute("cbName").as_string("");
                    V.address = readAddress(s);
                    CAP.smvs.push_back(std::move(V));
                }

                S.connectedAPs.push_back(std::move(CAP));
            }

            C.subNetworks.push_back(std::move(S));
        }
    }
    return C;
}

static Result<SclModel> parseDoc(pugi::xml_document &doc) {
    SclModel model{};

    auto root = doc.child("SCL");
    if (!root) {
        return Result<SclModel>({ErrorCode::XmlParseError, "Missing <SCL> root"});
    }
    model.version = root.attribute("version").as_string("");
    model.revision = root.attribute("revision").as_string("");

    // --- Substations
    for (auto ss : root.children("Substation")) {
        Substation S{};
        S.name = ss.attribute("name").as_string("");
        readLNodes(ss, S.lnodes);

        // scl/SclParser.cpp (dans parseSubstationNode(...))
        for (auto ptNode : ss.children("PowerTransformer")) {
            PowerTransformer pt;
            pt.name = ptNode.attribute("name").as_string();
            pt.desc = ptNode.attribute("desc").as_string();
            pt.type = ptNode.attribute("type").as_string();

            for (auto wNode : ptNode.children("TransformerWinding")) {
                TransformerWinding w;
                w.name = wNode.attribute("name").as_string();
                w.type = wNode.attribute("type").as_string();

                // TapChanger (optionnel)
                if (auto tc = wNode.child("TapChanger")) {
                    TapChangerInfo tci;
                    tci.name = tc.attribute("name").as_string();
                    tci.type = tc.attribute("type").as_string();
                    w.tapChanger = tci;
                }

                for (auto tNode : wNode.children("Terminal")) {
                    TerminalRef tr;
                    tr.name = tNode.attribute("name").as_string();
                    tr.cNodeName = tNode.attribute("cNodeName").as_string();
                    tr.connectivityPath = tNode.attribute("connectivityNode").as_string();
                    tr.substationName = tNode.attribute("substationName").as_string();
                    w.terminals.push_back(std::move(tr));
                }
                pt.windings.push_back(std::move(w));
            }
            S.powerTransformers.push_back(std::move(pt));
        }


        for (auto vl : ss.children("VoltageLevel")) {
            VoltageLevel V{};
            V.name = vl.attribute("name").as_string("");
            V.nomFreq = vl.attribute("nomFreq").as_string("");
            V.voltage = readVoltageNode(vl);
            readLNodes(vl, V.lnodes);
            for (auto bay : vl.children("Bay")) {
                Bay B{};
                B.name = bay.attribute("name").as_string("");
                readConnectivityNodes(bay, B.connectivityNodes);
                readConductingEquipments(bay, B.equipments);
                readLNodes(bay, B.lnodes);
                V.bays.push_back(std::move(B));
            }
            S.vlevels.push_back(std::move(V));
        }
        model.substations.push_back(std::move(S));
    }

    for (auto &ss : model.substations) {
        for (auto &pt : ss.powerTransformers) {
            for (auto &w : pt.windings) {
                w.resolvedEnds.clear();
                for (const auto &tr : w.terminals) {
                    TransformerWinding::ResolvedEnd re;
                    re.ss = !tr.substationName.empty() ? tr.substationName : ss.name;

                    if (!tr.connectivityPath.empty()) {
                        if (auto addr = parseConnectivityPath(tr.connectivityPath)) {
                            re.vl = addr->vl; re.bay = addr->bay; re.cn = addr->cn;
                        } else {
                            // fallback: cNodeName seul -> on le cherchera plus tard via index
                            re.cn = tr.cNodeName;
                        }
                    } else {
                        re.cn = tr.cNodeName;
                    }
                    w.resolvedEnds.push_back(re);
                }
            }
        }
    }

    // --- IEDs
    readIEDs(root, model.ieds);

    // --- Communication
    model.communication = readCommunication(root);

    return Result<SclModel>(std::move(model));
}

} // namespace

SclParser::SclParser() = default;

Result<SclModel> SclParser::parseFile(const std::string &path) {
    pugi::xml_document doc;
    pugi::xml_parse_result ok =
        doc.load_file(path.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!ok) {
        std::ostringstream oss;
        oss << "XML parse error: " << ok.description() << ", offset=" << ok.offset;
        return Result<SclModel>({ErrorCode::XmlParseError, oss.str()});
    }
    return parseDoc(doc);
}

Result<SclModel> SclParser::parseString(const std::string &xml) {
    pugi::xml_document doc;
    pugi::xml_parse_result ok =
        doc.load_string(xml.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!ok) {
        std::ostringstream oss;
        oss << "XML parse error: " << ok.description() << ", offset=" << ok.offset;
        return Result<SclModel>({ErrorCode::XmlParseError, oss.str()});
    }
    return parseDoc(doc);
}
