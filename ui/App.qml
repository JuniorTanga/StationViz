import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import StationViz           // AppContext / App (C++)
import "qml/pages"
import "qml/components"
import "qml/styles"
import "./common"

ApplicationWindow {
  id: win
  width: 1280
  height: 800
  visible: true
  title: "StationViz — FAT IEC 61850"

  Theme { id: theme }
  color: theme.window
  font.pixelSize: 16

  // ─────────────────────────── MENUS ───────────────────────────
  menuBar: MenuBar {
    Menu {
      title: "Fichier"
      MenuItem { text: "Ouvrir…"; onTriggered: openSclDlg.open() }
      MenuItem { text: "Effacer"; onTriggered: App.clear() }
      MenuSeparator {}
      MenuItem { text: "Quitter"; onTriggered: Qt.quit() }
    }
    Menu {
      title: "Vue"
      MenuItem { text: "Schéma unifilaire"; onTriggered: App.setViewMode("sld") }
      MenuItem { text: "IEDs";               onTriggered: App.setViewMode("ieds") }
      MenuItem { text: "Inventaire";         onTriggered: App.setViewMode("inventory") }
      MenuItem { text: "Communication";      onTriggered: App.setViewMode("comm") }
      MenuSeparator {}
      MenuItem {
        text: theme.darkMode ? "Mode clair" : "Mode sombre"
        // onTriggered: theme.darkMode = !theme.darkMode
      }
    }
  }

  // ───────────────────────── entête ────────────────────────────
  header: ToolBar {
    RowLayout {
      anchors.fill: parent
      spacing: 8
      Label { text: App.uiStore.currentFile || "Aucun fichier" }
      Item { Layout.fillWidth: true }
      Button { text: "Ouvrir"; onClicked: fileDialog.open() }
    }
  }

  // ───────────────────────── contenu ───────────────────────────
  ColumnLayout {
    anchors.fill: parent
    spacing: 0

    SegmentedTabs {
      id: tabs
      Layout.fillWidth: true
      currentId: App.uiStore.viewMode
      tabs: [
        { id: "ieds",      label: "IED" },
        { id: "sld",       label: "SUBSTATION" },
        { id: "inventory", label: "INVENTORY" },
        { id: "comm",      label: "COMMUNICATION" },
        { id: "tests",     label: "TESTS" }
      ]
      onTabClicked: (id) => App.uiStore.viewMode = id
    }

    StackLayout {
      id: stack
      Layout.fillWidth: true
      Layout.fillHeight: true

      // mapping id -> index
      currentIndex: {
        switch (App.uiStore.viewMode) {
        case "ieds":      return 0;
        case "sld":       return 1;
        case "inventory": return 2;
        case "comm":      return 3;
        case "tests":     return 4;
        default:          return 1;
        }
      }

      // 0 — Vue IED
      IedPage { }

      // 1 — Schéma unifilaire
      SldPage { }

      // 2 — Inventaire d’équipements (SS → VL → Bay)
      InventoryPage { }

      // 3 — Communication (placeholder)
      Page { Label { anchors.centerIn: parent; text: "Communication — à venir" } }

      // 4 — Tests FAT (placeholder)
      Page { Label { anchors.centerIn: parent; text: "Tests — à venir" } }
    }
  }

  // ───────────────────── boîtes d’ouverture ────────────────────
  FileDialog {
    id: fileDialog
    title: "Ouvrir un fichier SCL"
    nameFilters: [ "SCL files (*.scd *.cid *.icd *.ssd)", "All files (*)" ]
    onAccepted: App.openSclFile(selectedFile)
  }

  // Toast global
  Toast { id: toast }

  // Dialogue d’ouverture (drag&drop / bouton)
  SclOpenDialog {
    id: openSclDlg
    visible: !App.hasScl && !App.busy
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
