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
    std::string multiplier;  // ex: "k", "m", "M"
    bool valid {true};       // false si le texte n'était pas un nombre fini
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
    // tns:TransformerWinding permits several TapChanger plus one PhaseTapChanger,
    // so a single optional could not represent a real transformer.
    std::vector<TapChangerInfo> tapChangers;
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
    std::string desc; //optionelle
};

struct GseControlMeta {
    std::string name;    // @name
    std::string datSet;  // @datSet (nom du DataSet)
    std::string appID;   // optionnel
};

struct SmvControlMeta {
    std::string name;
    std::string datSet;
    std::string appID;   // optionnel
    std::string smpRate; // optionnel (via P dans Address réseau, sinon logger)
};

// tns:ReportControl. This is what makes a B-report (client) subscription
// possible, and it is unrelated to GSEControl: the two are separate control
// blocks declared side by side in LN0.
struct ReportControlMeta {
    std::string name;      // @name
    std::string rptID;     // @rptID, full MMS name, e.g. "MYIEDLD/LLN0$RP$urcbA"
    std::string datSet;    // @datSet, empty when the DataSet is implicit
    std::string confRev;   // @confRev
    std::string desc;      // @desc
    bool buffered {false}; // @buffered
    std::string intgPd;    // @intgPd, integrity period in ms
    // tns:TrgOps bit string. Defaults follow the usual convention of data
    // change + quality change + GI, since an IED shipped with TrgOps=0 delivers
    // nothing until an integrity period fires (and IntgPd often defaults to 0).
    struct TrgOps {
        bool dataChange {true};
        bool qualityChange {true};
        bool dataUpdate {false};
        bool integrity {false};
        bool generalInterrogation {true};
    } trgOps;
    // tns:OptFields. The SCL names each bit by attribute name (sequenceNumber,
    // timeStamp, ...) rather than by position, so read them by name.
    struct OptFields {
        bool seqNum {false};           // sequenceNumber
        bool timeStamp {false};        // timeStamp
        bool dataSet {false};          // dataSet
        bool reasonForInclusion {false};
        bool configRef {false};
        bool bufOvfl {false};
        bool entryID {false};
        bool confRev {false};
        bool subSeqNum {false};
    } optFields;
};

// tns:Inputs / tns:ExtRef: a report DataSet may reference data from another IED
// over GOOSE rather than from the local server.
struct ExtRefRef {
    std::string iedName, ldInst, prefix, lnClass, lnInst, doName, daName, fc;
    std::string intgPd;
};

// --- LN0 / DataSet / Controls (métadonnées minimales)
struct FcdaRef {
    std::string ldInst;    // optionnel si scope LN0 implicite
    std::string lnClass;   // ex: XCBR
    std::string lnInst;    // ex: 1
    std::string doName;    // ex: Pos
    std::string daName;    // ex: stVal (optionnel)
    std::string fc;        // ex: ST/MX/CO
};

struct DataSet {
    std::string name;
    std::vector<FcdaRef> members;
    std::vector<ExtRefRef> extRefs;  // <Inputs><ExtRef>, data sourced over GOOSE
};

struct Ln0Info {
    std::vector<DataSet> datasets;
    std::vector<GseControlMeta> gseCtrls;
    std::vector<SmvControlMeta> smvCtrls;
    std::vector<ReportControlMeta> rptCtrls; // ReportControl + ReportControlBlock
};

struct LogicalDevice {
    std::string inst;
    std::vector<LogicalNode> lns; // LN0 + LN*
    Ln0Info ln0;                  // metas de LN0
    std::string desc;
};

// --- Endpoints (index réseau prêts pour network core)
//
// The control block declared in LN0 is the source of truth: an endpoint exists
// because the IED publishes it, and the Communication section only supplies the
// MAC/APPID needed to find it on the wire. `addressDeclared` records whether
// that mapping was actually present, so a missing one can be diagnosed instead
// of silently yielding an endpoint with an empty MAC.
struct GseEndpoint {
    std::string iedName, ldInst, cbName;
    std::string mac, appid, vlanId, vlanPrio;
    std::string datasetRef; // nom du DataSet sur LN0
    std::string subNetwork; // SubNetwork that carried the Address mapping
    bool addressDeclared {false};
};

struct SvEndpoint {
    std::string iedName, ldInst, cbName;
    std::string mac, appid, vlanId, vlanPrio;
    std::string smpRate;
    std::string datasetRef;
    std::string subNetwork;
    bool addressDeclared {false};
};

struct MmsEndpoint {
    std::string iedName, apName;
    std::string ip;
    std::string port; // "102" par défaut si absent
    std::string subNetwork;
    // Where the address came from: the ConnectedAP, the AccessPoint, or a
    // Server. The first is canonical; the others are vendor variations that are
    // still widely emitted and were previously ignored entirely.
    enum class AddressSource { None, ConnectedAP, AccessPoint, Server };
    AddressSource addressSource {AddressSource::None};
    std::string serverName;
};

struct AccessPoint {
    std::string name;       // @name
    std::unordered_map<std::string, std::string> address; // <Address>/<P>
    // One entry per Server element: tns:AccessPoint allows an unbounded number.
    std::vector<std::unordered_map<std::string, std::string>> serverAddresses;
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
// SCL/Header
struct SclHeader {
    std::string id;
    std::string version;
    std::string revision;
    std::string toolID;
    std::string nameStructure;
    std::string text;
    struct HistoryItem {
        std::string version, revision, when, who, what, why;
    };
    std::vector<HistoryItem> history;
};

struct SclModel {
    std::string version;         // SCL @version
    std::string revision;        // SCL @revision
    SclHeader header;            // SCL/Header
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

// --- Vue « équipements depuis IEDs » (pour cas sans <Substation>)
struct EquipmentFromIED {
    std::string iedName;
    std::string ldInst;
    std::string prefix;
    std::string lnClass;
    std::string lnInst;
    std::vector<std::string> primaryAnchors; // ex: "SS:VL:BAY:CE:DISCONNECTOR_1"

    // NEW: descriptions récupérées
    std::string lnDesc;   // description du LN
    std::string ldDesc;   // description du LD parent

    // Helper: description combinée
    std::string combinedDesc() const {
        if (!lnDesc.empty() && !ldDesc.empty()) return lnDesc + " — " + ldDesc;
        return !lnDesc.empty() ? lnDesc : ldDesc;
    }
};

} // namespace scl
