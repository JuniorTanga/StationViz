import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

Rectangle {
  id: root
  // API
  property var tabs: [
    { id: "ieds", label: "IED", icon: "\u25A3" },          //
    { id: "sld",  label: "SUBSTATION", icon: "\u229E" },   //
    { id: "comm", label: "COMMUNICATION", icon: "\u21C4" },//
    { id: "tpl",  label: "TEMPLATES", icon: "\u25AF" }     //
  ]
  property string currentId: "sld"
  signal tabClicked(string id)

  // thème (ton pattern)
  Theme { id: theme }

  height: 44
  color: theme.panel          // bande teal (proche screenshot)
  border.color: "#1E8D86"

  RowLayout {
    anchors.fill: parent
    anchors.margins: 8
    spacing: 22

    Repeater {
      model: root.tabs
      delegate: Item {
        id: tab
        width: Math.max(140, label.implicitWidth + 38)
        height: parent.height - 8

        property bool selected: (modelData.id === root.currentId)

        Rectangle {
          anchors.fill: parent
          radius: 8
          color: tab.selected ? "#E8FBF7" : "transparent"
          border.color: tab.selected ? "#FFFFFF" : "transparent"
          opacity: tab.selected ? 1 : 0.25
        }

        Row {
          anchors.fill: parent
          anchors.margins: 10
          spacing: 8

          // icône (unicode simple : pas d’asset requis)
          Text {
            text: modelData.icon || ""
            color: tab.selected ? "#12524D" : "#0B2E2C"
            font.pixelSize: 16
            verticalAlignment: Text.AlignVCenter
          }

          // libellé
          Text {
            id: label
            text: modelData.label || ""
            color: tab.selected ? "#0B2E2C" : "#0B2E2C"
            opacity: tab.selected ? 1.0 : 0.9
            font.pixelSize: 13
            font.letterSpacing: 1.2
            font.bold: true
            verticalAlignment: Text.AlignVCenter
          }
        }

        MouseArea {
          anchors.fill: parent
          onClicked: root.tabClicked(modelData.id)
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
        }

        // soulignement violet façon capture
        Rectangle {
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.bottom: parent.bottom
          height: 3
          radius: 2
          color: tab.selected ? "#B36EF3" : "transparent"
        }
      }
    }

    Item { Layout.fillWidth: true } // pousse à gauche comme sur screenshot
  }
}
