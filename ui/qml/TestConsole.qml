import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
    anchors.fill: parent
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 12
        Label { text: "Test Console"; color: Theme.onSurface; font.pixelSize: 20; font.bold: true }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            TextField { Layout.fillWidth: true; placeholderText: "Chemin commande (ctlModelPath)" }
            Button { text: "Operate" }
        }
        Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 12; color: Theme.surface2; border.color: Theme.border
            Label { anchors.centerIn: parent; text: "Résultats de test / historique à venir" ; color: Theme.muted }
        }
    }
}
