#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <memory>

// Dépend de sclLib (monodossier scl/)
#include "SclManager.h"    // depuis scl/
#include "Result.h"         // depuis scl/

namespace sld {

// --- Catégories de noeuds et équipements
enum class NodeKind { ConnectivityNode, Bus, Equipment, Junction };

enum class EquipmentKind {
    Unknown, CB, DS, ES, CT, VT, PT, Transformer, Line, Cable, BusbarSection
};

inline const char* toString(NodeKind k) {
    switch (k) {
    case NodeKind::ConnectivityNode: return "ConnectivityNode";
    case NodeKind::Bus: return "Bus";
    case NodeKind::Equipment: return "Equipment";
    case NodeKind::Junction: return "Junction";
    }
    return "?";
}
inline const char* toString(EquipmentKind k) {
    switch (k) {
    case EquipmentKind::Unknown: return "Unknown";
    case EquipmentKind::CB: return "CB";
    case EquipmentKind::DS: return "DS";
    case EquipmentKind::ES: return "ES";
    case EquipmentKind::CT: return "CT";
    case EquipmentKind::VT: return "VT";
    case EquipmentKind::PT: return "PT";
    case EquipmentKind::Transformer: return "Transformer";
    case EquipmentKind::Line: return "Line";
    case EquipmentKind::Cable: return "Cable";
    case EquipmentKind::BusbarSection: return "BusbarSection";
    }
    return "?";
}

// --- Identifiants stables (string)
using NodeId = std::string;
using EdgeId = std::string;

// --- Nœud générique
struct Node {
    NodeId id;                // ex: "CN:SS/VL/BAY/CN1" ou "CE:SS/VL/BAY/Q0" ou "BUS:SS/VL/cluster#1"
    NodeKind kind {NodeKind::ConnectivityNode};
    EquipmentKind eKind {EquipmentKind::Unknown}; // pertinent si Equipment

    // Contexte SCL
    std::string ssName;
    std::string vlName;
    std::string bayName;

    std::string label;        // affichage (ex: CE name, CN name, Bus name)

    // Références SCL (non-owning, valides tant que SclModel vit)
    const scl::ConductingEquipment* ce {nullptr};
    const scl::ConnectivityNode*   cn {nullptr};

    // Liens logiques (indices de LN)
    std::vector<scl::LNodeRef> lnodes; // copie pour accès simple GUI
};

enum class EdgeKind { CE_to_CN, Equip_to_Bus, CN_Merge };

struct Edge {
    EdgeId id;       // ex: "E:CE:...->CN:..."
    NodeId from;
    NodeId to;
    EdgeKind kind {EdgeKind::CE_to_CN};

    // Informations Terminal/CN utiles au debug/GUI
    std::string terminalName;
    std::string cnPath;
};

struct Graph {
    std::unordered_map<NodeId, Node> nodes; // id -> node
    std::vector<Edge> edges;                // liste ordonnée
};

// --- Bus cluster (agrège plusieurs CN)
struct BusCluster {
    std::string ssName;
    std::string vlName;
    std::vector<NodeId> cnMembers;  // CN ids du bus
    NodeId busNodeId;               // id du Node (kind=Bus) créé dans le graphe condensé
    std::string label;              // nom pour l’affichage
};

// --- Feeder (chaîne depuis un bus)
struct Feeder {
    std::string id;         // BUSID + index
    std::string ssName;
    std::string vlName;
    NodeId busId;           // origine
    std::vector<NodeId> chain; // séquence d’Equipment nodes (dans l’ordre depuis le bus)
    std::string endpointType;  // Line/Transformer/Cable/Unknown
    int laneIndex {0};         // pour layout horizontal
};

// --- Coupler (CB/DS entre 2 bus d’un même VL)
struct BusCoupler {
    NodeId couplerEquipId;   // CE id
    NodeId busA;
    NodeId busB;
    bool   isBreaker {false};
    std::string ssName;
    std::string vlName;
};

// --- Lien transformateur (entre bus, possiblement VL différents)
struct TransformerLink {
    NodeId transformerId;    // CE id
    NodeId busA;             // si déterminable
    NodeId busB;             // si déterminable
    std::string ssA, vlA;
    std::string ssB, vlB;
};

// sld/Plan.h (structure JSON déjà utilisée côté QML)
struct PlanTransformer {
    std::string id;       // "TR:Sub1/T4"
    std::string ss;
    std::vector<std::string> buses; // bus IDs reliés (1..3)
    std::string label;    // "T4"
    bool hasTapChanger = false;
};

// --- Résultat SLD déjà condensé
struct SldPlan {
    Graph graph;                   // graphe condensé (Bus + Equipment)
    std::unordered_map<std::string, std::vector<NodeId>> rankTopBus;   // key=SS:VL
    std::unordered_map<std::string, std::vector<NodeId>> rankMiddleEq; // key=SS:VL

    // Enrichissements
    std::vector<BusCluster>  buses;       // clusters détectés
    std::vector<Feeder>      feeders;
    std::vector<BusCoupler>  couplers;
    std::vector<TransformerLink> transformers;
    std::vector<PlanTransformer> plan_transformers; // << NEW
};




// --- Paramétrage heuristiques
struct HeuristicsConfig {
    // CN de bus si degree >= threshold
    int busDegreeThreshold {3};
    // Motifs probables dans nom CN (insensible à la casse)
    std::vector<std::string> busNameHints {"BUS", "BUSBAR", "BB", "BARRE", "BAR"};
    // Profondeur max de recherche d’un feeder
    int feederMaxDepth {16};
    // Types considérés « transitifs » dans une chaîne feeder (on continue à travers)
    std::vector<EquipmentKind> seriesPassKinds { EquipmentKind::DS, EquipmentKind::CB, EquipmentKind::CT, EquipmentKind::VT };
    // Types terminaux probables d’un feeder
    std::vector<EquipmentKind> endpointKinds { EquipmentKind::Line, EquipmentKind::Cable, EquipmentKind::Transformer };
};

} // namespace sld
