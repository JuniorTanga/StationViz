<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="SVZ_DoubleBus_2Lines" nameStructure="IEDName"/>
  <Substation name="SS1">
    <VoltageLevel name="VL63" nomFreq="50" numPhases="3">
      <Voltage unit="V" multiplier="k">63</Voltage>

      <!-- ====== Jeux de barres ====== -->
      <Bay name="BUS1">
        <ConnectivityNode name="CN_BUS1" pathName="SS1/VL63/BUS1/CN_BUS1"/>
      </Bay>
      <Bay name="BUS2">
        <ConnectivityNode name="CN_BUS2" pathName="SS1/VL63/BUS2/CN_BUS2"/>
      </Bay>

      <!-- ====== Départ LINE 1 ====== -->
      <Bay name="LINE1">
        <!-- Sélection vers BUS1 -->
        <ConductingEquipment name="DS1-1" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS1"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUS1"/>
          <Terminal name="T2" cNodeName="CN_L1_SEL"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>
        <!-- Sélection vers BUS2 -->
        <ConductingEquipment name="DS1-2" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS2"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUS2"/>
          <Terminal name="T2" cNodeName="CN_L1_SEL"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>
        <!-- Disjoncteur -->
        <ConductingEquipment name="Q0-1" type="CBR">
          <Terminal name="T1" cNodeName="CN_L1_SEL"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
          <Terminal name="T2" cNodeName="CN_L1"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>
        <!-- TC série -->
        <ConductingEquipment name="CT1" type="CTR">
          <Terminal name="T1" cNodeName="CN_L1"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
          <Terminal name="T2" cNodeName="CN_L1_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>
        <!-- TT en dérivation -->
        <ConductingEquipment name="VT1" type="VTR">
          <Terminal name="T1" cNodeName="CN_L1"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE1"/>
        </ConductingEquipment>

        <!-- Nœuds LINE 1 -->
        <ConnectivityNode name="CN_L1_SEL" pathName="SS1/VL63/LINE1/CN_L1_SEL"/>
        <ConnectivityNode name="CN_L1"     pathName="SS1/VL63/LINE1/CN_L1"/>
        <ConnectivityNode name="CN_L1_OUT" pathName="SS1/VL63/LINE1/CN_L1_OUT"/>
      </Bay>

      <!-- ====== Couplage de barres (Bus Coupler) ====== -->
      <Bay name="COUPLER">
        <!-- DIS côté BUS1 -->
        <ConductingEquipment name="DS-C-1" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS1"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUS1"/>
          <Terminal name="T2" cNodeName="CN_C_IN"
                    substationName="SS1" voltageLevelName="VL63" bayName="COUPLER"/>
        </ConductingEquipment>
        <!-- TC série -->
        <ConductingEquipment name="CT-C" type="CTR">
          <Terminal name="T1" cNodeName="CN_C_IN"
                    substationName="SS1" voltageLevelName="VL63" bayName="COUPLER"/>
          <Terminal name="T2" cNodeName="CN_C_MID"
                    substationName="SS1" voltageLevelName="VL63" bayName="COUPLER"/>
        </ConductingEquipment>
        <!-- Disjoncteur -->
        <ConductingEquipment name="Q0-C" type="CBR">
          <Terminal name="T1" cNodeName="CN_C_MID"
                    substationName="SS1" voltageLevelName="VL63" bayName="COUPLER"/>
          <Terminal name="T2" cNodeName="CN_C_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="COUPLER"/>
        </ConductingEquipment>
        <!-- DIS côté BUS2 -->
        <ConductingEquipment name="DS-C-2" type="DIS">
          <Terminal name="T1" cNodeName="CN_C_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="COUPLER"/>
          <Terminal name="T2" cNodeName="CN_BUS2"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUS2"/>
        </ConductingEquipment>

        <!-- Nœuds COUPLER -->
        <ConnectivityNode name="CN_C_IN"  pathName="SS1/VL63/COUPLER/CN_C_IN"/>
        <ConnectivityNode name="CN_C_MID" pathName="SS1/VL63/COUPLER/CN_C_MID"/>
        <ConnectivityNode name="CN_C_OUT" pathName="SS1/VL63/COUPLER/CN_C_OUT"/>
      </Bay>

      <!-- ====== Départ LINE 2 ====== -->
      <Bay name="LINE2">
        <!-- Sélection vers BUS1 -->
        <ConductingEquipment name="DS2-1" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS1"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUS1"/>
          <Terminal name="T2" cNodeName="CN_L2_SEL"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>
        <!-- Sélection vers BUS2 -->
        <ConductingEquipment name="DS2-2" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS2"
                    substationName="SS1" voltageLevelName="VL63" bayName="BUS2"/>
          <Terminal name="T2" cNodeName="CN_L2_SEL"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>
        <!-- Disjoncteur -->
        <ConductingEquipment name="Q0-2" type="CBR">
          <Terminal name="T1" cNodeName="CN_L2_SEL"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
          <Terminal name="T2" cNodeName="CN_L2"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>
        <!-- TC série -->
        <ConductingEquipment name="CT2" type="CTR">
          <Terminal name="T1" cNodeName="CN_L2"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
          <Terminal name="T2" cNodeName="CN_L2_OUT"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>
        <!-- TT en dérivation -->
        <ConductingEquipment name="VT2" type="VTR">
          <Terminal name="T1" cNodeName="CN_L2"
                    substationName="SS1" voltageLevelName="VL63" bayName="LINE2"/>
        </ConductingEquipment>

        <!-- Nœuds LINE 2 -->
        <ConnectivityNode name="CN_L2_SEL" pathName="SS1/VL63/LINE2/CN_L2_SEL"/>
        <ConnectivityNode name="CN_L2"     pathName="SS1/VL63/LINE2/CN_L2"/>
        <ConnectivityNode name="CN_L2_OUT" pathName="SS1/VL63/LINE2/CN_L2_OUT"/>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>
