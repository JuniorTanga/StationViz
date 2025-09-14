import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
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
