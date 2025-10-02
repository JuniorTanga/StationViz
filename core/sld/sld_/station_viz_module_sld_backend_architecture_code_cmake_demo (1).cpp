// =============================================================
// StationViz / sld/ — Module SLD (backend) — VERSION AVANCÉE
// Monofolder layout (tous .h / .cpp dans sld/)
// Lib: sldLib  |  C++17 pur (indépendant de Qt)  |  construit sur sclLib
// =============================================================
// CONTENU DU DOSSIER sld/
//   CMakeLists.txt
//   SldTypes.h
//   SldBuilder.h
//   SldBuilder.cpp
//   SldManager.h
//   SldManager.cpp
//   demo_main.cpp          (binaire optionnel : SLD_BUILD_DEMO)
//
// Principe général (version avancée)
// 1) Construire un graphe brut biparti CE↔CN via sclLib (Terminals)
// 2) Détecter et **clustériser** les CN de bus (Union-Find + indices busbar)
// 3) Condenser en graphe SLD (Noeuds: **Bus**, **Equipment**) et **classer** :
//      - **BusCouplers** (CB/DS reliant 2 bus d’un même VL)
//      - **Transformers links** (liaisons entre bus de VL potentiellement différents)
//      - **Feeders** (chaînes depuis un bus jusqu’à un endpoint: Line/Transformer/Cable/CT/VT…)
// 4) Produire un **plan** (rangs par VL + « lanes » de feeders) et des **exports JSON enrichis**
// 5) Exposer des helpers de debug (printRaw/Condensed/Feeders/Couplers/Transformers/Stats)
// =============================================================

// =============================
// File: sld/SldTypes.h
// =============================
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <memory>

// Dépend de sclLib (monodossier scl/)
#include "SclManager.h"     // depuis scl/
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

// =============================
// File: sld/SldBuilder.h
// =============================
#pragma once
#include "SldTypes.h"

namespace sld {

class SldBuilder {
public:
    explicit SldBuilder(const scl::SclModel* model, const HeuristicsConfig& cfg = {});

    // 1) Graphe biparti CE↔CN
    scl::Status buildRaw(Graph& out) const;

    // 2) Clustering des CN de bus & condensation Bus + Equipment
    scl::Status clusterAndCondense(const Graph& raw, Graph& out, std::vector<BusCluster>& clusters) const;

    // 3) Détection des couplers / feeders / transformers et plan de layout
    SldPlan makePlan(const Graph& condensed, const std::vector<BusCluster>& clusters) const;

    // JSON utilitaires
    std::string toJson(const Graph& g) const;
    std::string planToJson(const SldPlan& p) const;

    // Utils
    static EquipmentKind mapEquipmentKind(const std::string& ceType);

private:
    const scl::SclModel* model_ {nullptr};
    HeuristicsConfig cfg_{};

    // internes
    static std::string upper(std::string s);
    static std::string keyVL(const std::string& ss, const std::string& vl);

    static NodeId makeCNId(const std::string& ss, const std::string& vl,
                           const std::string& bay, const std::string& cnPathOrName);
    static NodeId makeCEId(const std::string& ss, const std::string& vl,
                           const std::string& bay, const std::string& ceName);

    bool isLikelyBusCN(const std::string& cnNameOrPath, int degree) const;

    // Union-Find pour cluster CN
    struct DSU { mutable std::unordered_map<NodeId, NodeId> p; NodeId f(const NodeId& x) const { auto it=p.find(x); if(it==p.end()||it->second==x) return it==p.end()?x:it->second; return p[x]=f(it->second);} void u(const NodeId& a,const NodeId& b){auto ra=f(a), rb=f(b); if(ra!=rb) p[ra]=rb;} };

    // Aides à partir du graphe brut
    struct RawView {
        // adjacency
        std::unordered_map<NodeId, std::vector<NodeId>> cnToCE; // CN -> CE*
        std::unordered_map<NodeId, std::vector<NodeId>> ceToCN; // CE -> CN*
    };

    RawView buildRawView_(const Graph& raw) const;

    // Détection coupler & feeders
    void detectCouplers_(const Graph& condensed,
                         const std::vector<BusCluster>& clusters,
                         std::vector<BusCoupler>& out) const;

    void detectFeeders_(const Graph& raw, const Graph& condensed,
                        const std::vector<BusCluster>& clusters,
                        std::vector<Feeder>& out) const;

    void detectTransformers_(const Graph& raw, const std::vector<BusCluster>& clusters,
                             std::vector<TransformerLink>& out) const;
};

} // namespace sld

// =============================
// File: sld/SldBuilder.cpp
// =============================
#include "SldBuilder.h"
#include <sstream>
#include <queue>
#include <cctype>

using namespace sld;

SldBuilder::SldBuilder(const scl::SclModel* model, const HeuristicsConfig& cfg)
    : model_(model), cfg_(cfg) {}

static std::string joinPath4(const std::string& a,const std::string& b,const std::string& c,const std::string& d){
    std::string s; s.reserve(a.size()+b.size()+c.size()+d.size()+3);
    s+=a; s.push_back('/'); s+=b; s.push_back('/'); s+=c; s.push_back('/'); s+=d; return s;
}

NodeId SldBuilder::makeCNId(const std::string& ss,const std::string& vl,const std::string& bay,const std::string& cnPath){
    return std::string("CN:")+ss+"/"+vl+"/"+bay+"/"+cnPath;
}
NodeId SldBuilder::makeCEId(const std::string& ss,const std::string& vl,const std::string& bay,const std::string& ce){
    return std::string("CE:")+ss+"/"+vl+"/"+bay+"/"+ce;
}

std::string SldBuilder::upper(std::string s){ for(char& c: s) c=(char)std::toupper((unsigned char)c); return s; }
std::string SldBuilder::keyVL(const std::string& ss, const std::string& vl){ return ss+":"+vl; }

EquipmentKind SldBuilder::mapEquipmentKind(const std::string& ceType){
    auto t = upper(ceType);
    if (t=="CB" || t=="BREAKER" || t=="XCBR") return EquipmentKind::CB;
    if (t=="DS" || t=="DISCONNECTOR" || t=="XSWI") return EquipmentKind::DS;
    if (t=="ES" || t=="EARTHSWITCH") return EquipmentKind::ES;
    if (t=="CT" || t=="TCTR") return EquipmentKind::CT;
    if (t=="VT" || t=="PT" || t=="TVTR") return EquipmentKind::VT;
    if (t=="PTR" || t=="POWERTRANSFORMER" || t=="TRF") return EquipmentKind::Transformer;
    if (t=="LINE") return EquipmentKind::Line;
    if (t=="CABLE") return EquipmentKind::Cable;
    if (t=="BUSBAR" || t=="BUSBARSECTION") return EquipmentKind::BusbarSection;
    return EquipmentKind::Unknown;
}

bool SldBuilder::isLikelyBusCN(const std::string& nameOrPath, int degree) const {
    auto u = upper(nameOrPath);
    if (degree >= cfg_.busDegreeThreshold) return true;
    for (const auto& hint : cfg_.busNameHints) {
        if (u.find(hint) != std::string::npos) return true;
    }
    return false;
}

scl::Status SldBuilder::buildRaw(Graph& out) const {
    if (!model_) return scl::Status(scl::Error{scl::ErrorCode::LogicError, "SclModel is null"});
    out.nodes.clear(); out.edges.clear();

    // 1) Créer noeuds CN & CE, et arêtes CE->CN via Terminals
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                // CN nodes
                for (const auto& cn : bay.connectivityNodes) {
                    NodeId id = makeCNId(ss.name, vl.name, bay.name, !cn.pathName.empty()? cn.pathName : cn.name);
                    Node n; n.id = id; n.kind = NodeKind::ConnectivityNode; n.label = cn.name;
                    n.ssName = ss.name; n.vlName = vl.name; n.bayName = bay.name; n.cn = &cn;
                    out.nodes.emplace(id, std::move(n));
                }
                // CE nodes + edges to CN per terminal
                for (const auto& ce : bay.equipments) {
                    NodeId ceId = makeCEId(ss.name, vl.name, bay.name, ce.name);
                    Node n; n.id = ceId; n.kind = NodeKind::Equipment; n.label = ce.name;
                    n.ssName = ss.name; n.vlName = vl.name; n.bayName = bay.name; n.ce = &ce;
                    n.eKind = mapEquipmentKind(ce.type);
                    n.lnodes = ce.lnodes; // copie (liaison logique)
                    out.nodes.emplace(ceId, n);

                    for (const auto& t : ce.terminals) {
                        std::string cnPath = !t.connectivityNodeRef.empty()? t.connectivityNodeRef
                                             : (!t.cNodeName.empty()? joinPath4(ss.name, vl.name, bay.name, t.cNodeName) : "");
                        if (cnPath.empty()) continue; // terminal non câblé
                        NodeId cnId = makeCNId(ss.name, vl.name, bay.name, cnPath);
                        // assurer l'existence du CN (au cas où non listé)
                        if (out.nodes.find(cnId) == out.nodes.end()) {
                            Node cnN; cnN.id = cnId; cnN.kind = NodeKind::ConnectivityNode; cnN.label = t.cNodeName;
                            cnN.ssName = ss.name; cnN.vlName = vl.name; cnN.bayName = bay.name;
                            out.nodes.emplace(cnId, std::move(cnN));
                        }
                        Edge e; e.from = ceId; e.to = cnId; e.kind = EdgeKind::CE_to_CN;
                        e.id = std::string("E:") + ceId + "->" + cnId; e.terminalName = t.name; e.cnPath = cnPath;
                        out.edges.push_back(std::move(e));
                    }
                }
            }
        }
    }

    return scl::Status::Ok();
}

SldBuilder::RawView SldBuilder::buildRawView_(const Graph& raw) const {
    RawView v;
    for (const auto& e : raw.edges) if (e.kind == EdgeKind::CE_to_CN) {
        v.ceToCN[e.from].push_back(e.to);
        v.cnToCE[e.to].push_back(e.from);
    }
    return v;
}

scl::Status SldBuilder::clusterAndCondense(const Graph& raw, Graph& out, std::vector<BusCluster>& clusters) const {
    out.nodes.clear(); out.edges.clear(); clusters.clear();
    // copier équipements tels quels
    for (const auto& kv : raw.nodes) {
        const Node& n = kv.second; if (n.kind == NodeKind::Equipment) out.nodes.emplace(n.id, n);
    }

    // Degré CN et bus-likeness
    RawView v = buildRawView_(raw);
    std::unordered_map<NodeId, int> degree; for (const auto& kv : v.cnToCE) degree[kv.first] = (int)kv.second.size();

    // Heuristique bus: degree/nom + présence d’un BusbarSection adjacente
    std::unordered_set<NodeId> cnIsBus;
    for (const auto& kv : raw.nodes) {
        const Node& n = kv.second; if (n.kind != NodeKind::ConnectivityNode) continue;
        std::string nameOrPath = (n.cn && !n.cn->pathName.empty()) ? n.cn->pathName : n.label;
        int deg = degree[n.id];
        bool busy = isLikelyBusCN(nameOrPath, deg);
        if (!busy) {
            // si un CE de type BusbarSection touche ce CN, le marquer bus
            auto it = v.cnToCE.find(n.id);
            if (it != v.cnToCE.end()) {
                for (const auto& ceId : it->second) {
                    auto ne = raw.nodes.find(ceId);
                    if (ne != raw.nodes.end() && ne->second.eKind == EquipmentKind::BusbarSection) { busy = true; break; }
                }
            }
        }
        if (busy) cnIsBus.insert(n.id);
    }

    // Union-Find des CN bus connectés par BusbarSection ou par DS intra-VL
    DSU dsu;
    auto ensure = [&](const NodeId& x){ if (!dsu.p.count(x)) dsu.p[x]=x; };

    // init
    for (const auto& id : cnIsBus) ensure(id);

    // lier via CE de type BusbarSection
    for (const auto& kv : raw.nodes) {
        const Node& n = kv.second; if (n.kind != NodeKind::Equipment) continue;
        if (n.eKind == EquipmentKind::BusbarSection || n.eKind == EquipmentKind::DS) {
            // prendre les CN voisins marqués bus
            std::vector<NodeId> cnList;
            auto it = v.ceToCN.find(n.id);
            if (it != v.ceToCN.end()) {
                for (const auto& cnId : it->second) if (cnIsBus.count(cnId)) cnList.push_back(cnId);
            }
            // union entre tous les CN bus connectés par ce CE
            for (size_t i=1;i<cnList.size();++i) dsu.u(cnList[0], cnList[i]);
        }
    }

    // Former les clusters (par SS/VL)
    std::unordered_map<std::string, BusCluster> acc; // key = rootId
    for (const auto& id : cnIsBus) {
        NodeId r = dsu.f(id);
        const Node& cnNode = raw.nodes.at(id);
        std::string key = r; // cluster key
        auto& cl = acc[key];
        if (cl.cnMembers.empty()) { cl.ssName = cnNode.ssName; cl.vlName = cnNode.vlName; cl.label = cnNode.vlName+"-BUS"; }
        cl.cnMembers.push_back(id);
    }

    // Créer les nodes Bus et mapping CN->Bus
    std::unordered_map<NodeId, NodeId> cnToBus;
    int clusterIdx = 1;
    for (auto& kv : acc) {
        auto& cl = kv.second;
        cl.busNodeId = std::string("BUS:") + cl.ssName + "/" + cl.vlName + "/cluster#" + std::to_string(clusterIdx++);
        Node b; b.id = cl.busNodeId; b.kind = NodeKind::Bus; b.label = cl.label; b.ssName = cl.ssName; b.vlName = cl.vlName;
        out.nodes.emplace(b.id, std::move(b));
        for (const auto& cnId : cl.cnMembers) cnToBus[cnId] = cl.busNodeId;
        clusters.push_back(cl);
    }

    // Reconnecter CE -> Bus quand CN côté CE est dans un cluster bus
    for (const auto& e : raw.edges) if (e.kind == EdgeKind::CE_to_CN) {
        auto it = cnToBus.find(e.to);
        if (it != cnToBus.end()) {
            Edge ne; ne.kind = EdgeKind::Equip_to_Bus; ne.from = e.from; ne.to = it->second;
            ne.id = std::string("E:") + ne.from + "->" + ne.to; ne.terminalName = e.terminalName; ne.cnPath = e.cnPath;
            out.edges.push_back(std::move(ne));
        }
    }

    return scl::Status::Ok();
}

void SldBuilder::detectCouplers_(const Graph& condensed,
                                 const std::vector<BusCluster>& clusters,
                                 std::vector<BusCoupler>& out) const {
    // map bus node ids by VL for quick check
    std::unordered_map<std::string, std::unordered_set<NodeId>> vlBuses;
    for (const auto& cl : clusters) vlBuses[keyVL(cl.ssName, cl.vlName)].insert(cl.busNodeId);

    // Build CE->Bus adjacency from condensed edges
    std::unordered_map<NodeId, std::unordered_set<NodeId>> ceToBus;
    for (const auto& e : condensed.edges) if (e.kind==EdgeKind::Equip_to_Bus) ceToBus[e.from].insert(e.to);

    for (const auto& kv : condensed.nodes) {
        const Node& n = kv.second; if (n.kind!=NodeKind::Equipment) continue;
        auto it = ceToBus.find(n.id); if (it==ceToBus.end()) continue;
        // coupler si CE (CB/DS) touche deux bus distincts du même VL
        if (n.eKind==EquipmentKind::CB || n.eKind==EquipmentKind::DS) {
            const auto& buses = it->second; if (buses.size()>=2) {
                // vérifier même VL
                bool allSameVL = true; std::string key = keyVL(n.ssName, n.vlName);
                for (const auto& b : buses) {
                    if (!vlBuses[key].count(b)) { allSameVL=false; break; }
                }
                if (allSameVL) {
                    auto bi = buses.begin(); NodeId bA = *bi++; NodeId bB = *bi; // prendre deux
                    out.push_back(BusCoupler{ n.id, bA, bB, (n.eKind==EquipmentKind::CB), n.ssName, n.vlName });
                }
            }
        }
    }
}

void SldBuilder::detectFeeders_(const Graph& raw, const Graph& condensed,
                                const std::vector<BusCluster>& clusters,
                                std::vector<Feeder>& out) const {
    // Build adjacency raw for traversal
    RawView v = buildRawView_(raw);

    // map CN->Bus for quick avoid-back-to-bus
    std::unordered_map<NodeId, NodeId> cnToBus;
    for (const auto& cl : clusters) for (const auto& cn : cl.cnMembers) cnToBus[cn]=cl.busNodeId;

    // Build CE->Bus mapping from condensed
    std::unordered_map<NodeId, std::vector<NodeId>> ceToBus;
    for (const auto& e : condensed.edges) if (e.kind==EdgeKind::Equip_to_Bus) ceToBus[e.from].push_back(e.to);

    // Helper lambdas
    auto isPass = [&](EquipmentKind k){ for (auto x: cfg_.seriesPassKinds) if (x==k) return true; return false; };
    auto isEnd  = [&](EquipmentKind k){ for (auto x: cfg_.endpointKinds) if (x==k) return true; return false; };

    // For each CE connected to a bus, try to trace outward as a linear chain
    int feederCounter = 1;
    std::unordered_set<NodeId> usedStart; // avoid duplicates

    for (const auto& kv : condensed.nodes) {
        const Node& ceNode = kv.second; if (ceNode.kind!=NodeKind::Equipment) continue;
        auto itB = ceToBus.find(ceNode.id); if (itB==ceToBus.end()) continue; // not starting at bus

        // avoid bus couplers as feeders; they will be handled elsewhere
        if (ceNode.eKind==EquipmentKind::CB || ceNode.eKind==EquipmentKind::DS) {
            // it might be coupler; skip from feeder start if touches 2+ buses
            if (itB->second.size()>=2) continue;
        }

        // do a directed walk away from bus side
        // gather CN neighbors in raw
        auto itCNs = v.ceToCN.find(ceNode.id); if (itCNs==v.ceToCN.end()) continue;
        // pick a CN that is NOT a bus CN as outward direction
        NodeId startCN; for (const auto& cnId : itCNs->second) if (!cnToBus.count(cnId)) { startCN = cnId; break; }
        if (startCN.empty()) continue; // all CNs were bus — likely coupler or busbar

        // BFS/DFS linear walk
        std::unordered_set<NodeId> visitedCE; std::unordered_set<NodeId> visitedCN; visitedCE.insert(ceNode.id); visitedCN.insert(startCN);
        std::vector<NodeId> chain; chain.push_back(ceNode.id);
        NodeId currCN = startCN;
        int depth=0; EquipmentKind endpointK = EquipmentKind::Unknown;
        while (depth++ < cfg_.feederMaxDepth) {
            // step: CN -> CE (excluding ones already in chain and excluding CE that returns to bus exclusively)
            auto itCEs = v.cnToCE.find(currCN); if (itCEs==v.cnToCE.end()) break;
            NodeId nextCE;
            for (const auto& cand : itCEs->second) {
                if (visitedCE.count(cand)) continue;
                // if cand connects to a bus and cand != first CE, stop (branch back to bus)
                if (ceToBus.count(cand) && cand!=chain.front()) continue;
                nextCE = cand; break;
            }
            if (nextCE.empty()) break;
            visitedCE.insert(nextCE);
            chain.push_back(nextCE);
            const Node& nx = raw.nodes.at(nextCE);
            if (isEnd(nx.eKind)) { endpointK = nx.eKind; break; }
            // find next CN (not the one we came from, and not a bus CN)
            auto itCN2s = v.ceToCN.find(nextCE); if (itCN2s==v.ceToCN.end()) break;
            NodeId nextCN; for (const auto& cn2 : itCN2s->second) {
                if (visitedCN.count(cn2)) continue; if (cnToBus.count(cn2)) continue; nextCN = cn2; break; }
            if (nextCN.empty()) break; visitedCN.insert(nextCN); currCN = nextCN;
            if (!isPass(nx.eKind)) {
                // If non-pass kind encountered and not endpoint, still proceed but mark as potential end
            }
        }

        if (chain.size()>=1) {
            Feeder f; f.busId = itB->second.front(); f.ssName = ceNode.ssName; f.vlName = ceNode.vlName; f.chain = chain; f.endpointType = toString(endpointK);
            f.id = std::string("FEED:") + f.busId + "#" + std::to_string(feederCounter++);
            out.push_back(std::move(f));
        }
    }

    // assign lane indices per (SS:VL, bus)
    std::unordered_map<std::string, int> laneCounter;
    for (auto& f : out) {
        std::string key = keyVL(f.ssName,f.vlName)+"|"+f.busId; f.laneIndex = laneCounter[key]++;
    }
}

void SldBuilder::detectTransformers_(const Graph& raw, const std::vector<BusCluster>& clusters,
                                     std::vector<TransformerLink>& out) const {
    // Build CN->Bus map and CE->CN adjacency
    RawView v = buildRawView_(raw);
    std::unordered_map<NodeId, NodeId> cnToBus; for (const auto& cl:clusters) for (const auto& cn:cl.cnMembers) cnToBus[cn]=cl.busNodeId;

    // For each transformer CE, see buses on its terminals
    for (const auto& kv : raw.nodes) {
        const Node& n = kv.second; if (n.kind!=NodeKind::Equipment || n.eKind!=EquipmentKind::Transformer) continue;
        auto it = v.ceToCN.find(n.id); if (it==v.ceToCN.end()) continue;
        std::unordered_set<NodeId> buses;
        for (const auto& cn : it->second) { auto itb = cnToBus.find(cn); if (itb!=cnToBus.end()) buses.insert(itb->second); }
        if (buses.size()>=2) {
            auto bi = buses.begin(); NodeId bA=*bi++; NodeId bB=*bi; // pick two
            TransformerLink tl; tl.transformerId=n.id; tl.busA=bA; tl.busB=bB;
            // déduire VL depuis nodes
            const Node& a = raw.nodes.at(bA); const Node& b = raw.nodes.at(bB);
            tl.ssA = a.ssName; tl.vlA = a.vlName; tl.ssB = b.ssName; tl.vlB = b.vlName;
            out.push_back(std::move(tl));
        }
    }
}

SldPlan SldBuilder::makePlan(const Graph& condensed, const std::vector<BusCluster>& clusters) const {
    SldPlan plan; plan.graph = condensed; plan.buses = clusters;

    // Classement simple par VL
    for (const auto& kv : condensed.nodes) {
        const Node& n = kv.second; std::string key = keyVL(n.ssName, n.vlName);
        if (n.kind == NodeKind::Bus) plan.rankTopBus[key].push_back(n.id);
        else if (n.kind == NodeKind::Equipment) plan.rankMiddleEq[key].push_back(n.id);
    }

    // Couplers
    detectCouplers_(condensed, clusters, plan.couplers);

    // Feeders (basés sur graphe brut pour la marche détaillée)
    // On reconstruit rapidement un raw équivalent au condensed ? Non, on requiert l'appelant d’en fournir un si nécessaire.
    // Ici, on reconstruit raw minimal: on n’a pas ici 'raw'; l’appelant passera par SldManager qui possède raw.

    return plan;
}

std::string SldBuilder::toJson(const Graph& g) const {
    auto esc = [](const std::string& s){ std::string o; o.reserve(s.size()+8); for(char c: s){ if(c=='"'||c=='\'){o.push_back('\');o.push_back(c);} else if(c=='
'){o+="\n";} else o.push_back(c);} return o; };
    std::ostringstream os; os << "{""nodes"":" << "[";
    bool first=true;
    for (const auto& kv : g.nodes) {
        const Node& n = kv.second; if(!first) os << ","; first=false;
        os << "{""id"":"""<<esc(n.id) <<""",""kind"":"""<<esc(toString(n.kind))<<""",""label"":"""<<esc(n.label)
           <<""",""ss"":"""<<esc(n.ssName)<<""",""vl"":"""<<esc(n.vlName)<<""",""bay"":"""<<esc(n.bayName)<<"""";
        if (n.kind==NodeKind::Equipment) { os << ",""eKind"":"""<<esc(toString(n.eKind))<<""""; }
        os << "}";
    }
    os << "],""edges"":" << "[";
    first=true;
    for (const auto& e : g.edges) { if(!first) os << ","; first=false;
        os << "{""id"":"""<<esc(e.id)<<""",""from"":"""<<esc(e.from)<<""",""to"":"""<<esc(e.to)<<""",""kind"":""";
        switch(e.kind){ case EdgeKind::CE_to_CN: os<<"CE_to_CN"; break; case EdgeKind::Equip_to_Bus: os<<"Equip_to_Bus"; break; case EdgeKind::CN_Merge: os<<"CN_Merge"; break; }
        os << """,""terminal"":"""<<esc(e.terminalName)<<""",""cn"":"""<<esc(e.cnPath)<<"""}";
    }
    os << "]}";
    return os.str();
}

std::string SldBuilder::planToJson(const SldPlan& p) const {
    auto esc = [](const std::string& s){ std::string o; o.reserve(s.size()+8); for(char c: s){ if(c=='"'||c=='\'){o.push_back('\');o.push_back(c);} else if(c=='
'){o+="\n";} else o.push_back(c);} return o; };
    std::ostringstream os; os << "{""buses"":"<<"[";
    bool first=true;
    for (const auto& b : p.buses) { if(!first) os << ","; first=false;
        os << "{""id"":"""<<esc(b.busNodeId)<<""",""ss"":"""<<esc(b.ssName)<<""",""vl"":"""<<esc(b.vlName)<<""",""label"":"""<<esc(b.label)<<""",""members"":"<<"[";
        bool f2=true; for (const auto& m : b.cnMembers){ if(!f2) os << ","; f2=false; os << '"'<<esc(m)<<'"'; } os << "]}";
    }
    os << "],""couplers"":"<<"["; first=true;
    for (const auto& c : p.couplers){ if(!first) os<<","; first=false; os<<"{""equip"":"""<<esc(c.couplerEquipId)<<""",""busA"":"""<<esc(c.busA)<<""",""busB"":"""<<esc(c.busB)<<""",""type"":"""<<(c.isBreaker?"CB":"DS")<<"""}"; }
    os << "],""transformers"":"<<"["; first=true;
    for (const auto& t : p.transformers){ if(!first) os<<","; first=false; os<<"{""tr"":"""<<esc(t.transformerId)<<""",""busA"":"""<<esc(t.busA)<<""",""busB"":"""<<esc(t.busB)<<""",""vlA"":"""<<esc(t.vlA)<<""",""vlB"":"""<<esc(t.vlB)<<"""}"; }
    os << "],""feeders"":"<<"["; first=true;
    for (const auto& f : p.feeders){ if(!first) os<<","; first=false; os<<"{""id"":"""<<esc(f.id)<<""",""bus"":"""<<esc(f.busId)<<""",""vl"":"""<<esc(f.vlName)<<""",""lane"":"<<f.laneIndex<<",""endpoint"":"""<<esc(f.endpointType)<<""",""chain"":"<<"["; bool f3=true; for(const auto& n: f.chain){ if(!f3) os<<","; f3=false; os<<'"'<<esc(n)<<'"'; } os<<"]}"; }
    os << "]}";
    return os.str();
}

// =============================
// File: sld/SldManager.h
// =============================
#pragma once
#include "SldTypes.h"

namespace sld {

class SldManager {
public:
    SldManager(const scl::SclModel* model, HeuristicsConfig cfg = {});

    // Constructions
    scl::Status build(); // remplit raw_, clusters_, condensed_, plan_ (incl. feeders/couplers/transformers)

    // Accès
    const Graph& rawGraph() const { return raw_; }
    const Graph& condensedGraph() const { return condensed_; }
    const SldPlan& plan() const { return plan_; }

    // Debug helpers
    scl::Status printRaw() const;         // CE -> CN
    scl::Status printCondensed() const;   // Equip -> Bus
    scl::Status printFeeders() const;
    scl::Status printCouplers() const;
    scl::Status printTransformers() const;
    scl::Status printStats() const;

    // JSON
    std::string rawJson() const { return builder_.toJson(raw_); }
    std::string condensedJson() const { return builder_.toJson(condensed_); }
    std::string planJson() const { return builder_.planToJson(plan_); }

private:
    const scl::SclModel* model_ {nullptr};
    HeuristicsConfig cfg_{};
    SldBuilder builder_;

    Graph raw_;
    std::vector<BusCluster> clusters_;
    Graph condensed_;
    SldPlan plan_;
};

} // namespace sld

// =============================
// File: sld/SldManager.cpp
// =============================
#include "SldManager.h"
#include <iostream>

using namespace sld;

SldManager::SldManager(const scl::SclModel* model, HeuristicsConfig cfg)
    : model_(model), cfg_(cfg), builder_(model, cfg) {}

scl::Status SldManager::build() {
    auto st = builder_.buildRaw(raw_);
    if (!st) return st;
    st = builder_.clusterAndCondense(raw_, condensed_, clusters_);
    if (!st) return st;
    plan_ = builder_.makePlan(condensed_, clusters_);
    // compléter plan_ avec feeders & transformers (besoin du graphe raw pour la marche)
    builder_.detectFeeders_(raw_, condensed_, clusters_, plan_.feeders);
    builder_.detectTransformers_(raw_, clusters_, plan_.transformers);
    return scl::Status::Ok();
}

scl::Status SldManager::printRaw() const {
    if (raw_.nodes.empty()) return scl::Status(scl::Error{scl::ErrorCode::LogicError, "raw graph is empty"});
    std::cout << "[RAW] Nodes=" << raw_.nodes.size() << ", Edges=" << raw_.edges.size() << "
";
    for (const auto& kv : raw_.nodes) {
        const Node& n = kv.second;
        std::cout << "  N " << n.id << "  kind=" << toString(n.kind) << "  label=" << n.label
                  << "  ctx=" << n.ssName << "/" << n.vlName << "/" << n.bayName;
        if (n.kind==NodeKind::Equipment) std::cout << "  eKind=" << toString(n.eKind);
        std::cout << "
";
    }
    for (const auto& e : raw_.edges) {
        std::cout << "  E " << e.id << "  (" << e.from << ") -> (" << e.to << ")  term=" << e.terminalName << "  cn=" << e.cnPath << "
";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printCondensed() const {
    if (condensed_.nodes.empty()) return scl::Status(scl::Error{scl::ErrorCode::LogicError, "condensed graph is empty"});
    std::cout << "[CONDENSED] Nodes=" << condensed_.nodes.size() << ", Edges=" << condensed_.edges.size() << "
";
    for (const auto& kv : condensed_.nodes) {
        const Node& n = kv.second;
        std::cout << "  N " << n.id << "  kind=" << toString(n.kind) << "  label=" << n.label
                  << "  ctx=" << n.ssName << "/" << n.vlName << "/" << n.bayName;
        if (n.kind==NodeKind::Equipment) std::cout << "  eKind=" << toString(n.eKind);
        std::cout << "
";
    }
    for (const auto& e : condensed_.edges) {
        std::cout << "  E " << e.id << "  (" << e.from << ") -> (" << e.to << ")  kind=Equip_to_Bus  term=" << e.terminalName << "
";
    }
    std::cout << "  Buses: " << clusters_.size() << "
";
    return scl::Status::Ok();
}

scl::Status SldManager::printFeeders() const {
    if (plan_.feeders.empty()) { std::cout << "[FEEDERS] none
"; return scl::Status::Ok(); }
    std::cout << "[FEEDERS] count=" << plan_.feeders.size() << "
";
    for (const auto& f : plan_.feeders) {
        std::cout << "  " << f.id << "  VL=" << f.vlName << "  bus=" << f.busId << "  lane=" << f.laneIndex << "  endpoint=" << f.endpointType << "
    chain: ";
        for (const auto& n : f.chain) std::cout << n << " -> "; std::cout << "END
";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printCouplers() const {
    if (plan_.couplers.empty()) { std::cout << "[COUPLERS] none
"; return scl::Status::Ok(); }
    std::cout << "[COUPLERS] count=" << plan_.couplers.size() << "
";
    for (const auto& c : plan_.couplers) {
        std::cout << "  equip=" << c.couplerEquipId << "  (" << (c.isBreaker?"CB":"DS") << ")  "
                  << c.busA << " <-> " << c.busB << "  VL=" << c.vlName << "
";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printTransformers() const {
    if (plan_.transformers.empty()) { std::cout << "[TRANSFORMERS] none
"; return scl::Status::Ok(); }
    std::cout << "[TRANSFORMERS] count=" << plan_.transformers.size() << "
";
    for (const auto& t : plan_.transformers) {
        std::cout << "  tr=" << t.transformerId << "  " << t.busA << " (" << t.vlA << ") <=> " << t.busB << " (" << t.vlB << ")
";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printStats() const {
    std::cout << "[STATS] rawNodes=" << raw_.nodes.size() << ", rawEdges=" << raw_.edges.size()
              << ", condensedNodes=" << condensed_.nodes.size() << ", condensedEdges=" << condensed_.edges.size()
              << ", buses=" << clusters_.size() << ", feeders=" << plan_.feeders.size() << ", couplers=" << plan_.couplers.size() << ", transformers=" << plan_.transformers.size() << "
";
    return scl::Status::Ok();
}

// =============================
// File: sld/CMakeLists.txt
// =============================
cmake_minimum_required(VERSION 3.20)
project(sldLib LANGUAGES CXX)

option(SLD_BUILD_DEMO "Build SLD demo executable" ON)

add_library(sldLib
    SldBuilder.cpp
    SldManager.cpp
)

# Headers publics (monofolder)
target_include_directories(sldLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(sldLib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Dépend de sclLib (assurez add_subdirectory(scl) avant add_subdirectory(sld))
target_link_libraries(sldLib PUBLIC sclLib)

# Warnings
if (MSVC)
    target_compile_options(sldLib PRIVATE /W4)
else()
    target_compile_options(sldLib PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (SLD_BUILD_DEMO)
    add_executable(sld_demo demo_main.cpp)
    target_link_libraries(sld_demo PRIVATE sldLib)
    target_compile_features(sld_demo PRIVATE cxx_std_17)
endif()

// =============================
// File: sld/demo_main.cpp
// =============================
#include <iostream>
#include "SclManager.h"  // sclLib
#include "SldManager.h"  // sldLib

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.scl|.scd|.icd>
";
        return 1;
    }
    const std::string path = argv[1];

    // Charger SCL
    scl::SclManager sclMgr;
    auto st = sclMgr.loadScl(path);
    if (!st) { std::cerr << "Failed to load SCL: " << st.error().message << "
"; return 2; }

    // Construire SLD (version avancée)
    sld::HeuristicsConfig cfg; // ajustez au besoin
    sld::SldManager sldMgr(sclMgr.model(), cfg);
    st = sldMgr.build();
    if (!st) { std::cerr << "Failed to build SLD: " << st.error().message << "
"; return 3; }

    // Debug étendu
    std::cout << "
=== RAW CE->CN ===
"; sldMgr.printRaw();
    std::cout << "
=== CONDENSED Equip->Bus ===
"; sldMgr.printCondensed();
    std::cout << "
=== FEEDERS ===
"; sldMgr.printFeeders();
    std::cout << "
=== COUPLERS ===
"; sldMgr.printCouplers();
    std::cout << "
=== TRANSFORMERS ===
"; sldMgr.printTransformers();
    std::cout << "
=== STATS ===
"; sldMgr.printStats();

    // JSON enrichi (plan)
    std::cout << "
=== PLAN JSON ===
" << sldMgr.planJson() << "
";

    return 0;
}
