import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import StationViz
import "../components"
import "../styles"

Page {
  id: page
  title: "Inventaire"
  clip: true

  Theme { id: theme }

  // 0 = Physique (SS/VL/Bay), 1 = Depuis IEDs
  property int mode: 0

  header: ToolBar {
    background: Rectangle { color: theme.panel; border.color: theme.border }
    RowLayout {
      anchors.fill: parent; anchors.margins: 6; spacing: 8

      Label {
        text: mode === 0 ? "Inventaire physique (SS → VL → Bay)"
                         : "Inventaire depuis IEDs (LNs)"
        color: theme.subtext
        font.pixelSize: 16
        Layout.alignment: Qt.AlignVCenter
      }

      Item { Layout.fillWidth: true }

      // Remplace SegmentedButton par TabBar + TabButton (standard QQC2)
      TabBar {
        id: seg
        Layout.preferredWidth: 260
        currentIndex: page.mode
        onCurrentIndexChanged: page.mode = currentIndex

        TabButton { text: "Physique" }
        TabButton { text: "IEDs" }
      }
    }
  }

  ScrollView {
    id: sc
    anchors.fill: parent
    clip: true

    Loader {
      id: contentLoader
      width: sc.availableWidth
      sourceComponent: mode === 0 ? physicalView : iedsView
    }
  }

  // ====================== VUE PHYSIQUE ======================
  Component {
    id: physicalView
    Column {
      width: sc.availableWidth
      spacing: 16
      padding: 12

      Repeater {
        model: App.equipmentInventory ?? []
        delegate: Column {
          width: sc.availableWidth
          spacing: 8

          Label {
            text: "Sous-station : " + (modelData.ss ?? "")
            font.pixelSize: 16
            font.bold: true
            color: theme.text
            padding: 2
          }

          Repeater {
            model: modelData.vls
            delegate: Frame {
              width: sc.availableWidth
              padding: 12
              background: Rectangle { color: theme.surface; radius: 10; border.color: theme.border }

              ColumnLayout {
                width: sc.availableWidth
                spacing: 8

                Label {
                  text: "Niveau : " + (modelData.vl ?? "")
                  font.pixelSize: 15
                  font.bold: true
                  color: theme.accentText
                }

                Repeater {
                  model: modelData.bays
                  delegate: Column {
                    width: sc.availableWidth
                    spacing: 6

                    Label {
                      text: "Bay : " + (modelData.bay ?? "")
                      font.pixelSize: 14
                      color: theme.subtext
                    }

                    Flow {
                      width: sc.availableWidth
                      spacing: 12

                      Repeater {
                        model: modelData.items
                        delegate: Rectangle {
                          radius: 10
                          color: "#f8fbff"
                          border.color: "#d5e3f3"
                          border.width: 1
                          width: 230
                          height: 66

                          // bande colorée à gauche par type
                          Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 4
                            color: kindColor(modelData.kind)
                            radius: 3
                          }

                          Row {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            Image {
                              source: modelData.icon
                              sourceSize.width: 30
                              sourceSize.height: 30
                              fillMode: Image.PreserveAspectFit
                              smooth: true
                              width: 30; height: 30
                            }

                            Column {
                              spacing: 2
                              Label {
                                text: modelData.label
                                color: theme.text
                                font.pixelSize: 13
                                elide: Label.ElideRight
                                width: 160
                              }
                              Label {
                                text: modelData.kind
                                color: kindColor(modelData.kind)
                                font.pixelSize: 11
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

  // ====================== VUE IEDs ======================
  Component {
    id: iedsView
    Column {
      width: sc.availableWidth
      spacing: 16
      padding: 12

      Repeater {
        model: App.inventoryIED ?? []
        delegate: Frame {
          width: sc.availableWidth
          padding: 12
          background: Rectangle { color: theme.surface; radius: 10; border.color: theme.groupBorder }

          ColumnLayout {
            width: sc.availableWidth
            spacing: 8

            Label {
              text: "IED : " + (modelData.name ?? "")
              font.pixelSize: 16
              font.bold: true
              color: theme.text
            }

            Repeater {
              model: modelData.lds
              delegate: Column {
                width: sc.availableWidth
                spacing: 6

                Label {
                  text: "LD " + (modelData.inst ?? "")
                  font.pixelSize: 14
                  color: theme.subtext
                }

                Flow {
                  width: sc.availableWidth
                  spacing: 12

                  Repeater {
                    model: modelData.items
                    delegate: Rectangle {
                      radius: 10
                      color: "#fffaf4"
                      border.color: "#f0d6b1"
                      border.width: 1
                      width: 260
                      height: 68

                      Row {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Image {
                          source: modelData.icon
                          sourceSize.width: 28
                          sourceSize.height: 28
                          fillMode: Image.PreserveAspectFit
                          smooth: true
                          width: 28; height: 28
                        }

                        Column {
                          spacing: 2
                          Label {
                            text: modelData.label
                            color: theme.text
                            font.pixelSize: 13
                            elide: Label.ElideRight
                            width: 170
                          }
                          Row {
                            spacing: 6
                            Rectangle {
                              radius: 8
                              color: kindSoftColor(modelData.kind)
                              border.color: kindColor(modelData.kind)
                              height: 18
                              width: implicitWidth
                              Row {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 4
                                Label {
                                  text: modelData.kind
                                  color: kindColor(modelData.kind)
                                  font.pixelSize: 10
                                }
                              }
                            }
                          }
                        }
                      }

                      // anchors (chips)
                      Row {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 6
                        spacing: 4
                        Repeater {
                          model: modelData.anchors ?? []
                          delegate: Rectangle {
                            radius: 8
                            color: "#eef2f6"
                            border.color: "#cfd7df"
                            height: 18
                            width: implicitWidth
                            Row {
                              anchors.fill: parent
                              anchors.margins: 4
                              spacing: 4
                              Label { text: modelData; font.pixelSize: 10; color: "#5b6b7a" }
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

  // ====================== UTIL: couleurs par type ======================
  function kindColor(kind) {
    const k = (kind || "").toString().toUpperCase()
    if (k === "CB" || k === "CIRCUITBREAKER") return "#0a84ff"
    if (k === "DS" || k === "DISCONNECTOR")   return "#f59e0b"
    if (k === "CT")                            return "#10b981"
    if (k === "VT")                            return "#a855f7"
    if (k.startsWith("TRANSFORMER"))          return "#f97316"
    if (k === "LINE" || k.endsWith("_LINE"))  return "#6b7280"
    return "#64748b"
  }
  function kindSoftColor(kind) {
    const c = kindColor(kind)
    return Qt.lighter(c, 1.4)
  }
}
