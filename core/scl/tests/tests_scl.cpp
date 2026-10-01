#include <gtest/gtest.h>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <string>

#include "SclManager.h"
#include "SclParser.h"

using namespace scl;

// Unique per call so parallel/repeated runs cannot collide, and cleaned up by
// the caller. The previous version wrote a fixed relative path into whatever
// the CWD happened to be.
static std::atomic<unsigned> g_tmpCounter{0};
static std::string write_temp_scd(const std::string& xml) {
    std::string path = "test_tmp_" + std::to_string(g_tmpCounter++) + ".scd";
    std::ofstream f(path, std::ios::binary);
    f << xml;
    f.close();
    return path;
}
static void remove_temp_scd(const std::string& path) { std::remove(path.c_str()); }

static const char* kSCL = R"(<?xml version="1.0" encoding="UTF-8"?>
<SCL version="2007" revision="B">
  <Substation name="S1">
    <VoltageLevel name="VL1" nomFreq="50">
      <Voltage unit="V" multiplier="k">225</Voltage>
      <Bay name="BAY1">
        <ConnectivityNode name="CN1" pathName="S1/VL1/BAY1/CN1"/>
        <ConductingEquipment name="Q01" type="XSWI">
          <Terminal name="T1" connectivityNode="S1/VL1/BAY1/CN1"/>
          <LNode iedName="IED1" ldInst="LD1" lnClass="XSWI" lnInst="1"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>

  <IED name="IED1" manufacturer="ACME" type="CTRL">
    <AccessPoint name="AP1">
      <Address>
        <P type="IP">10.0.0.1</P>
        <P type="Port">102</P>
      </Address>
      <Server>
        <LDevice inst="LD1">
          <LN0 lnClass="LLN0">
            <DataSet name="DS_CB">
              <!-- 1) FCDA sans ldInst -> doit retomber sur LD1 -->
              <FCDA lnClass="XSWI" lnInst="1" doName="Pos" daName="stVal" fc="ST"/>
              <!-- 2) FCDA avec ldInst explicite -->
              <FCDA ldInst="LD1" lnClass="XSWI" lnInst="1" doName="Pos" daName="ctlVal" fc="CO"/>
            </DataSet>
            <GSEControl name="GoCB01" datSet="DS_CB" appID="0001"/>
            <SampledValueControl name="SvCB01" datSet="DS_CB" smvID="0101"/>
          </LN0>
          <LN lnClass="XSWI" inst="1"/>
          <LN lnClass="MMXU" inst="1"/>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>

  <Communication>
    <SubNetwork name="StationBus" type="8-MMS">
      <ConnectedAP iedName="IED1" apName="AP1">
        <Address>
          <P type="IP">10.0.0.1</P>
          <P type="Port">102</P>
        </Address>
        <GSE ldInst="LD1" cbName="GoCB01">
          <Address>
            <P type="APPID">1001</P>
            <P type="MAC-Address">01-0C-CD-01-00-01</P>
            <P type="VLAN-ID">100</P>
            <P type="VLAN-PRIORITY">4</P>
          </Address>
        </GSE>
        <SMV ldInst="LD1" cbName="SvCB01">
          <Address>
            <P type="APPID">2001</P>
            <P type="MAC-Address">01-0C-CD-04-00-01</P>
            <P type="VLAN-ID">200</P>
            <P type="VLAN-PRIORITY">5</P>
            <P type="SmpRate">80</P>
          </Address>
        </SMV>
      </ConnectedAP>
    </SubNetwork>
  </Communication>
</SCL>
)";

TEST(Scl, LoadAndIndexes) {
    // 1) Charger le SCL via SclManager (depuis un fichier temporaire)
    auto path = write_temp_scd(kSCL);
    SclManager mgr;
    auto st = mgr.loadScl(path);
    ASSERT_TRUE(static_cast<bool>(st)) << "loadScl failed";

    // 2) Équipements vus depuis IEDs (ignore MMXU/LLN0/LPHD)
    auto eqs = mgr.collectEquipmentFromIEDs(true);
    // On s'attend à XSWI1 uniquement
    ASSERT_FALSE(eqs.empty());
    // Filtrer sur IED1/LD1/XSWI1
    auto it = std::find_if(eqs.begin(), eqs.end(), [](const EquipmentFromIED& e){
        return e.iedName=="IED1" && e.ldInst=="LD1" && e.lnClass=="XSWI" && e.lnInst=="1";
    });
    ASSERT_TRUE(it != eqs.end()) << "XSWI1 not found in collectEquipmentFromIEDs()";

    // 3) Ancrage primaire (CE) recollé via la topologie
    // attendu : "S1:VL1:BAY1:CE:Q01"
    bool hasAnchor = false;
    for (auto& a : it->primaryAnchors) {
        if (a == "S1:VL1:BAY1:CE:Q01") { hasAnchor = true; break; }
    }
    EXPECT_TRUE(hasAnchor) << "primary anchor not attached to XSWI1";

    // 4) Normalisation CN & arêtes CE->CN
    auto edges = mgr.collectSldEdges();
    ASSERT_FALSE(edges.empty());
    bool foundEdge = false;
    for (const auto& e : edges) {
        if (e.ssName=="S1" && e.vlName=="VL1" && e.bayName=="BAY1" &&
            e.ceName=="Q01" && e.cnPath=="S1:VL1:BAY1:CN1") {
            foundEdge = true; break;
        }
    }
    EXPECT_TRUE(foundEdge) << "Expected normalized edge to logical CN not found";

    // 5) matchCN (full <-> logical)
    EXPECT_TRUE(mgr.matchCN("S1/VL1/BAY1/CN1", "S1:VL1:BAY1:CN1"));

    // 6) Endpoints MMS
    auto& mms = mgr.mmsEndpoints();
    auto itM = mms.find("IED1|AP1");
    ASSERT_TRUE(itM != mms.end());
    EXPECT_EQ(itM->second.ip, "10.0.0.1");
    EXPECT_EQ(itM->second.port, "102");

    // 7) Endpoints GSE
    auto& gses = mgr.gseEndpoints();
    auto itG = gses.find("IED1|LD1|GoCB01");
    ASSERT_TRUE(itG != gses.end());
    EXPECT_EQ(itG->second.datasetRef, "DS_CB");
    EXPECT_EQ(itG->second.appid, "1001");
    EXPECT_EQ(itG->second.mac, "01-0C-CD-01-00-01");
    EXPECT_EQ(itG->second.vlanId, "100");
    EXPECT_EQ(itG->second.vlanPrio, "4");

    // 8) Endpoints SMV
    auto& svs = mgr.svEndpoints();
    auto itS = svs.find("IED1|LD1|SvCB01");
    ASSERT_TRUE(itS != svs.end());
    EXPECT_EQ(itS->second.datasetRef, "DS_CB");
    EXPECT_EQ(itS->second.appid, "2001");
    EXPECT_EQ(itS->second.mac, "01-0C-CD-04-00-01");
    EXPECT_EQ(itS->second.vlanId, "200");
    EXPECT_EQ(itS->second.vlanPrio, "5");
    EXPECT_EQ(itS->second.smpRate, "80");

    // 9) Datasets + mapping FCDA -> datasets
    const auto& dsi = mgr.datasets();
    SclManager::DatasetKey dk{"IED1","LD1","DS_CB"};
    auto itD = dsi.find(dk);
    ASSERT_TRUE(itD != dsi.end()) << "Dataset DS_CB not indexed";
    ASSERT_NE(itD->second, nullptr);
    EXPECT_EQ(itD->second->name, "DS_CB");
    ASSERT_EQ(itD->second->members.size(), 2u);

    // Mapping FCDA -> datasets
    // Rappel: clé = (ldInst vide ? LD1 : ldInst) + "|lnClass+lnInst|doName|daName|fc"
    //  a) FCDA sans ldInst -> retombe sur LD1
    std::string fkey1 = "LD1|XSWI1|Pos|stVal|ST";
    //  b) FCDA avec ldInst = LD1
    std::string fkey2 = "LD1|XSWI1|Pos|ctlVal|CO";

    const auto& mapF = mgr.fcdaToDatasets();
    auto range1 = mapF.equal_range(fkey1);
    bool ok1 = false;
    for (auto it = range1.first; it != range1.second; ++it) {
        if (it->second.ied=="IED1" && it->second.ld=="LD1" && it->second.name=="DS_CB") { ok1 = true; break; }
    }
    EXPECT_TRUE(ok1) << "FCDA (Pos.stVal/ST) not mapped to DS_CB";

    auto range2 = mapF.equal_range(fkey2);
    bool ok2 = false;
    for (auto it = range2.first; it != range2.second; ++it) {
        if (it->second.ied=="IED1" && it->second.ld=="LD1" && it->second.name=="DS_CB") { ok2 = true; break; }
    }
    EXPECT_TRUE(ok2) << "FCDA (Pos.ctlVal/CO) not mapped to DS_CB";

    // 10) JSON (vue IEDs) — fumée rapide
    auto j = mgr.toJsonIEDs();
    // Doit contenir IED1 et XSWI1, avec anchor CE
    EXPECT_NE(j.find("\"IED1\""), std::string::npos);
    EXPECT_NE(j.find("\"XSWI\""), std::string::npos);
    EXPECT_NE(j.find("S1:VL1:BAY1:CE:Q01"), std::string::npos);

    remove_temp_scd(path);
}
