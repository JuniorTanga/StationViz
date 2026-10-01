<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="invalid_missing_lnType" version="1" revision="0"/>
  <IED name="IED_MISSING_LNTYPE">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD1">
          <LN lnClass="XCBR" lnType="XCBR_MISSING" inst="1"/>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
  <Substation name="SS1">
    <VoltageLevel name="VL1" nominalVoltage="225000">
      <Bay name="BUS1">
        <BusbarSection name="BB1">
          <Terminal name="T1" cNodeName="BB1" connectivityNode="SS1/VL1/BUS1/BB1"/>
        </BusbarSection>
      </Bay>
    </VoltageLevel>
  </Substation>
  <!-- DataTypeTemplates intentionally do NOT define XCBR_MISSING -->
  <DataTypeTemplates/>
</SCL>
