import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import StationViz
import "../components"
import "../styles"

Page {
  id: page
  title: "Inventaire"
  clip: true

  // Thème + fallbacks robustes
  Theme { id: theme }
  function co(v, fb) { return v === undefined ? fb : v }
  function fs(v, fb) { return (v>0) ? v : fb }

  // 0 = Physique (SS → VL → Bay), 1 = Depuis IEDs
  property int mode: 0

  // Couleurs par type d’équipement (physique)
  function kindColor(kind) {
    const k = (kind||"").toString().toUpperCase()
    if (k === "CB" || k === "CIRCUITBREAKER" || k === "BREAKER") return "#0a84ff"   // bleu
    if (k === "DS" || k === "DISCONNECTOR")                       return "#f59e0b"   // orange
    if (k === "CT")                                               return "#10b981"   // vert
    if (k === "VT")                                               return "#a855f7"   // violet
    if (k.indexOf("TRANSFORMER") === 0)                           return "#f97316"   // orange foncé
    if (k === "LINE" || k.endsWith("_LINE"))                      return "#6b7280"   // gris
    return "#64748b" // défaut
  }
  function kindSoftColor(kind) { return Qt.lighter(kindColor(kind), 1.35) }

  // Couleurs par classe LN (IED)
  function lnColor(cls) {
    const k = (cls||"").toString().toUpperCase()
    if (k === "XCBR") return "#0a84ff"
    if (k === "XSWI") return "#f59e0b"
    if (k === "TCTR" || k === "MMXU") return "#10b981"
    if (k === "TVTR") return "#a855f7"
    // protections usuelles (indicatif)
    if (k === "PTOC" || k === "PDIF" || k === "PTOV" || k === "PTUV") return "#ef4444" // rouge
    return "#64748b"
  }
  function lnSoftColor(cls) { return Qt.lighter(lnColor(cls), 1.35) }

  header: Rectangle {
    color: co(theme.panel, "#f5f7fb")
    border.color: co(theme.border, "#d9dee7")
    height: 44

    RowLayout {
      anchors.fill: parent
      anchors.margins: 8
      spacing: 12

      Label {
        text: mode === 0 ? "Inventaire physique (SS → VL → Bay)"
                         : "Inventaire depuis IEDs (LNs)"
        color: co(theme.subtext, "#4b5563")
        font.pixelSize: fs(theme.h6, 16)
        Layout.alignment: Qt.AlignVCenter
      }

      Item { Layout.fillWidth: true }

      TabBar {
        id: tabs
        currentIndex: page.mode
        onCurrentIndexChanged: page.mode = currentIndex
        Layout.preferredWidth: 260
        TabButton { text: "Physique" }
        TabButton { text: "IEDs" }
      }
    }
  }

  ScrollView {
    id: sc
    anchors.fill: parent
    clip: true
    contentWidth: availableWidth

    Loader {
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

          // SS Header
          Rectangle {
            width: sc.availableWidth
            height: 28
            radius: 6
            color: co(theme.surface, "#ffffff")
            border.color: co(theme.border, "#dbe3ee")
            Row {
              anchors.fill: parent
              anchors.margins: 6
              spacing: 8
              Label {
                text: "Sous-station : " + (modelData.ss ?? "")
                font.bold: true
                font.pixelSize: 16
                color: co(theme.text, "#111827")
              }
            }
          }

          // VL sections
          Repeater {
            model: modelData.vls
            delegate: Column {
              width: sc.availableWidth
              spacing: 8

              Rectangle {
                width: sc.availableWidth
                radius: 10
                color: co(theme.surface, "#ffffff")
                border.color: co(theme.border, "#dbe3ee")
                border.width: 1

                ColumnLayout {
                  width: sc.availableWidth
                  spacing: 8
                  anchors.margins: 12

                  Label {
                    text: "Niveau : " + (modelData.vl ?? "")
                    font.pixelSize: 15
                    font.bold: true
                    color: co(theme.accentText, "#0f172a")
                  }

                  // Par Bay
                  Repeater {
                    model: modelData.bays
                    delegate: Column {
                      width: sc.availableWidth
                      spacing: 6

                      Label {
                        text: "Bay : " + (modelData.bay ?? "")
                        font.pixelSize: 13
                        color: co(theme.subtext, "#475569")
                      }

                      // Cartes d’équipements (sans icônes)
                      Flow {
                        width: sc.availableWidth
                        spacing: 12

                        Repeater {
                          model: modelData.items
                          delegate: Rectangle {
                            radius: 10
                            color: "#ffffff"
                            border.color: "#d5e3f3"
                            border.width: 1
                            width: 240
                            height: 68

                            // bande latérale colorée
                            Rectangle {
                              anchors.left: parent.left
                              anchors.top: parent.top
                              anchors.bottom: parent.bottom
                              width: 4
                              color: kindColor(modelData.kind)
                              radius: 3
                            }

                            Column {
                              anchors.fill: parent
                              anchors.margins: 10
                              spacing: 6

                              Label {
                                text: modelData.label
                                color: co(theme.text, "#0f172a")
                                font.pixelSize: 13
                                elide: Label.ElideRight
                              }

                              // “chip” type
                              Rectangle {
                                radius: 8
                                color: kindSoftColor(modelData.kind)
                                border.color: kindColor(modelData.kind)
                                height: 20
                                width: implicitWidth
                                Row {
                                  anchors.fill: parent
                                  anchors.margins: 6
                                  spacing: 6
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
        delegate: Column {
          width: sc.availableWidth
          spacing: 8

          // IED Header
          Rectangle {
            width: sc.availableWidth
            height: 28
            radius: 6
            color: co(theme.surface, "#fff7ed")
            border.color: co(theme.border, "#f0d6b1")
            Row {
              anchors.fill: parent
              anchors.margins: 6
              spacing: 8
              Label {
                text: "IED : " + (modelData.name ?? "")
                font.bold: true
                font.pixelSize: 16
                color: co(theme.text, "#111827")
              }
            }
          }

          // LDs
          Repeater {
            model: modelData.lds
            delegate: Column {
              width: sc.availableWidth
              spacing: 6

              Label {
                text: "LD " + (modelData.inst ?? "")
                font.pixelSize: 13
                color: co(theme.subtext, "#6b7280")
              }

              Flow {
                width: sc.availableWidth
                spacing: 12

                Repeater {
                  model: modelData.items
                  delegate: Rectangle {
                    radius: 10
                    color: "#ffffff"
                    border.color: "#f0d6b1"
                    border.width: 1
                    width: 270
                    height: 76

                    // bande de classe LN
                    Rectangle {
                      anchors.left: parent.left
                      anchors.top: parent.top
                      anchors.bottom: parent.bottom
                      width: 4
                      color: lnColor(modelData.lnClass || modelData.kind)
                      radius: 3
                    }

                    Column {
                      anchors.fill: parent
                      anchors.margins: 10
                      spacing: 6

                      Label {
                        text: modelData.label
                        color: co(theme.text, "#111827")
                        font.pixelSize: 13
                        elide: Label.ElideRight
                      }

                      // chip LN class
                      Rectangle {
                        radius: 8
                        color: lnSoftColor(modelData.lnClass || modelData.kind)
                        border.color: lnColor(modelData.lnClass || modelData.kind)
                        height: 20
                        width: implicitWidth
                        Row {
                          anchors.fill: parent
                          anchors.margins: 6
                          spacing: 6
                          Label {
                            text: (modelData.lnClass || modelData.kind)
                            color: lnColor(modelData.lnClass || modelData.kind)
                            font.pixelSize: 11
                          }
                        }
                      }
                    }

                    // anchors (chips en bas à droite)
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
                            Label { text: modelData; font.pixelSize: 10; color: "#334155" }
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
