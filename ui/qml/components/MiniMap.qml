import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

Item {
  id: root
  property var nodeModel   // NodeModel*
  property real panX
  property real panY
  property real zoom
  property real viewW
  property real viewH

  width: 220; height: 150

  Theme{
    id: theme
  }

  // calc bbox
  function bbox() {
    var minX=1e12, minY=1e12, maxX=-1e12, maxY=-1e12
    for (var i=0;i<nodeModel.rowCount();++i) {
      var n = nodeModel.get(i) // on ajoute un getter utilitaire côté C++ si besoin ; sinon expose items[]
      if (!n) continue
      var x = n.x, y = n.y
      if (x<minX) minX=x; if (y<minY) minY=y
      if (x>maxX) maxX=x; if (y>maxY) maxY=y
    }
    if (minX>maxX) return {minX:0, minY:0, maxX:1, maxY:1}
    return {minX:minX, minY:minY, maxX:maxX, maxY:maxY}
  }

  Canvas {
    id: canvas
    anchors.fill: parent
    onPaint: {
      var ctx = getContext("2d")
      ctx.resetTransform()
      ctx.fillStyle = theme.panel; ctx.fillRect(0,0,width,height)
      ctx.strokeStyle = "#DDDDDD"; ctx.strokeRect(0,0,width,height)

      if (!root.nodeModel) return
      var b = root.bbox()
      var bw = b.maxX-b.minX, bh = b.maxY-b.minY
      if (bw<=0 || bh<=0) return
      var sx = width / bw, sy = height / bh, s = Math.min(sx, sy)

      // nodes
      ctx.save()
      ctx.translate(-b.minX*s, -b.minY*s)
      ctx.scale(s, s)
      ctx.fillStyle = theme.edge
      for (var i=0;i<nodeModel.rowCount();++i) {
        var n = nodeModel.get(i)
        if (!n) continue
        ctx.fillRect(n.x-1, n.y-1, 2, 2)
      }

      // viewport (pan/zoom)
      var vx = (-root.panX) / root.zoom
      var vy = (-root.panY) / root.zoom
      var vw = root.viewW / root.zoom
      var vh = root.viewH / root.zoom

      ctx.strokeStyle = theme.accent
      ctx.lineWidth = 1/s
      ctx.strokeRect(vx, vy, vw, vh)

      ctx.restore()
    }
  }
}
