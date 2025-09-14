<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="Poste_A">
    <VoltageLevel name="VL1" nomFreq="50" numPhases="3">
      <Voltage>225000</Voltage>
      <Bay name="Bay1">
        <ConductingEquipment name="CB1" type="CBR" desc="Disjoncteur principal" />
        <ConductingEquipment name="DS1" type="DIS" desc="Sectionneur départ" iedName="IED1" />
      </Bay>
    </VoltageLevel>
  </Substation>

  <IED name="IED1" manufacturer="Siemens">
    <AccessPoint name="AP1"/>
  </IED>
</SCL>
