import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

Pane {
  id: root
  property string name
  property var lds      // array
  property int mms: 0
  property int gse: 0
  property int sv: 0

  Theme { id: theme }
  padding: 10
  background: Rectangle { color: theme.panel; radius: 8; border.color: "#DADADA" }

  ColumnLayout {
    spacing: 8
    anchors.fill: parent

    RowLayout {
      Layout.fillWidth: true
      spacing: 10
      Label { text: root.name; font.bold: true; color: theme.text }

      Rectangle { width: 6; height: 6; radius: 3; color: theme.accent }
      Label { text: "MMS: " + root.mms; color: theme.subtext }
      Rectangle { width: 6; height: 6; radius: 3; color: "#2E7D32" }
      Label { text: "GOOSE: " + root.gse; color: theme.subtext }
      Rectangle { width: 6; height: 6; radius: 3; color: "#1565C0" }
      Label { text: "SV: " + root.sv; color: theme.subtext }

      Item { Layout.fillWidth: true }
    }

    // Colonnes LD
    Flickable {
      Layout.fillWidth: true
      Layout.fillHeight: true
      contentWidth: flow.implicitWidth
      contentHeight: flow.implicitHeight
      clip: true

      Flow {
        id: flow
        width: Math.max(parent.width, implicitWidth)
        spacing: 12

        Repeater {
          model: root.lds ?? []
          delegate: Frame {
            padding: 8
            background: Rectangle { color: "#FFFFFF"; radius: 6; border.color: "#E0E0E0" }
            Column {
              spacing: 6
              Label { text: modelData.inst || ""; color: theme.text; font.bold: true }
              Flow {
                width: 260
                spacing: 6
                Repeater {
                  model: modelData.equipments || []
                  delegate: Rectangle {
                    radius: 10; height: 22
                    color: "#F4F6F8"; border.color: "#D0D5DA"
                    Text {
                      anchors.centerIn: parent
                      text: (modelData.label || "")
                      color: theme.text
                      font.pixelSize: 12
                    }
                    width: Math.max(60, implicitWidth)
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
