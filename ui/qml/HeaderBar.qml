import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

ToolBar {
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
