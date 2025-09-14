import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
    anchors.fill: parent
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 12
        Label { text: "Live Monitor"; color: Theme.onSurface; font.pixelSize: 20; font.bold: true }
        Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 12; color: Theme.surface2; border.color: Theme.border
            Label { anchors.centerIn: parent; text: "Abonnements GOOSE / lectures MMS (à connecter)" ; color: Theme.muted }
        }
    }
}
