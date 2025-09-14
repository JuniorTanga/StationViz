import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
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
