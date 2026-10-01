import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import StationViz 1.0
import "../components"
import "../styles"

Page {
  id: page
  title: "IEDs"

  Theme { id: theme }

  header: ToolBar {
    background: Rectangle { color: theme.panel; border.color: "#DDDDDD" }
    RowLayout {
      anchors.fill: parent; anchors.margins: 6; spacing: 8
      Label { text: "Total IEDs: " + App.ieds.count; color: theme.subtext }
      Item { Layout.fillWidth: true }
    }
  }

  GridView {
    id: grid
    anchors.fill: parent
    cellWidth: 360
    cellHeight: 220
    model: App.ieds
    interactive: true
    delegate: IedCard {
      width: 340; height: 200
      name: model.name
      lds: model.lds
      mms: model.mms
      gse: model.gse
      sv: model.sv
    }
  }
}
