<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="invalid_orphan_terminal" version="1" revision="0"/>
  <Substation name="SS1">
    <VoltageLevel name="VL1" nominalVoltage="225000">
      <Bay name="BUS1">
        <BusbarSection name="BB1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
        </BusbarSection>
      </Bay>
      <Bay name="L1">
        <!-- ERROR: Terminal points to non-existent connectivity node -->
        <ConductingEquipment type="DIS" name="DS1-1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
          <Terminal name="T2" cNodeName="CN_NOT_FOUND" connectivityNode="SS1/VL1/L1/CN_NOT_FOUND"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>
  <DataTypeTemplates/>
</SCL>
