// =============================================================
// StationViz / scl/  — Module SCL complet (pugixml)
// Monofolder layout (tous .h / .cpp dans scl/)
// Lib: sclLib  |  C++17 pur (indépendant de Qt)  |  robuste & extensible
// =============================================================
// CONTENU DU DOSSIER scl/
//   CMakeLists.txt
//   SclTypes.h
//   Result.h
//   SclParser.h
//   SclParser.cpp
//   SclManager.h
//   SclManager.cpp
//   demo_main.cpp          (binaire optionnel : SCL_BUILD_DEMO)
//
// Points clés
// - Prise en compte des subtilités SCL utiles à FAT + SLD + Network :
//   * Substation/VoltageLevel/Bay/ConductingEquipment/Terminal/ConnectivityNode
//   * Voltage avec <Voltage unit= multiplier=>
//   * LNode (liaison primaire ↔︎ IED/LD/LN via iedName/ldInst/prefix/lnClass/lnInst)
//   * IED/AccessPoint/Server/LDevice/LN0/LN (parsing robuste aux variantes)
//   * Communication/SubNetwork/ConnectedAP/Address + GSE + SMV (GOOSE/SV)
// - Index & résolutions : IED par nom, LDevice/LN, ConnectivityNode par chemin
// - API SLD-ready : collecte d’arêtes (CE ↔︎ CN), accès terminaux, JSON utilitaires
// - Debug helpers : printSubstations/printIEDs/printCommunication/printTopology
// - Error handling : Result<T>, Status, ErrorCode
// =============================================================

// =============================
// File: scl/Result.h
// =============================
#pragma once
#include <string>
#include <utility>

namespace scl {

enum class ErrorCode {
    None = 0,
    FileNotFound,
    XmlParseError,
    SchemaNotSupported,
    MissingMandatoryField,
    InvalidPath,
    LogicError,
};

struct Error {
    ErrorCode code {ErrorCode::None};
    std::string message;
};

template<typename T>
class Result {
public:
    Result(const T& value) : ok_(true), value_(value) {}
    Result(T&& value) : ok_(true), value_(std::move(value)) {}
    Result(Error e) : ok_(false), err_(std::move(e)) {}

    explicit operator bool() const { return ok_; }
    const T& value() const { return value_; }
    T& value() { return value_; }
    const Error& error() const { return err_; }

    const T* operator->() const { return &value_; }
    T* operator->() { return &value_; }

private:
    bool ok_ = false;
    T value_{};
    Error err_{};
};

class Status {
public:
    Status() = default; // OK
    explicit Status(Error e) : ok_(false), err_(std::move(e)) {}
    static Status Ok() { return Status(); }

    explicit operator bool() const { return ok_; }
    const Error& error() const { return err_; }
private:
    bool ok_ = true;
    Error err_{};
};

} // namespace scl

// =============================
// File: scl/SclTypes.h
// =============================
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace scl {

// --- Utilitaires de valeur physique
struct ScalarWithUnit {
    double value {0.0};
    std::string unit;        // ex: "V", "A", "Hz"
    std::string multiplier;  // ex: "k", "m", "M" (IEC 61850 SI multiplier)
};

// --- Topologie primaire (Substation)
struct Terminal {
    std::string name;                 // @name
    std::string connectivityNodeRef;  // @connectivityNode (chemin) si présent
    std::string cNodeName;            // @cNodeName (ancienne forme)
};

struct ConnectivityNode {
    std::string name;       // @name
    std::string pathName;   // @pathName (souvent "SS/VL/BAY/CN")
};

struct LNodeRef {           // LNode lié à l’équipement/bay/voltagelevel/substation
    std::string iedName;    // @iedName
    std::string ldInst;     // @ldInst
    std::string prefix;     // @prefix (optionnel)
    std::string lnClass;    // @lnClass
    std::string lnInst;     // @lnInst
};

struct ConductingEquipment {
    std::string name;       // @name
    std::string type;       // @type (CB, DS, PT, CT, ...)
    std::vector<Terminal> terminals;        // <Terminal>
    std::vector<LNodeRef> lnodes;           // <LNode> sous CE
};

struct Bay {
    std::string name;       // @name
    std::vector<ConnectivityNode> connectivityNodes; // <ConnectivityNode>
    std::vector<ConductingEquipment> equipments;     // <ConductingEquipment>
    std::vector<LNodeRef> lnodes;                    // <LNode> sous Bay
};

struct VoltageLevel {
    std::string name;       // @name
    std::string nomFreq;    // @nomFreq (optionnel)
    std::optional<ScalarWithUnit> voltage; // <Voltage unit= multiplier=>value
    std::vector<Bay> bays;
    std::vector<LNodeRef> lnodes;          // <LNode> sous VL
};

struct Substation {
    std::string name;       // @name
    std::vector<VoltageLevel> vlevels;
    std::vector<LNodeRef> lnodes;          // <LNode> sous Substation
};

// --- IED / LDevice / LN
struct LogicalNode {
    std::string prefix;  // @prefix
    std::string lnClass; // @lnClass
    std::string inst;    // @inst (LN0 a inst = "")
};

struct LogicalDevice {
    std::string inst;               // @inst
    std::vector<LogicalNode> lns;   // LN0 + LN*
};

struct AccessPoint {
    std::string name;       // @name
    std::unordered_map<std::string, std::string> address; // <Address>/<P>
    std::vector<LogicalDevice> ldevices; // via AccessPoint/Server/LDevice
};

struct IED {
    std::string name;       // @name
    std::string manufacturer; // @manufacturer
    std::string type;         // @type
    std::vector<AccessPoint> accessPoints;
    std::vector<LogicalDevice> ldevices; // si présents directement (fallback)
};

// --- Communication (réseau)
struct GSE { // GOOSE mapping
    std::string ldInst;     // @ldInst
    std::string cbName;     // @cbName
    std::unordered_map<std::string, std::string> address; // <Address>/<P>
};

struct SMV { // Sampled Values mapping
    std::string ldInst;     // @ldInst
    std::string cbName;     // @cbName
    std::unordered_map<std::string, std::string> address; // <Address>/<P>
};

struct ConnectedAP {
    std::string iedName;    // @iedName
    std::string apName;     // @apName
    std::unordered_map<std::string, std::string> address; // IP, MAC, VLAN...
    std::vector<GSE> gses;
    std::vector<SMV> smvs;
};

struct SubNetwork {
    std::string name;       // @name
    std::string type;       // @type (ex: "8-MMS", "8-1", "9-2-LE")
    std::unordered_map<std::string, std::string> props; // BitRate, etc.
    std::vector<ConnectedAP> connectedAPs;
};

struct Communication {
    std::vector<SubNetwork> subNetworks;
};

// --- Modèle global + indexes
struct SclModel {
    std::string version;         // SCL @version
    std::string revision;        // SCL @revision
    std::vector<Substation> substations;
    std::vector<IED> ieds;
    Communication communication;
};

// --- Aide SLD : arêtes CE↔CN
struct EdgeCEtoCN {
    std::string ssName;    // pour contexte
    std::string vlName;
    std::string bayName;
    std::string ceName;    // ConductingEquipment
    std::string cnPath;    // chemin de ConnectivityNode
};

// --- Résolution d'un LNodeRef
struct ResolvedLNode {
    const IED* ied {nullptr};
    const LogicalDevice* ld {nullptr};
    const LogicalNode* ln {nullptr};
};

} // namespace scl

// =============================
// File: scl/SclParser.h
// =============================
#pragma once
#include <string>
#include "Result.h"
#include "SclTypes.h"

namespace scl {

class SclParser {
public:
    SclParser();
    Result<SclModel> parseFile(const std::string& path);
    Result<SclModel> parseString(const std::string& xml);
};

} // namespace scl

// =============================
// File: scl/SclParser.cpp
// =============================
#include "SclParser.h"
#include <pugixml.hpp>
#include <sstream>

using namespace scl;

namespace {

static std::unordered_map<std::string, std::string> readAddress(const pugi::xml_node& parent) {
    std::unordered_map<std::string, std::string> res;
    if (auto addr = parent.child("Address")) {
        for (auto p : addr.children("P")) {
            std::string key = p.attribute("type").as_string("");
            std::string val = p.text().as_string("");
            if (!key.empty()) res[key] = val;
        }
    }
    return res;
}

static void readLNodes(const pugi::xml_node& parent, std::vector<LNodeRef>& out) {
    for (auto ln : parent.children("LNode")) {
        LNodeRef r{};
        r.iedName = ln.attribute("iedName").as_string("");
        r.ldInst  = ln.attribute("ldInst").as_string("");
        r.prefix  = ln.attribute("prefix").as_string("");
        r.lnClass = ln.attribute("lnClass").as_string("");
        r.lnInst  = ln.attribute("lnInst").as_string("");
        out.push_back(std::move(r));
    }
}

static void readTerminals(const pugi::xml_node& ceNode, std::vector<Terminal>& out) {
    for (auto t : ceNode.children("Terminal")) {
        Terminal term{};
        term.name = t.attribute("name").as_string("");
        term.connectivityNodeRef = t.attribute("connectivityNode").as_string("");
        term.cNodeName = t.attribute("cNodeName").as_string("");
        out.push_back(std::move(term));
    }
}

static void readConnectivityNodes(const pugi::xml_node& parent, std::vector<ConnectivityNode>& out) {
    for (auto cn : parent.children("ConnectivityNode")) {
        ConnectivityNode c{};
        c.name = cn.attribute("name").as_string("");
        c.pathName = cn.attribute("pathName").as_string("");
        out.push_back(std::move(c));
    }
}

static void readConductingEquipments(const pugi::xml_node& parent, std::vector<ConductingEquipment>& out) {
    for (auto ce : parent.children("ConductingEquipment")) {
        ConductingEquipment e{};
        e.name = ce.attribute("name").as_string("");
        e.type = ce.attribute("type").as_string("");
        readTerminals(ce, e.terminals);
        readLNodes(ce, e.lnodes);
        out.push_back(std::move(e));
    }
}

static std::optional<ScalarWithUnit> readVoltageNode(const pugi::xml_node& vl) {
    if (auto volt = vl.child("Voltage")) {
        ScalarWithUnit sv{};
        sv.value = std::stod(std::string(volt.text().as_string("0")));
        sv.unit = volt.attribute("unit").as_string("");
        sv.multiplier = volt.attribute("multiplier").as_string("");
        return sv;
    }
    return std::nullopt;
}

static void readLogicalNodes(const pugi::xml_node& ldNode, std::vector<LogicalNode>& out) {
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
        l.prefix  = ln.attribute("prefix").as_string("");
        l.lnClass = ln.attribute("lnClass").as_string("");
        l.inst    = ln.attribute("inst").as_string("");
        out.push_back(std::move(l));
    }
}

static void readLDevicesUnder(const pugi::xml_node& parent, std::vector<LogicalDevice>& out) {
    for (auto ld : parent.children("LDevice")) {
        LogicalDevice d{};
        d.inst = ld.attribute("inst").as_string("");
        readLogicalNodes(ld, d.lns);
        out.push_back(std::move(d));
    }
}

static void readIEDs(const pugi::xml_node& root, std::vector<IED>& out) {
    for (auto ied : root.children("IED")) {
        IED I{};
        I.name = ied.attribute("name").as_string("");
        I.manufacturer = ied.attribute("manufacturer").as_string("");
        I.type = ied.attribute("type").as_string("");

        // 1) LDevice directement sous IED (peu fréquent mais toléré par certains outils)
        readLDevicesUnder(ied, I.ldevices);

        // 2) AccessPoint/Server/LDevice (forme canonique)
        for (auto ap : ied.children("AccessPoint")) {
            AccessPoint A{}; A.name = ap.attribute("name").as_string("");
            A.address = readAddress(ap);
            if (auto server = ap.child("Server")) {
                readLDevicesUnder(server, A.ldevices);
            }
            I.accessPoints.push_back(std::move(A));
        }

        out.push_back(std::move(I));
    }
}

static Communication readCommunication(const pugi::xml_node& root) {
    Communication C{};
    if (auto comm = root.child("Communication")) {
        for (auto sn : comm.children("SubNetwork")) {
            SubNetwork S{}; S.name = sn.attribute("name").as_string("");
            S.type = sn.attribute("type").as_string("");
            // propriétés diverses
            for (auto p : sn.children("Text")) {
                (void)p; // placeholder si besoin
            }
            // parfois des P directement sous SubNetwork (BitRate, etc.)
            for (auto p : sn.children("P")) {
                std::string key = p.attribute("type").as_string("");
                if (!key.empty()) S.props[key] = p.text().as_string("");
            }

            for (auto cap : sn.children("ConnectedAP")) {
                ConnectedAP CAP{};
                CAP.iedName = cap.attribute("iedName").as_string("");
                CAP.apName  = cap.attribute("apName").as_string("");
                CAP.address = readAddress(cap);

                // GSE / SMV
                for (auto g : cap.children("GSE")) {
                    GSE G{}; G.ldInst = g.attribute("ldInst").as_string("");
                    G.cbName = g.attribute("cbName").as_string("");
                    G.address = readAddress(g);
                    CAP.gses.push_back(std::move(G));
                }
                for (auto s : cap.children("SMV")) {
                    SMV V{}; V.ldInst = s.attribute("ldInst").as_string("");
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

static Result<SclModel> parseDoc(pugi::xml_document& doc) {
    SclModel model{};

    auto root = doc.child("SCL");
    if (!root) {
        return Result<SclModel>({ErrorCode::XmlParseError, "Missing <SCL> root"});
    }
    model.version  = root.attribute("version").as_string("");
    model.revision = root.attribute("revision").as_string("");

    // --- Substations
    for (auto ss : root.children("Substation")) {
        Substation S{}; S.name = ss.attribute("name").as_string("");
        readLNodes(ss, S.lnodes);
        for (auto vl : ss.children("VoltageLevel")) {
            VoltageLevel V{}; V.name = vl.attribute("name").as_string("");
            V.nomFreq = vl.attribute("nomFreq").as_string("");
            V.voltage = readVoltageNode(vl);
            readLNodes(vl, V.lnodes);
            for (auto bay : vl.children("Bay")) {
                Bay B{}; B.name = bay.attribute("name").as_string("");
                readConnectivityNodes(bay, B.connectivityNodes);
                readConductingEquipments(bay, B.equipments);
                readLNodes(bay, B.lnodes);
                V.bays.push_back(std::move(B));
            }
            S.vlevels.push_back(std::move(V));
        }
        model.substations.push_back(std::move(S));
    }

    // --- IEDs
    readIEDs(root, model.ieds);

    // --- Communication
    model.communication = readCommunication(root);

    return Result<SclModel>(std::move(model));
}

} // anon

SclParser::SclParser() = default;

Result<SclModel> SclParser::parseFile(const std::string& path) {
    pugi::xml_document doc;
    pugi::xml_parse_result ok = doc.load_file(path.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!ok) {
        std::ostringstream oss; oss << "XML parse error: " << ok.description() << ", offset=" << ok.offset;
        return Result<SclModel>({ErrorCode::XmlParseError, oss.str()});
    }
    return parseDoc(doc);
}

Result<SclModel> SclParser::parseString(const std::string& xml) {
    pugi::xml_document doc;
    pugi::xml_parse_result ok = doc.load_string(xml.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!ok) {
        std::ostringstream oss; oss << "XML parse error: " << ok.description() << ", offset=" << ok.offset;
        return Result<SclModel>({ErrorCode::XmlParseError, oss.str()});
    }
    return parseDoc(doc);
}

// =============================
// File: scl/SclManager.h
// =============================
#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include "Result.h"
#include "SclTypes.h"

namespace scl {

class SclManager {
public:
    SclManager();

    // Charge et parse un fichier SCL + construit les indexes
    Status loadScl(const std::string& filepath);

    // Accès lecture au modèle
    const SclModel* model() const { return model_ ? &(*model_) : nullptr; }

    // Debug helpers
    Status printSubstations() const;
    Status printIEDs() const;
    Status printCommunication() const;
    Status printTopology() const; // CE ↔ CN

    // Requêtes simples
    Result<const Substation*> findSubstation(const std::string& name) const;
    Result<const IED*> findIED(const std::string& name) const;

    // Résolution d’un LNodeRef
    Result<ResolvedLNode> resolveLNodeRef(const LNodeRef& ref) const;

    // Aides SLD/Network
    std::vector<EdgeCEtoCN> collectSldEdges() const; // liste des arêtes CE↔CN
    std::vector<ConnectivityNode> getConnectivityNodes(const std::string& ss,
                                                       const std::string& vl,
                                                       const std::string& bay) const;

    // JSON utilitaires (sorties rapides pour debug/GUI)
    std::string toJsonSubstations() const;   // Substations + topology légère
    std::string toJsonNetwork() const;       // Communication

private:
    void buildIndexes_();

    const LogicalDevice* findLD_(const IED& ied, const std::string& ldInst) const;
    const LogicalNode*   findLN_(const LogicalDevice& ld, const std::string& lnClass,
                                 const std::string& lnInst, const std::string& prefix) const;

    // Indexes
    std::unique_ptr<SclModel> model_;
    std::unordered_map<std::string, const IED*> iedByName_;
    // CN index par chemin (pathName ou fallback composé "SS/VL/BAY/CN")
    std::unordered_map<std::string, const ConnectivityNode*> cnByPath_;
};

} // namespace scl

// =============================
// File: scl/SclManager.cpp
// =============================
#include "SclManager.h"
#include "SclParser.h"
#include <iostream>
#include <sstream>

using namespace scl;

SclManager::SclManager() = default;

Status SclManager::loadScl(const std::string& filepath) {
    SclParser parser;
    auto res = parser.parseFile(filepath);
    if (!res) {
        return Status(Error{res.error().code, std::string("loadScl: ") + res.error().message});
    }
    model_ = std::make_unique<SclModel>(std::move(res.value()));
    buildIndexes_();
    return Status::Ok();
}

void SclManager::buildIndexes_() {
    iedByName_.clear();
    cnByPath_.clear();

    if (!model_) return;
    for (const auto& i : model_->ieds) {
        iedByName_[i.name] = &i;
    }

    // Index des ConnectivityNode par path
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                for (const auto& cn : bay.connectivityNodes) {
                    std::string path = !cn.pathName.empty() ? cn.pathName
                        : (ss.name + "/" + vl.name + "/" + bay.name + "/" + cn.name);
                    cnByPath_[path] = &cn;
                }
            }
        }
    }
}

Status SclManager::printSubstations() const {
    if (!model_) return Status(Error{ErrorCode::LogicError, "No SCL loaded"});

    std::cout << "SCL version=" << model_->version << ", revision=" << model_->revision << "
";
    for (const auto& ss : model_->substations) {
        std::cout << "Substation: " << ss.name << "
";
        for (const auto& vl : ss.vlevels) {
            std::cout << "  VoltageLevel: " << vl.name;
            if (vl.voltage) {
                std::cout << "  <Voltage=" << vl.voltage->value << vl.voltage->multiplier << vl.voltage->unit << ">";
            }
            if (!vl.nomFreq.empty()) std::cout << "  (nomFreq=" << vl.nomFreq << ")";
            std::cout << "
";
            for (const auto& bay : vl.bays) {
                std::cout << "    Bay: " << bay.name << "
";
                for (const auto& cn : bay.connectivityNodes) {
                    std::cout << "      CN: name=" << cn.name << ", path=" << cn.pathName << "
";
                }
                for (const auto& ce : bay.equipments) {
                    std::cout << "      CE: name=" << ce.name << ", type=" << ce.type << "
";
                    for (const auto& t : ce.terminals) {
                        std::cout << "        Terminal: name=" << t.name
                                  << ", connectivityNode=" << t.connectivityNodeRef
                                  << ", cNodeName=" << t.cNodeName << "
";
                    }
                    for (const auto& ln : ce.lnodes) {
                        std::cout << "        LNode: ied=" << ln.iedName << ", ldInst=" << ln.ldInst
                                  << ", " << ln.prefix << ln.lnClass << ln.lnInst << "
";
                    }
                }
            }
        }
    }
    return Status::Ok();
}

Status SclManager::printIEDs() const {
    if (!model_) return Status(Error{ErrorCode::LogicError, "No SCL loaded"});
    for (const auto& ied : model_->ieds) {
        std::cout << "IED: " << ied.name
                  << ", mfg=" << ied.manufacturer
                  << ", type=" << ied.type << "
";
        for (const auto& ap : ied.accessPoints) {
            std::cout << "  AccessPoint: " << ap.name << "  Address(P): ";
            bool first = true; for (const auto& kv : ap.address) { if (!first) std::cout << ", "; first=false; std::cout << kv.first << "=" << kv.second; }
            std::cout << "
";
            for (const auto& ld : ap.ldevices) {
                std::cout << "    LDevice: inst=" << ld.inst << "
";
                for (const auto& ln : ld.lns) {
                    std::cout << "      LN: " << ln.prefix << ln.lnClass << ln.inst << "
";
                }
            }
        }
        for (const auto& ld : ied.ldevices) {
            std::cout << "  (Direct) LDevice: inst=" << ld.inst << "
";
            for (const auto& ln : ld.lns) {
                std::cout << "      LN: " << ln.prefix << ln.lnClass << ln.inst << "
";
            }
        }
    }
    return Status::Ok();
}

Status SclManager::printCommunication() const {
    if (!model_) return Status(Error{ErrorCode::LogicError, "No SCL loaded"});
    const auto& C = model_->communication;
    for (const auto& sn : C.subNetworks) {
        std::cout << "SubNetwork: " << sn.name << " (type=" << sn.type << ")
";
        if (!sn.props.empty()) {
            std::cout << "  Props: "; bool first=true; for (const auto& kv: sn.props){ if(!first) std::cout<<", "; first=false; std::cout<<kv.first<<"="<<kv.second; } std::cout<<"
";
        }
        for (const auto& cap : sn.connectedAPs) {
            std::cout << "  ConnectedAP: ied=" << cap.iedName << ", ap=" << cap.apName << "
";
            if (!cap.address.empty()) {
                std::cout << "    Address: "; bool first=true; for (const auto& kv : cap.address){ if(!first) std::cout<<", "; first=false; std::cout<<kv.first<<"="<<kv.second;} std::cout<<"
";
            }
            for (const auto& g : cap.gses) {
                std::cout << "    GSE: ldInst=" << g.ldInst << ", cbName=" << g.cbName << "  P{";
                bool first=true; for (const auto& kv : g.address){ if(!first) std::cout<<", "; first=false; std::cout<<kv.first<<"="<<kv.second;} std::cout<<"}
";
            }
            for (const auto& v : cap.smvs) {
                std::cout << "    SMV: ldInst=" << v.ldInst << ", cbName=" << v.cbName << "  P{";
                bool first=true; for (const auto& kv : v.address){ if(!first) std::cout<<", "; first=false; std::cout<<kv.first<<"="<<kv.second;} std::cout<<"}
";
            }
        }
    }
    return Status::Ok();
}

Status SclManager::printTopology() const {
    auto edges = collectSldEdges();
    std::cout << "Topology edges (CE -> CN):
";
    for (const auto& e : edges) {
        std::cout << "  [" << e.ssName << "/" << e.vlName << "/" << e.bayName << "] "
                  << e.ceName << " -> " << e.cnPath << "
";
    }
    return Status::Ok();
}

Result<const Substation*> SclManager::findSubstation(const std::string& name) const {
    if (!model_) return Result<const Substation*>({ErrorCode::LogicError, "No SCL loaded"});
    for (const auto& ss : model_->substations) {
        if (ss.name == name) return Result<const Substation*>(&ss);
    }
    return Result<const Substation*>({ErrorCode::InvalidPath, "Substation not found: " + name});
}

Result<const IED*> SclManager::findIED(const std::string& name) const {
    if (!model_) return Result<const IED*>({ErrorCode::LogicError, "No SCL loaded"});
    auto it = iedByName_.find(name);
    if (it != iedByName_.end()) return Result<const IED*>(it->second);
    return Result<const IED*>({ErrorCode::InvalidPath, "IED not found: " + name});
}

const LogicalDevice* SclManager::findLD_(const IED& ied, const std::string& ldInst) const {
    for (const auto& ld : ied.ldevices) if (ld.inst == ldInst) return &ld;
    for (const auto& ap : ied.accessPoints) {
        for (const auto& ld : ap.ldevices) if (ld.inst == ldInst) return &ld;
    }
    return nullptr;
}

const LogicalNode* SclManager::findLN_(const LogicalDevice& ld, const std::string& lnClass,
                                       const std::string& lnInst, const std::string& prefix) const {
    for (const auto& ln : ld.lns) {
        if (ln.lnClass == lnClass && ln.inst == lnInst && ln.prefix == prefix) return &ln;
    }
    return nullptr;
}

Result<ResolvedLNode> SclManager::resolveLNodeRef(const LNodeRef& ref) const {
    auto it = iedByName_.find(ref.iedName);
    if (it == iedByName_.end())
        return Result<ResolvedLNode>({ErrorCode::InvalidPath, "Unknown IED: " + ref.iedName});

    const IED* ied = it->second;
    const LogicalDevice* ld = findLD_(*ied, ref.ldInst);
    if (!ld) return Result<ResolvedLNode>({ErrorCode::InvalidPath, "Unknown LDevice: " + ref.ldInst});

    const LogicalNode* ln = findLN_(*ld, ref.lnClass, ref.lnInst, ref.prefix);
    if (!ln) return Result<ResolvedLNode>({ErrorCode::InvalidPath, "Unknown LN: " + ref.lnClass + ref.lnInst});

    ResolvedLNode r{ied, ld, ln};
    return Result<ResolvedLNode>(r);
}

std::vector<EdgeCEtoCN> SclManager::collectSldEdges() const {
    std::vector<EdgeCEtoCN> edges;
    if (!model_) return edges;
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                for (const auto& ce : bay.equipments) {
                    for (const auto& t : ce.terminals) {
                        std::string cnPath = !t.connectivityNodeRef.empty() ? t.connectivityNodeRef
                                             : (!t.cNodeName.empty() ? (ss.name + "/" + vl.name + "/" + bay.name + "/" + t.cNodeName) : "");
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

std::vector<ConnectivityNode> SclManager::getConnectivityNodes(const std::string& ss,
                                                               const std::string& vl,
                                                               const std::string& bay) const {
    std::vector<ConnectivityNode> out;
    if (!model_) return out;
    for (const auto& S : model_->substations) if (S.name == ss) {
        for (const auto& V : S.vlevels) if (V.name == vl) {
            for (const auto& B : V.bays) if (B.name == bay) {
                out = B.connectivityNodes; return out;
            }
        }
    }
    return out;
}

// --- JSON (très simple, sans échappement poussé — suffisant pour debug)
static std::string jsonEscape(const std::string& s){
    std::string o; o.reserve(s.size()+8);
    for(char c: s){ if(c=='"'||c=='\') { o.push_back('\'); o.push_back(c);} else if(c=='
'){ o += "\n";} else o.push_back(c);} return o;
}

std::string SclManager::toJsonSubstations() const {
    std::ostringstream os; os << "{""substations"":" << "[";
    if (!model_) { os << "]}"; return os.str(); }
    bool firstSS=true;
    for (const auto& ss : model_->substations) {
        if(!firstSS) os << ","; firstSS=false;
        os << "{""name"":"<< '"' << jsonEscape(ss.name) << '"' << ",""vlevels"":"<<"[";
        bool firstVL=true;
        for (const auto& vl : ss.vlevels) {
            if(!firstVL) os << ","; firstVL=false;
            os << "{""name"":"<< '"' << jsonEscape(vl.name) << '"';
            if (vl.voltage) {
                os << ",""voltage"":{""value"":"<< vl.voltage->value << ",""unit"":"""<< jsonEscape(vl.voltage->unit) <<""",""mult"":"""<< jsonEscape(vl.voltage->multiplier) <<"""}";
            }
            os << ",""bays"":"<<"[";
            bool firstB=true;
            for (const auto& bay : vl.bays) {
                if(!firstB) os << ","; firstB=false;
                os << "{""name"":"<< '"' << jsonEscape(bay.name) << '"' << ",""equipments"":"<<"[";
                bool firstE=true;
                for (const auto& ce : bay.equipments) {
                    if(!firstE) os << ","; firstE=false;
                    os << "{""name"":"<< '"' << jsonEscape(ce.name) << '"' << ",""type"":"<< '"' << jsonEscape(ce.type) << '"' << ",""terminals"":"<<"[";
                    bool firstT=true;
                    for (const auto& t : ce.terminals) {
                        if(!firstT) os << ","; firstT=false;
                        os << "{""name"":"<< '"' << jsonEscape(t.name) << '"' << ",""cn"":"<< '"' << jsonEscape(!t.connectivityNodeRef.empty()?t.connectivityNodeRef:t.cNodeName) << '"' << "}";
                    }
                    os << "]}";
                }
                os << "]}";
            }
            os << "]}";
        }
        os << "]}";
    }
    os << "]}";
    return os.str();
}

std::string SclManager::toJsonNetwork() const {
    std::ostringstream os; os << "{""subnetworks"":" << "[";
    if (!model_) { os << "]}"; return os.str(); }
    bool firstSN=true;
    for (const auto& sn : model_->communication.subNetworks) {
        if(!firstSN) os << ","; firstSN=false;
        os << "{""name"":"<< '"' << jsonEscape(sn.name) << '"' << ",""type"":"<< '"' << jsonEscape(sn.type) << '"' << ",""connectedAPs"":"<<"[";
        bool firstCAP=true;
        for (const auto& cap : sn.connectedAPs) {
            if(!firstCAP) os << ","; firstCAP=false;
            os << "{""ied"":"<< '"' << jsonEscape(cap.iedName) << '"' << ",""ap"":"<< '"' << jsonEscape(cap.apName) << '"' << ",""gses"":"<<"[";
            bool firstG=true; for (const auto& g : cap.gses) { if(!firstG) os << ","; firstG=false; os << "{""ld"":"""<< jsonEscape(g.ldInst) <<""",""cb"":"""<< jsonEscape(g.cbName) <<"""}"; }
            os << "],""smvs"":"<<"[";
            bool firstS=true; for (const auto& v : cap.smvs) { if(!firstS) os << ","; firstS=false; os << "{""ld"":"""<< jsonEscape(v.ldInst) <<""",""cb"":"""<< jsonEscape(v.cbName) <<"""}"; }
            os << "]}";
        }
        os << "]}";
    }
    os << "]}";
    return os.str();
}

// =============================
// File: scl/CMakeLists.txt
// =============================
cmake_minimum_required(VERSION 3.20)
project(sclLib LANGUAGES CXX)

option(SCL_BUILD_DEMO "Build SCL demo executable" ON)

add_library(sclLib
    SclParser.cpp
    SclManager.cpp
)

# Entêtes publics (monofolder)
target_include_directories(sclLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(sclLib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Dépendance pugixml
find_package(pugixml QUIET)
if (pugixml_FOUND)
    target_link_libraries(sclLib PUBLIC pugixml::pugixml)
else()
    include(FetchContent)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.14
    )
    FetchContent_MakeAvailable(pugixml)
    target_link_libraries(sclLib PUBLIC pugixml)
endif()

# Warnings
if (MSVC)
    target_compile_options(sclLib PRIVATE /W4)
else()
    target_compile_options(sclLib PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (SCL_BUILD_DEMO)
    add_executable(scl_demo demo_main.cpp)
    target_link_libraries(scl_demo PRIVATE sclLib)
    target_compile_features(scl_demo PRIVATE cxx_std_17)
endif()

// =============================
// File: scl/demo_main.cpp
// =============================
#include <iostream>
#include "SclManager.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.scl|.scd|.icd>
";
        return 1;
    }
    const std::string path = argv[1];

    scl::SclManager mgr;
    auto st = mgr.loadScl(path);
    if (!st) {
        std::cerr << "Failed to load SCL: " << st.error().message << "
";
        return 2;
    }

    std::cout << "=== Substations ===
"; mgr.printSubstations();
    std::cout << "
=== IEDs ===
"; mgr.printIEDs();
    std::cout << "
=== Communication ===
"; mgr.printCommunication();
    std::cout << "
=== Topology (CE -> CN) ===
"; mgr.printTopology();

    std::cout << "
=== JSON (Substations) ===
" << mgr.toJsonSubstations() << "
";
    std::cout << "
=== JSON (Network) ===
" << mgr.toJsonNetwork() << "
";

    // Exemple: résolution d'un LNodeRef venu d'un CE
    if (auto ssRes = mgr.findSubstation("SS1")) {
        // rien de spécifique ici; juste démonstration d'appel
    }

    return 0;
}
