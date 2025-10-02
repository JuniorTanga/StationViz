#include "SldManager.h"
#include <iostream>

using namespace sld;

SldManager::SldManager(const scl::SclManager* sclMgr, HeuristicsConfig cfg)
    : sclMgr_(sclMgr), cfg_(std::move(cfg)), builder_(sclMgr ? sclMgr->model() : nullptr, cfg_) {}

scl::Status SldManager::build(){
    raw_.clear(); rawIdx_.nodeById.clear();
    condensed_.clear(); condIdx_.nodeById.clear();
    clusters_.clear(); plan_ = SldPlan{};

    auto st = builder_.buildRaw(raw_, rawIdx_);
    if (!st) return st;
    st = builder_.clusterAndCondense(raw_, rawIdx_, condensed_, condIdx_, clusters_);
    if (!st) return st;
    st = builder_.makePlan(raw_, rawIdx_, condensed_, condIdx_, clusters_, plan_, sclMgr_);
    if (!st) return st;

    notify_();
    return scl::Status::Ok();
}

scl::Status SldManager::printStats() const {
    std::cout << "[SLD] rawV=" << num_vertices(raw_)
              << " rawE=" << num_edges(raw_)
              << " condensedV=" << num_vertices(condensed_)
              << " condensedE=" << num_edges(condensed_)
              << " buses=" << clusters_.size()
              << " feeders=" << plan_.feeders.size()
              << " couplers=" << plan_.couplers.size()
              << " transformers=" << plan_.transformers.size()
              << std::endl;
    return scl::Status::Ok();
}
