<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="double_bus_2lines" version="1" revision="1"/>
  <Substation name="SS1">
    <VoltageLevel name="VL1" nominalVoltage="225000">
      <!-- Busbar 1 -->
      <Bay name="BUS1">
        <BusbarSection name="BB1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
        </BusbarSection>
      </Bay>
      <!-- Busbar 2 -->
      <Bay name="BUS2">
        <BusbarSection name="BB2">
          <Terminal name="T1" cNodeName="BB2" connectivityNode="SS1/VL1/BUS2/BB2"/>
        </BusbarSection>
      </Bay>

      <!-- Bus Coupler (between BB1 and BB2) -->
      <Bay name="BC">
        <ConnectivityNode name="CN_BC_1" pathName="SS1/VL1/BC/CN_BC_1"/>
        <ConnectivityNode name="CN_BC_2" pathName="SS1/VL1/BC/CN_BC_2"/>
        <ConductingEquipment type="DIS" name="DSC-1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
          <Terminal name="T2" cNodeName="CN_BC_1" connectivityNode="SS1/VL1/BC/CN_BC_1"/>
        </ConductingEquipment>
        <ConductingEquipment type="CBR" name="CBC">
          <Terminal name="T1" cNodeName="CN_BC_1" connectivityNode="SS1/VL1/BC/CN_BC_1"/>
          <Terminal name="T2" cNodeName="CN_BC_2" connectivityNode="SS1/VL1/BC/CN_BC_2"/>
        </ConductingEquipment>
        <ConductingEquipment type="DIS" name="DSC-2">
          <Terminal name="T1" cNodeName="CN_BC_2" connectivityNode="SS1/VL1/BC/CN_BC_2"/>
          <Terminal name="T2" cNodeName="BB2" connectivityNode="SS1/VL1/BUS2/BB2"/>
        </ConductingEquipment>
      </Bay>

      <!-- Line 1 connected to BB1 -->
      <Bay name="L1">
        <ConnectivityNode name="CN_L1_1" pathName="SS1/VL1/L1/CN_L1_1"/>
        <ConnectivityNode name="CN_L1_2" pathName="SS1/VL1/L1/CN_L1_2"/>
        <ConnectivityNode name="L1"       pathName="SS1/VL1/L1/L1"/>
        <ConductingEquipment type="DIS" name="DS1-1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
          <Terminal name="T2" cNodeName="CN_L1_1" connectivityNode="SS1/VL1/L1/CN_L1_1"/>
        </ConductingEquipment>
        <ConductingEquipment type="CBR" name="CB1">
          <Terminal name="T1" cNodeName="CN_L1_1" connectivityNode="SS1/VL1/L1/CN_L1_1"/>
          <Terminal name="T2" cNodeName="CN_L1_2" connectivityNode="SS1/VL1/L1/CN_L1_2"/>
        </ConductingEquipment>
        <ConductingEquipment type="DIS" name="DS1-2">
          <Terminal name="T1" cNodeName="CN_L1_2" connectivityNode="SS1/VL1/L1/CN_L1_2"/>
          <Terminal name="T2" cNodeName="L1" connectivityNode="SS1/VL1/L1/L1"/>
        </ConductingEquipment>
      </Bay>

      <!-- Line 2 connected to BB2 -->
      <Bay name="L2">
        <ConnectivityNode name="CN_L2_1" pathName="SS1/VL1/L2/CN_L2_1"/>
        <ConnectivityNode name="CN_L2_2" pathName="SS1/VL1/L2/CN_L2_2"/>
        <ConnectivityNode name="L2"       pathName="SS1/VL1/L2/L2"/>
        <ConductingEquipment type="DIS" name="DS2-1">
          <Terminal name="T1" cNodeName="BB2" connectivityNode="SS1/VL1/BUS2/BB2"/>
          <Terminal name="T2" cNodeName="CN_L2_1" connectivityNode="SS1/VL1/L2/CN_L2_1"/>
        </ConductingEquipment>
        <ConductingEquipment type="CBR" name="CB2">
          <Terminal name="T1" cNodeName="CN_L2_1" connectivityNode="SS1/VL1/L2/CN_L2_1"/>
          <Terminal name="T2" cNodeName="CN_L2_2" connectivityNode="SS1/VL1/L2/CN_L2_2"/>
        </ConductingEquipment>
        <ConductingEquipment type="DIS" name="DS2-2">
          <Terminal name="T1" cNodeName="CN_L2_2" connectivityNode="SS1/VL1/L2/CN_L2_2"/>
          <Terminal name="T2" cNodeName="L2" connectivityNode="SS1/VL1/L2/L2"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>
  <Communication/>
  <DataTypeTemplates/>
</SCL>
