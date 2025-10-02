import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import StationViz 1.0
import "../styles"

Page {
  id: page
  title: "IEDs — par travée (SS / VL / BAY)"

  Theme { id: theme }
  background: Rectangle { color: theme.window }

  header: ToolBar {
    background: Rectangle { color: theme.panel; border.color: "#DDDDDD" }
    RowLayout {
      anchors.fill: parent; anchors.margins: 6; spacing: 8
      Label { text: "Total IEDs: " + App.ieds.count; color: theme.subtext }
      Item { Layout.fillWidth: true }
    }
  }

  ScrollView {
    anchors.fill: parent
    clip: true

    Column {
      id: rootCol
      width: parent.width
      spacing: 18

      Repeater {
        model: App.iedGroups  // liste de groupes { ss, vl, bays:[...] }

        delegate: Column {
          width: rootCol.width
          spacing: 8

          // Titre du groupe SS/VL
          Rectangle {
            width: parent.width; height: 34; radius: 6
            color: "#F5F7FA"; border.color: "#E0E5EA"
            Row {
              anchors.fill: parent; anchors.margins: 8; spacing: 12
              Label { text: (modelData.ss || "—") + " — " + (modelData.vl || "—"); font.bold: true; color: theme.text }
            }
          }

          // Colonnes par BAY
          Flickable {
            width: parent.width
            height: Math.max(260, contentItem.implicitHeight)
            contentWidth: row.implicitWidth
            contentHeight: row.implicitHeight
            clip: true

            Row {
              id: row
              spacing: 16

              Repeater {
                model: modelData.bays || []
                delegate: Frame {
                  width: 360; padding: 8
                  background: Rectangle { color: "#FFFFFF"; radius: 8; border.color: "#E0E0E0" }

                  Column {
                    width: parent.width; spacing: 8

                    // Titre BAY
                    Rectangle {
                      width: parent.width; height: 28; radius: 4
                      color: "#EEF2F6"; border.color: "#D7DEE6"
                      Row {
                        anchors.fill: parent; anchors.margins: 6
                        Label { text: modelData.name || "(Bay ?)"; color: theme.text; font.bold: true }
                      }
                    }

                    // IEDs dans la travée
                    Repeater {
                      model: modelData.ieds || []
                      delegate: Frame {
                        width: parent.width; padding: 8
                        background: Rectangle { color: "#FAFBFC"; radius: 6; border.color: "#E6EBF0" }

                        Column {
                          width: parent.width; spacing: 6

                          Row {
                            spacing: 10
                            Label { text: modelData.name; font.bold: true; color: theme.text }
                            Rectangle { width: 6; height: 6; radius: 3; color: theme.accent }
                            Label { text: "MMS: " + (modelData.mms || 0); color: theme.subtext }
                            Rectangle { width: 6; height: 6; radius: 3; color: "#2E7D32" }
                            Label { text: "GOOSE: " + (modelData.gse || 0); color: theme.subtext }
                            Rectangle { width: 6; height: 6; radius: 3; color: "#1565C0" }
                            Label { text: "SV: " + (modelData.sv || 0); color: theme.subtext }
                          }

                          // LDs -> équipements
                          Repeater {
                            model: modelData.lds || []
                            delegate: Column {
                              width: parent.width; spacing: 4
                              Label { text: (modelData.inst || "LD"); font.bold: true; color: theme.text }

                              Flow {
                                width: parent.width; spacing: 6
                                Repeater {
                                  model: modelData.equipments || []
                                  delegate: Rectangle {
                                    radius: 10; height: 22
                                    color: "#F4F6F8"; border.color: "#D0D5DA"
                                    Text {
                                      anchors.centerIn: parent
                                      text: (modelData.label || "")
                                      color: theme.text; font.pixelSize: 12
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
              }
            }
          }
        }
      }
    }
  }
}
