<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="SVZ_230to33_Detailed" nameStructure="IEDName"/>
  <Substation name="SS1">

    <!-- ===================== NIVEAU 230 kV ===================== -->
    <VoltageLevel name="VL230" nomFreq="50" numPhases="3">
      <Voltage unit="V" multiplier="k">230</Voltage>

      <!-- Bus 230 kV -->
      <Bay name="BUS230">
        <ConnectivityNode name="CN_BUS230" pathName="SS1/VL230/BUS230/CN_BUS230"/>
        <!-- VT de bus -->
        <ConductingEquipment name="VT3" type="VTR">
          <Terminal name="T1" cNodeName="CN_BUS230"
                    substationName="SS1" voltageLevelName="VL230" bayName="BUS230"/>
        </ConductingEquipment>
      </Bay>

      <!-- Arrivée 230 kV : Tx. Line 1 -->
      <Bay name="TX_LINE1_230">
        <!-- Mesures amont -->
        <ConductingEquipment name="VT1" type="VTR">
          <Terminal name="T1" cNodeName="CN_L1_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT1" type="CTR">
          <Terminal name="T1" cNodeName="CN_L1_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
          <Terminal name="T2" cNodeName="CN_L1_CT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
        </ConductingEquipment>
        <!-- Mise à la terre (interrupteur de terre) -->
        <ConductingEquipment name="ES1" type="ERS">
          <Terminal name="T1" cNodeName="CN_L1_CT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
        </ConductingEquipment>
        <!-- Chaîne DS–CB–DS vers Bus -->
        <ConductingEquipment name="DS1" type="DIS">
          <Terminal name="T1" cNodeName="CN_L1_CT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
          <Terminal name="T2" cNodeName="CN_L1_CB_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB1" type="CBR">
          <Terminal name="T1" cNodeName="CN_L1_CB_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
          <Terminal name="T2" cNodeName="CN_L1_CB_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS2" type="DIS">
          <Terminal name="T1" cNodeName="CN_L1_CB_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE1_230"/>
          <Terminal name="T2" cNodeName="CN_BUS230"
                    substationName="SS1" voltageLevelName="VL230" bayName="BUS230"/>
        </ConductingEquipment>

        <!-- Nœuds de l'arrivée L1 -->
        <ConnectivityNode name="CN_L1_IN"     pathName="SS1/VL230/TX_LINE1_230/CN_L1_IN"/>
        <ConnectivityNode name="CN_L1_CT"     pathName="SS1/VL230/TX_LINE1_230/CN_L1_CT"/>
        <ConnectivityNode name="CN_L1_CB_IN"  pathName="SS1/VL230/TX_LINE1_230/CN_L1_CB_IN"/>
        <ConnectivityNode name="CN_L1_CB_OUT" pathName="SS1/VL230/TX_LINE1_230/CN_L1_CB_OUT"/>
      </Bay>

      <!-- Arrivée 230 kV : Tx. Line 2 -->
      <Bay name="TX_LINE2_230">
        <ConductingEquipment name="VT2" type="VTR">
          <Terminal name="T1" cNodeName="CN_L2_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT2" type="CTR">
          <Terminal name="T1" cNodeName="CN_L2_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
          <Terminal name="T2" cNodeName="CN_L2_CT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="ES2" type="ERS">
          <Terminal name="T1" cNodeName="CN_L2_CT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS3" type="DIS">
          <Terminal name="T1" cNodeName="CN_L2_CT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
          <Terminal name="T2" cNodeName="CN_L2_CB_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB2" type="CBR">
          <Terminal name="T1" cNodeName="CN_L2_CB_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
          <Terminal name="T2" cNodeName="CN_L2_CB_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS4" type="DIS">
          <Terminal name="T1" cNodeName="CN_L2_CB_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX_LINE2_230"/>
          <Terminal name="T2" cNodeName="CN_BUS230"
                    substationName="SS1" voltageLevelName="VL230" bayName="BUS230"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_L2_IN"     pathName="SS1/VL230/TX_LINE2_230/CN_L2_IN"/>
        <ConnectivityNode name="CN_L2_CT"     pathName="SS1/VL230/TX_LINE2_230/CN_L2_CT"/>
        <ConnectivityNode name="CN_L2_CB_IN"  pathName="SS1/VL230/TX_LINE2_230/CN_L2_CB_IN"/>
        <ConnectivityNode name="CN_L2_CB_OUT" pathName="SS1/VL230/TX_LINE2_230/CN_L2_CB_OUT"/>
      </Bay>

      <!-- Chaînes HV vers transformateurs -->
      <Bay name="TX1_HV_230">
        <ConductingEquipment name="DS5" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS230"
                    substationName="SS1" voltageLevelName="VL230" bayName="BUS230"/>
          <Terminal name="T2" cNodeName="CN_T1_HV_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB3" type="CBR">
          <Terminal name="T1" cNodeName="CN_T1_HV_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"/>
          <Terminal name="T2" cNodeName="CN_T1_HV_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS6" type="DIS">
          <Terminal name="T1" cNodeName="CN_T1_HV_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"/>
          <Terminal name="T2" cNodeName="CN_T1_CT_HV"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT3" type="CTR">
          <Terminal name="T1" cNodeName="CN_T1_CT_HV"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"/>
          <Terminal name="T2" cNodeName="CN_T1_HV_TERM"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_T1_HV_IN"   pathName="SS1/VL230/TX1_HV_230/CN_T1_HV_IN"/>
        <ConnectivityNode name="CN_T1_HV_OUT"  pathName="SS1/VL230/TX1_HV_230/CN_T1_HV_OUT"/>
        <ConnectivityNode name="CN_T1_CT_HV"   pathName="SS1/VL230/TX1_HV_230/CN_T1_CT_HV"/>
        <ConnectivityNode name="CN_T1_HV_TERM" pathName="SS1/VL230/TX1_HV_230/CN_T1_HV_TERM"/>
      </Bay>

      <Bay name="TX2_HV_230">
        <ConductingEquipment name="DS7" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS230"
                    substationName="SS1" voltageLevelName="VL230" bayName="BUS230"/>
          <Terminal name="T2" cNodeName="CN_T2_HV_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB4" type="CBR">
          <Terminal name="T1" cNodeName="CN_T2_HV_IN"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"/>
          <Terminal name="T2" cNodeName="CN_T2_HV_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS8" type="DIS">
          <Terminal name="T1" cNodeName="CN_T2_HV_OUT"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"/>
          <Terminal name="T2" cNodeName="CN_T2_CT_HV"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT4" type="CTR">
          <Terminal name="T1" cNodeName="CN_T2_CT_HV"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"/>
          <Terminal name="T2" cNodeName="CN_T2_HV_TERM"
                    substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_T2_HV_IN"   pathName="SS1/VL230/TX2_HV_230/CN_T2_HV_IN"/>
        <ConnectivityNode name="CN_T2_HV_OUT"  pathName="SS1/VL230/TX2_HV_230/CN_T2_HV_OUT"/>
        <ConnectivityNode name="CN_T2_CT_HV"   pathName="SS1/VL230/TX2_HV_230/CN_T2_CT_HV"/>
        <ConnectivityNode name="CN_T2_HV_TERM" pathName="SS1/VL230/TX2_HV_230/CN_T2_HV_TERM"/>
      </Bay>
    </VoltageLevel>

    <!-- ===================== NIVEAU 33 kV ===================== -->
    <VoltageLevel name="VL33" nomFreq="50" numPhases="3">
      <Voltage unit="V" multiplier="k">33</Voltage>

      <!-- Deux sections de barres 33 kV reliées par une section de barre (DS15–CB11–DS16) -->
      <Bay name="BUS33_A">
        <ConnectivityNode name="CN_BUS33_A" pathName="SS1/VL33/BUS33_A/CN_BUS33_A"/>
      </Bay>
      <Bay name="BUS33_B">
        <ConnectivityNode name="CN_BUS33_B" pathName="SS1/VL33/BUS33_B/CN_BUS33_B"/>
      </Bay>
      <Bay name="BUS_SECTION">
        <ConductingEquipment name="DS15" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS33_A"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_A"/>
          <Terminal name="T2" cNodeName="CN_SEC_MID_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS_SECTION"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB11" type="CBR">
          <Terminal name="T1" cNodeName="CN_SEC_MID_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS_SECTION"/>
          <Terminal name="T2" cNodeName="CN_SEC_MID_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS_SECTION"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS16" type="DIS">
          <Terminal name="T1" cNodeName="CN_SEC_MID_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS_SECTION"/>
          <Terminal name="T2" cNodeName="CN_BUS33_B"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_B"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_SEC_MID_IN"  pathName="SS1/VL33/BUS_SECTION/CN_SEC_MID_IN"/>
        <ConnectivityNode name="CN_SEC_MID_OUT" pathName="SS1/VL33/BUS_SECTION/CN_SEC_MID_OUT"/>
      </Bay>

      <!-- Retours LV des transformateurs -->
      <Bay name="TX1_LV_33">
        <!-- VT côté 33 kV -->
        <ConductingEquipment name="VT4" type="VTR">
          <Terminal name="T1" cNodeName="CN_T1_LV_TERM"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX1_LV_33"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT5" type="CTR">
          <Terminal name="T1" cNodeName="CN_T1_LV_TERM"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX1_LV_33"/>
          <Terminal name="T2" cNodeName="CN_T1_LV_CT_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX1_LV_33"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB5" type="CBR">
          <Terminal name="T1" cNodeName="CN_T1_LV_CT_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX1_LV_33"/>
          <Terminal name="T2" cNodeName="CN_T1_LV_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX1_LV_33"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS9" type="DIS">
          <Terminal name="T1" cNodeName="CN_T1_LV_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX1_LV_33"/>
          <Terminal name="T2" cNodeName="CN_BUS33_A"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_A"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_T1_LV_TERM"   pathName="SS1/VL33/TX1_LV_33/CN_T1_LV_TERM"/>
        <ConnectivityNode name="CN_T1_LV_CT_OUT" pathName="SS1/VL33/TX1_LV_33/CN_T1_LV_CT_OUT"/>
        <ConnectivityNode name="CN_T1_LV_CB_OUT" pathName="SS1/VL33/TX1_LV_33/CN_T1_LV_CB_OUT"/>
      </Bay>

      <Bay name="TX2_LV_33">
        <ConductingEquipment name="VT5" type="VTR">
          <Terminal name="T1" cNodeName="CN_T2_LV_TERM"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX2_LV_33"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT6" type="CTR">
          <Terminal name="T1" cNodeName="CN_T2_LV_TERM"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX2_LV_33"/>
          <Terminal name="T2" cNodeName="CN_T2_LV_CT_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX2_LV_33"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB6" type="CBR">
          <Terminal name="T1" cNodeName="CN_T2_LV_CT_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX2_LV_33"/>
          <Terminal name="T2" cNodeName="CN_T2_LV_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX2_LV_33"/>
        </ConductingEquipment>
        <ConductingEquipment name="DS10" type="DIS">
          <Terminal name="T1" cNodeName="CN_T2_LV_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="TX2_LV_33"/>
          <Terminal name="T2" cNodeName="CN_BUS33_B"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_B"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_T2_LV_TERM"   pathName="SS1/VL33/TX2_LV_33/CN_T2_LV_TERM"/>
        <ConnectivityNode name="CN_T2_LV_CT_OUT" pathName="SS1/VL33/TX2_LV_33/CN_T2_LV_CT_OUT"/>
        <ConnectivityNode name="CN_T2_LV_CB_OUT" pathName="SS1/VL33/TX2_LV_33/CN_T2_LV_CB_OUT"/>
      </Bay>

      <!-- Départs 33 kV -->
      <Bay name="FEEDER1">
        <ConductingEquipment name="DS11" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS33_A"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_A"/>
          <Terminal name="T2" cNodeName="CN_F1_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER1"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB7" type="CBR">
          <Terminal name="T1" cNodeName="CN_F1_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER1"/>
          <Terminal name="T2" cNodeName="CN_F1_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER1"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT7" type="CTR">
          <Terminal name="T1" cNodeName="CN_F1_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER1"/>
          <Terminal name="T2" cNodeName="CN_F1_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER1"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_F1_IN"     pathName="SS1/VL33/FEEDER1/CN_F1_IN"/>
        <ConnectivityNode name="CN_F1_CB_OUT" pathName="SS1/VL33/FEEDER1/CN_F1_CB_OUT"/>
        <ConnectivityNode name="CN_F1_OUT"    pathName="SS1/VL33/FEEDER1/CN_F1_OUT"/>
      </Bay>

      <Bay name="FEEDER2">
        <ConductingEquipment name="DS12" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS33_A"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_A"/>
          <Terminal name="T2" cNodeName="CN_F2_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER2"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB8" type="CBR">
          <Terminal name="T1" cNodeName="CN_F2_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER2"/>
          <Terminal name="T2" cNodeName="CN_F2_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER2"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT8" type="CTR">
          <Terminal name="T1" cNodeName="CN_F2_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER2"/>
          <Terminal name="T2" cNodeName="CN_F2_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER2"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_F2_IN"     pathName="SS1/VL33/FEEDER2/CN_F2_IN"/>
        <ConnectivityNode name="CN_F2_CB_OUT" pathName="SS1/VL33/FEEDER2/CN_F2_CB_OUT"/>
        <ConnectivityNode name="CN_F2_OUT"    pathName="SS1/VL33/FEEDER2/CN_F2_OUT"/>
      </Bay>

      <Bay name="FEEDER3">
        <ConductingEquipment name="DS13" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS33_B"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_B"/>
          <Terminal name="T2" cNodeName="CN_F3_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER3"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB9" type="CBR">
          <Terminal name="T1" cNodeName="CN_F3_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER3"/>
          <Terminal name="T2" cNodeName="CN_F3_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER3"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT9" type="CTR">
          <Terminal name="T1" cNodeName="CN_F3_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER3"/>
          <Terminal name="T2" cNodeName="CN_F3_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER3"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_F3_IN"     pathName="SS1/VL33/FEEDER3/CN_F3_IN"/>
        <ConnectivityNode name="CN_F3_CB_OUT" pathName="SS1/VL33/FEEDER3/CN_F3_CB_OUT"/>
        <ConnectivityNode name="CN_F3_OUT"    pathName="SS1/VL33/FEEDER3/CN_F3_OUT"/>
      </Bay>

      <Bay name="FEEDER4">
        <ConductingEquipment name="DS14" type="DIS">
          <Terminal name="T1" cNodeName="CN_BUS33_B"
                    substationName="SS1" voltageLevelName="VL33" bayName="BUS33_B"/>
          <Terminal name="T2" cNodeName="CN_F4_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER4"/>
        </ConductingEquipment>
        <ConductingEquipment name="CB10" type="CBR">
          <Terminal name="T1" cNodeName="CN_F4_IN"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER4"/>
          <Terminal name="T2" cNodeName="CN_F4_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER4"/>
        </ConductingEquipment>
        <ConductingEquipment name="CT10" type="CTR">
          <Terminal name="T1" cNodeName="CN_F4_CB_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER4"/>
          <Terminal name="T2" cNodeName="CN_F4_OUT"
                    substationName="SS1" voltageLevelName="VL33" bayName="FEEDER4"/>
        </ConductingEquipment>

        <ConnectivityNode name="CN_F4_IN"     pathName="SS1/VL33/FEEDER4/CN_F4_IN"/>
        <ConnectivityNode name="CN_F4_CB_OUT" pathName="SS1/VL33/FEEDER4/CN_F4_CB_OUT"/>
        <ConnectivityNode name="CN_F4_OUT"    pathName="SS1/VL33/FEEDER4/CN_F4_OUT"/>
      </Bay>
    </VoltageLevel>

    <!-- ===================== TRANSFORMATEURS (HV ↔ LV) ===================== -->
    <!-- Modélisation par PowerTransformer avec 2 enroulements et terminaux 
         rattachés aux CN des deux niveaux (HV/LV). -->
    <PowerTransformer name="T1">
      <TransformerWinding name="T1-HV">
        <Terminal name="T1"
          substationName="SS1" voltageLevelName="VL230" bayName="TX1_HV_230"
          cNodeName="CN_T1_HV_TERM"/>
      </TransformerWinding>
      <TransformerWinding name="T1-LV">
        <Terminal name="T2"
          substationName="SS1" voltageLevelName="VL33" bayName="TX1_LV_33"
          cNodeName="CN_T1_LV_TERM"/>
      </TransformerWinding>
    </PowerTransformer>

    <PowerTransformer name="T2">
      <TransformerWinding name="T2-HV">
        <Terminal name="T1"
          substationName="SS1" voltageLevelName="VL230" bayName="TX2_HV_230"
          cNodeName="CN_T2_HV_TERM"/>
      </TransformerWinding>
      <TransformerWinding name="T2-LV">
        <Terminal name="T2"
          substationName="SS1" voltageLevelName="VL33" bayName="TX2_LV_33"
          cNodeName="CN_T2_LV_TERM"/>
      </TransformerWinding>
    </PowerTransformer>

  </Substation>
</SCL>
