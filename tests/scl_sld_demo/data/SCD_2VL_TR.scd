<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007B" revision="B" release="4">
  <Header id="SCD_2VL_TR">
    <History/>
  </Header>
<Substation name="SS1">
  <VoltageLevel name="VL1" nomFreq="50" numPhases="3">
    <Voltage unit="V" multiplier="k">380</Voltage>
    <Bay name="BUS1">
      <ConnectivityNode name="BUS1" pathName="SS1/VL1/BUS1/BUS1"/>
    </Bay>
  </VoltageLevel>
  <VoltageLevel name="VL2" nomFreq="50" numPhases="3">
    <Voltage unit="V" multiplier="k">225</Voltage>
    <Bay name="BUS2">
      <ConnectivityNode name="BUS2" pathName="SS1/VL2/BUS2/BUS2"/>
    </Bay>
  </VoltageLevel>
  <!-- PowerTransformer is a child of Substation per IEC 61850-6, not of
       VoltageLevel. The previous version of this fixture nested it inside
       VoltageLevel, which is not schema valid, so the parser (correctly)
       ignored it and the transformer never reached the SLD. -->
  <PowerTransformer name="TR1" type="PT">
    <TransformerWinding name="W1" type="PTW">
      <Terminal name="t" connectivityNode="SS1/VL1/BUS1/BUS1"/>
    </TransformerWinding>
    <TransformerWinding name="W2" type="PTW">
      <Terminal name="t" connectivityNode="SS1/VL2/BUS2/BUS2"/>
    </TransformerWinding>
  </PowerTransformer>
</Substation><DataTypeTemplates>
  <LNodeType id="tLN0" lnClass="LLN0">
    <DO name="Mod" type="tENS"/>
  </LNodeType>
  <DOType id="tENS" cdc="ENS">
    <DA name="stVal" bType="Enum"/>
  </DOType>
</DataTypeTemplates></SCL>
