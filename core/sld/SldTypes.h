#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>

#include "SclManager.h"

namespace sld {

enum class EquipmentKind {
    Unknown, CB, DS, ES, CT, VT, PT, Transformer, Line, Cable, BusbarSection
};

const char* toString(EquipmentKind k);
EquipmentKind equipmentKindFromSclType(const std::string& t);

// ---------- Station topology ----------
//
// Derived from the SCL Bay structure rather than inferred from a graph. Every
// ConductingEquipment belongs to a Bay, and that grouping is the topology:
//
//   - a Bay with no ConductingEquipment is a busbar;
//   - a Bay with equipment and exactly one terminal reaching outside the Bay is
//     a feeder, and that terminal names the busbar it hangs off;
//   - a Bay with equipment and exactly two external terminals is a coupler;
//   - a PowerTransformer winding terminal names the Bay it sits in, and that Bay
//     names its busbar.
//
// This replaces a CE<->CN graph plus heuristics. See docs/PLAN.md section 4 for
// why the inference approach could not work: flattening a station into a graph
// discards the Bay grouping, after which a busbar and a bay junction have
// identical neighbourhoods.

// A ConnectivityNode referenced from a terminal, resolved to its owning Bay.
// Terminals carry either @connectivityNode (a full pathName) or @cNodeName (a
// bare name scoped to the Bay). CN short names repeat across Bays, so a bare
// name is only ever resolved within the owning Bay.
struct Ref {
    std::string bay;
    std::string cn;
    bool ok {false};
};

// Stable, human-meaningful identifiers. The UI persists these in QSettings, so
// they must be deterministic across runs and independent of iteration order.
using NodeId = std::string;

NodeId makeBusId(const std::string& ss, const std::string& vl, const std::string& bay);
NodeId makeEquipId(const std::string& ss, const std::string& vl,
                   const std::string& bay, const std::string& name);

struct Bus {
    NodeId id;
    std::string ss, vl;
    std::string label;         // bay name
    std::string bay;
    std::vector<std::string> cnPaths;   // CN pathNames inside this bay
};

// The ordered role chain of one bay's equipment, resolved by walking the bay's
// own CE<->CN graph outward from its bus-side terminal. Ordering is therefore a
// fact read from the SCL, not a guess from equipment kinds.
struct FeederRoles {
    std::string busSideDisconnector;  // first DS/CB seen from the bus side
    std::string breaker;
    std::string currentTransformer;
    std::string voltageTransformer;
    std::string lineSideDisconnector;
    std::string endpoint;             // Line/Cable name
    std::string endpointKind;         // "Line"/"Cable"/"CT"/""
};

struct Feeder {
    NodeId id;
    std::string ss, vl, bay;
    NodeId busId;                      // busbar this feeder hangs off
    std::string busLabel;
    std::vector<std::string> chain;    // ordered equipment ids, bus side first
    FeederRoles roles;
    std::string endpointType;
    int laneIndex {0};
};

struct BusCoupler {
    NodeId couplerEquipId;
    NodeId busA, busB;
    bool isBreaker {false};
    std::string ss, vl, bay;
};

// One link per winding, so a three-winding transformer yields three links
// rather than being forced into an (busA, busB) pair it does not have.
struct TransformerWinding {
    std::string winding;
    std::string ss, vl;
    NodeId bus;
    std::string busLabel;
};

struct TransformerLink {
    NodeId transformerId;              // "TR:SS/T1"
    std::string ss, label;
    bool hasTapChanger {false};
    std::vector<TransformerWinding> windings;
    // Buses touched, sorted, for a quick membership test in the UI.
    std::vector<NodeId> buses;
};

struct PlanTransformer {
    std::string id;
    std::string ss, vl, label;
    std::vector<NodeId> buses;
    bool hasTapChanger {false};
};

// One row of the flat node list the UI lays out. Kind is "Bus", "Equipment" or
// "Junction" so AppContext's existing branches keep working.
struct NodeRecord {
    std::string id, ss, vl, bay, label;
    std::string kind;    // NodeKind as a string
    std::string eKind;   // EquipmentKind as a string, empty for Bus/Junction
};

struct SldPlan {
    std::vector<NodeRecord> nodes;     // buses + equipment, in stable order
    std::vector<Bus>   buses;
    std::vector<Feeder> feeders;
    std::vector<BusCoupler> couplers;
    std::vector<TransformerLink> transformers;
    std::vector<PlanTransformer> plan_transformers;
    std::vector<scl::EquipmentFromIED> equipmentsFromIEDs;

    // Populated when the SCL could not be interpreted; surfaced to the UI
    // instead of silently producing an empty diagram.
    std::vector<std::string> warnings;
};

} // namespace sld