import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
  id: root
  property string selectionId: ""
  property var    model

  ColumnLayout {
    anchors.fill: parent
    spacing: 8

    Label { text: selectionId ? "Élément sélectionné" : "Aucune sélection"; font.bold: true }
    Text { text: selectionId; elide: Text.ElideRight; wrapMode: Text.Wrap }

    // Mini table (id, kind, label, x, y)
    ListView {
      id: details
      Layout.fillWidth: true
      Layout.fillHeight: true
      model: model
      delegate: Item {
        // au lieu de 'visible: id === root.selectionId'
        visible: model.id === root.selectionId
        width: ListView.view.width
        height: implicitHeight

        Column {
          spacing: 4
          // Accès explicite aux rôles via 'model.*' pour éviter toute collision
          Text { text: "Type: " + (model.kind || "") }
          Text { text: "Libellé: " + (model.label || "") }
          Text { text: "Position: (" + Math.round(model.x || 0) + ", " + Math.round(model.y || 0) + ")" }
          // (ajoute d'autres champs si besoin, ex: état)
          // Text { text: "État: " + (model.state || "normal") }
        }
      }
    }
  }
}
