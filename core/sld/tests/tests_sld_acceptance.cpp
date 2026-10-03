// Acceptance table for the SLD engine.
//
// The engine derives the diagram from the SCL Bay structure rather than
// inferring it from a graph, so these tests assert the derivation's contract
// directly, per fixture:
//
//   bus         = a Bay with no ConductingEquipment
//   feeder      = a Bay with equipment and exactly one bay-external terminal,
//                 and that terminal names its busbar
//   coupler     = a Bay with equipment and exactly two bay-external terminals
//   transformer = one link per winding, the winding's Bay naming its busbar
//
// Before this file existed, sld.csv recorded counts and nothing asserted on
// them, which is how "transformers=0 on every fixture" survived.
//
// Fixtures are addressed relative to the source tree. The large SCDs are
// gitignored, so those tests skip rather than fail when absent.
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "SldManager.h"

namespace {

std::string fixture(const std::string& rel) {
    return std::string(TEST_FIXTURE_DIR) + "/" + rel;
}

struct Counts {
    int buses = 0, feeders = 0, couplers = 0, transformers = 0;
    int unknownEndpoints = 0;
    int transformerWindings = 0;
    std::vector<std::string> warnings;
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
    c.buses        = static_cast<int>(plan.buses.size());
    c.feeders      = static_cast<int>(plan.feeders.size());
    c.couplers     = static_cast<int>(plan.couplers.size());
    c.transformers = static_cast<int>(plan.transformers.size());
    c.warnings     = plan.warnings;
    for (const auto& f : plan.feeders)
        if (f.endpointType.empty()) ++c.unknownEndpoints;
    for (const auto& t : plan.transformers)
        c.transformerWindings += static_cast<int>(t.windings.size());
    return c;
}

void skipIfAbsent(const std::string& p, const char* why) {
    if (!std::filesystem::exists(p)) GTEST_SKIP() << why;
}

// --- single bus, two lines ------------------------------------------------
// SCD_SB_2L: one equipment-free Bay (the busbar) and two equipment Bays, each
// holding DIS + CBR + CTR + VTR. So one bus and two feeders, no coupler.

TEST(SldAcceptance, SingleBusTwoLines) {
    const std::string p = fixture("SCD_SB_2L.scd");
    skipIfAbsent(p, "fixture absent");
    const Counts c = analyse(p);
    EXPECT_EQ(c.buses, 1);
    EXPECT_EQ(c.feeders, 2);
    EXPECT_EQ(c.couplers, 0);
    EXPECT_EQ(c.transformers, 0);
    EXPECT_TRUE(c.warnings.empty()) << c.warnings.front();
}

TEST(SldAcceptance, FeederRolesAreResolvedNotSniffed) {
    const std::string p = fixture("SCD_SB_2L.scd");
    skipIfAbsent(p, "fixture absent");
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));
    ASSERT_EQ(mgr.plan().feeders.size(), 2u);

    // The chain order used to be sniffed from equipment kinds, which placed the
    // CT after the line-side disconnector. It is now read from the bay's own
    // CE<->CN links, so the breaker must come before the CT.
    for (const auto& f : mgr.plan().feeders) {
        EXPECT_FALSE(f.roles.busSideDisconnector.empty()) << f.id;
        EXPECT_FALSE(f.roles.breaker.empty())             << f.id;
        EXPECT_FALSE(f.roles.currentTransformer.empty())  << f.id;
        EXPECT_FALSE(f.roles.voltageTransformer.empty())  << f.id;
        EXPECT_TRUE(f.roles.lineSideDisconnector.empty())
            << "one disconnector per feeder in this fixture";

        auto indexOf = [&](const std::string& id) {
            return std::find(f.chain.begin(), f.chain.end(), id) - f.chain.begin();
        };
        EXPECT_LT(indexOf(f.roles.busSideDisconnector), indexOf(f.roles.breaker))
            << "bus-side disconnector must precede the breaker";
        EXPECT_LT(indexOf(f.roles.breaker), indexOf(f.roles.currentTransformer))
            << "the CT hangs off the breaker side, not after the line side";
    }
}

// --- double bus with a coupler -------------------------------------------
// SCD_DB_COUPLER: two equipment-free Bays (BUSA, BUSB) and three equipment
// Bays. The COUPLER bay holds a lone CBR with two external terminals, so it is
// the coupler; L1 and L2 each have one.

TEST(SldAcceptance, DoubleBusWithCoupler) {
    const std::string p = fixture("SCD_DB_COUPLER.scd");
    skipIfAbsent(p, "fixture absent");
    const Counts c = analyse(p);
    EXPECT_EQ(c.buses, 2);
    EXPECT_EQ(c.feeders, 2);
    EXPECT_EQ(c.couplers, 1);
    EXPECT_TRUE(c.warnings.empty()) << c.warnings.front();
}

TEST(SldAcceptance, CouplerLinksTheTwoBusbars) {
    const std::string p = fixture("SCD_DB_COUPLER.scd");
    skipIfAbsent(p, "fixture absent");
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));
    ASSERT_EQ(mgr.plan().couplers.size(), 1u);

    const auto& cp = mgr.plan().couplers[0];
    EXPECT_TRUE(cp.isBreaker);
    EXPECT_FALSE(cp.busA.empty());
    EXPECT_FALSE(cp.busB.empty());
    EXPECT_NE(cp.busA, cp.busB);

    // Each feeder must hang off a real busbar.
    std::set<std::string> busIds;
    for (const auto& b : mgr.plan().buses) busIds.insert(b.id);
    for (const auto& f : mgr.plan().feeders)
        EXPECT_TRUE(busIds.count(f.busId)) << "feeder " << f.id << " has no bus";
}

// --- two voltage levels, one transformer ---------------------------------
// SCD_2VL_TR has two equipment-free Bays and no ConductingEquipment at all. The
// PowerTransformer is a child of Substation, so before this rewrite it produced
// transformers=0 on every fixture.

TEST(SldAcceptance, TwoVoltageLevelsOneTransformer) {
    const std::string p = fixture("SCD_2VL_TR.scd");
    skipIfAbsent(p, "fixture absent");
    const Counts c = analyse(p);
    EXPECT_EQ(c.buses, 2) << "one busbar bay per voltage level";
    EXPECT_EQ(c.transformers, 1);
    EXPECT_EQ(c.transformerWindings, 2)
        << "one link per winding, across both voltage levels";
    EXPECT_TRUE(c.warnings.empty()) << c.warnings.front();
}

// --- a real two-substation station ---------------------------------------
// substation.scd: 2 substations, 5 voltage levels, 45 ConductingEquipment,
// 6 busbar bays, 15 equipment bays, and T4/T3/T2 with 3/3/2 windings. Its
// ConnectivityNode short names repeat across bays ("IN" appears in most), so
// resolution must key on the full pathName.

TEST(SclAcceptance, RealSubstation) {
    const std::string p = fixture("../../tests_files/substation.scd");
    skipIfAbsent(p, "fixture absent (gitignored)");

    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    const auto& model = *sm.model();
    int busBays = 0, equipBays = 0;
    for (const auto& ss : model.substations)
        for (const auto& vl : ss.vlevels)
            for (const auto& bay : vl.bays)
                (bay.equipments.empty() ? busBays : equipBays)++;
    EXPECT_EQ(busBays, 6);
    EXPECT_EQ(equipBays, 15);

    const Counts c = analyse(p);
    EXPECT_EQ(c.buses, 6)         << "one bus per equipment-free bay";
    // 8 of the 15 equipment bays host a transformer winding (T4:3, T3:3, T2:2),
    // so they are transformer bays rather than feeders.
    EXPECT_EQ(c.transformers, 3)  << "T4, T3 and T2 were all reported as 0";
    EXPECT_EQ(c.transformerWindings, 8) << "3 + 3 + 2 windings";
    EXPECT_EQ(c.feeders, equipBays - 8) << "one feeder per remaining equipment bay";
    EXPECT_TRUE(c.warnings.empty()) << c.warnings.front();
    EXPECT_EQ(c.unknownEndpoints, 0);
}

TEST(SclAcceptance, RealSubstationTransformerWindsingsOnDistinctBuses) {
    const std::string p = fixture("../../tests_files/substation.scd");
    skipIfAbsent(p, "fixture absent (gitignored)");
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));

    for (const auto& t : mgr.plan().transformers) {
        EXPECT_GE(t.windings.size(), 2u) << t.transformerId;
        std::set<std::string> buses;
        for (const auto& w : t.windings) {
            EXPECT_FALSE(w.bus.empty())
                << t.transformerId << " winding " << w.winding << " has no bus";
            buses.insert(w.bus);
        }
        // T4 sits at 380/110/30 kV, so its three windings reach three
        // different busbars.
        EXPECT_EQ(buses.size(), t.windings.size())
            << t.transformerId << " windings collapsed onto one bus";
    }
}

// --- determinism ---------------------------------------------------------
// Ids persist into QSettings, so two runs of the same input must agree.

TEST(SldAcceptance, IsDeterministicAcrossRuns) {
    const std::string p = fixture("../../tests_files/substation.scd");
    skipIfAbsent(p, "fixture absent (gitignored)");
    const std::string a = [&]{ scl::SclManager sm; sm.loadScl(p);
                               sld::SldManager m(&sm); m.build(); return m.planJson(); }();
    const std::string b = [&]{ scl::SclManager sm; sm.loadScl(p);
                               sld::SldManager m(&sm); m.build(); return m.planJson(); }();
    EXPECT_EQ(a, b) << "planJson() must be byte-identical across runs";
}

TEST(SldAcceptance, LanesAreContiguousAndStable) {
    const std::string p = fixture("../../tests_files/substation.scd");
    skipIfAbsent(p, "fixture absent (gitignored)");
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));

    std::set<int> lanes;
    std::set<std::string> ids;
    for (const auto& f : mgr.plan().feeders) {
        EXPECT_TRUE(lanes.insert(f.laneIndex).second) << "duplicate lane " << f.laneIndex;
        EXPECT_TRUE(ids.insert(f.id).second)           << "duplicate feeder id " << f.id;
    }
    EXPECT_EQ(*lanes.rbegin(), static_cast<int>(lanes.size()) - 1)
        << "lanes must be 0..n-1 with no gaps";
}

// --- invariants that must hold for every fixture ------------------------

TEST(SldAcceptance, EveryFeederAndCouplerAttachesToAKnownBus) {
    const std::string p = fixture("../../tests_files/substation.scd");
    skipIfAbsent(p, "fixture absent (gitignored)");
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));

    std::set<std::string> busIds;
    for (const auto& b : mgr.plan().buses) busIds.insert(b.id);
    for (const auto& f : mgr.plan().feeders)
        EXPECT_EQ(busIds.count(f.busId), 1u) << f.id;
    for (const auto& c : mgr.plan().couplers) {
        EXPECT_EQ(busIds.count(c.busA), 1u) << c.couplerEquipId;
        EXPECT_EQ(busIds.count(c.busB), 1u) << c.couplerEquipId;
    }
    for (const auto& t : mgr.plan().transformers)
        for (const auto& w : t.windings)
            EXPECT_EQ(busIds.count(w.bus), 1u)
                << t.transformerId << "/" << w.winding;
}

TEST(SldAcceptance, NodeIdsAreUnique) {
    const std::string p = fixture("../../tests_files/substation.scd");
    skipIfAbsent(p, "fixture absent (gitignored)");
    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadScl(p)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));

    std::set<std::string> ids;
    for (const auto& n : mgr.plan().nodes)
        EXPECT_TRUE(ids.insert(n.id).second) << "duplicate node id " << n.id;
}

// A bay the engine cannot place must be reported, never silently dropped. This
// guards against the class of defect where an unrecognised shape yields an
// empty diagram.

TEST(SldAcceptance, UnexplainedShapeProducesNoFeedersAndIsReported) {
    // A Bay whose only equipment leaves the station entirely: no bus to hang
    // from. It must not be invented as a feeder or coupler.
    const std::string xml =
        "<?xml version=\"1.0\"?>"
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"SS1\">"
        "<VoltageLevel name=\"VL1\">"
        "<Bay name=\"BUS\"><ConnectivityNode name=\"B1\" pathName=\"SS1/VL1/BUS/B1\"/></Bay>"
        "<Bay name=\"ORPHAN\"><ConnectivityNode name=\"O1\" pathName=\"SS1/VL1/ORPHAN/O1\"/>"
        "<ConductingEquipment type=\"CBR\" name=\"CB9\">"
        "<Terminal name=\"t\" connectivityNode=\"SS1/VL1/ORPHAN/O1\"/>"
        "<Terminal name=\"t2\" cNodeName=\"ELSEWHERE\"/>"
        "</ConductingEquipment></Bay>"
        "</VoltageLevel></Substation></SCL>\n";

    scl::SclManager sm;
    ASSERT_TRUE(static_cast<bool>(sm.loadSclString(xml)));
    sld::SldManager mgr(&sm);
    ASSERT_TRUE(static_cast<bool>(mgr.build()));

    EXPECT_EQ(mgr.plan().buses.size(), 1u);
    EXPECT_EQ(mgr.plan().feeders.size(), 0u)
        << "a bay with no resolvable bus must not become a feeder";
    EXPECT_EQ(mgr.plan().couplers.size(), 0u);
    EXPECT_FALSE(mgr.plan().warnings.empty())
        << "an unplaceable bay must be reported, not silently dropped";
}

} // namespace