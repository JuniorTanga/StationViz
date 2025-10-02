#include "SldBuilder.h"
#include "JsonWriter.h"
#include <cctype>
#include <cerrno>
#include <iostream> //pour le debuggage à enlever après
#include <map>
#include <queue>
#include <sstream>

#include "SldBuilder.h"
#include <unordered_set>
#include <algorithm>

using namespace sld;

namespace {

// Renvoie le dernier segment après le dernier '/' (utile en fallback par cNodeName).
static std::string lastSegment(const std::string& s) {
    if (s.empty()) return s;
    auto p = s.find_last_of('/');
    return (p == std::string::npos) ? s : s.substr(p + 1);
}

// Recherche si un cluster contient un CN donné (NodeId exact).
static bool clusterHasCN_exact(const BusCluster& cl, const NodeId& cnId) {
    // sldTypes.h: BusCluster::cnMembers est un std::vector<NodeId>
    return std::find(cl.cnMembers.begin(), cl.cnMembers.end(), cnId) != cl.cnMembers.end();
}

// Fallback: recherche un CN par nom *suffixe* (match sur le dernier segment) et même Substation.
static std::optional<const BusCluster*> findClusterByCnNameSuffix(
    const std::vector<BusCluster>& clusters,
    const std::string& ssName,
    const std::string& cnNameSuffix)
{
    for (const auto& cl : clusters) {
        if (cl.ssName != ssName) continue;
        for (const auto& cnId : cl.cnMembers) {
            if (lastSegment(cnId) == cnNameSuffix)
                return &cl;
        }
    }
    return std::nullopt;
}

// Index rapide: busId -> (ssName, vlName) (pratique pour remplir Feeder.vlName)
struct BusIndex {
    std::unordered_map<NodeId, std::pair<std::string,std::string>> byBusId;
    explicit BusIndex(const std::vector<BusCluster>& clusters) {
        for (const auto& cl : clusters) {
            byBusId.emplace(cl.busNodeId, std::make_pair(cl.ssName, cl.vlName));
        }
    }
};



} // namespace

static std::string makeTransformerNodeId(const std::string& ss, const std::string& trName) {
    return "TR:" + ss + "/" + trName;
}


static std::string baseNameFromPath(const std::string& absPath) {
    auto pos = absPath.find_last_of('/');
    return pos == std::string::npos ? absPath : absPath.substr(pos+1);
}
static void splitAbsCNPath(const std::string& p, std::string& ss, std::string& vl, std::string& bay, std::string& name) {
    // attendu: "SS/VL/BAY/CNNAME" (min 4 segments)
    size_t a=0, b=p.find('/');
    std::string segs[4]; int i=0;
    while (b!=std::string::npos && i<3) { segs[i++] = p.substr(a, b-a); a=b+1; b=p.find('/', a); }
    segs[i++] = p.substr(a);
    ss = (i>0?segs[0]:""); vl=(i>1?segs[1]:""); bay=(i>2?segs[2]:""); name=(i>3?segs[3]:"");
}
static inline std::string keyNameVL(const std::string& ss, const std::string& vl, const std::string& name) {
    return ss + "|" + vl + "|" + name;
}
static inline std::string keyAbs(const std::string& absPath) { return absPath; }

// ID canonique d’un CN quand on connaît le chemin absolu SCL
static inline NodeId makeCNIdFromAbs(const std::string& absPath) {
    return std::string("CN:") + absPath;
}

SldBuilder::SldBuilder(const scl::SclModel *model, const HeuristicsConfig &cfg)
    : model_(model), cfg_(cfg) {}

static std::string joinPath4(const std::string &a, const std::string &b,
                             const std::string &c, const std::string &d) {
    std::string s;
    s.reserve(a.size() + b.size() + c.size() + d.size() + 3);
    s += a;
    s.push_back('/');
    s += b;
    s.push_back('/');
    s += c;
    s.push_back('/');
    s += d;
    return s;
}

NodeId SldBuilder::makeCNId(const std::string &ss, const std::string &vl,
                            const std::string &bay, const std::string &cnPath) {
    return std::string("CN:") + ss + "/" + vl + "/" + bay + "/" + cnPath;
}
NodeId SldBuilder::makeCEId(const std::string &ss, const std::string &vl,
                            const std::string &bay, const std::string &ce) {
    return std::string("CE:") + ss + "/" + vl + "/" + bay + "/" + ce;
}

std::string SldBuilder::upper(std::string s) {
    for (char &c : s)
        c = (char)std::toupper((unsigned char)c);
    return s;
}
std::string SldBuilder::keyVL(const std::string &ss, const std::string &vl) {
    return ss + ":" + vl;
}

static std::string trimU(std::string s) {
    auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'; };
    size_t a=0,b=s.size();
    while (a<b && issp((unsigned char)s[a])) ++a;
    while (b>a && issp((unsigned char)s[b-1])) --b;
    s = s.substr(a,b-a);
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

EquipmentKind SldBuilder::mapEquipmentKind(const std::string &ceType) {
    auto t = trimU(ceType);

    // synonymes / LN hints fréquents
    if (t == "CBR" || t == "CB" || t == "BREAKER" || t == "XCBR")
        return EquipmentKind::CB;
    if (t == "DIS" || t == "DS" || t == "DISCONNECTOR" || t == "XSWI" ||
        t == "SWITCH")
        return EquipmentKind::DS;
    if (t == "ES" || t == "EARTHSWITCH" || t == "EGND")
        return EquipmentKind::ES;

    if (t == "CTR" || t == "CT" || t == "TCTR" || t == "CURRENTTRANSFORMER")
        return EquipmentKind::CT;
    if (t == "VTR" || t == "VT" || t == "PT" || t == "TVTR" ||
        t == "VOLTAGETRANSFORMER")
        return EquipmentKind::VT;

    if (t == "PTR" || t == "POWERTRANSFORMER" || t == "TRF" || t == "TRANSFORMER")
        return EquipmentKind::Transformer;

    if (t == "LINE" || t == "FEEDER")
        return EquipmentKind::Line;
    if (t == "CABLE")
        return EquipmentKind::Cable;
    if (t == "BUSBAR" || t == "BUSBARSECTION" || t == "BBS")
        return EquipmentKind::BusbarSection;

    return EquipmentKind::Unknown;
}

bool SldBuilder::isLikelyBusCN(const std::string &nameOrPath,
                               int degree) const {
    auto u = upper(nameOrPath);
    if (degree >= cfg_.busDegreeThreshold)
        return true;
    for (const auto &hint : cfg_.busNameHints) {
        if (u.find(hint) != std::string::npos)
            return true;
    }
    return false;
}

scl::Status SldBuilder::buildRaw(Graph& out) const {
    if (!model_) return scl::Status(scl::Error{scl::ErrorCode::LogicError, "SclModel is null"});
    out.nodes.clear(); out.edges.clear();

    // Index CN existants par (abs path) et par (SS,VL,NAME) pour canoniser
    struct CNIdx { NodeId id; const scl::ConnectivityNode* cn; std::string absPath; };
    std::unordered_map<std::string, CNIdx> byAbs;     // key = "SS/VL/BAY/NAME"
    std::unordered_map<std::string, CNIdx> byNameVL;  // key = "SS|VL|NAME" (première occurrence)

    // 1) Pass: créer tous les CN déclarés dans les Bays + index
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                for (const auto& cn : bay.connectivityNodes) {
                    // chemin absolu officiel si présent, sinon fabriqué
                    const std::string abs = !cn.pathName.empty()
                                                ? cn.pathName
                                                : (ss.name + "/" + vl.name + "/" + bay.name + "/" + cn.name);
                    NodeId id = makeCNIdFromAbs(abs);
                    Node n;
                    n.id = id; n.kind = NodeKind::ConnectivityNode;
                    n.label = cn.name.empty() ? baseNameFromPath(abs) : cn.name;
                    n.ssName = ss.name; n.vlName = vl.name; n.bayName = bay.name; n.cn = &cn;
                    out.nodes.emplace(id, n);

                    CNIdx idx{ id, &cn, abs };
                    byAbs.emplace(keyAbs(abs), idx);
                    // enregistre le premier (SS,VL,NAME)
                    const std::string kNVL = keyNameVL(ss.name, vl.name, n.label);
                    if (!byNameVL.count(kNVL)) byNameVL.emplace(kNVL, idx);
                }
            }
        }
    }

    // helper: obtenir (et au besoin créer) un CN par nom dans (SS,VL)
    auto ensureCNByNameVL = [&](const std::string& ss, const std::string& vl,
                                const std::string& bay, const std::string& name)->NodeId {
        const std::string kNVL = keyNameVL(ss, vl, name);
        auto it = byNameVL.find(kNVL);
        if (it != byNameVL.end()) return it->second.id;
        // créer un CN « local » dans ce bay (fallback)
        const std::string abs = ss + "/" + vl + "/" + bay + "/" + name;
        NodeId id = makeCNIdFromAbs(abs);
        Node n;
        n.id = id; n.kind = NodeKind::ConnectivityNode; n.label = name;
        n.ssName = ss; n.vlName = vl; n.bayName = bay; n.cn = nullptr; // synthétique
        out.nodes.emplace(id, n);
        CNIdx idx{ id, nullptr, abs };
        byAbs.emplace(keyAbs(abs), idx);
        byNameVL.emplace(kNVL, idx);
        return id;
    };

    // 2) Pass: créer CE et arêtes CE->CN en résolvant les CN de façon canonique
    for (const auto& ss : model_->substations) {
        for (const auto& vl : ss.vlevels) {
            for (const auto& bay : vl.bays) {
                // CE node
                for (const auto& ce : bay.equipments) {
                    NodeId ceId = makeCEId(ss.name, vl.name, bay.name, ce.name);
                    Node n; n.id = ceId; n.kind = NodeKind::Equipment; n.label = ce.name;
                    n.ssName = ss.name; n.vlName = vl.name; n.bayName = bay.name; n.ce = &ce;
                    n.eKind = mapEquipmentKind(ce.type);
                    n.lnodes = ce.lnodes; // copie
                    out.nodes.emplace(ceId, n);

                    for (const auto& t : ce.terminals) {
                        NodeId cnId;
                        // 1) priorité au connectivityNodeRef (chemin absolu)
                        if (!t.connectivityNodeRef.empty()) {
                            auto it = byAbs.find(keyAbs(t.connectivityNodeRef));
                            if (it != byAbs.end()) {
                                cnId = it->second.id;
                            } else {
                                // CN non déclaré dans <ConnectivityNode> — on le crée synthétiquement à partir du chemin
                                std::string ss2, vl2, bay2, name2;
                                splitAbsCNPath(t.connectivityNodeRef, ss2, vl2, bay2, name2);
                                NodeId nid = makeCNIdFromAbs(t.connectivityNodeRef);
                                Node cnN; cnN.id = nid; cnN.kind = NodeKind::ConnectivityNode;
                                cnN.label = !name2.empty() ? name2 : baseNameFromPath(t.connectivityNodeRef);
                                cnN.ssName = ss2; cnN.vlName = vl2; cnN.bayName = bay2;
                                out.nodes.emplace(nid, cnN);
                                CNIdx idx{ nid, nullptr, t.connectivityNodeRef };
                                byAbs.emplace(keyAbs(t.connectivityNodeRef), idx);
                                byNameVL.emplace(keyNameVL(ss2, vl2, cnN.label), idx);
                                cnId = nid;
                            }
                        }
                        // 2) sinon, cNodeName → on tente de résoudre dans (SS,VL)
                        else if (!t.cNodeName.empty()) {
                            cnId = ensureCNByNameVL(ss.name, vl.name, bay.name, t.cNodeName);
                        } else {
                            continue; // terminal non câblé
                        }

                        Edge e; e.from = ceId; e.to = cnId; e.kind = EdgeKind::CE_to_CN;
                        e.id = std::string("E:") + ceId + "->" + cnId; e.terminalName = t.name;
                        e.cnPath = !t.connectivityNodeRef.empty() ? t.connectivityNodeRef : t.cNodeName;
                        out.edges.push_back(std::move(e));
                    }
                }
            }
        }
    }

    return scl::Status::Ok();
}

SldBuilder::RawView SldBuilder::buildRawView_(const Graph &raw) const {
    RawView v;
    for (const auto &e : raw.edges)
        if (e.kind == EdgeKind::CE_to_CN) {
            v.ceToCN[e.from].push_back(e.to);
            v.cnToCE[e.to].push_back(e.from);
        }
    return v;
}

scl::Status
SldBuilder::clusterAndCondense(const Graph &raw, Graph &out,
                               std::vector<BusCluster> &clusters) const {
    out.nodes.clear();
    out.edges.clear();
    clusters.clear();
    // copier équipements tels quels
    for (const auto &kv : raw.nodes) {
        const Node &n = kv.second;
        if (n.kind == NodeKind::Equipment)
            out.nodes.emplace(n.id, n);
    }

    // Degré CN et bus-likeness
    RawView v = buildRawView_(raw);
    std::unordered_map<NodeId, int> degree;
    for (const auto &kv : v.cnToCE)
        degree[kv.first] = (int)kv.second.size();

    // Heuristique bus: degree/nom + présence d’un BusbarSection adjacente
    std::unordered_set<NodeId> cnIsBus;
    for (const auto &kv : raw.nodes) {
        const Node &n = kv.second;
        if (n.kind != NodeKind::ConnectivityNode)
            continue;
        std::string nameOrPath =
            (n.cn && !n.cn->pathName.empty()) ? n.cn->pathName : n.label;
        int deg = degree[n.id];
        bool busy = isLikelyBusCN(nameOrPath, deg);
        if (!busy) {
            // si un CE de type BusbarSection touche ce CN, le marquer bus
            auto it = v.cnToCE.find(n.id);
            if (it != v.cnToCE.end()) {
                for (const auto &ceId : it->second) {
                    auto ne = raw.nodes.find(ceId);
                    if (ne != raw.nodes.end() &&
                        ne->second.eKind == EquipmentKind::BusbarSection) {
                        busy = true;
                        break;
                    }
                }
            }
        }
        if (busy)
            cnIsBus.insert(n.id);
    }

    // Union-Find des CN bus connectés par BusbarSection ou par DS intra-VL
    DSU dsu;
    auto ensure = [&](const NodeId &x) {
        if (!dsu.p.count(x))
            dsu.p[x] = x;
    };

    // init
    for (const auto &id : cnIsBus)
        ensure(id);

    // lier via CE de type BusbarSection
    for (const auto &kv : raw.nodes) {
        const Node &n = kv.second;
        if (n.kind != NodeKind::Equipment)
            continue;
        if (n.eKind == EquipmentKind::BusbarSection ||
            n.eKind == EquipmentKind::DS) {
            // prendre les CN voisins marqués bus
            std::vector<NodeId> cnList;
            auto it = v.ceToCN.find(n.id);
            if (it != v.ceToCN.end()) {
                for (const auto &cnId : it->second)
                    if (cnIsBus.count(cnId))
                        cnList.push_back(cnId);
            }
            // union entre tous les CN bus connectés par ce CE
            for (size_t i = 1; i < cnList.size(); ++i)
                dsu.u(cnList[0], cnList[i]);
        }
    }

    // Former les clusters (par SS/VL)
    std::unordered_map<std::string, BusCluster> acc; // key = rootId
    for (const auto &id : cnIsBus) {
        NodeId r = dsu.f(id);
        const Node &cnNode = raw.nodes.at(id);
        std::string key = r; // cluster key
        auto &cl = acc[key];
        if (cl.cnMembers.empty()) {
            cl.ssName = cnNode.ssName;
            cl.vlName = cnNode.vlName;

            if (cl.label.empty()) {
                std::string base = "BUS";
                if (!cl.cnMembers.empty()) {
                    const auto& firstCnId = cl.cnMembers.front(); // "CN:SS/VL/BAY/NAME"
                    auto pos = firstCnId.find_last_of('/');
                    base = (pos==std::string::npos) ? firstCnId : firstCnId.substr(pos+1);
                }
                cl.label = cl.vlName + "-" + base; // ex : "E1-BB1", "E1-BB2", "M1-BB1"
            }

        }
        cl.cnMembers.push_back(id);
    }


    // Créer les nodes Bus et mapping CN->Bus
    std::unordered_map<NodeId, NodeId> cnToBus;
    int clusterIdx = 1;
    for (auto& kv : acc) {
        auto& cl = kv.second;

        // Calcule un label parlant à partir du 1er CN membre (toujours)
        std::string base = "BUS";
        if (!cl.cnMembers.empty()) {
            const auto& firstCnId = cl.cnMembers.front(); // "CN:SS/VL/BAY/NAME"
            auto pos = firstCnId.find_last_of('/');
            base = (pos == std::string::npos) ? firstCnId : firstCnId.substr(pos + 1);
        }
        cl.label = cl.vlName + "-" + base;  // ex: "E1-BB1", "E1-BB2", "M1-BB1"

        cl.busNodeId = std::string("BUS:") + cl.ssName + "/" + cl.vlName + "/cluster#" + std::to_string(clusterIdx++);

        Node b;
        b.id = cl.busNodeId;
        b.kind = NodeKind::Bus;
        b.label = cl.label;                 // <--- label final ici
        b.ssName = cl.ssName;
        b.vlName = cl.vlName;
        out.nodes.emplace(b.id, std::move(b));

        for (const auto& cnId : cl.cnMembers)
            cnToBus[cnId] = cl.busNodeId;

        clusters.push_back(cl);
    }

    // Reconnecter CE -> Bus quand CN côté CE est dans un cluster bus
    for (const auto &e : raw.edges)
        if (e.kind == EdgeKind::CE_to_CN) {
            auto it = cnToBus.find(e.to);
            if (it != cnToBus.end()) {
                Edge ne;
                ne.kind = EdgeKind::Equip_to_Bus;
                ne.from = e.from;
                ne.to = it->second;
                ne.id = std::string("E:") + ne.from + "->" + ne.to;
                ne.terminalName = e.terminalName;
                ne.cnPath = e.cnPath;
                out.edges.push_back(std::move(ne));
            }
        }

    return scl::Status::Ok();
}

void SldBuilder::detectCouplers_(const Graph &condensed,
                                 const std::vector<BusCluster> &clusters,
                                 std::vector<BusCoupler> &out) const {
    // map bus node ids by VL for quick check
    std::unordered_map<std::string, std::unordered_set<NodeId>> vlBuses;
    for (const auto &cl : clusters)
        vlBuses[keyVL(cl.ssName, cl.vlName)].insert(cl.busNodeId);

    // Build CE->Bus adjacency from condensed edges
    std::unordered_map<NodeId, std::unordered_set<NodeId>> ceToBus;
    for (const auto &e : condensed.edges)
        if (e.kind == EdgeKind::Equip_to_Bus)
            ceToBus[e.from].insert(e.to);

    for (const auto &kv : condensed.nodes) {
        const Node &n = kv.second;
        if (n.kind != NodeKind::Equipment)
            continue;
        auto it = ceToBus.find(n.id);
        if (it == ceToBus.end())
            continue;
        // coupler si CE (CB/DS) touche deux bus distincts du même VL
        if (n.eKind == EquipmentKind::CB || n.eKind == EquipmentKind::DS) {
            const auto &buses = it->second;
            if (buses.size() >= 2) {
                // vérifier même VL
                bool allSameVL = true;
                std::string key = keyVL(n.ssName, n.vlName);
                for (const auto &b : buses) {
                    if (!vlBuses[key].count(b)) {
                        allSameVL = false;
                        break;
                    }
                }

                // debug temporaire
                std::cerr << "[COUPLER?] " << n.id << " buses=";
                for (auto& bId : buses) std::cerr << bId << " ";
                std::cerr << " | keyVL=" << key << " ok=" << allSameVL << "\n";


                if (allSameVL) {
                    auto bi = buses.begin();
                    NodeId bA = *bi++;
                    NodeId bB = *bi; // prendre deux
                    out.push_back(BusCoupler{n.id, bA, bB, (n.eKind == EquipmentKind::CB),
                                             n.ssName, n.vlName});
                }
            }
        }
    }
}

void SldBuilder::detectFeeders_(const Graph &raw, const Graph &condensed,
                                const std::vector<BusCluster> &clusters,
                                std::vector<Feeder> &out) const {
    // Build adjacency raw for traversal
    RawView v = buildRawView_(raw);

    // map CN->Bus for quick avoid-back-to-bus
    std::unordered_map<NodeId, NodeId> cnToBus;
    for (const auto &cl : clusters)
        for (const auto &cn : cl.cnMembers)
            cnToBus[cn] = cl.busNodeId;

    // Build CE->Bus mapping from condensed
    std::unordered_map<NodeId, std::vector<NodeId>> ceToBus;
    for (const auto &e : condensed.edges)
        if (e.kind == EdgeKind::Equip_to_Bus)
            ceToBus[e.from].push_back(e.to);

    // Helper lambdas
    auto isPass = [&](EquipmentKind k) {
        for (auto x : cfg_.seriesPassKinds)
            if (x == k)
                return true;
        return false;
    };
    auto isEnd = [&](EquipmentKind k) {
        for (auto x : cfg_.endpointKinds)
            if (x == k)
                return true;
        return false;
    };

    // For each CE connected to a bus, try to trace outward as a linear chain
    int feederCounter = 1;
    std::unordered_set<NodeId> usedStart; // avoid duplicates

    for (const auto &kv : condensed.nodes) {
        const Node &ceNode = kv.second;
        if (ceNode.kind != NodeKind::Equipment)
            continue;
        auto itB = ceToBus.find(ceNode.id);
        if (itB == ceToBus.end())
            continue; // not starting at bus

        // avoid bus couplers as feeders; they will be handled elsewhere
        if (ceNode.eKind == EquipmentKind::CB ||
            ceNode.eKind == EquipmentKind::DS) {
            // it might be coupler; skip from feeder start if touches 2+ buses
            if (itB->second.size() >= 2)
                continue;
        }

        // do a directed walk away from bus side
        // gather CN neighbors in raw
        auto itCNs = v.ceToCN.find(ceNode.id);
        if (itCNs == v.ceToCN.end())
            continue;
        // pick a CN that is NOT a bus CN as outward direction
        NodeId startCN;
        for (const auto &cnId : itCNs->second)
            if (!cnToBus.count(cnId)) {
                startCN = cnId;
                break;
            }
        if (startCN.empty())
            continue; // all CNs were bus — likely coupler or busbar

        // BFS/DFS linear walk
        std::unordered_set<NodeId> visitedCE;
        std::unordered_set<NodeId> visitedCN;
        visitedCE.insert(ceNode.id);
        visitedCN.insert(startCN);
        std::vector<NodeId> chain;
        chain.push_back(ceNode.id);
        NodeId currCN = startCN;
        int depth = 0;
        EquipmentKind endpointK = EquipmentKind::Unknown;
        while (depth++ < cfg_.feederMaxDepth) {
            // step: CN -> CE (excluding ones already in chain and excluding CE that
            // returns to bus exclusively)
            auto itCEs = v.cnToCE.find(currCN);
            if (itCEs == v.cnToCE.end())
                break;
            NodeId nextCE;
            for (const auto &cand : itCEs->second) {
                if (visitedCE.count(cand))
                    continue;
                // if cand connects to a bus and cand != first CE, stop (branch back to
                // bus)
                if (ceToBus.count(cand) && cand != chain.front())
                    continue;
                nextCE = cand;
                break;
            }
            if (nextCE.empty())
                break;
            visitedCE.insert(nextCE);
            chain.push_back(nextCE);
            const Node &nx = raw.nodes.at(nextCE);
            if (isEnd(nx.eKind)) {
                endpointK = nx.eKind;
                break;
            }
            // find next CN (not the one we came from, and not a bus CN)
            auto itCN2s = v.ceToCN.find(nextCE);
            if (itCN2s == v.ceToCN.end())
                break;
            NodeId nextCN;
            for (const auto &cn2 : itCN2s->second) {
                if (visitedCN.count(cn2))
                    continue;
                if (cnToBus.count(cn2))
                    continue;
                nextCN = cn2;
                break;
            }
            if (nextCN.empty())
                break;
            visitedCN.insert(nextCN);
            currCN = nextCN;
            if (!isPass(nx.eKind)) {
                // If non-pass kind encountered and not endpoint, still proceed but mark
                // as potential end
            }
        }

        // Si on n'a pas pu avancer et que le premier CE est déjà un endpoint (ex: Transformer)
        if (chain.size()==1 && endpointK==EquipmentKind::Unknown) {
            const Node& start = raw.nodes.at(chain.front());
            // marquer Transformer/Line/etc. comme endpoint
            for (auto x : cfg_.endpointKinds)
                if (x == start.eKind) { endpointK = start.eKind; break; }
        }


        if (chain.size() >= 1) {
            Feeder f;
            f.busId = itB->second.front();
            f.ssName = ceNode.ssName;
            f.vlName = ceNode.vlName;
            f.chain = chain;
            f.endpointType = toString(endpointK);
            f.id = std::string("FEED:") + f.busId + "#" +
                   std::to_string(feederCounter++);

            out.push_back(std::move(f));
        }
    }

    // assign lane indices per (SS:VL, bus)
    std::unordered_map<std::string, int> laneCounter;
    for (auto &f : out) {
        std::string key = keyVL(f.ssName, f.vlName) + "|" + f.busId;
        f.laneIndex = laneCounter[key]++;
    }
}

void SldBuilder::detectTransformers_(const Graph &raw,
                                     const std::vector<BusCluster> &clusters,
                                     std::vector<TransformerLink> &out) const {
    // Build CN->Bus map and CE->CN adjacency
    RawView v = buildRawView_(raw);
    std::unordered_map<NodeId, NodeId> cnToBus;
    for (const auto &cl : clusters)
        for (const auto &cn : cl.cnMembers)
            cnToBus[cn] = cl.busNodeId;

    // For each transformer CE, see buses on its terminals
    for (const auto &kv : raw.nodes) {
        const Node &n = kv.second;
        if (n.kind != NodeKind::Equipment || n.eKind != EquipmentKind::Transformer)
            continue;
        auto it = v.ceToCN.find(n.id);
        if (it == v.ceToCN.end())
            continue;
        std::unordered_set<NodeId> buses;
        for (const auto &cn : it->second) {
            auto itb = cnToBus.find(cn);
            if (itb != cnToBus.end())
                buses.insert(itb->second);
        }
        if (buses.size() >= 2) {
            auto bi = buses.begin();
            NodeId bA = *bi++;
            NodeId bB = *bi; // pick two
            TransformerLink tl;
            tl.transformerId = n.id;
            tl.busA = bA;
            tl.busB = bB;
            // déduire VL depuis nodes
            const Node &a = raw.nodes.at(bA);
            const Node &b = raw.nodes.at(bB);
            tl.ssA = a.ssName;
            tl.vlA = a.vlName;
            tl.ssB = b.ssName;
            tl.vlB = b.vlName;
            out.push_back(std::move(tl));
        }
    }
}

// petit utilitaire: existe dans un set/vector ?
static bool containsCN(const std::unordered_set<NodeId>& S, const NodeId& id) {
    return S.find(id) != S.end();
}
static bool containsCN(const std::vector<NodeId>& V, const NodeId& id) {
    return std::find(V.begin(), V.end(), id) != V.end();
}

// Adapter ici SI ton BusCluster n'expose pas cnIds :
template <typename ClusterT>
static bool clusterContainsCN(const ClusterT& c, const NodeId& cn) {
    // Essayez cnIds (unordered_set)
    if constexpr (std::is_same_v<decltype(c.cnMembers), std::unordered_set<NodeId>>) {
        return containsCN(c.cnMembers, cn);
    } else {
        // Ou cnIds en vector
        return containsCN(c.cnMembers, cn);
    }
}

void SldBuilder::integratePowerTransformers_(const std::vector<BusCluster>& clusters,
                                             SldPlan& out) const
{
    if (!model_) return;

    // Index bus -> (ss, vl) pour compléter Feeder.vlName proprement
    BusIndex busIndex{clusters};

    // Évite de créer deux fois le même feeder "Transformer" pour (bus, transfo)
    std::unordered_set<std::string> dedup;

    // Pour compter localement les feeders par transfo (unique ID stable)
    std::unordered_map<std::string, int> perTrCounter; // key = busId + "|" + trName

    // Parcourt toutes les Substations et PowerTransformer
    for (const auto& ss : model_->substations) {
        const std::string& ssName = ss.name;

        // Si le modèle SCL n’a pas (encore) powerTransformers, on saute.
        if (ss.powerTransformers.empty())
            continue;

        for (const auto& pt : ss.powerTransformers) {
            const std::string trNodeId = makeTransformerNodeId(ssName, pt.name);
            std::vector<std::string> busesForTr;
            bool hasTapChanger = false;

            // --- Collecte des “ends” (chaque winding possède 1..n terminaux)
            for (const auto& w : pt.windings) {
                if (w.tapChanger) hasTapChanger = true;

                // Deux cas :
                //  (A) Tu as ajouté w.resolvedEnds (ss, vl, bay, cn) en post-parse -> on l'utilise
                //  (B) Sinon fallback via Terminal.cNodeName / connectivityNode
                if (!w.resolvedEnds.empty()) {
                    for (const auto& re : w.resolvedEnds) {
                        // CN NodeId exact si on a ss/vl/bay/cn
                        NodeId cnId = makeCNId(re.ss, re.vl, re.bay, re.cn);

                        // On cherche le cluster (bus) qui contient ce CN exact.
                        const BusCluster* busCl = nullptr;
                        for (const auto& cl : clusters) {
                            if (clusterHasCN_exact(cl, cnId)) { busCl = &cl; break; }
                        }

                        // Fallback: si pas trouvé, et si on a au moins un nom de CN, essai sur suffixe
                        if (!busCl) {
                            auto opt = findClusterByCnNameSuffix(clusters, ssName, re.cn);
                            if (opt) busCl = *opt;
                        }

                        if (!busCl) continue;

                        const std::string& busId = busCl->busNodeId;
                        // Dédoublonner les feeders "TR" pour ce (bus, transfo)
                        std::string key = busId + "|" + pt.name;
                        int count = ++perTrCounter[key];
                        const std::string feederId = "FEED:" + busId + "#TR#" + pt.name + "#" + std::to_string(count);

                        if (dedup.insert(feederId).second) {
                            Feeder f;
                            f.id = feederId;
                            f.ssName = ssName;
                            // VL depuis l’index bus si possible, sinon l’adresse résolue
                            auto itBus = busIndex.byBusId.find(busId);
                            f.vlName = (itBus != busIndex.byBusId.end()) ? itBus->second.second : re.vl;
                            f.busId = busId;
                            f.chain.clear();
                            f.chain.push_back(trNodeId);
                            f.endpointType = "Transformer";
                            f.laneIndex = 0; // le layout fera la répartition
                            out.feeders.push_back(std::move(f));
                        }

                        if (std::find(busesForTr.begin(), busesForTr.end(), busId) == busesForTr.end())
                            busesForTr.push_back(busId);
                    }
                } else {
                    // --- Fallback si resolvedEnds non disponible : on utilise les Terminals
                    for (const auto& t : w.terminals) {
                        // 1) On tente de parser directement le NodeId exact si tu as bay/vl dans le chemin
                        std::string cnName = t.cNodeName;
                        std::string guessVL, guessBay;

                        // Si ton parseur SCL expose déjà t.connectivityPath:
                        // format ".../<VL>/<BAY>/<CN>"
                        if (!t.connectivityPath.empty()) {
                            // Récupérer les 3 derniers segments
                            std::vector<std::string> segs;
                            std::string cur;
                            for (char c : t.connectivityPath) {
                                if (c == '/') { if (!cur.empty()) { segs.push_back(cur); cur.clear(); } }
                                else cur.push_back(c);
                            }
                            if (!cur.empty()) segs.push_back(cur);
                            if (segs.size() >= 3) {
                                cnName  = segs.back();
                                guessBay = segs[segs.size() - 2];
                                guessVL  = segs[segs.size() - 3];
                            }
                        }

                        NodeId cnId;
                        if (!guessVL.empty() && !guessBay.empty() && !cnName.empty())
                            cnId = makeCNId(ssName, guessVL, guessBay, cnName);

                        const BusCluster* busCl = nullptr;
                        if (!cnId.empty()) {
                            for (const auto& cl : clusters) {
                                if (clusterHasCN_exact(cl, cnId)) { busCl = &cl; break; }
                            }
                        }
                        if (!busCl && !cnName.empty()) {
                            auto opt = findClusterByCnNameSuffix(clusters, ssName, cnName);
                            if (opt) busCl = *opt;
                        }
                        if (!busCl) continue;

                        const std::string& busId = busCl->busNodeId;
                        std::string key = busId + "|" + pt.name;
                        int count = ++perTrCounter[key];
                        const std::string feederId = "FEED:" + busId + "#TR#" + pt.name + "#" + std::to_string(count);

                        if (dedup.insert(feederId).second) {
                            Feeder f;
                            f.id = feederId;
                            f.ssName = ssName;
                            auto itBus = busIndex.byBusId.find(busId);
                            f.vlName = (itBus != busIndex.byBusId.end()) ? itBus->second.second : guessVL;
                            f.busId = busId;
                            f.chain = { makeTransformerNodeId(ssName, pt.name) };
                            f.endpointType = "Transformer";
                            f.laneIndex = 0;
                            out.feeders.push_back(std::move(f));
                        }
                        if (std::find(busesForTr.begin(), busesForTr.end(), busId) == busesForTr.end())
                            busesForTr.push_back(busId);
                    }
                }
            }

            // Résumé consolidé dans plan_transformers (utile pour rendu inter-bus futur)
            if (!busesForTr.empty()) {
                PlanTransformer p;
                p.id = trNodeId;
                p.ss = ssName;
                p.label = pt.name;
                p.hasTapChanger = hasTapChanger;
                p.buses = std::move(busesForTr);
                out.plan_transformers.push_back(std::move(p));
            }
        }
    }
}


SldPlan SldBuilder::makePlan(const Graph &condensed,
                             const std::vector<BusCluster> &clusters) const {
    SldPlan plan;
    plan.graph = condensed;
    plan.buses = clusters;

    // après avoir rempli plan.buses
    std::sort(plan.buses.begin(), plan.buses.end(),
              [](const BusCluster &a, const BusCluster &b) {
        if (a.vlName != b.vlName)
            return a.vlName < b.vlName;
        return a.label < b.label;
              });

    // Classement simple par VL
    for (const auto &kv : condensed.nodes) {
        const Node &n = kv.second;
        std::string key = keyVL(n.ssName, n.vlName);
        if (n.kind == NodeKind::Bus)
            plan.rankTopBus[key].push_back(n.id);
        else if (n.kind == NodeKind::Equipment)
            plan.rankMiddleEq[key].push_back(n.id);
    }

    // Couplers
    detectCouplers_(condensed, clusters, plan.couplers);

    // Feeders (basés sur graphe brut pour la marche détaillée)
    // On reconstruit rapidement un raw équivalent au condensed ? Non, on requiert
    // l'appelant d’en fournir un si nécessaire. Ici, on reconstruit raw minimal:
    // on n’a pas ici 'raw'; l’appelant passera par SldManager qui possède raw.

    integratePowerTransformers_(clusters, plan);

    return plan;
}

std::string SldBuilder::toJson(const Graph& g) const {
    JsonWriter w;
    w.beginObject();

    // nodes
    w.key("nodes").beginArray();
    for (const auto& kv : g.nodes) {
        const Node& n = kv.second;
        w.beginObject();
        w.key("id").value(n.id);
        w.key("kind").value(toString(n.kind));
        if (!n.label.empty()) w.key("label").value(n.label);
        if (!n.ssName.empty()) w.key("ss").value(n.ssName);
        if (!n.vlName.empty()) w.key("vl").value(n.vlName);
        if (!n.bayName.empty()) w.key("bay").value(n.bayName);
        if (n.kind == NodeKind::Equipment)
            w.key("eKind").value(toString(n.eKind));
        w.endObject();
    }
    w.endArray();

    // edges
    w.key("edges").beginArray();
    for (const auto& e : g.edges) {
        w.beginObject();
        w.key("id").value(e.id);
        w.key("from").value(e.from);
        w.key("to").value(e.to);
        w.key("kind").value(edgeKindToString(e.kind));
        if (!e.terminalName.empty()) w.key("terminal").value(e.terminalName);
        if (!e.cnPath.empty())       w.key("cn").value(e.cnPath);
        w.endObject();
    }
    w.endArray();

    w.endObject();
    return w.str();
}

std::string SldBuilder::planToJson(const SldPlan& p) const {
    JsonWriter w;
    w.beginObject();

    // buses
    w.key("buses").beginArray();
    for (const auto& b : p.buses) {
        w.beginObject();
        w.key("id").value(b.busNodeId);
        if (!b.ssName.empty()) w.key("ss").value(b.ssName);
        if (!b.vlName.empty()) w.key("vl").value(b.vlName);
        if (!b.label.empty())  w.key("label").value(b.label);

        w.key("members").beginArray();
        for (const auto& m : b.cnMembers) w.value(m);
        w.endArray();

        w.endObject();
    }
    w.endArray();

    // couplers
    w.key("couplers").beginArray();
    for (const auto& c : p.couplers) {
        w.beginObject();
        w.key("equip").value(c.couplerEquipId);
        w.key("busA").value(c.busA);
        w.key("busB").value(c.busB);
        w.key("type").value(c.isBreaker ? "CB" : "DS");
        if (!c.ssName.empty()) w.key("ss").value(c.ssName);
        if (!c.vlName.empty()) w.key("vl").value(c.vlName);
        w.endObject();
    }
    w.endArray();

    // transformers
    w.key("transformers").beginArray();
    for (const auto& t : p.transformers) {
        w.beginObject();
        w.key("tr").value(t.transformerId);
        if (!t.busA.empty()) w.key("busA").value(t.busA);
        if (!t.busB.empty()) w.key("busB").value(t.busB);
        if (!t.vlA.empty())  w.key("vlA").value(t.vlA);
        if (!t.vlB.empty())  w.key("vlB").value(t.vlB);
        w.endObject();
    }
    w.endArray();

    // feeders
    w.key("feeders").beginArray();
    for (const auto& f : p.feeders) {
        w.beginObject();
        w.key("id").value(f.id);
        if (!f.busId.empty()) w.key("bus").value(f.busId);
        if (!f.ssName.empty()) w.key("ss").value(f.ssName);
        if (!f.vlName.empty()) w.key("vl").value(f.vlName);
        w.key("lane").value(f.laneIndex);
        if (!f.endpointType.empty()) w.key("endpoint").value(f.endpointType);

        w.key("chain").beginArray();
        for (const auto& n : f.chain) w.value(n);
        w.endArray();

        w.endObject();
    }
    w.endArray();

    w.endObject();
    return w.str();
}
