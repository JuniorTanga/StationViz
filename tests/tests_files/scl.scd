<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <Header id="StationVizDemo" version="1" revision="0"/>
  <!-- ===================== SUBSTATION ===================== -->
  <Substation name="S12">
    <!-- ===== E1 (225 kV) ===== -->
    <VoltageLevel name="E1" nomFreq="50">
      <Voltage unit="kV" multiplier="k">225</Voltage>
      <!-- Bay W1 avec CN de bus et équipements -->
      <Bay name="W1">
        <!-- CN de bus (deux barres pour clustering + coupler) -->
        <ConnectivityNode name="BB1" pathName="S12/E1/W1/BB1"/>
        <ConnectivityNode name="BB2" pathName="S12/E1/W1/BB2"/>

        <!-- BusbarSection (relie des segments de BB1 pour marquer 'bus-like') -->
        <ConductingEquipment name="BBS1" type="BusbarSection">
          <Terminal name="T1" cNodeName="BB1"/>
          <Terminal name="T2" cNodeName="BB1"/>
        </ConductingEquipment>

        <!-- Coupler: Breaker entre BB1 et BB2 -->
        <ConductingEquipment name="COUP1" type="CB">
          <Terminal name="A" cNodeName="BB1"/>
          <Terminal name="B" cNodeName="BB2"/>
          <!-- Lien logique vers IED PROT2 -->
          <LNode iedName="PROT2" ldInst="LD1" lnClass="XCBR" lnInst="1"/>
        </ConductingEquipment>

        <!-- Feeder Q01: CB -> DS -> CT -> LINE (oriente depuis BB1) -->
        <ConductingEquipment name="Q01_CB" type="CB">
          <Terminal name="T1" cNodeName="BB1"/>
          <Terminal name="T2" cNodeName="Q01_CN1"/>
          <LNode iedName="PROT1" ldInst="LD1" lnClass="XCBR" lnInst="1"/>
        </ConductingEquipment>

        <ConductingEquipment name="Q01_DS" type="DS">
          <Terminal name="T1" cNodeName="Q01_CN1"/>
          <Terminal name="T2" cNodeName="Q01_CN2"/>
          <LNode iedName="PROT1" ldInst="LD1" lnClass="XSWI" lnInst="1"/>
        </ConductingEquipment>

        <ConductingEquipment name="Q01_CT" type="CT">
          <Terminal name="T1" cNodeName="Q01_CN2"/>
          <Terminal name="T2" cNodeName="Q01_CN3"/>
          <LNode iedName="PROT1" ldInst="LD1" lnClass="TCTR" lnInst="1"/>
        </ConductingEquipment>

        <ConductingEquipment name="Q01_LINE" type="Line">
          <Terminal name="T1" cNodeName="Q01_CN3"/>
          <Terminal name="T2" cNodeName="Q01_EXT"/>
        </ConductingEquipment>
      </Bay>

      <!-- Bay TR1 sur E1 : borne HV du transformer -->
      <Bay name="TR1_E1">
        <ConnectivityNode name="TR1_HV" pathName="S12/E1/TR1_E1/TR1_HV"/>
        <ConductingEquipment name="T1" type="PowerTransformer">
          <!-- borne côté E1 : au bus BB2 -->
          <Terminal name="H1" cNodeName="BB2"/>
          <!-- borne vers le LV (relié au bus M1/BB1 via l'autre VL) -->
          <Terminal name="X1" cNodeName="TR1_HV"/>
          <LNode iedName="PROT2" ldInst="LD1" lnClass="PTRC" lnInst="1"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>

    <!-- ===== M1 (20 kV) ===== -->
    <VoltageLevel name="M1" nomFreq="50">
      <Voltage unit="kV" multiplier="k">20</Voltage>
      <Bay name="W1">
        <ConnectivityNode name="BB1" pathName="S12/M1/W1/BB1"/>
        <!-- Petite section de barre pour marquer bus -->
        <ConductingEquipment name="BBS_M1" type="BusbarSection">
          <Terminal name="T1" cNodeName="BB1"/>
          <Terminal name="T2" cNodeName="BB1"/>
        </ConductingEquipment>
      </Bay>

      <!-- Bay TR1_M1 : borne LV du même transformer (idée : lier deux bus de VL différents) -->
      <Bay name="TR1_M1">
        <ConnectivityNode name="TR1_LV" pathName="S12/M1/TR1_M1/TR1_LV"/>
        <ConductingEquipment name="T1_M1" type="PowerTransformer">
          <!-- borne côté M1 : au bus BB1 (M1) -->
          <Terminal name="X1" cNodeName="BB1"/>
          <!-- liaison fictive vers le HV (pour compléter la topologie LV) -->
          <Terminal name="H1" cNodeName="TR1_LV"/>
          <LNode iedName="PROT2" ldInst="LD1" lnClass="PTRC" lnInst="2"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>

  <!-- ===================== IEDs ===================== -->
  <IED name="PROT1" manufacturer="DemoCorp" type="Protection">
    <AccessPoint name="AP1">
      <Address>
        <P type="IP">192.168.10.11</P>
      </Address>
      <Server>
        <LDevice inst="LD1">
          <LN0 lnClass="LLN0" inst=""/>
          <LN lnClass="XCBR" inst="1"/>
          <LN lnClass="XSWI" inst="1"/>
          <LN lnClass="TCTR" inst="1"/>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>

  <IED name="PROT2" manufacturer="DemoCorp" type="BayCtrl">
    <AccessPoint name="AP1">
      <Address>
        <P type="IP">192.168.10.12</P>
      </Address>
      <Server>
        <LDevice inst="LD1">
          <LN0 lnClass="LLN0" inst=""/>
          <LN lnClass="XCBR" inst="1"/>
          <LN lnClass="PTRC" inst="1"/>
          <LN lnClass="PTRC" inst="2"/>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>

  <IED name="SCADA1" manufacturer="DemoCorp" type="HMI">
    <AccessPoint name="AP1">
      <Address>
        <P type="IP">192.168.20.50</P>
      </Address>
      <Server>
        <LDevice inst="LD_STA">
          <LN0 lnClass="LLN0" inst=""/>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>

  <!-- ===================== Communication ===================== -->
  <Communication>
    <!-- Process bus: GOOSE + SV -->
    <SubNetwork name="ProcessBus" type="8-1">
      <ConnectedAP iedName="PROT1" apName="AP1">
        <Address>
          <P type="IP">192.168.10.11</P>
          <P type="Subnet">255.255.255.0</P>
          <P type="MAC-Address">01-0C-CD-01-00-11</P>
          <P type="VLAN-ID">100</P>
          <P type="VLAN-PRIORITY">4</P>
        </Address>
        <GSE ldInst="LD1" cbName="GOOSE1">
          <Address>
            <P type="MAC-Address">01-0C-CD-01-00-01</P>
            <P type="APPID">1001</P>
            <P type="VLAN-ID">100</P>
            <P type="VLAN-PRIORITY">4</P>
          </Address>
        </GSE>
        <SMV ldInst="LD1" cbName="SMV1">
          <Address>
            <P type="MAC-Address">01-0C-CD-04-00-01</P>
            <P type="APPID">2001</P>
            <P type="VLAN-ID">101</P>
            <P type="VLAN-PRIORITY">4</P>
          </Address>
        </SMV>
      </ConnectedAP>

      <ConnectedAP iedName="PROT2" apName="AP1">
        <Address>
          <P type="IP">192.168.10.12</P>
          <P type="Subnet">255.255.255.0</P>
          <P type="MAC-Address">01-0C-CD-01-00-12</P>
          <P type="VLAN-ID">100</P>
          <P type="VLAN-PRIORITY">4</P>
        </Address>
        <GSE ldInst="LD1" cbName="GOOSE2">
          <Address>
            <P type="MAC-Address">01-0C-CD-01-00-02</P>
            <P type="APPID">1002</P>
            <P type="VLAN-ID">100</P>
            <P type="VLAN-PRIORITY">4</P>
          </Address>
        </GSE>
      </ConnectedAP>
    </SubNetwork>

    <!-- Station bus: MMS -->
    <SubNetwork name="StationBus" type="8-MMS">
      <ConnectedAP iedName="SCADA1" apName="AP1">
        <Address>
          <P type="IP">192.168.20.50</P>
          <P type="Subnet">255.255.255.0</P>
        </Address>
      </ConnectedAP>
    </SubNetwork>
  </Communication>
</SCL>
