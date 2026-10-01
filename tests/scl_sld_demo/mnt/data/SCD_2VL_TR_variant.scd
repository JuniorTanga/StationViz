<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007B" revision="B" release="4">
  <Header id="SCD_2VL_TR_variant">
    <History/>
  </Header>
<Substation name="SS1">
  <VoltageLevel name="VL1" nomFreq="50" numPhases="3">
    <Bay name="BUS1">
      <ConnectivityNode name="BUS1" pathName="SS1/VL1/BUS1/BUS1"/>
    </Bay>
  </VoltageLevel>
  <VoltageLevel name="VL2" nomFreq="50" numPhases="3">
    <Bay name="BUS2">
      <ConnectivityNode name="BUS2" pathName="SS1/VL2/BUS2/BUS2"/>
    </Bay>
  </VoltageLevel>
  <VoltageLevel name="VL1">
    <PowerTransformer name="TR1">
      <TransformerWinding name="W1">
        <Terminal name="t" connectivityNode="SS1/VL1/BUS1/BUS1"/>
      </TransformerWinding>
    </PowerTransformer>
  </VoltageLevel>
  <VoltageLevel name="VL2">
    <PowerTransformer name="TR1">
      <TransformerWinding name="W2">
        <Terminal name="t" connectivityNode="SS1/VL2/BUS2/BUS2"/>
      </TransformerWinding>
    </PowerTransformer>
  </VoltageLevel>
<Private type="ACME"><Note>variant transformer</Note></Private></Substation><DataTypeTemplates>
  <LNodeType id="tLN0" lnClass="LLN0">
    <DO name="Mod" type="tENS"/>
  </LNodeType>
  <DOType id="tENS" cdc="ENS">
    <DA name="stVal" bType="Enum"/>
  </DOType>
</DataTypeTemplates></SCL>
