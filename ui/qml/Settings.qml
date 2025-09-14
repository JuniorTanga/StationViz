import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
    anchors.fill: parent
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 12
        Label { text: "Settings"; color: Theme.onSurface; font.pixelSize: 20; font.bold: true }
        GroupBox {
            title: "Apparence"
            Layout.fillWidth: true
            Column { anchors.margins: 8; anchors.fill: parent; spacing: 8
                CheckBox { text: "Mode sombre (fixe)"; checked: true; enabled: false }
            }
        }
        GroupBox {
            title: "IEC 61850"
            Layout.fillWidth: true
            Column { anchors.margins: 8; anchors.fill: parent; spacing: 8
                TextField { placeholderText: "Adresse MMS par défaut"; text: "127.0.0.1" }
                TextField { placeholderText: "Port MMS"; text: "102" }
            }
        }
    }
}
