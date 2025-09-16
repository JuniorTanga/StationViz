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

struct TerminalRef {
    std::string name;
    std::string cNodeName;        // ex: CONNECTIVITY_NODE83
    std::string connectivityPath; // ex: ".../S1 380kV/BAY_T4_2/CONNECTIVITY_NODE83"
    std::string substationName;   // ex: "Sub1"
};

// --- Topologie primaire (Substation)
struct Terminal {
    std::string name;                 // @name
    std::string connectivityNodeRef;  // @connectivityNode (chemin) si présent
    std::string cNodeName;            // @cNodeName (ancienne forme)
};

struct TapChangerInfo {
    std::string name;
    std::string type; // "LTC", "DETC", etc.
};

struct TransformerWinding {
    std::string name;             // T4_1
    std::string type;             // PTW
    std::vector<TerminalRef> terminals;
    std::optional<TapChangerInfo> tapChanger;
    // Résolution post-parse :
    struct ResolvedEnd {
        std::string ss, vl, bay, cn; // CN logique
    };
    std::vector<ResolvedEnd> resolvedEnds; // taille = terminals.size()
};

struct PowerTransformer {
    std::string name;             // T4
    std::string desc;
    std::string type;             // PTR
    std::vector<TransformerWinding> windings;
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
    std::vector<PowerTransformer> powerTransformers;
    std::vector<LNodeRef> lnodes;          // <LNode> sous Substation
};

struct CNAddress {
    std::string ss, vl, bay, cn;
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
