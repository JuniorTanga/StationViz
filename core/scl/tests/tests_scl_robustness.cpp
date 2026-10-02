#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <string>

#include "SclManager.h"
#include "SclParser.h"
#include "Result.h"

using namespace scl;

namespace {

std::string loadInto(SclManager& mgr, const std::string& xml) {
    return mgr.loadSclString(xml).error().message;
}

static const std::string kHeader = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

// --- 1. A ConnectedAP naming an IED that is not in the file.
// Before the fix this threw std::out_of_range out of loadScl, escaping into
// the Qt slot and terminating the process, while leaving the manager loaded.
TEST(SclRobustness, ConnectedApWithUnknownIedDoesNotThrow) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\">\n"
        "<Communication><SubNetwork name=\"SN1\" type=\"8-MMS\">"
        "<ConnectedAP iedName=\"REMOTE_IED_NOT_IN_FILE\" apName=\"AP1\">"
        "<GSE ldInst=\"LD1\" cbName=\"GoCB01\">"
        "<P type=\"MAC-Address\">01-0C-CD-01-00-01</P><P type=\"APPID\">0001</P></GSE>"
        "<SMV ldInst=\"LD1\" cbName=\"SvCB01\">"
        "<P type=\"MAC-Address\">01-0C-CD-01-00-01</P><P type=\"APPID\">4001</P></SMV>"
        "</ConnectedAP></SubNetwork></Communication></SCL>\n";

    SclManager mgr;
    ASSERT_NO_THROW(loadInto(mgr, xml));

    bool reported = false;
    for (const auto& d : mgr.diagnostics())
        if (d.code == ErrorCode::InvalidIedRef) reported = true;
    EXPECT_TRUE(reported) << "an unknown ConnectedAP@iedName must be diagnosed";
}

// --- 2. Malformed <Voltage>. std::stod used to throw on garbage and silently
// accepted nan/inf, which then reached the JSON output.
TEST(SclRobustness, MalformedVoltageDoesNotThrow) {
    for (const char* v : {"abc", "1e999", "nan", "inf", "", "380.5V"}) {
        const std::string xml = std::string(kHeader) +
            "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
            "<VoltageLevel name=\"VL1\"><Voltage unit=\"V\" multiplier=\"k\">" + v +
            "</Voltage><Bay name=\"B1\">"
            "<ConnectivityNode name=\"C1\" pathName=\"S1/VL1/B1/C1\"/></Bay>"
            "</VoltageLevel></Substation></SCL>\n";

        SclManager mgr;
        ASSERT_NO_THROW(loadInto(mgr, xml)) << "Voltage=\"" << v << "\"";
        ASSERT_NE(mgr.model(), nullptr);
        ASSERT_FALSE(mgr.model()->substations.empty());
        const auto& vl = mgr.model()->substations[0].vlevels[0];
        ASSERT_TRUE(vl.voltage.has_value());
        EXPECT_TRUE(std::isfinite(vl.voltage->value))
            << "Voltage=\"" << v << "\" produced a non-finite value";
    }
}

TEST(SclRobustness, WellFormedVoltageIsParsed) {
    const std::string xml = std::string(kHeader) +
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
        "<VoltageLevel name=\"VL1\"><Voltage unit=\"V\" multiplier=\"k\">380.5</Voltage>"
        "<Bay name=\"B1\"><ConnectivityNode name=\"C1\" pathName=\"S1/VL1/B1/C1\"/></Bay>"
        "</VoltageLevel></Substation></SCL>\n";
    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    const auto& v = mgr.model()->substations[0].vlevels[0].voltage;
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(v->valid);
    EXPECT_DOUBLE_EQ(v->value, 380.5);
}

// --- 3. Prefixed XML namespace. pugixml is not namespace aware, so a
// <scl:SCL> document used to fail with "Missing <SCL> root".
TEST(SclRobustness, PrefixedNamespaceIsAccepted) {
    const std::string xml =
        "<?xml version=\"1.0\"?>\n"
        "<scl:SCL xmlns:scl=\"http://www.iec.ch/61850/2003/SCL\" version=\"2007\" revision=\"B\">\n"
        "<scl:Header id=\"NS\" toolID=\"t\"><scl:Text>hello</scl:Text></scl:Header>\n"
        "<scl:Substation name=\"S1\"><scl:VoltageLevel name=\"VL1\">\n"
        "<scl:Bay name=\"B1\"><scl:ConnectivityNode name=\"C1\" pathName=\"S1/VL1/B1/C1\"/></scl:Bay>"
        "</scl:VoltageLevel></scl:Substation></scl:SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    ASSERT_NE(mgr.model(), nullptr);
    ASSERT_EQ(mgr.model()->substations.size(), 1u);
    EXPECT_EQ(mgr.model()->substations[0].name, "S1");
    EXPECT_EQ(mgr.model()->substations[0].vlevels[0].bays[0].name, "B1");
    EXPECT_EQ(mgr.model()->header.id, "NS");
}

// --- 4. tns:AccessPoint allows an unbounded number of Server elements.
// Reading only ap.child("Server") silently dropped every LD after the first.
TEST(SclRobustness, AccessPointWithTwoServersKeepsBothLDevices) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\"><AccessPoint name=\"AP1\">"
        "<Server><LDevice inst=\"LD1\"><LN0 lnClass=\"LLN0\"/></LDevice></Server>"
        "<Server><LDevice inst=\"LD2\"><LN0 lnClass=\"LLN0\"/></LDevice></Server>"
        "</AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    ASSERT_EQ(mgr.model()->ieds.size(), 1u);
    ASSERT_EQ(mgr.model()->ieds[0].accessPoints.size(), 1u);
    EXPECT_EQ(mgr.model()->ieds[0].accessPoints[0].ldevices.size(), 2u);
}

// --- 5. ED2 wraps PowerTransformer in <Equipment>/<Container>, which made
// powerTransformers=0 on a valid file.
TEST(SclRobustness, EquipmentWrappedPowerTransformerIsFound) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
        "<Equipment name=\"EQ1\">"
        "<PowerTransformer name=\"T1\" type=\"PT\">"
        "<TransformerWinding name=\"HV\" type=\"PTW\">"
        "<Terminal name=\"t1\" connectivityNode=\"S1/VL1/B1/C1\"/>"
        "<TapChanger name=\"TC1\" type=\"LTC\"/>"
        "<TapChanger name=\"TC2\" type=\"DETC\"/>"
        "<PhaseTapChanger name=\"PTC\" type=\"PTC\"/>"
        "</TransformerWinding></PowerTransformer></Equipment>"
        "</Substation></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    ASSERT_EQ(mgr.model()->substations.size(), 1u);
    const auto& trs = mgr.model()->substations[0].powerTransformers;
    ASSERT_EQ(trs.size(), 1u) << "PowerTransformer inside <Equipment> was skipped";
    ASSERT_EQ(trs[0].windings.size(), 1u);
    EXPECT_EQ(trs[0].windings[0].tapChangers.size(), 3u)
        << "a winding may carry several TapChanger plus a PhaseTapChanger";
}

TEST(SclRobustness, DirectPowerTransformerStillParsed) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
        "<PowerTransformer name=\"T1\" type=\"PT\"><TransformerWinding name=\"HV\" type=\"PTW\"/>"
        "</PowerTransformer></Substation></SCL>\n";
    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    EXPECT_EQ(mgr.model()->substations[0].powerTransformers.size(), 1u);
}

// --- 6. Result<T> accessors used to dereference a disengaged optional, which
// is undefined behaviour rather than a diagnosable failure.
TEST(ResultAccess, AccessorsThrowInsteadOfUndefinedBehaviour) {
    Result<int> good(42);
    EXPECT_TRUE(good.has_value());
    EXPECT_EQ(good.value(), 42);
    EXPECT_THROW((void)good.error(), BadResultAccess);

    Result<int> bad(Error{ErrorCode::LogicError, "boom"});
    EXPECT_FALSE(bad);
    EXPECT_THROW((void)bad.value(), BadResultAccess);
    EXPECT_THROW((void)bad.operator->(), BadResultAccess);
    EXPECT_EQ(bad.error().code, ErrorCode::LogicError);
}

TEST(ResultAccess, ErrorCodesHaveNames) {
    EXPECT_STREQ(to_string(ErrorCode::InvalidIedRef), "InvalidIedRef");
    EXPECT_STREQ(to_string(ErrorCode::BrokenConnectivityNode), "BrokenConnectivityNode");
    EXPECT_STREQ(to_string(ErrorCode::DuplicateDataSetName), "DuplicateDataSetName");
}

// --- 7. matchCN must handle both the '/' path form and the ':' logical form.
// lastSegment() split on '/' only, so comparing a full path against a logical
// key could never succeed.
TEST(SclIndex, MatchCnHandlesBothSeparators) {
    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(kHeader +
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
        "<VoltageLevel name=\"VL1\"><Bay name=\"B1\">"
        "<ConnectivityNode name=\"C1\" pathName=\"S1/VL1/B1/C1\"/>"
        "</Bay></VoltageLevel></Substation></SCL>\n"));

    EXPECT_TRUE(mgr.matchCN("S1/VL1/B1/C1", "S1:VL1:B1:C1"));
    EXPECT_TRUE(mgr.matchCN("S1/VL1/B1/C1", "S1/VL1/B1/C1"));
    EXPECT_FALSE(mgr.matchCN("S1/VL1/B1/C1", "S1:VL1:B1:OTHER"));
}

// --- 8. hasErrors(): loadScl returns Ok even when diagnostics hold errors.
TEST(SclDiagnostics, HasErrorsReflectsSeverity) {
    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(kHeader +
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
        "<VoltageLevel name=\"VL1\"><Bay name=\"B1\">"
        "<ConnectivityNode name=\"C1\" pathName=\"S1/VL1/B1/C1\"/>"
        "<ConductingEquipment type=\"CBR\" name=\"CB1\">"
        "<Terminal name=\"t\" connectivityNode=\"S1/VL1/B1/MISSING\"/>"
        "</ConductingEquipment></Bay></VoltageLevel></Substation></SCL>\n"));

    EXPECT_TRUE(mgr.loadSclString(kHeader +
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
        "<VoltageLevel name=\"VL1\"><Bay name=\"B1\">"
        "<ConnectivityNode name=\"C1\" pathName=\"S1/VL1/B1/C1\"/>"
        "<ConductingEquipment type=\"CBR\" name=\"CB1\">"
        "<Terminal name=\"t\" connectivityNode=\"S1/VL1/B1/MISSING\"/>"
        "</ConductingEquipment></Bay></VoltageLevel></Substation></SCL>\n"));

    bool found = false;
    for (const auto& d : mgr.diagnostics())
        if (d.code == ErrorCode::BrokenConnectivityNode) found = true;
    EXPECT_TRUE(found) << "Terminal pointing at a missing CN must be diagnosed";
    EXPECT_TRUE(mgr.hasErrors());
}

// --- 9. A missing LDevice must be reported once, not as a cascade.
TEST(SclDiagnostics, MissingLDeviceIsReportedOnce) {
    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\"><GSEControl name=\"GoCB01\" datSet=\"DS1\"/></LN0>"
        "</LDevice></Server></AccessPoint></IED>"
        "<Communication><SubNetwork name=\"SN1\" type=\"8-MMS\">"
        "<ConnectedAP iedName=\"IED1\" apName=\"AP1\">"
        "<GSE ldInst=\"LD99\" cbName=\"GoCB01\"><P type=\"APPID\">0001</P></GSE>"
        "</ConnectedAP></SubNetwork></Communication></SCL>\n"));

    int ldRefs = 0, cascades = 0;
    for (const auto& d : mgr.diagnostics()) {
        if (d.code == ErrorCode::InvalidLdRef) ++ldRefs;
        if (d.code == ErrorCode::ControlBlockNotFound) ++cascades;
    }
    EXPECT_EQ(ldRefs, 1) << "the root cause should be reported exactly once";
    EXPECT_EQ(cascades, 0)
        << "ControlBlockNotFound is a cascade: with no LDevice the control "
           "block was never looked up";
}

// --- 10. Duplicate ids must be diagnosed instead of silently collapsing.
TEST(SclDiagnostics, DuplicateConnectivityNodePathIsDiagnosed) {
    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(kHeader +
        "<SCL version=\"2007\" revision=\"B\"><Substation name=\"S1\">"
        "<VoltageLevel name=\"VL1\"><Bay name=\"B1\">"
        "<ConnectivityNode name=\"C1\" pathName=\"S1/VL1/B1/DUP\"/>"
        "<ConnectivityNode name=\"C2\" pathName=\"S1/VL1/B1/DUP\"/>"
        "</Bay></VoltageLevel></Substation></SCL>\n"));

    bool found = false;
    for (const auto& d : mgr.diagnostics())
        if (d.code == ErrorCode::DuplicateConnectivityNode) found = true;
    EXPECT_TRUE(found) << "two CNs sharing a pathName must be diagnosed";
}

TEST(SclDiagnostics, DuplicateDataSetNameIsDiagnosed) {
    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<DataSet name=\"DS\"><FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"MMXU\" lnInst=\"1\" doName=\"Op\" fc=\"MX\"/></DataSet>"
        "<DataSet name=\"DS\"><FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"MMXU\" lnInst=\"1\" doName=\"Op\" fc=\"MX\"/></DataSet>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n"));

    bool found = false;
    for (const auto& d : mgr.diagnostics())
        if (d.code == ErrorCode::DuplicateDataSetName) found = true;
    EXPECT_TRUE(found) << "two DataSets with the same name in one LN0 must be diagnosed";
}

} // namespace

// ============================================================================
// ReportControl / RptEnabled / Inputs / ExtRef.
//
// These are the control blocks a client subscribes to for B-reports. They were
// entirely absent, which made it impossible to write a report handler and so
// blocked the whole FAT supervision loop.
// ============================================================================

TEST(SclReportControl, ReportControlIsParsed) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<DataSet name=\"dsUrgent\">"
        "<FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"GGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\"/>"
        "</DataSet>"
        "<ReportControl name=\"urcbUrgent\" rptID=\"IED1LD1/LLN0$RP$urcbUrgent\""
        " confRev=\"1\" buffered=\"true\" intgPd=\"5000\" desc=\"urgent alarms\">"
        "<TrgOps dchg=\"true\" qchg=\"true\" dupd=\"false\" intg=\"false\" gi=\"true\"/>"
        "<OptFields sequenceNumber=\"true\" timeStamp=\"true\" reasonForInclusion=\"true\"/>"
        "</ReportControl>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    ASSERT_EQ(mgr.model()->ieds.size(), 1u);
    const auto& ap = mgr.model()->ieds[0].accessPoints[0];
    ASSERT_EQ(ap.ldevices.size(), 1u);
    const auto& rcs = ap.ldevices[0].ln0.rptCtrls;
    ASSERT_EQ(rcs.size(), 1u);

    const auto& rc = rcs[0];
    EXPECT_EQ(rc.name, "urcbUrgent");
    EXPECT_EQ(rc.rptID, "IED1LD1/LLN0$RP$urcbUrgent");
    EXPECT_EQ(rc.confRev, "1");
    EXPECT_EQ(rc.desc, "urgent alarms");
    EXPECT_EQ(rc.intgPd, "5000");
    EXPECT_TRUE(rc.buffered) << "@buffered=\"true\" was ignored";

    // TrgOps bits are named attributes (dchg/qchg/dupd/intg/gi).
    EXPECT_TRUE (rc.trgOps.dataChange);
    EXPECT_TRUE (rc.trgOps.qualityChange);
    EXPECT_FALSE(rc.trgOps.dataUpdate);
    EXPECT_FALSE(rc.trgOps.integrity);
    EXPECT_TRUE (rc.trgOps.generalInterrogation);

    // OptFields likewise.
    EXPECT_TRUE (rc.optFields.seqNum);
    EXPECT_TRUE (rc.optFields.timeStamp);
    EXPECT_TRUE (rc.optFields.reasonForInclusion);
    EXPECT_FALSE(rc.optFields.dataSet);
}

TEST(SclReportControl, ReportControlBlockIsAlsoParsed) {
    // tns:ReportControlBlock is the historical name for a buffered block and
    // carries the same attributes.
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<DataSet name=\"brcbTime\"><FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"GGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\"/></DataSet>"
        "<ReportControlBlock name=\"brcbTime\" rptID=\"IED1LD1/LLN0$BR$brcbTime\" confRev=\"1\" buffered=\"true\"/>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    const auto& rcs = mgr.model()->ieds[0].accessPoints[0].ldevices[0].ln0.rptCtrls;
    ASSERT_EQ(rcs.size(), 1u);
    EXPECT_EQ(rcs[0].name, "brcbTime");
    EXPECT_TRUE(rcs[0].buffered);
}

TEST(SclReportControl, TrgOpsDefaultsWhenAbsent) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<DataSet name=\"urcbA\"><FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"GGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\"/></DataSet>"
        "<ReportControl name=\"urcbA\" rptID=\"IED1LD1/LLN0$RP$urcbA\" confRev=\"1\"/>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    const auto& rc = mgr.model()->ieds[0].accessPoints[0].ldevices[0].ln0.rptCtrls[0];
    // No <TrgOps>: an IED shipped this way delivers nothing until an integrity
    // period fires, so the defaults must ask for data/quality change + GI.
    EXPECT_TRUE (rc.trgOps.dataChange);
    EXPECT_TRUE (rc.trgOps.qualityChange);
    EXPECT_TRUE (rc.trgOps.generalInterrogation);
    EXPECT_FALSE(rc.trgOps.integrity);
}

TEST(SclReportControl, ExplicitMissingDatasetIsAnError) {
    // An explicit @datSet that resolves to nothing is an unambiguous
    // authoring error.
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<ReportControl name=\"urcbA\" datSet=\"dsGhost\" rptID=\"IED1LD1/LLN0$RP$urcbA\" confRev=\"1\"/>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    bool found = false;
    for (const auto& d : mgr.diagnostics())
        if (d.code == ErrorCode::DatasetNotFound && d.severity == SclManager::Severity::Error)
            found = true;
    EXPECT_TRUE(found) << "an explicit @datSet with no DataSet must be an error";
    EXPECT_TRUE(mgr.hasErrors());
}

TEST(SclReportControl, ImplicitMissingDatasetIsOnlyAWarning) {
    // No @datSet and no matching <DataSet>: the RCB relies on an implicit
    // DataSet the SCL never declares. Real vendor files do this extensively
    // (the Siemens export declares 55 RCB names and one DataSet), and the block
    // is still subscribable, so this must not fail the document.
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<ReportControl name=\"urcbGhost\" rptID=\"IED1LD1/LLN0$RP$urcbGhost\" confRev=\"1\"/>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    bool warned = false;
    for (const auto& d : mgr.diagnostics())
        if (d.code == ErrorCode::DatasetNotFound && d.severity == SclManager::Severity::Warning)
            warned = true;
    EXPECT_TRUE(warned) << "an undeclared implicit DataSet should be reported";
    EXPECT_FALSE(mgr.hasErrors())
        << "a vendor-style implicit DataSet must not fail a valid document";
}

TEST(SclReportControl, ImplicitDatasetNameIsAccepted) {
    // An unbuffered RCB addresses an implicit DataSet named after the control
    // block, so a missing @datSet is legitimate when that DataSet exists.
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<DataSet name=\"urcbA\"><FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"GGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\"/></DataSet>"
        "<ReportControl name=\"urcbA\" rptID=\"IED1LD1/LLN0$RP$urcbA\" confRev=\"1\" buffered=\"false\"/>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    for (const auto& d : mgr.diagnostics())
        EXPECT_NE(d.code, ErrorCode::DatasetNotFound)
            << "an unbuffered RCB must accept the implicit DataSet: " << d.message;
}

TEST(SclReportControl, ExtRefIsParsed) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<DataSet name=\"dsWithExt\">"
        "<FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"GGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\"/>"
        "<Inputs><ExtRef iedName=\"IED2\" ldInst=\"LD1\" prefix=\"\" lnClass=\"XCBR\" lnInst=\"1\""
        " doName=\"Pos\" daName=\"stVal\" fc=\"ST\" intgPd=\"1000\"/></Inputs>"
        "</DataSet>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));
    const auto& ds = mgr.model()->ieds[0].accessPoints[0].ldevices[0].ln0.datasets[0];
    ASSERT_EQ(ds.members.size(), 1u);
    ASSERT_EQ(ds.extRefs.size(), 1u) << "<Inputs><ExtRef> was not parsed";
    EXPECT_EQ(ds.extRefs[0].iedName, "IED2");
    EXPECT_EQ(ds.extRefs[0].lnClass, "XCBR");
    EXPECT_EQ(ds.extRefs[0].doName, "Pos");
    EXPECT_EQ(ds.extRefs[0].daName, "stVal");
    EXPECT_EQ(ds.extRefs[0].intgPd, "1000");
}

TEST(SclReportControl, RealSiemensIcdYieldsReportControls) {
    // station1.scd is a real Siemens SIEDIG export: 36 IEDs and ~1980
    // ReportControl elements. Guarded because the file is gitignored.
    SclManager mgr;
    const auto st = mgr.loadScl("tests/tests_files/station1.scd");
    if (!st) GTEST_SKIP() << "station1.scd not available (gitignored large fixture)";
    ASSERT_NE(mgr.model(), nullptr);

    size_t rcbCount = 0, bufferedCount = 0, withRptId = 0;
    for (const auto& ied : mgr.model()->ieds) {
        auto count = [&](const LogicalDevice& ld) {
            for (const auto& rc : ld.ln0.rptCtrls) {
                ++rcbCount;
                if (rc.buffered) ++bufferedCount;
                if (!rc.rptID.empty()) ++withRptId;
            }
        };
        for (const auto& ld : ied.ldevices) count(ld);
        for (const auto& ap : ied.accessPoints)
            for (const auto& ld : ap.ldevices) count(ld);
    }
    printf("      station1.scd: %zu ReportControls (%zu buffered, %zu with rptID)\n",
           rcbCount, bufferedCount, withRptId);
    EXPECT_GT(rcbCount, 1000u);
    EXPECT_EQ(withRptId, rcbCount) << "every RCB should carry an rptID";
    EXPECT_GT(bufferedCount, 0u) << "expected some buffered RCBs in a real IED";
}

TEST(SclReportControl, ManagerIndexesAndResolvesRcbReferences) {
    const std::string xml = kHeader +
        "<SCL version=\"2007\" revision=\"B\"><IED name=\"IED1\">"
        "<AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD1\">"
        "<LN0 lnClass=\"LLN0\">"
        "<DataSet name=\"dsA\"><FCDA ldInst=\"LD1\" prefix=\"\" lnClass=\"GGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\"/></DataSet>"
        "<ReportControl name=\"urcbA\" datSet=\"dsA\" confRev=\"1\"/>"
        "<ReportControl name=\"brcbB\" datSet=\"dsA\" confRev=\"1\" buffered=\"true\"/>"
        "</LN0></LDevice></Server></AccessPoint></IED></SCL>\n";

    SclManager mgr;
    ASSERT_TRUE(mgr.loadSclString(xml));

    const auto& idx = mgr.reportControls();
    EXPECT_EQ(idx.size(), 2u);
    EXPECT_NE(idx.find("IED1|LD1|urcbA"), idx.end());
    EXPECT_NE(idx.find("IED1|LD1|brcbB"), idx.end());

    const auto forIed = mgr.reportControlsOf("IED1");
    ASSERT_EQ(forIed.size(), 2u);
    EXPECT_EQ(forIed[0]->name, "brcbB");
    EXPECT_EQ(forIed[1]->name, "urcbA");
    EXPECT_TRUE(mgr.reportControlsOf("NO_SUCH_IED").empty());

    // With no rptID in the SCL, the reference must be built with '$'
    // separators: '$RP$' for unbuffered, '$BR$' for buffered.
    EXPECT_EQ(SclManager::rcbReference("LD1", *forIed[1]), "LD1/LLN0$RP$dsA");
    EXPECT_EQ(SclManager::rcbReference("LD1", *forIed[0]), "LD1/LLN0$BR$dsA");
}

TEST(SclReportControl, RcbReferencePrefersSclRptId) {
    // rptID is what the server reports and what libiec61850 matches, so it
    // must win over any reconstruction from the parts.
    ReportControlMeta rc;
    rc.name = "urcbA";
    rc.datSet = "dsA";
    rc.rptID = "IED1LD1/LLN0$RP$urcbA";
    EXPECT_EQ(SclManager::rcbReference("LD1", rc), "IED1LD1/LLN0$RP$urcbA");
}
