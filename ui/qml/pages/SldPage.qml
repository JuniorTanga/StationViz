import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import StationViz
import "../components"
import "../styles"

Page {
  id: page
  title: "Schéma unifilaire"

  header: SldToolbar {
    id: toolbar
    onOpenClicked: openDialog.open()
    onFitClicked: sld.fitToContent()
    onZoomIn: sld.zoom *= 1.1
    onZoomOut: sld.zoom /= 1.1
  }
  Theme{
    id: theme
  }
  background: Rectangle { color: theme.window }

  ColumnLayout {
    anchors.fill: parent
    spacing: 0

    RowLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 0

      // --- conteneur pile pour le graphe + overlay labels ---
      Item {
        id: sldStack
        Layout.fillWidth: true
        Layout.fillHeight: true

        SldView {
          id: sld
          anchors.fill: parent
          nodes: App.nodes
          edges: App.edges
          iconsEnabled: true

          edgeColor: theme._L_edge
          nodeColor: theme._L_node
          selectionColor: theme._L_accent
          selectionId: App.uiStore.selectionId

          onNodeClicked: (nodeId) => App.uiStore.selectionId = nodeId

          Legend {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 12
            width: 200
          }

          MiniMap {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            nodeModel: App.nodes
            panX: sld.panX; panY: sld.panY; zoom: sld.zoom
            viewW: sld.width; viewH: sld.height
          }
        }

        // Overlay des libellés, ancré à sldStack (pas au RowLayout)
        SldLabelsOverlay {
          anchors.fill: parent
          sldView: sld
          nodeModel: App.nodes
          zoomThreshold: 0.8
        }
      }

      // panneau à droite
      PropertyPanel {
        Layout.preferredWidth: 340
        selectionId: App.uiStore.selectionId
        model: App.nodes
      }
    }

    // Barre de statut
    Rectangle {
      Layout.fillWidth: true
      height: 28
      color: theme._L_panel
      border.color: "#DDDDDD"
      Row {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 16
        Label { text: "Nœuds: " + App.nodes.count }
        Label { text: "Arêtes: " + App.edges.count }
        Label { text: "Zoom: " + sld.zoom.toFixed(2) }
        Label { text: "Fichier: " + (App.uiStore.currentFile || "—") }
      }
    }
  }

  Shortcut { sequence: "Ctrl+0"; onActivated: sld.fitToContent() }
  Shortcut { sequence: "+";     onActivated: sld.zoom *= 1.1 }
  Shortcut { sequence: "-";     onActivated: sld.zoom /= 1.1 }
  Shortcut { sequence: "F";     onActivated: sld.fitToContent() }

  Dialog {
    id: openDialog
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    title: "Ouvrir un SCL"
    contentItem: Column {
      spacing: 8
      TextField { id: path; placeholderText: "Chemin du fichier..." }
      Button { text: "Parcourir…"; onClicked: fileDialog.open() }
    }
    onAccepted: App.openSclFile(path.text)
  }

  FileDialog {
    id: fileDialog
    title: "Ouvrir un fichier SCL"
    nameFilters: ["SCL files (*.scd *.cid *.icd)", "All files (*)"]
    onAccepted: {
        App.openSclUrl(selectedFile)
        openDialog.close()
      }
  }
}

