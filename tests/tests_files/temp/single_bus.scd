<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="SVZ_SingleBus_2Lines_DS" nameStructure="IEDName"/>
  <Substation name="SS1">
    <VoltageLevel name="VL63" nomFreq="50" numPhases="3">
      <Voltage unit="V" multiplier="k">63</Voltage>

      <!-- Bus unique -->
      <Bay name="BUSBAR">
        <ConnectivityNode name="CN_BUS" pathName="SS1/VL63/BUSBAR/CN_BUS"/>
      </Bay>

      <!-- ====== DEPART LINE 1 ====== -->
      <Bay name="LINE1">
        <!-- Sectionneur amont -->
        <ConductingEquipment name="DS1-1" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUSBAR"/>
          <Terminal name="T2" cNodeName="CN_Q1_IN"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>

        <!-- Disjoncteur -->
        <ConductingEquipment name="Q0-1" type="CBR">
          <Terminal name="T1" cNodeName="CN_Q1_IN"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
          <Terminal name="T2" cNodeName="CN_Q1_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>

        <!-- Sectionneur aval -->
        <ConductingEquipment name="DS1-2" type="DIS">
          <Terminal name="T1" cNodeName="CN_Q1_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
          <Terminal name="T2" cNodeName="CN_L1"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>

        <!-- TC (série) -->
        <ConductingEquipment name="CT1" type="CTR">
          <Terminal name="T1" cNodeName="CN_L1"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
          <Terminal name="T2" cNodeName="CN_L1_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>

        <!-- TT (dérivation sur le nœud CN_L1) -->
        <ConductingEquipment name="VT1" type="VTR">
          <Terminal name="T1" cNodeName="CN_L1"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>

        <!-- Nœuds de connectivité du départ 1 -->
        <ConnectivityNode name="CN_Q1_IN"  pathName="SS1/VL63/LINE1/CN_Q1_IN"/>
        <ConnectivityNode name="CN_Q1_OUT" pathName="SS1/VL63/LINE1/CN_Q1_OUT"/>
        <ConnectivityNode name="CN_L1"     pathName="SS1/VL63/LINE1/CN_L1"/>
        <ConnectivityNode name="CN_L1_OUT" pathName="SS1/VL63/LINE1/CN_L1_OUT"/>
      </Bay>

      <!-- ====== DEPART LINE 2 ====== -->
      <Bay name="LINE2">
        <!-- Sectionneur amont -->
        <ConductingEquipment name="DS2-1" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUSBAR"/>
          <Terminal name="T2" cNodeName="CN_Q2_IN"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>

        <!-- Disjoncteur -->
        <ConductingEquipment name="Q0-2" type="CBR">
          <Terminal name="T1" cNodeName="CN_Q2_IN"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
          <Terminal name="T2" cNodeName="CN_Q2_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>

        <!-- Sectionneur aval -->
        <ConductingEquipment name="DS2-2" type="DIS">
          <Terminal name="T1" cNodeName="CN_Q2_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
          <Terminal name="T2" cNodeName="CN_L2"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>

        <!-- TC (série) -->
        <ConductingEquipment name="CT2" type="CTR">
          <Terminal name="T1" cNodeName="CN_L2"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
          <Terminal name="T2" cNodeName="CN_L2_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>

        <!-- TT (dérivation) -->
        <ConductingEquipment name="VT2" type="VTR">
          <Terminal name="T1" cNodeName="CN_L2"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>

        <!-- Nœuds de connectivité du départ 2 -->
        <ConnectivityNode name="CN_Q2_IN"  pathName="SS1/VL63/LINE2/CN_Q2_IN"/>
        <ConnectivityNode name="CN_Q2_OUT" pathName="SS1/VL63/LINE2/CN_Q2_OUT"/>
        <ConnectivityNode name="CN_L2"     pathName="SS1/VL63/LINE2/CN_L2"/>
        <ConnectivityNode name="CN_L2_OUT" pathName="SS1/VL63/LINE2/CN_L2_OUT"/>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>
