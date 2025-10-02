#pragma once
#include "SldTypes.h"
#include "SldBuilder.h"

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
