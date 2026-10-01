<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="invalid_duplicate_id" version="1" revision="0"/>
  <Substation name="SS1">
    <VoltageLevel name="VL1" nominalVoltage="225000">
      <!-- Duplicate Bay names (BUS1 appears twice) -->
      <Bay name="BUS1">
        <BusbarSection name="BB1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
        </BusbarSection>
      </Bay>
      <Bay name="BUS1">
        <ConnectivityNode name="CN_X" pathName="SS1/VL1/BUS1/CN_X"/>
        <ConductingEquipment type="DIS" name="DSX">
          <Terminal name="T1" cNodeName="CN_X" connectivityNode="SS1/VL1/BUS1/CN_X"/>
          <Terminal name="T2" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>
  <DataTypeTemplates/>
</SCL>
