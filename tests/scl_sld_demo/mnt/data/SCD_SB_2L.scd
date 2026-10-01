<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007B" revision="B" release="4">
  <Header id="SCD_SB_2L">
    <History/>
  </Header>
<Substation name="SS1">
  <VoltageLevel name="VL1" nomFreq="50" numPhases="3">
    <Bay name="BUS">
      <ConnectivityNode name="BUS1" pathName="SS1/VL1/BUS/BUS1"/>
    </Bay>
    <Bay name="L1">
      <ConnectivityNode name="IN" pathName="SS1/VL1/L1/IN"/>
      <ConnectivityNode name="OUT" pathName="SS1/VL1/L1/OUT"/>
      <ConductingEquipment type="DIS" name="L1-DS">
        <Terminal name="t1" connectivityNode="SS1/VL1/BUS/BUS1"/>
        <Terminal name="t2" connectivityNode="SS1/VL1/L1/IN"/>
      </ConductingEquipment>
      <ConductingEquipment type="CBR" name="L1-CB">
        <Terminal name="a" connectivityNode="SS1/VL1/L1/IN"/>
        <Terminal name="b" connectivityNode="SS1/VL1/L1/OUT"/>
      </ConductingEquipment>
      <ConductingEquipment type="CTR" name="L1-CT">
        <Terminal name="t" connectivityNode="SS1/VL1/L1/OUT"/>
      </ConductingEquipment>
      <ConductingEquipment type="VTR" name="L1-VT">
        <Terminal name="t" connectivityNode="SS1/VL1/L1/OUT"/>
      </ConductingEquipment>
    </Bay>
    <Bay name="L2">
      <ConnectivityNode name="IN" pathName="SS1/VL1/L2/IN"/>
      <ConnectivityNode name="OUT" pathName="SS1/VL1/L2/OUT"/>
      <ConductingEquipment type="DIS" name="L2-DS">
        <Terminal name="t1" connectivityNode="SS1/VL1/BUS/BUS1"/>
        <Terminal name="t2" connectivityNode="SS1/VL1/L2/IN"/>
      </ConductingEquipment>
      <ConductingEquipment type="CBR" name="L2-CB">
        <Terminal name="a" connectivityNode="SS1/VL1/L2/IN"/>
        <Terminal name="b" connectivityNode="SS1/VL1/L2/OUT"/>
      </ConductingEquipment>
      <ConductingEquipment type="CTR" name="L2-CT">
        <Terminal name="t" connectivityNode="SS1/VL1/L2/OUT"/>
      </ConductingEquipment>
      <ConductingEquipment type="VTR" name="L2-VT">
        <Terminal name="t" connectivityNode="SS1/VL1/L2/OUT"/>
      </ConductingEquipment>
    </Bay>
  </VoltageLevel>
</Substation><DataTypeTemplates>
  <LNodeType id="tLN0" lnClass="LLN0">
    <DO name="Mod" type="tENS"/>
  </LNodeType>
  <DOType id="tENS" cdc="ENS">
    <DA name="stVal" bType="Enum"/>
  </DOType>
</DataTypeTemplates></SCL>
