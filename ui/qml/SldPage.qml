import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
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
