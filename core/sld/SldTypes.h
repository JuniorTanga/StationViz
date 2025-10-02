#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <memory>

#include <boost/graph/adjacency_list.hpp>
#include <boost/pending/disjoint_sets.hpp>

#include "SclManager.h"
#include "SldConfig.h"

namespace sld {

enum class NodeKind { ConnectivityNode, Bus, Equipment, Junction };
enum class EquipmentKind {
    Unknown, CB, DS, ES, CT, VT, PT, Transformer, Line, Cable, BusbarSection
};

inline const char* toString(NodeKind k){
    switch(k){
        case NodeKind::ConnectivityNode: return "ConnectivityNode";
        case NodeKind::Bus: return "Bus";
        case NodeKind::Equipment: return "Equipment";
        case NodeKind::Junction: return "Junction";
    }
    return "?";
}
inline const char* toString(EquipmentKind k){
    switch(k){
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

using NodeId = std::string;
using EdgeId = std::string;

// ---------- BGL vertex/edge properties ----------
struct VertexProp {
    NodeId id;
    NodeKind kind {NodeKind::ConnectivityNode};
    EquipmentKind eKind {EquipmentKind::Unknown};

    // contexte SCL
    std::string ss, vl, bay;
    std::string label;

    // refs SCL pour Equipment/CN
    const scl::ConductingEquipment* ce {nullptr};
    const scl::ConnectivityNode*   cn {nullptr};

    // LNodeRefs (pour pont Network)
    std::vector<scl::LNodeRef> lnodes;
};

enum class EdgeKind { CE_to_CN, Equip_to_Bus, CN_Merge };

struct EdgeProp {
    EdgeKind kind {EdgeKind::CE_to_CN};
    std::string id;
    std::string terminalName;
    std::string cnPath; // useful for debug
};

// Undirected raw/condensed graphs (simple pour CE<->CN)
using BoostGraph = boost::adjacency_list<
    boost::vecS, boost::vecS, boost::undirectedS,
    VertexProp, EdgeProp>;

using V = boost::graph_traits<BoostGraph>::vertex_descriptor;
using E = boost::graph_traits<BoostGraph>::edge_descriptor;

// Pour retrouver vite un sommet depuis un NodeId stable
struct Index {
    std::unordered_map<NodeId, V> nodeById;
};

// ---- Bus cluster / plan objets (compat QML)
struct BusCluster {
    std::string ss;
    std::string vl;
    std::vector<NodeId> cnMembers; // NodeId des CN membres
    NodeId busNodeId;              // id du vertex "BUS:..."
    std::string label;
};

struct Feeder {
    std::string id;      // "FEED:BUS#k"
    std::string ss, vl;
    NodeId busId;
    std::vector<NodeId> chain; // CE ids
    std::string endpointType;  // "Line"/"Transformer"/"Cable"/"Unknown"
    int laneIndex {0};
};

struct BusCoupler {
    NodeId couplerEquipId; // CE id
    NodeId busA, busB;
    bool   isBreaker {false};
    std::string ss, vl;
};

struct TransformerLink {
    NodeId transformerId;
    NodeId busA, busB;
    std::string ssA, vlA, ssB, vlB;
};

struct PlanTransformer {
    std::string id;    // "TR:SS/T4"
    std::string ss;
    std::vector<std::string> buses;
    std::string label;
    bool hasTapChanger{false};
};

struct SldPlan {
    BoostGraph condensed;
    Index      idx; // index du condensed
    std::unordered_map<std::string, std::vector<NodeId>> rankTopBus;   // key=SS:VL
    std::unordered_map<std::string, std::vector<NodeId>> rankMiddleEq; // key=SS:VL
    std::vector<BusCluster>  buses;
    std::vector<Feeder>      feeders;
    std::vector<BusCoupler>  couplers;
    std::vector<TransformerLink> transformers;
    std::vector<PlanTransformer> plan_transformers;

    // Enrichissements pour QML (pont SCL/Network)
    std::vector<scl::EquipmentFromIED> equipmentsFromIEDs;
};

} // namespace sld
