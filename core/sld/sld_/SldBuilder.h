#pragma once
#include "SldTypes.h"

namespace sld {

class SldBuilder {
public:
    explicit SldBuilder(const scl::SclModel *model,
                        const HeuristicsConfig &cfg = {});

    // 1) Graphe biparti CE↔CN
    scl::Status buildRaw(Graph &out) const;

    // 2) Clustering des CN de bus & condensation Bus + Equipment
    scl::Status clusterAndCondense(const Graph &raw, Graph &out,
                                   std::vector<BusCluster> &clusters) const;

    // 3) Détection des couplers / feeders / transformers et plan de layout
    SldPlan makePlan(const Graph &condensed,
                     const std::vector<BusCluster> &clusters) const;

    void integratePowerTransformers_(const std::vector<BusCluster>& clusters,
                                     SldPlan& out) const;

    // JSON utilitaires
    std::string toJson(const Graph &g) const;
    std::string planToJson(const SldPlan &p) const;

    // Utils
    static EquipmentKind mapEquipmentKind(const std::string &ceType);

    // Détection coupler & feeders
    void detectCouplers_(const Graph &condensed,
                         const std::vector<BusCluster> &clusters,
                         std::vector<BusCoupler> &out) const;

    void detectFeeders_(const Graph &raw, const Graph &condensed,
                        const std::vector<BusCluster> &clusters,
                        std::vector<Feeder> &out) const;

    void detectTransformers_(const Graph &raw,
                             const std::vector<BusCluster> &clusters,
                             std::vector<TransformerLink> &out) const;

private:
    const scl::SclModel *model_{nullptr};
    HeuristicsConfig cfg_{};

    // internes
    static std::string upper(std::string s);
    static std::string keyVL(const std::string &ss, const std::string &vl);

    static NodeId makeCNId(const std::string &ss, const std::string &vl,
                           const std::string &bay,
                           const std::string &cnPathOrName);
    static NodeId makeCEId(const std::string &ss, const std::string &vl,
                           const std::string &bay, const std::string &ceName);

    static const char* edgeKindToString(EdgeKind k) {
        switch (k) {
        case EdgeKind::CE_to_CN:     return "CE_to_CN";
        case EdgeKind::Equip_to_Bus: return "Equip_to_Bus";
        case EdgeKind::CN_Merge:     return "CN_Merge";
        }
        return "?";
    }

    bool isLikelyBusCN(const std::string &cnNameOrPath, int degree) const;

    // Union-Find pour cluster CN
    struct DSU {
        mutable std::unordered_map<NodeId, NodeId> p;
        NodeId f(const NodeId &x) const {
            auto it = p.find(x);
            if (it == p.end() || it->second == x)
                return it == p.end() ? x : it->second;
            return p[x] = f(it->second);
        }
        void u(const NodeId &a, const NodeId &b) {
            auto ra = f(a), rb = f(b);
            if (ra != rb)
                p[ra] = rb;
        }
    };

    // Aides à partir du graphe brut
    struct RawView {
        // adjacency
        std::unordered_map<NodeId, std::vector<NodeId>> cnToCE; // CN -> CE*
        std::unordered_map<NodeId, std::vector<NodeId>> ceToCN; // CE -> CN*
    };

    RawView buildRawView_(const Graph &raw) const;
};

} // namespace sld
