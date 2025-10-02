import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import StationViz // module où AppContext est enregistré
import "qml/pages"
import "qml/components"
import "qml/styles"
import "./common"

ApplicationWindow {
  id: win
  width: 1280; height: 800; visible: true
  title: "StationViz — FAT IEC 61850"
  Theme { id: theme }
  color : theme.window

  menuBar: MenuBar {
    Menu {
      title: "Fichier"
      MenuItem { text: "Ouvrir…"; onTriggered: openSclDlg.open() }
      //MenuItem { text: "Ouvrir SCL…"; onTriggered: fileDialog.open() }
      MenuItem { text: "Effacer"; onTriggered: App.clear() }
      MenuSeparator {}
      MenuItem { text: "Quitter"; onTriggered: Qt.quit() }
    }
    Menu {
      title: "Vue"
      MenuItem { text: "Schéma unifilaire"; onTriggered: App.setViewMode("sld") }
      MenuItem { text: "IEDs"; onTriggered: App.setViewMode("ieds") }
      MenuItem { text: "Communication"; onTriggered: App.setViewMode("comms") }
      MenuSeparator {}
      MenuItem {
        text: theme.darkMode ? "Mode clair" : "Mode sombre"
        //onTriggered: theme.darkMode = !theme.darkMode
      }
    }


  }

  header: ToolBar {
    RowLayout {
      anchors.fill: parent; spacing: 8
      Label { text: App.uiStore.currentFile || "Aucun fichier" }
      Item { Layout.fillWidth: true }
      Button { text: "Ouvrir"; onClicked: fileDialog.open() }
    }
  }
/*
  StackLayout {
    id: stack
    anchors.fill: parent
    currentIndex: App.uiStore.viewMode === "sld" ? 0 : (App.uiStore.viewMode === "ieds" ? 1 : 2)

    // Page 0: SLD
    Loader { source: "qml/pages/SldPage.qml" }

    // Page 1: IEDs (placeholder)
    //Item { Label { anchors.centerIn: parent; text: "Vue IEDs (à venir)" } }
    Loader { source: "qml/pages/IedPage.qml" }

    // Page 2: Communication (placeholder)
    Item { Label { anchors.centerIn: parent; text: "Vue Communication (à venir)" } }
  }
*/

  ColumnLayout {
      anchors.fill: parent
      spacing: 0

      // 1) barre d’onglets (switch de vue)
      SegmentedTabs {
        id: tabs
        Layout.fillWidth: true
        currentId: App.uiStore.viewMode
        tabs: [
          { id: "ieds", label: "IED"},
          { id: "sld",  label: "SUBSTATION"},
          { id: "comm", label: "COMMUNICATION"},
          { id: "tests",label: "TESTS"} // engrenage simple
        ]
        onTabClicked: (id) => App.uiStore.viewMode = id
      }

      // 2) contenu des pages
      StackLayout {
        id: stack
        Layout.fillWidth: true
        Layout.fillHeight: true

        // mapping viewMode -> index
        currentIndex: {
          switch (App.uiStore.viewMode) {
          case "ieds":  return 0;
          case "sld":   return 1;
          case "comm":  return 2;
          case "tests": return 3;
          default:      return 1;
          }
        }

        // IEDs par travée (nouvelle page que tu viens d’ajouter)
        IedPage { }

        // Unifilaire
        SldPage { }

        // Communication (placeholder pour l’instant)
        Page {
          Label { anchors.centerIn: parent; text: "Communication — à venir"; }
        }

        // Tests FAT (placeholder)
        Page {
          Label { anchors.centerIn: parent; text: "Tests — à venir"; }
        }
      }
    }

  FileDialog {
    id: fileDialog
    title: "Ouvrir un fichier SCL"
    nameFilters: ["SCL files (*.scd *.cid *.icd *.ssd)", "All files (*)"]
    onAccepted: App.openSclFile(selectedFile)
  }

  // Toast global
  Toast { id: toast }

  // Dialog d’ouverture SCL
  SclOpenDialog {
    id: openSclDlg
    visible: !App.hasScl && !App.busy  // s’affiche au démarrage si aucun fichier
    onRequestOpenUrl: function(u) {
      busy = true
      App.loadSclAsync(u)
    }
  }

  // Réactions aux signaux C++
  Connections {
    target: App
    function onBusyChanged() {
      openSclDlg.busy = App.busy
    }
    function onFileLoaded(ok) {
      openSclDlg.busy = false
      if (ok) {
        openSclDlg.close()
        toast.flash("Fichier SCL chargé avec succès ✓", true)
      } else {
        toast.flash("Échec du chargement SCL", false)
      }
    }
  }
}
