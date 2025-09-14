import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
    id: root
    width: 220
    Rectangle { anchors.fill: parent; color: Theme.surface2; border.color: Theme.border }

    signal navigate(string route)

    Column {
        anchors.fill: parent; anchors.margins: 8; spacing: 6
        Repeater {
            model: [
                {label: "Dashboard", route: "dashboard"},
                {label: "SCL Explorer", route: "scl"},
                {label: "SLD Viewer", route: "sld"},
                {label: "Live Monitor", route: "monitor"},
                {label: "Test Console", route: "console"},
                {label: "Logs & Reports", route: "logs"},
                {label: "Settings", route: "settings"}
            ]
            delegate: Button {
                text: modelData.label
                background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: 8 }
                contentItem: Label { text: parent.text; color: Theme.onSurface; leftPadding: 12; rightPadding: 12 }
                onClicked: root.navigate(modelData.route)
            }
        }
    }
}
