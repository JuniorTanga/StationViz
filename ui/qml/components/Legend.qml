import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

Pane {
  id: root
  padding: 8
  background: Rectangle { color: theme.panel; radius: 6; border.color: "#DDDDDD" }
  Theme {
    id: theme
  }

  ColumnLayout {
    spacing: 6
    Label { text: "Légende"; font.bold: true }

    Repeater {
      model: [
        { label: "Barre (Bus)", color: theme.edge },
        { label: "Feeder",     color: theme.node },
        { label: "Coupler",    color: theme.accent },
        { label: "Transformateur", color: theme.subtext }
      ]
      delegate: RowLayout {
        spacing: 8
        Rectangle { width: 18; height: 2; color: modelData.color; Layout.alignment: Qt.AlignVCenter }
        Label { text: modelData.label; color: theme.text }
      }
    }
  }
}
