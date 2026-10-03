<?xml version="1.0" encoding="UTF-8"?>
<!--
  Two-bay arrangement used to check the SLD derivation against a double-bus
  section.

  Rewritten to match how vendors actually export: one Bay per circuit, the
  busbar in its own equipment-free Bay, and connectivity given by full
  @connectivityNode paths. The previous version put a busbar, a coupler, a
  feeder and a transformer all inside a single Bay using only @cNodeName, which
  is not a shape any vendor produces, so it exercised an edge case rather than
  the real thing.
-->
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007B" revision="B" release="4">
  <Header id="SCL_DOUBLE_BUS">
    <Text>Double bus with section, one transformer</Text>
    <History>
      <Hitem version="1" revision="1" when="2026-01-01" who="fixture" what="initial"/>
    </History>
  </Header>

<Substation name="S1">
  <!-- ============ E1: 380 kV ============ -->
  <VoltageLevel name="E1" nomFreq="50" numPhases="3">
    <Voltage unit="V" multiplier="k">380</Voltage>

    <!-- Busbar bay: no ConductingEquipment, so it is a bus. -->
    <Bay name="BB1">
      <ConnectivityNode name="BB1_CN" pathName="S1/E1/BB1/BB1_CN"/>
    </Bay>

    <!-- Feeder Q01: bus -> CB -> DS -> CT -> Line -->
    <Bay name="Q01">
      <ConnectivityNode name="IN"  pathName="S1/E1/Q01/IN"/>
      <ConnectivityNode name="MID" pathName="S1/E1/Q01/MID"/>
      <ConnectivityNode name="OUT" pathName="S1/E1/Q01/OUT"/>
      <ConnectivityNode name="END" pathName="S1/E1/Q01/END"/>
      <ConductingEquipment name="Q01_CB" type="CBR">
        <Terminal name="T1" connectivityNode="S1/E1/BB1/BB1_CN"/>
        <Terminal name="T2" connectivityNode="S1/E1/Q01/IN"/>
        <LNode iedName="PROT1" ldInst="LD1" lnClass="XCBR" lnInst="1"/>
      </ConductingEquipment>
      <ConductingEquipment name="Q01_DS" type="DIS">
        <Terminal name="T1" connectivityNode="S1/E1/Q01/IN"/>
        <Terminal name="T2" connectivityNode="S1/E1/Q01/MID"/>
        <LNode iedName="PROT1" ldInst="LD1" lnClass="XSWI" lnInst="1"/>
      </ConductingEquipment>
      <ConductingEquipment name="Q01_CT" type="CTR">
        <Terminal name="T1" connectivityNode="S1/E1/Q01/MID"/>
        <Terminal name="T2" connectivityNode="S1/E1/Q01/OUT"/>
        <LNode iedName="PROT1" ldInst="LD1" lnClass="TCTR" lnInst="1"/>
      </ConductingEquipment>
      <ConductingEquipment name="Q01_LINE" type="LIN">
        <Terminal name="T1" connectivityNode="S1/E1/Q01/OUT"/>
        <Terminal name="T2" connectivityNode="S1/E1/Q01/END"/>
        <LNode iedName="PROT1" ldInst="LD1" lnClass="CSWI" lnInst="1"/>
      </ConductingEquipment>
    </Bay>

    <!-- Feeder Q02: bus -> CB -> CT -> Line (no line-side disconnector) -->
    <Bay name="Q02">
      <ConnectivityNode name="IN"  pathName="S1/E1/Q02/IN"/>
      <ConnectivityNode name="OUT" pathName="S1/E1/Q02/OUT"/>
      <ConnectivityNode name="END" pathName="S1/E1/Q02/END"/>
      <ConductingEquipment name="Q02_CB" type="CBR">
        <Terminal name="T1" connectivityNode="S1/E1/BB1/BB1_CN"/>
        <Terminal name="T2" connectivityNode="S1/E1/Q02/IN"/>
        <LNode iedName="PROT2" ldInst="LD1" lnClass="XCBR" lnInst="1"/>
      </ConductingEquipment>
      <ConductingEquipment name="Q02_CT" type="CTR">
        <Terminal name="T1" connectivityNode="S1/E1/Q02/IN"/>
        <Terminal name="T2" connectivityNode="S1/E1/Q02/OUT"/>
        <LNode iedName="PROT2" ldInst="LD1" lnClass="TCTR" lnInst="1"/>
      </ConductingEquipment>
      <ConductingEquipment name="Q02_LINE" type="LIN">
        <Terminal name="T1" connectivityNode="S1/E1/Q02/OUT"/>
        <Terminal name="T2" connectivityNode="S1/E1/Q02/END"/>
        <LNode iedName="PROT2" ldInst="LD1" lnClass="CSWI" lnInst="1"/>
      </ConductingEquipment>
    </Bay>
  </VoltageLevel>

  <!-- ============ M1: 225 kV ============ -->
  <VoltageLevel name="M1" nomFreq="50" numPhases="3">
    <Voltage unit="V" multiplier="k">225</Voltage>

    <Bay name="BB2">
      <ConnectivityNode name="BB2_CN" pathName="S1/M1/BB2/BB2_CN"/>
    </Bay>

    <!--
      Transformer bay: the winding terminal sits in this equipment bay, whose
      single external connection reaches BB2. This is the layout substation.scd
      uses, and the one SCD_2VL_TR does not.
    -->
    <Bay name="TRAFO">
      <ConnectivityNode name="IN"  pathName="S1/M1/TRAFO/IN"/>
      <ConnectivityNode name="WND" pathName="S1/M1/TRAFO/WND"/>
      <ConductingEquipment name="T1_CB" type="CBR">
        <Terminal name="T1" connectivityNode="S1/M1/BB2/BB2_CN"/>
        <Terminal name="T2" connectivityNode="S1/M1/TRAFO/IN"/>
        <LNode iedName="TRAFO_PROT" ldInst="LD1" lnClass="XCBR" lnInst="1"/>
      </ConductingEquipment>
      <ConductingEquipment name="T1_DS" type="DIS">
        <Terminal name="T1" connectivityNode="S1/M1/TRAFO/IN"/>
        <Terminal name="T2" connectivityNode="S1/M1/TRAFO/WND"/>
        <LNode iedName="TRAFO_PROT" ldInst="LD1" lnClass="XSWI" lnInst="1"/>
      </ConductingEquipment>
    </Bay>
  </VoltageLevel>

  <PowerTransformer name="T1" type="PT">
    <TransformerWinding name="HV" type="PTW">
      <Terminal name="t" connectivityNode="S1/M1/TRAFO/WND"/>
    </TransformerWinding>
    <TransformerWinding name="LV" type="PTW">
      <Terminal name="t" connectivityNode="S1/E1/BB1/BB1_CN"/>
      <TapChanger name="T1_TC" type="LTC"/>
    </TransformerWinding>
  </PowerTransformer>
</Substation>

<IED name="PROT1" manufacturer="Fixture" configVersion="1.0">
  <AccessPoint name="AP1">
    <Server>
      <Authentication/>
      <LDevice inst="LD1">
        <LN0 lnClass="LLN0" inst="" lnType="LLN0$os">
          <DataSet name="dsTrip">
            <FCDA ldInst="LD1" prefix="" lnClass="XCBR" lnInst="1" doName="Pos" fc="ST"/>
          </DataSet>
          <ReportControl name="urcbTrip" datSet="dsTrip" confRev="1" buffered="true">
            <TrgOps dchg="true" qchg="true" gi="true"/>
          </ReportControl>
        </LN0>
        <LN prefix="" lnClass="XCBR" inst="1" lnType="XCBR$os"/>
        <LN prefix="" lnClass="XSWI" inst="1" lnType="XSWI$os"/>
        <LN prefix="" lnClass="TCTR" inst="1" lnType="TCTR$os"/>
        <LN prefix="" lnClass="CSWI" inst="1" lnType="CSWI$os"/>
      </LDevice>
    </Server>
  </AccessPoint>
  <Services>
    <DynAssociation/>
    <ReportSettings/>
    <SettingGroups/>
  </Services>
</IED>

<IED name="PROT2" manufacturer="Fixture" configVersion="1.0">
  <AccessPoint name="AP1">
    <Server>
      <Authentication/>
      <LDevice inst="LD1">
        <LN0 lnClass="LLN0" inst="" lnType="LLN0$os"/>
        <LN prefix="" lnClass="XCBR" inst="1" lnType="XCBR$os"/>
        <LN prefix="" lnClass="TCTR" inst="1" lnType="TCTR$os"/>
        <LN prefix="" lnClass="CSWI" inst="1" lnType="CSWI$os"/>
      </LDevice>
    </Server>
  </AccessPoint>
</IED>

<IED name="TRAFO_PROT" manufacturer="Fixture" configVersion="1.0">
  <AccessPoint name="AP1">
    <Server>
      <Authentication/>
      <LDevice inst="LD1">
        <LN0 lnClass="LLN0" inst="" lnType="LLN0$os"/>
        <LN prefix="" lnClass="XCBR" inst="1" lnType="XCBR$os"/>
        <LN prefix="" lnClass="XSWI" inst="1" lnType="XSWI$os"/>
      </LDevice>
    </Server>
  </AccessPoint>
</IED>

<DataTypeTemplates>
  <LNodeType id="LLN0$os" lnClass="LLN0">
    <DO name="Mod" type="INC$os"/>
  </LNodeType>
  <LNodeType id="XCBR$os" lnClass="XCBR">
    <DO name="Pos" type="POS$os"/>
  </LNodeType>
  <LNodeType id="XSWI$os" lnClass="XSWI">
    <DO name="Pos" type="POS$os"/>
  </LNodeType>
  <LNodeType id="TCTR$os" lnClass="TCTR">
    <DO name="Op" type="ACT$os"/>
  </LNodeType>
  <LNodeType id="CSWI$os" lnClass="CSWI">
    <DO name="Op" type="ACT$os"/>
  </LNodeType>

  <DOType id="INC$os" cdc="INC">
    <DA name="stVal" bType="Enum" type="Mod"/>
  </DOType>
  <DOType id="ACT$os" cdc="ACT">
    <DA name="general" bType="BOOLEAN"/>
  </DOType>
  <DOType id="POS$os" cdc="POS">
    <DA name="general" bType="Enum" type="Mod"/>
    <DA name="stVal" bType="DpPos" type="Pos"/>
  </DOType>
  <EnumType id="Mod">
    <EnumVal ord="1">on</EnumVal>
    <EnumVal ord="2">blocked</EnumVal>
  </EnumType>
</DataTypeTemplates>

</SCL>