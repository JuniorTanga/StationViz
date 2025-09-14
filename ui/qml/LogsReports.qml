import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
    anchors.fill: parent
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 12
        Label { text: "Logs & Reports"; color: Theme.onSurface; font.pixelSize: 20; font.bold: true }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            model: 10
            delegate: Rectangle {
                height: 44; width: ListView.view.width; color: index%2? Theme.surface: Theme.surface2; border.color: Theme.border
                Row { anchors.fill: parent; anchors.margins: 12; spacing: 12
                    Label { text: "Log entry " + index; color: Theme.onSurface }
                    Label { text: "INFO"; color: Theme.success }
                }
            }
        }
    }
}
