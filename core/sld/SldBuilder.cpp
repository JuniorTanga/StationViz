#include "SldBuilder.h"
#include <algorithm>
#include <queue>
#include <set>

using nlohmann::json;
using namespace sld;

static std::string lastSeg(const std::string& s){
    auto p = s.find_last_of('/');
    return (p==std::string::npos)? s : s.substr(p+1);
}
static std::string keyAbs(const std::string& ss, const std::string& vl,
                          const std::string& bay, const std::string& name){
    return ss+"/"+vl+"/"+bay+"/"+name;
}

SldBuilder::SldBuilder(const scl::SclModel* model, const HeuristicsConfig& cfg)
    : model_(model), cfg_(cfg) {}

// ---- utils
std::string SldBuilder::keyVL(const std::string& ss, const std::string& vl){
    return ss+":"+vl;
}
std::string SldBuilder::makeCNIdAbs(const std::string& ss, const std::string& vl,
                                    const std::string& bay, const std::string& name){
    return "CN:"+ss+"/"+vl+"/"+bay+"/"+name;
}
std::string SldBuilder::makeCEId(const std::string& ss, const std::string& vl,
                                 const std::string& bay, const std::string& ce){
    return "CE:"+ss+"/"+vl+"/"+bay+"/"+ce;
}
std::string SldBuilder::makeBusId(const std::string& ss, const std::string& vl, int n){
    return "BUS:"+ss+"/"+vl+"/cluster#"+std::to_string(n);
}
std::string SldBuilder::upper(std::string s){
    for(char& c:s) c = (char)std::toupper((unsigned char)c);
    return s;
}
bool SldBuilder::isLikelyBusCN(const std::string& nameOrPath, int degree) const{

    const int hardMin = 4;
    const int thr = std::max(cfg_.busDegreeThreshold, hardMin);
    if (degree >= thr) return true;

    // Name hints still allowed, but require at least degree 2 (not leaf CN).
    auto u = upper(nameOrPath);
    bool hint = false;
    for (const auto& h : cfg_.busNameHints) {
        if (u.find(h) != std::string::npos) { hint = true; break; }
    }
    if (hint && degree >= 2) return true;
    return false;
}

V SldBuilder::ensureVertex(BoostGraph& g, Index& idx, const VertexProp& vp){
    auto it = idx.nodeById.find(vp.id);
    if (it != idx.nodeById.end()) return it->second;
    V v = boost::add_vertex(g);
    g[v] = vp;
    idx.nodeById.emplace(vp.id, v);
    return v;
}
std::optional<V> SldBuilder::findVertex(const Index& idx, const NodeId& id){
    auto it = idx.nodeById.find(id);
    if (it==idx.nodeById.end()) return std::nullopt;
    return it->second;
}

EquipmentKind SldBuilder::mapEquipmentKind(const std::string& ceType){
    auto up = upper(ceType);
    if (up=="CBR" || up=="CB" || up=="BREAKER" || up=="XCBR") return EquipmentKind::CB;
    if (up=="DIS" || up=="DS" || up=="DISCONNECTOR" || up=="XSWI" || up=="SWITCH") return EquipmentKind::DS;
    if (up=="ES" || up=="EARTHSWITCH" || up=="EGND") return EquipmentKind::ES;
    if (up=="CTR" || up=="CT" || up=="TCTR" || up=="CURRENTTRANSFORMER") return EquipmentKind::CT;
    if (up=="VTR" || up=="VT" || up=="PT" || up=="TVTR" || up=="VOLTAGETRANSFORMER") return EquipmentKind::VT;
    if (up=="PTR" || up=="POWERTRANSFORMER" || up=="TRANSFORMER") return EquipmentKind::Transformer;
    if (up=="LINE" || up=="FEEDER") return EquipmentKind::Line;
    if (up=="CABLE") return EquipmentKind::Cable;
    if (up=="BUSBAR" || up=="BUSBARSECTION" || up=="BBS") return EquipmentKind::BusbarSection;
    return EquipmentKind::Unknown;
}

// -------- 1) buildRaw --------
scl::Status SldBuilder::buildRaw(BoostGraph& g, Index& idx) const {
    g.clear(); idx.nodeById.clear();
    if (!model_) return scl::Status(scl::Error{scl::ErrorCode::LogicError,"SclModel is null"});

    // 1. Créer CN nodes connus
    std::unordered_map<std::string, NodeId> absToId;
    for (const auto& ss : model_->substations){
        for (const auto& vl : ss.vlevels){
            for (const auto& bay : vl.bays){
                for (const auto& cn : bay.connectivityNodes){
                    const std::string abs = !cn.pathName.empty() ?
                        cn.pathName : keyAbs(ss.name, vl.name, bay.name, cn.name);
                    VertexProp vp;
                    vp.id   = "CN:"+abs;
                    vp.kind = NodeKind::ConnectivityNode;
                    vp.label= cn.name.empty()? lastSeg(abs):cn.name;
                    vp.ss=ss.name; vp.vl=vl.name; vp.bay=bay.name; vp.cn=&cn;
                    ensureVertex(g, idx, vp);
                    absToId.emplace(abs, vp.id);
                }
            }
        }
    }

    auto ensureCN = [&](const std::string& ss, const std::string& vl,
                        const std::string& bay, const std::string& name)->NodeId{
        auto abs = keyAbs(ss,vl,bay,name);
        auto it = absToId.find(abs);
        if (it!=absToId.end()) return it->second;
        VertexProp vp;
        vp.id = makeCNIdAbs(ss,vl,bay,name);
        vp.kind = NodeKind::ConnectivityNode;
        vp.label = name;
        vp.ss=ss; vp.vl=vl; vp.bay=bay; vp.cn=nullptr; // synthétique
        ensureVertex(g, idx, vp);
        absToId.emplace(abs, vp.id);
        return vp.id;
    };

    // 2. Créer CE + arêtes CE->CN
    for (const auto& ss : model_->substations){
        for (const auto& vl : ss.vlevels){
            for (const auto& bay : vl.bays){
                for (const auto& ce : bay.equipments){
                    VertexProp ev;
                    ev.id = makeCEId(ss.name,vl.name,bay.name,ce.name);
                    ev.kind = NodeKind::Equipment;
                    ev.eKind = mapEquipmentKind(ce.type);
                    ev.label = ce.name; ev.ss=ss.name; ev.vl=vl.name; ev.bay=bay.name; ev.ce=&ce;
                    ev.lnodes = ce.lnodes; // pont Network
                    V vCE = ensureVertex(g, idx, ev);

                    for (const auto& t : ce.terminals){
                        std::string cnId;
                        if (!t.connectivityNodeRef.empty()){
                            // t.connectivityNodeRef = "SS/VL/BAY/CN"
                            cnId = "CN:"+t.connectivityNodeRef;
                            // créer si inconnu
                            if (!findVertex(idx, cnId)){
                                std::string ss2,vl2,bay2,name2;
                                // split
                                auto s = t.connectivityNodeRef;
                                size_t a=0; size_t b=s.find('/');
                                std::vector<std::string> segs;
                                while (b!=std::string::npos){ segs.push_back(s.substr(a,b-a)); a=b+1; b=s.find('/',a); }
                                segs.push_back(s.substr(a));
                                if (segs.size()>=4){
                                    VertexProp vp;
                                    vp.id=cnId; vp.kind=NodeKind::ConnectivityNode;
                                    vp.ss=segs[0]; vp.vl=segs[1]; vp.bay=segs[2];
                                    vp.label=segs[3]; vp.cn=nullptr;
                                    ensureVertex(g, idx, vp);
                                }
                            }
                        } else if (!t.cNodeName.empty()){
                            cnId = ensureCN(ss.name, vl.name, bay.name, t.cNodeName);
                        } else { continue; }

                        auto vCNopt = findVertex(idx, cnId);
                        if (!vCNopt) continue;
                        V vCN = *vCNopt;

                        auto [e,ok] = boost::add_edge(vCE, vCN, g);
                        g[e].kind = EdgeKind::CE_to_CN;
                        g[e].id = "E:"+ev.id+"->"+cnId;
                        g[e].terminalName = t.name;
                        g[e].cnPath = !t.connectivityNodeRef.empty() ? t.connectivityNodeRef : t.cNodeName;
                    }
                }
            }
        }
    }

    return scl::Status::Ok();
}

SldBuilder::RawAdj SldBuilder::buildAdj(const BoostGraph& raw, const Index& idx) const{
    RawAdj r;
    for (auto eIt = edges(raw); eIt.first != eIt.second; ++eIt.first){
        E e = *eIt.first;
        V a = source(e, raw);
        V b = target(e, raw);
        const auto& va = raw[a];
        const auto& vb = raw[b];
        if (raw[e].kind != EdgeKind::CE_to_CN) continue;
        if (va.kind==NodeKind::Equipment && vb.kind==NodeKind::ConnectivityNode){
            r.ceToCN[va.id].push_back(vb.id);
            r.cnToCE[vb.id].push_back(va.id);
        } else if (vb.kind==NodeKind::Equipment && va.kind==NodeKind::ConnectivityNode){
            r.ceToCN[vb.id].push_back(va.id);
            r.cnToCE[va.id].push_back(vb.id);
        }
    }
    return r;
}

// -------- 2) cluster + condense --------
scl::Status SldBuilder::clusterAndCondense(const BoostGraph& raw, const Index& rawIdx,
                                           BoostGraph& out, Index& idxOut,
                                           std::vector<BusCluster>& clusters) const {
    out.clear(); idxOut.nodeById.clear(); clusters.clear();

    // copier equipments
    for (auto vIt = vertices(raw); vIt.first != vIt.second; ++vIt.first){
        V v = *vIt.first;
        const auto& pv = raw[v];
        if (pv.kind == NodeKind::Equipment){
            ensureVertex(out, idxOut, pv);
        }
    }

    // degree CN + bus-likeness
    RawAdj adj = buildAdj(raw, rawIdx);
    std::unordered_map<NodeId,int> degree;
    for (const auto& kv : adj.cnToCE) degree[kv.first] = (int)kv.second.size();

    std::unordered_set<NodeId> isBusCN;
    for (auto vIt = vertices(raw); vIt.first != vIt.second; ++vIt.first){
        V v = *vIt.first;
        const auto& pv = raw[v];
        if (pv.kind != NodeKind::ConnectivityNode) continue;
        std::string name = pv.cn && !pv.cn->pathName.empty() ? pv.cn->pathName : pv.label;
        if (isLikelyBusCN(name, degree[pv.id])){
            isBusCN.insert(pv.id);
        }else{
            // heuristique: CN adjacent à BusbarSection -> bus
            auto itC = adj.cnToCE.find(pv.id);
            if (itC != adj.cnToCE.end()){
                for (const auto& ceId : itC->second){
                    auto vCE = findVertex(rawIdx, ceId);
                    if (vCE && raw[*vCE].eKind == EquipmentKind::BusbarSection){
                        isBusCN.insert(pv.id); break;
                    }
                }
            }
        }
    }

    // DSU par (SS:VL) — ne clusterise pas à travers les VL
    // On regroupe d’abord par SS:VL
    std::unordered_map<std::string, std::vector<NodeId>> busCNByVL;
    for (const auto& cnId : isBusCN){
        auto vCN = *findVertex(rawIdx, cnId);
        const auto& pv = raw[vCN];
        busCNByVL[keyVL(pv.ss, pv.vl)].push_back(cnId);
    }

    int clusterIndex = 1;
    for (auto& [kvl, cnList] : busCNByVL){
        // DSU indexation 0..n-1
        std::unordered_map<NodeId,int> idx;
        std::vector<int> parent(cnList.size());
        for (size_t i=0;i<cnList.size();++i){ idx[cnList[i]] = (int)i; parent[i]=(int)i; }
        auto findp = [&](int x){ while(parent[x]!=x) x=parent[x]=parent[parent[x]]; return x; };
        auto uni = [&](int a,int b){ a=findp(a); b=findp(b); if(a!=b) parent[a]=b; };

        // Unions : CN bus reliés via BBS ou DS
        for (const auto& cePair : adj.ceToCN){
            auto vCEopt = findVertex(rawIdx, cePair.first);
            if (!vCEopt) continue;
            const auto& ceV = raw[*vCEopt];
            if (ceV.kind!=NodeKind::Equipment) continue;
            if (ceV.eKind!=EquipmentKind::BusbarSection && ceV.eKind!=EquipmentKind::DS) continue;
            // collect CN bus voisins
            std::vector<int> local;
            for (const auto& cnId : cePair.second){
                if (!isBusCN.count(cnId)) continue;
                auto itx = idx.find(cnId);
                if (itx!=idx.end()) local.push_back(itx->second);
            }
            for (size_t i=1;i<local.size();++i) uni(local[0], local[i]);
        }

        // regroupe par racine
        std::unordered_map<int, std::vector<NodeId>> groups;
        for (size_t i=0;i<cnList.size();++i) groups[findp((int)i)].push_back(cnList[i]);

        // crée les Bus + edges Equip->Bus
        for (auto& [root,list] : groups){
            if (list.empty()) continue;
            const auto& vCN = raw[*findVertex(rawIdx, list.front())];
            BusCluster bc;
            bc.ss = vCN.ss; bc.vl = vCN.vl;
            bc.cnMembers = list;
            bc.label = vCN.vl + "-" + lastSeg(list.front());
            bc.busNodeId = makeBusId(bc.ss, bc.vl, clusterIndex++);

            VertexProp bv; bv.id=bc.busNodeId; bv.kind=NodeKind::Bus;
            bv.ss=bc.ss; bv.vl=bc.vl; bv.label=bc.label;
            V vBus = ensureVertex(out, idxOut, bv);

            clusters.push_back(bc);

            // rediriger CE voisins -> Bus
            for (const auto& cnId : list){
                auto itCEs = adj.cnToCE.find(cnId);
                if (itCEs==adj.cnToCE.end()) continue;
                for (const auto& ceId : itCEs->second){
                    auto vCE = findVertex(idxOut, ceId); // CE déjà copié
                    if (!vCE) continue;
                    auto [e,ok] = boost::add_edge(*vCE, vBus, out);
                    out[e].kind = EdgeKind::Equip_to_Bus;
                    out[e].id   = "E:"+out[*vCE].id+"->"+out[vBus].id;
                }
            }
        }
    }

    return scl::Status::Ok();
}

// -------- 3a) Couplers --------
scl::Status SldBuilder::detectCouplers(const BoostGraph& condensed, const Index&,
                                       const std::vector<BusCluster>& clusters,
                                       std::vector<BusCoupler>& out) const {
    // index bus par (SS:VL)
    std::unordered_map<std::string, std::unordered_set<NodeId>> busByVL;
    for (const auto& cl : clusters)
        busByVL[keyVL(cl.ss, cl.vl)].insert(cl.busNodeId);

    // CE touchant 2 bus distincts dans le même VL => coupler
    for (auto vIt=vertices(condensed); vIt.first!=vIt.second; ++vIt.first){
        V v = *vIt.first;
        const auto& pv = condensed[v];
        if (pv.kind != NodeKind::Equipment) continue;
        if (pv.eKind != EquipmentKind::CB && pv.eKind != EquipmentKind::DS) continue;

        std::unordered_set<NodeId> buses;
        for (auto aeIt = adjacent_vertices(v, condensed); aeIt.first!=aeIt.second; ++aeIt.first){
            V w = *aeIt.first;
            if (condensed[w].kind == NodeKind::Bus) buses.insert(condensed[w].id);
        }
        if (buses.size() >= 2){
            auto it = buses.begin();
            BusCoupler c;
            c.couplerEquipId = pv.id;
            c.busA = *it++; c.busB = *it;
            c.isBreaker = (pv.eKind == EquipmentKind::CB);
            c.ss = pv.ss; c.vl = pv.vl;

            // même VL ?
            if (busByVL[keyVL(pv.ss,pv.vl)].count(c.busA) &&
                busByVL[keyVL(pv.ss,pv.vl)].count(c.busB))
                out.push_back(std::move(c));
        }
    }
    return scl::Status::Ok();
}

// -------- 3b) Feeders --------
scl::Status SldBuilder::detectFeeders(const BoostGraph& raw, const Index& rawIdx,
                                      const BoostGraph& condensed, const Index&,
                                      const std::vector<BusCluster>& clusters,
                                      std::vector<Feeder>& out) const {
    // CN->Bus map
    std::unordered_map<NodeId, NodeId> cnToBus;
    for (const auto& cl : clusters)
        for (const auto& cnId : cl.cnMembers)
            cnToBus[cnId] = cl.busNodeId;

    // CE->Bus map (condensed)
    std::unordered_map<NodeId, std::vector<NodeId>> ceToBus;
    for (auto vIt=vertices(condensed); vIt.first!=vIt.second; ++vIt.first){
        V v = *vIt.first;
        const auto& pv = condensed[v];
        if (pv.kind != NodeKind::Equipment) continue;
        for (auto aIt = adjacent_vertices(v, condensed); aIt.first!=aIt.second; ++aIt.first){
            if (condensed[*aIt.first].kind == NodeKind::Bus)
                ceToBus[pv.id].push_back(condensed[*aIt.first].id);
        }
    }

    // Raw adjacency for walks
    RawAdj adj = buildAdj(raw, rawIdx);

    auto isPass = [&](EquipmentKind k){
        for (int x : cfg_.seriesPassKinds) if ((int)k==x) return true;
        return false;
    };
    auto isEnd = [&](EquipmentKind k){
        for (int x : cfg_.endpointKinds) if ((int)k==x) return true;
        return false;
    };

    int counter=1;
    for (auto vIt=vertices(condensed); vIt.first!=vIt.second; ++vIt.first){
        const auto& ce = condensed[*vIt.first];
        if (ce.kind!=NodeKind::Equipment) continue;
        auto itB = ceToBus.find(ce.id);
        if (itB==ceToBus.end()) continue;

        // éviter les couplers (touchent 2+ bus)
        if ((ce.eKind==EquipmentKind::CB || ce.eKind==EquipmentKind::DS) &&
            itB->second.size()>=2)
            continue;

        // depuis le CE de départ, trouver dans raw un CN « non bus » pour partir
        auto itCNs = adj.ceToCN.find(ce.id);
        if (itCNs==adj.ceToCN.end()) continue;
        NodeId startCN;
        for (const auto& cnId : itCNs->second){
            if (!cnToBus.count(cnId)){ startCN = cnId; break; }
        }
        if (startCN.empty()) continue;

        // marche linéaire simple CE↔CN en s’éloignant du bus
        std::unordered_set<NodeId> visCE, visCN;
        visCE.insert(ce.id); visCN.insert(startCN);
        std::vector<NodeId> chain; chain.push_back(ce.id);
        NodeId currCN = startCN;
        EquipmentKind endK = EquipmentKind::Unknown;
        int depth=0;
        while (depth++ < cfg_.feederMaxDepth){
            // CN -> CE (non visité, pas un retour au bus)
            auto itCEs = adj.cnToCE.find(currCN);
            if (itCEs==adj.cnToCE.end()) break;
            NodeId nextCE;
            for (const auto& cand : itCEs->second){
                if (visCE.count(cand)) continue;
                if (ceToBus.count(cand) && cand!=chain.front()) continue;
                nextCE = cand; break;
            }
            if (nextCE.empty()) break;
            visCE.insert(nextCE);
            chain.push_back(nextCE);
            const auto& pvNext = raw[*findVertex(rawIdx, nextCE)];
            if (isEnd(pvNext.eKind)){ endK = pvNext.eKind; break; }

            // CE -> CN suivant (non-bus, non visité)
            auto itCN2 = adj.ceToCN.find(nextCE);
            if (itCN2==adj.ceToCN.end()) break;
            NodeId nextCN;
            for (const auto& cn2 : itCN2->second){
                if (visCN.count(cn2)) continue;
                if (cnToBus.count(cn2)) continue;
                nextCN = cn2; break;
            }
            if (nextCN.empty()) break;
            visCN.insert(nextCN);
            currCN = nextCN;
            if (!isPass(pvNext.eKind)){
                // on pourrait s’arrêter ici si besoin; on continue prudemment
            }
        }

        Feeder f;
        f.busId = itB->second.front();
        f.ss = ce.ss; f.vl = ce.vl;
        f.chain = std::move(chain);
        f.endpointType = toString(endK);
        f.id = "FEED:"+f.busId+"#"+std::to_string(counter++);
        out.push_back(std::move(f));
    }

    // Lane per (SS:VL|bus)
    std::unordered_map<std::string,int> lane;
    for (auto& f : out){
        std::string key = keyVL(f.ss,f.vl)+"|"+f.busId;
        f.laneIndex = lane[key]++;
    }

    return scl::Status::Ok();
}

// -------- 3c) Transformers --------
scl::Status SldBuilder::detectTransformers(const BoostGraph& raw, const Index& rawIdx,
                                           const std::vector<BusCluster>& clusters,
                                           std::vector<TransformerLink>& out) const {
    // CN->Bus
    std::unordered_map<NodeId, NodeId> cnToBus;
    for (const auto& cl : clusters)
        for (const auto& cn : cl.cnMembers)
            cnToBus[cn] = cl.busNodeId;

    // CE transformer -> buses sur ses CN
    RawAdj adj = buildAdj(raw, rawIdx);
    for (auto vIt=vertices(raw); vIt.first!=vIt.second; ++vIt.first){
        V v = *vIt.first;
        const auto& pv = raw[v];
        if (pv.kind!=NodeKind::Equipment || pv.eKind!=EquipmentKind::Transformer) continue;
        std::set<NodeId> buses;
        auto itCNs = adj.ceToCN.find(pv.id);
        if (itCNs==adj.ceToCN.end()) continue;
        for (const auto& cnId : itCNs->second){
            auto itb = cnToBus.find(cnId);
            if (itb!=cnToBus.end()) buses.insert(itb->second);
        }
        if (buses.size()>=2){
            auto it=buses.begin();
            TransformerLink tl;
            tl.transformerId = pv.id;
            tl.busA = *it++; tl.busB = *it;

            auto findCluster = [&](const NodeId& bus)->std::pair<std::string,std::string>{
                for (const auto& cl : clusters) if (cl.busNodeId == bus) return {cl.ss, cl.vl};
                return {"",""};
            };
            auto [ssA, vlA] = findCluster(tl.busA);
            auto [ssB, vlB] = findCluster(tl.busB);
            tl.ssA = ssA; tl.vlA = vlA; tl.ssB = ssB; tl.vlB = vlB;

            out.push_back(std::move(tl));
        }
    }
    return scl::Status::Ok();
}

// -------- 4) makePlan --------
scl::Status SldBuilder::makePlan(const BoostGraph& raw, const Index& rawIdx,
                                 const BoostGraph& condensed, const Index& cIdx,
                                 const std::vector<BusCluster>& clusters,
                                 SldPlan& plan,
                                 const scl::SclManager* sclMgr) const {
    plan.condensed = condensed; // copie légère (BGL stocke contigu vecS)
    plan.idx = cIdx;
    plan.buses = clusters;

    // ranks
    for (auto vIt=vertices(condensed); vIt.first!=vIt.second; ++vIt.first){
        const auto& pv = condensed[*vIt.first];
        auto key = keyVL(pv.ss, pv.vl);
        if (pv.kind==NodeKind::Bus) plan.rankTopBus[key].push_back(pv.id);
        else if (pv.kind==NodeKind::Equipment) plan.rankMiddleEq[key].push_back(pv.id);
    }

    for (auto& kv : plan.rankTopBus)   std::sort(kv.second.begin(), kv.second.end());
    for (auto& kv : plan.rankMiddleEq) std::sort(kv.second.begin(), kv.second.end());

    // couplers / feeders / transformers
    detectCouplers(condensed, cIdx, clusters, plan.couplers);
    detectFeeders(raw, rawIdx, condensed, cIdx, clusters, plan.feeders);
    detectTransformers(raw, rawIdx, clusters, plan.transformers);

    // PowerTransformers -> plan_transformers (en utilisant SCL résolu)
    if (sclMgr){
        // réutilise la logique du module SCL (ends déjà résolus)
        // Ici on expose seulement l’agrégat pour rendu global
        for (const auto& ss : sclMgr->model()->substations){
            for (const auto& pt : ss.powerTransformers){
                PlanTransformer P;
                P.id = "TR:"+ss.name+"/"+pt.name;
                P.ss = ss.name;
                P.label = pt.name;
                P.hasTapChanger = false;
                for (const auto& w : pt.windings)
                    if (w.tapChanger){ P.hasTapChanger = true; break; }

                // buses reliés (si détectables dans clusters)
                std::unordered_set<std::string> busSet;
                for (const auto& w : pt.windings){
                    for (const auto& re : w.resolvedEnds){
                        const NodeId cnId = makeCNIdAbs(re.ss, re.vl, re.bay, re.cn);
                        for (const auto& cl : clusters){
                            if (std::find(cl.cnMembers.begin(), cl.cnMembers.end(), cnId) != cl.cnMembers.end()){
                                busSet.insert(cl.busNodeId);
                            }
                        }
                    }
                }
                for (auto& b : busSet) P.buses.push_back(b);
                if (!P.buses.empty()) plan.plan_transformers.push_back(std::move(P));
            }
        }

        // Ajoute aussi la vue IEDs (extrait par SCL) → QML l’utilisera dans une autre vue
        plan.equipmentsFromIEDs = sclMgr->collectEquipmentFromIEDs(true);
    }

    return scl::Status::Ok();
}

// ---------- JSON ----------
json SldBuilder::toJsonRaw(const BoostGraph& g) const {
    json J; J["nodes"] = json::array(); J["edges"] = json::array();
    for (auto vIt=vertices(g); vIt.first!=vIt.second; ++vIt.first){
        const auto& pv = g[*vIt.first];
        json n{{"id",pv.id},{"kind",toString(pv.kind)}};
        if (!pv.label.empty()) n["label"]=pv.label;
        if (!pv.ss.empty()) n["ss"]=pv.ss;
        if (!pv.vl.empty()) n["vl"]=pv.vl;
        if (!pv.bay.empty()) n["bay"]=pv.bay;
        if (pv.kind==NodeKind::Equipment) n["eKind"]=toString(pv.eKind);
        if (!pv.lnodes.empty()){
            n["lnodes"] = json::array();
            for (const auto& lr: pv.lnodes){
                json jl;
                if (!lr.iedName.empty()) jl["ied"]=lr.iedName;
                if (!lr.ldInst.empty())  jl["ld"]=lr.ldInst;
                if (!lr.prefix.empty())  jl["prefix"]=lr.prefix;
                if (!lr.lnClass.empty()) jl["lnClass"]=lr.lnClass;
                if (!lr.lnInst.empty())  jl["lnInst"]=lr.lnInst;
                n["lnodes"].push_back(std::move(jl));
            }
        }
        J["nodes"].push_back(std::move(n));
    }
    for (auto eIt=edges(g); eIt.first!=eIt.second; ++eIt.first){
        const auto& pe = g[*eIt.first];
        V a = source(*eIt.first, g), b = target(*eIt.first, g);
        json e{{"id",pe.id},{"from",g[a].id},{"to",g[b].id}};
        switch (pe.kind){
            case EdgeKind::CE_to_CN: e["kind"]="CE_to_CN"; break;
            case EdgeKind::Equip_to_Bus: e["kind"]="Equip_to_Bus"; break;
            case EdgeKind::CN_Merge: e["kind"]="CN_Merge"; break;
        }
        if (!pe.terminalName.empty()) e["terminal"]=pe.terminalName;
        if (!pe.cnPath.empty())       e["cn"]=pe.cnPath;
        J["edges"].push_back(std::move(e));
    }
    return J;
}

json SldBuilder::toJsonCondensed(const BoostGraph& g) const {
    // identique au raw, juste le contenu du graphe diffère
    return toJsonRaw(g);
}

json SldBuilder::toJsonPlan(const SldPlan& p) const {
    json J;

    // condensed graph
    J["graph"] = toJsonCondensed(p.condensed);

    // buses
    J["buses"] = json::array();
    for (const auto& b : p.buses){
        json jb{{"id",b.busNodeId},{"ss",b.ss},{"vl",b.vl},{"label",b.label}};
        jb["members"]=b.cnMembers;
        J["buses"].push_back(std::move(jb));
    }

    // ranks
    J["ranks"] = json::object();
    for (const auto& kv : p.rankTopBus)   J["ranks"]["top"][kv.first]    = kv.second;
    for (const auto& kv : p.rankMiddleEq) J["ranks"]["middle"][kv.first] = kv.second;

    // couplers
    J["couplers"] = json::array();
    for (const auto& c : p.couplers){
        json jc{{"equip",c.couplerEquipId},{"busA",c.busA},{"busB",c.busB}};
        jc["type"] = c.isBreaker? "CB":"DS";
        jc["ss"] = c.ss; jc["vl"] = c.vl;
        J["couplers"].push_back(std::move(jc));
    }

    // transformers links
    J["transformers"] = json::array();
    for (const auto& t : p.transformers){
        json jt{{"tr",t.transformerId},{"busA",t.busA},{"busB",t.busB},
                {"vlA",t.vlA},{"vlB",t.vlB}};
        J["transformers"].push_back(std::move(jt));
    }

    // feeders
    J["feeders"] = json::array();
    for (const auto& f : p.feeders){
        json jf{{"id",f.id},{"bus",f.busId},{"ss",f.ss},{"vl",f.vl},
                {"lane",f.laneIndex},{"endpoint",f.endpointType}};
        jf["chain"] = f.chain;
        J["feeders"].push_back(std::move(jf));
    }

    // plan_transformers (aggrégé)
    J["plan_transformers"] = json::array();
    for (const auto& t : p.plan_transformers){
        json jt{{"id",t.id},{"ss",t.ss},{"label",t.label},{"hasTapChanger",t.hasTapChanger}};
        jt["buses"] = t.buses;
        J["plan_transformers"].push_back(std::move(jt));
    }

    // IEDs view (du SCL) à embarquer pour QML
    if (!p.equipmentsFromIEDs.empty()){
        J["ieds"] = json::array();
        // Regrouper par IED -> LD
        std::map<std::string,std::map<std::string,std::vector<scl::EquipmentFromIED>>> byIed;
        for (auto e : p.equipmentsFromIEDs) byIed[e.iedName][e.ldInst].push_back(std::move(e));
        for (auto& [ied,lds] : byIed){
            json jIed; jIed["name"]=ied; jIed["lds"]=json::array();
            for (auto& [ldInst, eqs] : lds){
                json jLd; jLd["inst"]=ldInst; jLd["equipments"]=json::array();
                for (auto& e : eqs){
                    json je{{"lnClass",e.lnClass},{"lnInst",e.lnInst}};
                    if (!e.prefix.empty()) je["prefix"]=e.prefix;
                    if (!e.primaryAnchors.empty()) je["anchors"]=e.primaryAnchors;
                    jLd["equipments"].push_back(std::move(je));
                }
                jIed["lds"].push_back(std::move(jLd));
            }
            J["ieds"].push_back(std::move(jIed));
        }
    }

    return J;
}
