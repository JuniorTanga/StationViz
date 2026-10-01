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
