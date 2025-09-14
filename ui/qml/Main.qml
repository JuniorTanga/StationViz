import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "Theme.js" as Theme

ApplicationWindow {
    id: win
    visible: true
    width: 1280; height: 800
    title: "StationViz"
    color: Theme.surface

    property string route: "dashboard"

    ToolBar {
        title: route === "dashboard" ? "Dashboard"
             : route === "scl" ? "SCL Explorer"
             : route === "sld" ? "SLD Viewer"
             : route === "monitor" ? "Live Monitor"
             : route === "console" ? "Test Console"
             : route === "logs" ? "Logs & Reports"
             : route === "settings" ? "Settings"
             : "StationViz"
        rightContent: [ Button { text: "Charger SCL"; onClicked: fileDialog.open() } ]

        id: header
        height: 48
        background: Rectangle { color: Theme.surface2; border.color: Theme.border }
        property alias title: titleLabel.text
        property alias rightContent: rightRow.data

        Row {
            anchors.fill: parent; anchors.margins: 8; spacing: 12
            Label { id: titleLabel; text: "StationViz"; color: Theme.onSurface; font.pixelSize: 18; font.bold: true }
            //Item { Layout.fillWidth: true; width: 1 }
            Row { id: rightRow; spacing: 8 }
        }
    }

    FileDialog {
        id: fileDialog
        nameFilters: ["SCL files (*.icd *.scd *.cid *.iid)"]
        onAccepted: { if (sclManager) sclManager.load(selectedFile) }
    }

    RowLayout {
        anchors.fill: parent
        Item {
            id: nav
            Layout.preferredWidth: 220
            //onNavigate: (r)=> win.route = r
        }
        Rectangle { width: 1; color: Theme.border }
        Loader {
            id: content
            Layout.fillWidth: true; Layout.fillHeight: true
            sourceComponent: route === "dashboard" ? dashboard
                            : route === "scl" ? scl
                            : route === "sld" ? sld
                            : route === "monitor" ? monitor
                            : route === "console" ? consolePg
                            : route === "logs" ? logs
                            : settings
        }
    }

    Component { id: dashboard; Item {
            anchors.fill: parent
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Label { text: "Dashboard"; color: Theme.onSurface; font.pixelSize: 20; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 140; radius: 12; color: Theme.surface2; border.color: Theme.border
                        Label { anchors.centerIn: parent; text: "Dernier SCL chargé" ; color: Theme.onSurface }
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 140; radius: 12; color: Theme.surface2; border.color: Theme.border
                        Label { anchors.centerIn: parent; text: "Connexions IED (MMS/GOOSE)" ; color: Theme.onSurface }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 12; color: Theme.surface2; border.color: Theme.border
                    Label { anchors.centerIn: parent; text: "Graphes et KPIs à venir" ; color: Theme.muted }
                }
            }
        }
 }

    Component { id: scl; Item {
            anchors.fill: parent
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Label { text: "SCL Explorer"; color: Theme.onSurface; font.pixelSize: 20; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
                    TreeView {
                        id: tree
                        Layout.preferredWidth: 360; Layout.fillHeight: true
                        model: [] // TODO: bind to SclManager
                    }
                    Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 12; color: Theme.surface2; border.color: Theme.border
                        Label { anchors.centerIn: parent; text: "Détails élément SCL" ; color: Theme.onSurface }
                    }
                }
            }
        }
 }


    Component { id: sld; Item {
            anchors.fill: parent
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Label { text: "SLD Viewer"; color: Theme.onSurface; font.pixelSize: 20; font.bold: true }
                SldView {
                    id: sldView
                    Layout.fillWidth: true; Layout.fillHeight: true
                    nodeModel: sldNodeModel
                    edgeModel: sldEdgeModel
                    onNodeClicked: (id)=> { console.log("clicked", id) }
                }
            }
        }
    }


    // Component { id: monitor; SldPage {} }
    // Component { id: consolePg; SldPage {} }
    // Component { id: logs; SldPage {} }
    // Component { id: settings; SldPage {} }
}
