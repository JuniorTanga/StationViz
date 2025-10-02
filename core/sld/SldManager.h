#pragma once
#include "SldTypes.h"
#include "SldBuilder.h"
#include <functional>

namespace sld {

class SldManager {
public:
    explicit SldManager(const scl::SclManager* sclMgr,
                        HeuristicsConfig cfg = {});

    // construit tout: raw -> clusters/condensed -> plan
    scl::Status build();

    // accès
    const BoostGraph& raw() const { return raw_; }
    const Index& rawIndex() const { return rawIdx_; }
    const BoostGraph& condensed() const { return condensed_; }
    const Index& condensedIndex() const { return condIdx_; }
    const SldPlan& plan() const { return plan_; }

    // JSON
    std::string rawJson() const { return builder_.toJsonRaw(raw_).dump(); }
    std::string condensedJson() const { return builder_.toJsonCondensed(condensed_).dump(); }
    std::string planJson() const { return builder_.toJsonPlan(plan_).dump(); }

    // Logging helpers
    scl::Status printStats() const;

    // Observabilité (pour wrapper Qt → QML)
    using Callback = std::function<void(const SldManager&)>;
    void onUpdated(Callback cb){ callbacks_.push_back(std::move(cb)); }

private:
    const scl::SclManager* sclMgr_{nullptr};
    HeuristicsConfig cfg_;
    SldBuilder builder_;

    BoostGraph raw_; Index rawIdx_;
    std::vector<BusCluster> clusters_;
    BoostGraph condensed_; Index condIdx_;
    SldPlan plan_;

    std::vector<Callback> callbacks_;
    void notify_() const { for (auto& cb : const_cast<SldManager*>(this)->callbacks_) cb(*this); }
};

} // namespace sld
