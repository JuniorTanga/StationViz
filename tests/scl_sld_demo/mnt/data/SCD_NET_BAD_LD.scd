<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007B" revision="B" release="4">
  <Header id="SCD_NET_BAD_LD">
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
</Substation><IED name="IED1">
  <AccessPoint name="AP1">
    <Server>
      <LDevice inst="LD0">
        <LN0 lnClass="LLN0" inst="" lnType="tLN0">

        <DataSet name="D1">
          <FCDA ldInst="LD0" lnClass="LLN0" lnInst="" doName="Mod" fc="ST"/>
        </DataSet>
          <GSEControl name="G1" type="GOOSE" datSet="D1" appID="0001" confRev="1"/>
          <SampledValueControl name="SV1" datSet="D1" smvID="SV1" multicast="true" confRev="1">
            <SamplingRate>4000</SamplingRate>
          </SampledValueControl>
        </LN0>
      </LDevice>
    </Server>
  </AccessPoint>
</IED><Communication>
  <SubNetwork name="LAN" type="8-MMS">
    <ConnectedAP iedName="IED1" apName="AP1">
      <Address>
        <P type="IP">192.168.1.10</P>
        <P type="IP-SUBNET">255.255.255.0</P>
        <P type="IP-GATEWAY">192.168.1.1</P>
        <P type="OSI-TPKT-Port">102</P>
      </Address>
      <GSE ldInst="LD99" cbName="G1">
        <Address>
          <P type="MAC-Address">01-0C-CD-01-00-01</P>
          <P type="APPID">0001</P>
          <P type="VLAN-ID">000</P>
          <P type="VLAN-PRIORITY">4</P>
        </Address>
      </GSE>
      <SMV ldInst="LD99" cbName="SV1">
        <Address>
          <P type="MAC-Address">01-0C-CD-04-00-01</P>
          <P type="APPID">4001</P>
          <P type="VLAN-ID">000</P>
          <P type="VLAN-PRIORITY">4</P>
          <P type="SmpRate">4000</P>
        </Address>
      </SMV>
    </ConnectedAP>
  </SubNetwork>
</Communication><DataTypeTemplates>
  <LNodeType id="tLN0" lnClass="LLN0">
    <DO name="Mod" type="tENS"/>
  </LNodeType>
  <DOType id="tENS" cdc="ENS">
    <DA name="stVal" bType="Enum"/>
  </DOType>
</DataTypeTemplates></SCL>
