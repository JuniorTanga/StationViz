import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import StationViz
import "../styles"

ToolBar {
  id: root

  signal openClicked()
  signal fitClicked()
  signal zoomIn()
  signal zoomOut()

  Theme {
    id: theme
  }

  background: Rectangle {
    color: theme.panel
    border.color: "#DDDDDD"
  }

  RowLayout {
    anchors.fill: parent
    anchors.margins: 6
    spacing: 8

    ToolButton { text: "Ouvrir";    onClicked: root.openClicked() }
    ToolButton { text: "Adapter";   onClicked: root.fitClicked() }
    ToolButton { text: "+";         onClicked: root.zoomIn() }
    ToolButton { text: "–";         onClicked: root.zoomOut() }

    Label {
      Layout.fillWidth: true
      horizontalAlignment: Text.AlignRight
      text: App && App.uiStore ? (App.uiStore.currentFile || "—") : "—"
      color: theme.subtext
    }
  }
}

