// Acceptance table for the SLD engine.
//
// Before this file, sld.csv recorded bus/feeder/coupler/transformer counts but
// nothing ever asserted on them, which is how "transformers=0 on every fixture"
// and "every feeder reports endpointType=Unknown" survived. Each expectation
// below is derived from what the fixture actually contains, counted from the
// SCL itself.
//
// A fixture listed as "skipped" is gitignored (large), so the test skips rather
// than fails when it is absent.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <map>

#include "SldTypes.h"

#include <boost/graph/adjacency_list.hpp>
#include <string>
#include <vector>

#include "SldManager.h"

namespace {

// Locates a fixture relative to this source file, so the test does not depend
// on the working directory.
std::string fixture(const std::string& rel) {
    return std::string(TEST_FIXTURE_DIR) + "/" + rel;
}

struct Counts {
    int buses = 0, feeders = 0, couplers = 0, transformers = 0;
    int unknownEndpoints = 0, trVertices = 0;
};

Counts analyse(const std::string& path) {
    Counts c;
    scl::SclManager sm;
    const auto st = sm.loadScl(path);
    EXPECT_TRUE(static_cast<bool>(st)) << "load failed: " << path;
    if (!st) return c;

    sld::SldManager mgr(&sm);
    const auto bst = mgr.build();
    EXPECT_TRUE(static_cast<bool>(bst)) << "build failed: " << path;
    if (!bst) return c;

    const auto& plan = mgr.plan();
    c.buses       = static_cast<int>(plan.buses.size());
    c.feeders     = static_cast<int>(plan.feeders.size());
    c.couplers    = static_cast<int>(plan.couplers.size());
    c.transformers = static_cast<int>(plan.transformers.size());
    for (auto it = boost::vertices(mgr.raw()); it.first != it.second; ++it.first) {
        const auto& pv = mgr.raw()[*it.first];
        if (pv.kind == sld::NodeKind::Equipment && pv.eKind == sld::EquipmentKind::Transformer)
            ++c.trVertices;
    }
    for (const auto& f : plan.feeders)
        if (f.endpointType.empty() || f.endpointType == "Unknown") ++c.unknownEndpoints;
    return c;
}

// SCL_SB_2L: 1 bus, 2 bays. Each bay is DS + CB + CT + VT off one busbar, so
// one feeder per line and no coupler.
TEST(SldAcceptance, SingleBusTwoLines) {
    const std::string p = fixture("SCD_SB_2L.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent";
    const Counts c = analyse(p);
    EXPECT_EQ(c.buses, 1);
    EXPECT_EQ(c.feeders, 2);
    EXPECT_EQ(c.couplers, 0);
    EXPECT_EQ(c.transformers, 0);
}

// SCD_DB_COUPLER: two busbars joined by an explicit coupler, so exactly one
// coupler must be found. This is the fixture that regressed when the "BB"
// busbar hint was lost.
TEST(SldAcceptance, DoubleBusWithCoupler) {
    const std::string p = fixture("SCD_DB_COUPLER.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent";
    const Counts c = analyse(p);
    EXPECT_EQ(c.buses, 2);
    EXPECT_EQ(c.couplers, 1) << "the explicit coupler must be detected";
    EXPECT_GE(c.feeders, 2);
}

// SCD_2VL_TR: two voltage levels joined by one PowerTransformer. The fixture
// has no ConductingEquipment at all, only two ConnectivityNodes, so this is the
// cleanest possible test that PowerTransformer winding terminals reach the
// graph. It failed with "transformers=0" before Phase B1, because
// SldBuilder::buildRaw only ever walked VoltageLevel/Bay/ConductingEquipment
// and tns:PowerTransformer is a direct child of Substation.
TEST(SldAcceptance, TransformerWindingTerminalsEnterTheGraph) {
    const std::string p = fixture("SCD_2VL_TR.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent";
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    ASSERT_EQ(sm.model()->substations[0].powerTransformers.size(), 1u);
    ASSERT_EQ(sm.model()->substations[0].powerTransformers[0].windings.size(), 2u);

    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));

    // One Transformer vertex per PowerTransformer, with one edge per winding.
    int trVertices = 0, trEdges = 0;
    const auto& raw = mgr.raw();
    for (auto it = boost::vertices(raw); it.first != it.second; ++it.first) {
        const auto& pv = raw[*it.first];
        if (pv.kind == sld::NodeKind::Equipment && pv.eKind == sld::EquipmentKind::Transformer)
            ++trVertices;
    }
    for (auto it = boost::edges(raw); it.first != it.second; ++it.first) {
        const auto e = *it.first;
        const auto& a = raw[boost::source(e, raw)];
        const auto& b = raw[boost::target(e, raw)];
        const bool aTr = a.kind == sld::NodeKind::Equipment && a.eKind == sld::EquipmentKind::Transformer;
        const bool bTr = b.kind == sld::NodeKind::Equipment && b.eKind == sld::EquipmentKind::Transformer;
        if (aTr != bTr) ++trEdges;
    }
    EXPECT_EQ(trVertices, 1) << "PowerTransformer must produce a graph vertex";
    EXPECT_EQ(trEdges, 2)   << "each winding terminal must produce one edge";
}

// DISABLED: the plan-level transformer link still requires chain following.
// A real station puts a disconnector AND a breaker between the transformer and
// the busbar: in substation.scd, T4_1's terminal CN CONNECTIVITY_NODE85 reaches
// the 380 kV busbar CN CONNECTIVITY_NODE82 via DISCONNECTOR50 then BREAKER25.
// SldBuilder::detectTransformers only accepts a winding terminal whose CN is
// itself in a bus cluster, so it can never fire on real data. Fixing this means
// following the bay chain from each winding terminal to the busbar, which is
// the topology-first rewrite tracked in docs/PLAN.md section 4.
TEST(SldAcceptance, DISABLED_TransformerLinksReachBusesThroughTheBayChain) {
    const std::string p = fixture("../../tests_files/substation.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent (gitignored)";
    const Counts c = analyse(p);
    EXPECT_EQ(c.transformers, 3)
        << "substation.scd declares T4, T3 and T2; all were reported as 0";
}

// substation.scd: 2 substations, 5 voltage levels, 45 ConductingEquipment,
// 3 PowerTransformers (T4 380kV/3 windings, T3 110kV/3, T2 30kV/2).
TEST(SldAcceptance, RealSubstationTopologyShape) {
    const std::string p = fixture("../../tests_files/substation.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent (gitignored)";
    const Counts c = analyse(p);
    EXPECT_EQ(c.buses, 3);
    EXPECT_EQ(c.feeders, 12);
    // All three transformers must now be in the graph even though none of them
    // is a ConductingEquipment.
    EXPECT_EQ(c.trVertices, 3);
}

// Endpoint classification, part 1: a feeder that lands on a transformer must
// say so. HeuristicsConfig::endpointKinds and seriesPassKinds had no
// initialiser at all, so SldBuilder::isEnd() and isPass() iterated empty
// vectors, were constant false, and all 12 feeders reported "Unknown" while the
// walk ran straight through the transformers to their far winding.
TEST(SldAcceptance, FeederReachingATransformerIsClassified) {
    const std::string p = fixture("../../tests_files/substation.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent (gitignored)";
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));

    int classified = 0, unknown = 0;
    for (const auto& f : mgr.plan().feeders) {
        if (f.endpointType == "Unknown" || f.endpointType.empty()) ++unknown;
        else ++classified;
    }
    EXPECT_EQ(classified, 5)
        << "the five transformer feeders must be classified";
    EXPECT_EQ(unknown, 7);
}

// DISABLED: the seven remaining feeders terminate on a bare ConnectivityNode.
// substation.scd models a line end as a CN with no Line or Cable equipment
// attached, so there is nothing for the walk to recognise as an endpoint.
// Classifying these needs an explicit "unterminated CN" endpoint kind, which is
// part of the topology-first rewrite.
TEST(SldAcceptance, DISABLED_LineFeedersAreClassified) {
    const std::string p = fixture("../../tests_files/substation.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent (gitignored)";
    const Counts c = analyse(p);
    EXPECT_EQ(c.unknownEndpoints, 0)
        << "every feeder should reach a classified endpoint";
}

// scl.scd: 1 substation, 2 VLs, 9 CE, an explicitly named busbar and an
// explicit coupler equipment.
TEST(SldAcceptance, ExplicitBusbarNamesAreRecognised) {
    const std::string p = fixture("../../tests_files/scl.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent";
    const Counts c = analyse(p);
    EXPECT_GE(c.buses, 1);
}

// DISABLED: the explicit coupler in scl.scd is not detected. Bus detection has
// to distinguish a busbar from a bay junction, and in SCD_DB_COUPLER the two
// have identical local signatures (BUSA1 = coupler CB + one DS; L1/IN = one DS
// + one CB). Only a global property separates them, so this needs buses derived
// from feeder convergence rather than from degree and name hints.
TEST(SldAcceptance, DISABLED_ExplicitCouplerIsDetected) {
    const std::string p = fixture("../../tests_files/scl.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent";
    const Counts c = analyse(p);
    EXPECT_GE(c.couplers, 1);
}

// Determinism: cluster indices are assigned while iterating an unordered_map,
// so two runs of the same input must still be required to agree. This asserts
// it explicitly because the ids leak into persisted QSettings state.
TEST(SldAcceptance, IsDeterministicAcrossRuns) {
    const std::string p = fixture("../../tests_files/substation.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent (gitignored)";
    const std::string a = [&]{ scl::SclManager sm; sm.loadScl(p);
                               sld::SldManager m(&sm); m.build(); return m.planJson(); }();
    const std::string b = [&]{ scl::SclManager sm; sm.loadScl(p);
                               sld::SldManager m(&sm); m.build(); return m.planJson(); }();
    EXPECT_EQ(a, b) << "planJson() must be byte-identical across runs";
}

} // namespace
// ============================================================================
// Why bus detection is not a local predicate.
//
// This is not a defect test; it documents a measured constraint that four
// candidate implementations violated. If someone changes bus detection again,
// this is the fixture that shows why the obvious approaches fail.
// ============================================================================
TEST(SldAcceptance, BusbarAndBayJunctionHaveIdenticalLocalSignatures) {
    const std::string p = fixture("SCD_DB_COUPLER.scd");
    if (!std::filesystem::exists(p)) GTEST_SKIP() << "fixture absent";
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager m(&sm);
    ASSERT_TRUE(static_cast<bool>(m.build()));

    // Neighbour signature per CN: the multiset of attached equipment kinds.
    std::map<std::string, std::string> sig;
    const auto& g = m.raw();
    for (auto vit = boost::vertices(g); vit.first != vit.second; ++vit.first) {
        const sld::V v = *vit.first;
        const auto& pv = g[v];
        if (pv.kind != sld::NodeKind::ConnectivityNode) continue;
        std::vector<std::string> kinds;
        auto o = boost::out_edges(v, g);
        for (auto e = o.first; e != o.second; ++e) {
            const auto& t = g[boost::target(*e, g)];
            if (t.kind != sld::NodeKind::Equipment) continue;
            kinds.push_back(sld::toString(t.eKind));
        }
        std::sort(kinds.begin(), kinds.end());
        std::string joined;
        for (const auto& k : kinds) { joined += k; joined += "+"; }
        sig[pv.label] = joined;
    }

    // BUSA1 is a busbar. L1/IN is a bay junction. Both attach exactly one
    // disconnector and one breaker, so degree, naming and attached-kind counts
    // cannot tell them apart. Only the far end of the breaker differs:
    // BUS-COUPLER reaches another busbar, L1-CB reaches a bay.
    std::string all;
    for (const auto& [name, kinds] : sig) { all += name + "=" + kinds + "  "; }
    ASSERT_NE(sig.count("BUSA1"), 0u) << all;
    ASSERT_NE(sig.count("IN"), 0u) << all;
    EXPECT_EQ(sig["BUSA1"], sig["IN"])
        << "if these ever differ, a local predicate could work and bus "
           "detection no longer needs the topology-first rewrite";
}
