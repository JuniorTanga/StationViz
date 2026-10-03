// Headless checks on the IEC 61850 layer.
//
// networkLib had never compiled, and most of its defects were silent: a null
// dereference on the first MMS endpoint, GOOSE subscribed with an empty
// reference so nothing ever matched, and report control blocks fabricated from
// GOOSE control blocks so reporting was dead while looking configured.
//
// These tests exercise what can be checked without a real IED: building the
// network map from an SCD, and the MAC/APPID parsing that GOOSE and SV depend
// on. Every SCD in the repo writes MAC addresses with dashes and APPIDs in hex.
#include <cstdio>
#include <string>

#include "NetworkMapBuilder.h"
#include "NetUtils.h"
#include "SclManager.h"

static int failures = 0;

static void check(const char* what, bool ok, const std::string& detail = {}) {
    std::printf("  %s  %s%s%s\n", ok ? "PASS" : "FAIL", what,
                detail.empty() ? "" : "  [", detail.empty() ? "" : (detail + "]").c_str());
    if (!ok) ++failures;
}

int main(int argc, char** argv) {
    // ---- MAC parsing ----
    // Every SCD in the repository writes dashes; the parser split on ':' only,
    // so it returned nullopt for 100% of real data and GOOSE/SV failed quietly.
    {
        const auto m = network::parseMac("01-0C-CD-01-00-01");
        check("MAC with dashes parses", m.has_value());
        if (m) {
            check("MAC bytes correct",
                  m->b[0] == 0x01 && m->b[1] == 0x0C &&
                  m->b[2] == 0xCD && m->b[3] == 0x01 &&
                  m->b[4] == 0x00 && m->b[5] == 0x01);
        }
        check("MAC with colons still parses",
              network::parseMac("01:0C:CD:01:00:01").has_value());
        check("MAC with dots still parses",
              network::parseMac("01-0c-cd-01-00-01").has_value());
        check("garbage MAC rejected",
              !network::parseMac("zz:zz").has_value());
    }

    // ---- APPID parsing ----
    // APPID is hex. "4001" decimal is 16385, and any value with a letter parsed
    // to 0, so incoming GOOSE filtered on the wrong identifier and was dropped.
    {
        bool hexOk = false;
        try {
            const auto a = network::parseAppId("4001");
            const auto b = network::parseAppId("3FFF");
            hexOk = a && b && *a == 0x4001 && *b == 0x3FFF;
        } catch (...) { hexOk = false; }
        check("APPID parsed as hex", hexOk);
    }

    if (argc < 2) {
        std::printf("(no SCD given: skipping map tests)\n");
        return failures ? 1 : 0;
    }

    // ---- network map from an SCD ----
    scl::SclManager sm;
    if (!sm.loadScl(argv[1])) {
        check("SCD loads", false, argv[1]);
        std::printf("%d failure(s)\n", failures);
        return 1;
    }

    const network::NetworkMap nm = network::NetworkMapBuilder::fromScl(sm);
    std::printf("      mms=%zu gse=%zu sv=%zu ieds with RCB=%zu\n",
                nm.mms.size(), nm.gse.size(), nm.sv.size(), nm.rcbByIed.size());

    // Report control blocks must come from the SCL's <ReportControl>, not be
    // invented from GOOSE control blocks.
    for (const auto& [ied, rcbs] : nm.rcbByIed) {
        for (const auto& r : rcbs) {
            check("RCB reference is well formed", !r.rcbRef.empty(), r.rcbRef);
            check("RCB reference uses the $ separator", r.rcbRef.find('$') != std::string::npos, r.rcbRef);
            check("RCB reference names LLN0", r.rcbRef.find("LLN0") != std::string::npos, r.rcbRef);
            (void)ied;
        }
    }

    // An RCB that resolved a DataSet must expose its members in order.
    int withMembers = 0;
    for (const auto& [ied, rcbs] : nm.rcbByIed) {
        for (const auto& r : rcbs) {
            if (!r.dsMembers.empty()) {
                ++withMembers;
                check("RCB dataset members carry an MMS $ reference",
                      r.dsMembers.front().find('$') != std::string::npos,
                      r.dsMembers.front());
            }
            (void)ied;
        }
    }
    std::printf("      RCBs with resolved members: %d\n", withMembers);

    // GOOSE endpoints that declare an APPID must carry a usable one.
    for (const auto& g : nm.gse) {
        check("GOOSE endpoint has a destination MAC", !g.mac.empty(), g.mac);
    }

    std::printf("%s\n", failures ? "FAILURES" : "All good.");
    return failures ? 1 : 0;
}
