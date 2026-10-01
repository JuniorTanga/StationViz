import QtQuick
import QtQuick.Controls
import "../styles"

Item {
  id: root
  // API
  property var sldView       // SldView (obligatoire)
  property var nodeModel     // NodeModel (obligatoire)
  property real zoomThreshold: 0.5
  property bool showIdsWhenNoLabel: true


  Theme { id: theme }

  anchors.fill: parent
  visible: sldView && nodeModel && sldView.zoom >= zoomThreshold
  clip: true
  z: 1000
  enabled: false

  function viewX(wx) { return (sldView ? (sldView.panX + sldView.zoom * wx) : wx) }
  function viewY(wy) { return (sldView ? (sldView.panY + sldView.zoom * wy) : wy) }

  Repeater {
    id: rep
    // IMPORTANT: utiliser la propriété réactive 'count'
    model: nodeModel ? nodeModel.count : 0

    delegate: Item {
      property var n: nodeModel.get(index)
      property bool visibleKind: n && (n.kind !== "Junction" && n.kind !== "ConnectivityNode")
      visible: root.visible && visibleKind

      x: root.viewX(n ? n.x : 0) + 10
      y: root.viewY(n ? n.y : 0) - 12
      width: label.implicitWidth + 8
      height: label.implicitHeight + 4

      Rectangle {
        anchors.fill: parent
        radius: 3
        color: "#80FFFFFF"      // light
        border.color: "#DDDDDD"
      }
      Text {
        id: label
        anchors.centerIn: parent
        text: {
          if (!n) return ""
          const t = (n.label && n.label.length) ? n.label : (root.showIdsWhenNoLabel ? n.id : "")
          return t
        }
        color: theme.text
        font.pixelSize: 12
        elide: Text.ElideRight
      }
    }
  }
}
