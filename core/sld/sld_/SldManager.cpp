#include "SldManager.h"
#include <iostream>

using namespace sld;

SldManager::SldManager(const scl::SclModel *model, HeuristicsConfig cfg)
    : model_(model), cfg_(cfg), builder_(model, cfg) {}

scl::Status SldManager::build() {
    auto st = builder_.buildRaw(raw_);
    if (!st)
        return st;
    st = builder_.clusterAndCondense(raw_, condensed_, clusters_);
    if (!st)
        return st;
    plan_ = builder_.makePlan(condensed_, clusters_);
    // compléter plan_ avec feeders & transformers (besoin du graphe raw pour la
    // marche)
    builder_.detectFeeders_(raw_, condensed_, clusters_, plan_.feeders);
    builder_.detectTransformers_(raw_, clusters_, plan_.transformers);
    return scl::Status::Ok();
}

scl::Status SldManager::printRaw() const {
    if (raw_.nodes.empty())
        return scl::Status(
            scl::Error{scl::ErrorCode::LogicError, "raw graph is empty"});
    std::cout << "[RAW] Nodes=" << raw_.nodes.size()
              << ", Edges=" << raw_.edges.size() << "\n";
    for (const auto &kv : raw_.nodes) {
        const Node &n = kv.second;
        std::cout << "  N " << n.id << "  kind=" << toString(n.kind)
                  << "  label=" << n.label << "  ctx=" << n.ssName << "/"
                  << n.vlName << "/" << n.bayName;
        if (n.kind == NodeKind::Equipment)
            std::cout << "  eKind=" << toString(n.eKind);
        std::cout << "\n";
    }
    for (const auto &e : raw_.edges) {
        std::cout << "  E " << e.id << "  (" << e.from << ") -> (" << e.to
                  << ")  term=" << e.terminalName << "  cn=" << e.cnPath << "\n";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printCondensed() const {
    if (condensed_.nodes.empty())
        return scl::Status(
            scl::Error{scl::ErrorCode::LogicError, "condensed graph is empty"});
    std::cout << "[CONDENSED] Nodes=" << condensed_.nodes.size()
              << ", Edges=" << condensed_.edges.size() << "\n";
    for (const auto &kv : condensed_.nodes) {
        const Node &n = kv.second;
        std::cout << "  N " << n.id << "  kind=" << toString(n.kind)
                  << "  label=" << n.label << "  ctx=" << n.ssName << "/"
                  << n.vlName << "/" << n.bayName;
        if (n.kind == NodeKind::Equipment)
            std::cout << "  eKind=" << toString(n.eKind);
        std::cout << "\n";
    }
    for (const auto &e : condensed_.edges) {
        std::cout << "  E " << e.id << "  (" << e.from << ") -> (" << e.to
                  << ")  kind=Equip_to_Bus  term=" << e.terminalName << "\n";
    }
    std::cout << "  Buses: " << clusters_.size() << "\n";
    return scl::Status::Ok();
}

scl::Status SldManager::printFeeders() const {
    if (plan_.feeders.empty()) {
        std::cout << "[FEEDERS] none";
        return scl::Status::Ok();
    }
    std::cout << "[FEEDERS] count=" << plan_.feeders.size() << "\n";
    for (const auto &f : plan_.feeders) {
        std::cout << "  " << f.id << "  VL=" << f.vlName << "  bus=" << f.busId
                  << "  lane=" << f.laneIndex << "  endpoint=" << f.endpointType
                  << "chain: ";
        for (const auto &n : f.chain)
            std::cout << n << " -> ";
        std::cout << "END";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printCouplers() const {
    if (plan_.couplers.empty()) {
        std::cout << "[COUPLERS] none";
        return scl::Status::Ok();
    }
    std::cout << "[COUPLERS] count=" << plan_.couplers.size() << "\n";
    for (const auto &c : plan_.couplers) {
        std::cout << "  equip=" << c.couplerEquipId << "  ("
                  << (c.isBreaker ? "CB" : "DS") << ")  " << c.busA << " <-> "
                  << c.busB << "  VL=" << c.vlName << "\n";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printTransformers() const {
    if (plan_.transformers.empty()) {
        std::cout << "[TRANSFORMERS] none";
        return scl::Status::Ok();
    }
    std::cout << "[TRANSFORMERS] count=" << plan_.transformers.size() << "\n";
    for (const auto &t : plan_.transformers) {
        std::cout << "  tr=" << t.transformerId << "  " << t.busA << " (" << t.vlA
                  << ") <=> " << t.busB << " (" << t.vlB << ")";
    }
    return scl::Status::Ok();
}

scl::Status SldManager::printStats() const {
    std::cout << "[STATS] rawNodes=" << raw_.nodes.size()
              << ", rawEdges=" << raw_.edges.size()
              << ", condensedNodes=" << condensed_.nodes.size()
              << ", condensedEdges=" << condensed_.edges.size()
              << ", buses=" << clusters_.size()
              << ", feeders=" << plan_.feeders.size()
              << ", couplers=" << plan_.couplers.size()
              << ", transformers=" << plan_.transformers.size() << "\n";
    return scl::Status::Ok();
}
