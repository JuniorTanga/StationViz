#pragma once
#include "SldTypes.h"
#include "SldConfig.h"
#include <nlohmann/json.hpp>

namespace sld {

class SldBuilder {
public:
    explicit SldBuilder(const scl::SclModel* model,
                        const HeuristicsConfig& cfg = {});

    // 1) Raw: CE <-> CN
    scl::Status buildRaw(BoostGraph& g, Index& idx) const;

    // 2) Cluster Bus CNs & condense to Equip <-> Bus
    scl::Status clusterAndCondense(const BoostGraph& raw, const Index& rawIdx,
                                   BoostGraph& condensed, Index& cIdx,
                                   std::vector<BusCluster>& clusters) const;

    // 3) Detect couplers / feeders / transformers
    scl::Status detectCouplers(const BoostGraph& condensed, const Index& cIdx,
                               const std::vector<BusCluster>& clusters,
                               std::vector<BusCoupler>& out) const;

    scl::Status detectFeeders(const BoostGraph& raw, const Index& rawIdx,
                              const BoostGraph& condensed, const Index& cIdx,
                              const std::vector<BusCluster>& clusters,
                              std::vector<Feeder>& out) const;

    scl::Status detectTransformers(const BoostGraph& raw, const Index& rawIdx,
                                   const std::vector<BusCluster>& clusters,
                                   std::vector<TransformerLink>& out) const;

    // 4) Build plan (ranks + transformers + add EquipFromIEDs)
    scl::Status makePlan(const BoostGraph& raw, const Index& rawIdx,
                         const BoostGraph& condensed, const Index& cIdx,
                         const std::vector<BusCluster>& clusters,
                         SldPlan& plan,
                         const scl::SclManager* sclMgr) const;

    // mapping CE type / LN class to EquipmentKind
    static EquipmentKind mapEquipmentKind(const std::string& ceType);

    // JSON
    nlohmann::json toJsonRaw(const BoostGraph& g) const;
    nlohmann::json toJsonCondensed(const BoostGraph& g) const;
    nlohmann::json toJsonPlan(const SldPlan& p) const;

private:
    const scl::SclModel* model_{nullptr};
    HeuristicsConfig cfg_{};

    // utils
    static std::string keyVL(const std::string& ss, const std::string& vl);
    static std::string makeCNIdAbs(const std::string& ss, const std::string& vl,
                                   const std::string& bay, const std::string& name);
    static std::string makeCEId(const std::string& ss, const std::string& vl,
                                const std::string& bay, const std::string& ce);
    static std::string makeBusId(const std::string& ss, const std::string& vl, int n);

    static std::string upper(std::string s);
    bool isLikelyBusCN(const std::string& nameOrPath, int degree) const;

    // create/find vertex by NodeId in a graph/index
    static V ensureVertex(BoostGraph& g, Index& idx, const VertexProp& vp);
    static std::optional<V> findVertex(const Index& idx, const NodeId& id);

    // raw adjacency views
    struct RawAdj {
        std::unordered_map<NodeId, std::vector<NodeId>> cnToCE;
        std::unordered_map<NodeId, std::vector<NodeId>> ceToCN;
    };
    RawAdj buildAdj(const BoostGraph& raw, const Index& idx) const;
};

} // namespace sld
