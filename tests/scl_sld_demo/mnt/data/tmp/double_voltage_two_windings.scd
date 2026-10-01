<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="double_voltage_two_windings" version="1" revision="0"/>
  <Substation name="SS1">

    <!-- Power transformer linking HV and MV -->
    <PowerTransformer name="T1">
      <Terminal name="T1-HV" cNodeName="CN_T1_HV" connectivityNode="SS1/VL_HV/TX/CN_T1_HV"/>
      <Terminal name="T1-MV" cNodeName="CN_T1_MV" connectivityNode="SS1/VL_MV/TX/CN_T1_MV"/>
    </PowerTransformer>

    <!-- High Voltage level -->
    <VoltageLevel name="VL_HV" nominalVoltage="225000">
      <!-- HV busbar -->
      <Bay name="BUS_HV">
        <BusbarSection name="BBH1">
          <Terminal name="T1" cNodeName="BBH1" connectivityNode="SS1/VL_HV/BUS_HV/BBH1"/>
        </BusbarSection>
      </Bay>

      <!-- HV side of transformer and line bay -->
      <Bay name="TX">
        <ConnectivityNode name="CN_T1_HV" pathName="SS1/VL_HV/TX/CN_T1_HV"/>
      </Bay>

      <Bay name="HV_L1">
        <ConnectivityNode name="CN_HV1" pathName="SS1/VL_HV/HV_L1/CN_HV1"/>
        <ConnectivityNode name="CN_HV2" pathName="SS1/VL_HV/HV_L1/CN_HV2"/>
        <ConnectivityNode name="HV_FEEDER" pathName="SS1/VL_HV/HV_L1/HV_FEEDER"/>
        <ConductingEquipment type="DIS" name="HV_DS1-1">
          <Terminal name="T1" cNodeName="BBH1" connectivityNode="SS1/VL_HV/BUS_HV/BBH1"/>
          <Terminal name="T2" cNodeName="CN_HV1" connectivityNode="SS1/VL_HV/HV_L1/CN_HV1"/>
        </ConductingEquipment>
        <ConductingEquipment type="CBR" name="HV_CB1">
          <Terminal name="T1" cNodeName="CN_HV1" connectivityNode="SS1/VL_HV/HV_L1/CN_HV1"/>
          <Terminal name="T2" cNodeName="CN_T1_HV" connectivityNode="SS1/VL_HV/TX/CN_T1_HV"/>
        </ConductingEquipment>
        <ConductingEquipment type="DIS" name="HV_DS1-2">
          <Terminal name="T1" cNodeName="CN_T1_HV" connectivityNode="SS1/VL_HV/TX/CN_T1_HV"/>
          <Terminal name="T2" cNodeName="CN_HV2" connectivityNode="SS1/VL_HV/HV_L1/CN_HV2"/>
        </ConductingEquipment>
        <ConductingEquipment type="DIS" name="HV_OUT">
          <Terminal name="T1" cNodeName="CN_HV2" connectivityNode="SS1/VL_HV/HV_L1/CN_HV2"/>
          <Terminal name="T2" cNodeName="HV_FEEDER" connectivityNode="SS1/VL_HV/HV_L1/HV_FEEDER"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>

    <!-- Medium Voltage level -->
    <VoltageLevel name="VL_MV" nominalVoltage="63000">
      <!-- MV busbar -->
      <Bay name="BUS_MV">
        <BusbarSection name="BBM1">
          <Terminal name="T1" cNodeName="BBM1" connectivityNode="SS1/VL_MV/BUS_MV/BBM1"/>
        </BusbarSection>
      </Bay>

      <!-- MV side of transformer -->
      <Bay name="TX">
        <ConnectivityNode name="CN_T1_MV" pathName="SS1/VL_MV/TX/CN_T1_MV"/>
      </Bay>

      <!-- MV feeder bay -->
      <Bay name="MV_L1">
        <ConnectivityNode name="CN_MV1" pathName="SS1/VL_MV/MV_L1/CN_MV1"/>
        <ConnectivityNode name="CN_MV2" pathName="SS1/VL_MV/MV_L1/CN_MV2"/>
        <ConnectivityNode name="MV_FEEDER" pathName="SS1/VL_MV/MV_L1/MV_FEEDER"/>
        <ConductingEquipment type="DIS" name="MV_DS1-1">
          <Terminal name="T1" cNodeName="BBM1" connectivityNode="SS1/VL_MV/BUS_MV/BBM1"/>
          <Terminal name="T2" cNodeName="CN_MV1" connectivityNode="SS1/VL_MV/MV_L1/CN_MV1"/>
        </ConductingEquipment>
        <ConductingEquipment type="CBR" name="MV_CB1">
          <Terminal name="T1" cNodeName="CN_MV1" connectivityNode="SS1/VL_MV/MV_L1/CN_MV1"/>
          <Terminal name="T2" cNodeName="CN_T1_MV" connectivityNode="SS1/VL_MV/TX/CN_T1_MV"/>
        </ConductingEquipment>
        <ConductingEquipment type="DIS" name="MV_DS1-2">
          <Terminal name="T1" cNodeName="CN_T1_MV" connectivityNode="SS1/VL_MV/TX/CN_T1_MV"/>
          <Terminal name="T2" cNodeName="CN_MV2" connectivityNode="SS1/VL_MV/MV_L1/CN_MV2"/>
        </ConductingEquipment>
        <ConductingEquipment type="DIS" name="MV_OUT">
          <Terminal name="T1" cNodeName="CN_MV2" connectivityNode="SS1/VL_MV/MV_L1/CN_MV2"/>
          <Terminal name="T2" cNodeName="MV_FEEDER" connectivityNode="SS1/VL_MV/MV_L1/MV_FEEDER"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>

  </Substation>
  <Communication/>
  <DataTypeTemplates/>
</SCL>
