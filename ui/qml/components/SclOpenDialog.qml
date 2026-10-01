import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../styles"

Dialog {
  id: dlg
  modal: true
  focus: true
  x: (parent ? (parent.width  - width)/2 : 100)
  y: (parent ? (parent.height - height)/2 : 80)
  width: Math.min(720, parent ? parent.width*0.9 : 720)
  background: Rectangle { color: theme.panel; radius: 10; border.color: "#D0D5DA" }
  padding: 14

  property url selectedUrl: ""
  property alias busy: busyRect.visible

  Theme { id: theme }

  signal requestOpenUrl(url url)

  header: RowLayout {
    spacing: 8
    Label { text: "Charger un fichier SCL"; font.bold: true; color: theme.text }
  }

  contentItem: ColumnLayout {
    spacing: 12

    // Zone drag & drop
    Rectangle {
      id: drop
      Layout.fillWidth: true
      Layout.preferredHeight: 160
      radius: 10
      color: theme.surface
      border.color: "#C8CDD3"
      border.width: 1

      Column {
        anchors.centerIn: parent
        spacing: 6
        Label { text: "Glissez un fichier ici ou cliquez pour parcourir"; color: theme.subtext }
        Label { text: selectedUrl ? selectedUrl.toString() : ""; color: theme.text; elide: Text.ElideRight; width: parent.width - 40 }
        Button {
          text: "Parcourir…"
          enabled: !dlg.busy
          onClicked: fileDialog.open()
        }
      }

      DropArea {
        anchors.fill: parent
        onDropped: function(ev) {
          if (ev.urls && ev.urls.length>0) dlg.selectedUrl = ev.urls[0]
        }
      }

      // Overlay busy
      Rectangle {
        id: busyRect
        anchors.fill: parent
        color: "#80000000"
        radius: 10
        visible: false
        Column {
          anchors.centerIn: parent
          spacing: 10
          BusyIndicator { running: true }
          Label { text: "Analyse du fichier…"; color: "white" }
        }
      }
    }

    // Actions
    RowLayout {
      Layout.fillWidth: true
      spacing: 8
      Item { Layout.fillWidth: true }
      Button {
        text: "Annuler"; enabled: !dlg.busy
        onClicked: dlg.close()
      }
      Button {
        text: "Charger"; enabled: !dlg.busy && dlg.selectedUrl
        icon.name: "document-open"
        onClicked: dlg.requestOpenUrl(dlg.selectedUrl)
      }
    }
  }

  FileDialog {
    id: fileDialog
    title: "Ouvrir un fichier SCL"
    nameFilters: ["SCL (*.scd *.cid *.icd *.iid *.ssd)","Tous les fichiers (*)"]
    onAccepted: dlg.selectedUrl = selectedFile
  }
}
