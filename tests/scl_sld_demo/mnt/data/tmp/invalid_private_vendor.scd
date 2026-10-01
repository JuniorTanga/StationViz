<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="invalid_private_vendor" version="1" revision="0"/>
  <Substation name="SS1">
    <Private type="VendorX">This is a vendor-specific block that tools should ignore with a warning.</Private>
    <VoltageLevel name="VL1" nominalVoltage="225000">
      <Bay name="BUS1">
        <BusbarSection name="BB1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
        </BusbarSection>
      </Bay>
    </VoltageLevel>
  </Substation>
  <DataTypeTemplates/>
</SCL>
