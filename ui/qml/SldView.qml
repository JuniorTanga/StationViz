import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import "Theme.js" as Theme
import "IconResolver.js" as IconResolver

Item {
    id: root
    property var nodeModel
    property var edgeModel
    property real zoom: 1.0
    property real panX: 0
    property real panY: 0
    signal nodeClicked(string id)

    Rectangle { anchors.fill: parent; color: Theme.surface }

    // edges
    Repeater {
        model: edgeModel
        delegate: Shape {
            x: root.panX; y: root.panY; scale: root.zoom
            ShapePath {
                strokeWidth: 2
                strokeColor: Theme.onSurface
                fillColor: "transparent"
                readonly property var pts: points
                startX: pts.length>0 ? pts[0].x : 0
                startY: pts.length>0 ? pts[0].y : 0
                PathPolyline { path: parent; start: pts }
            }
        }
    }

    // nodes
    Repeater {
        model: nodeModel
        delegate: Item {
            x: root.panX + x*root.zoom; y: root.panY + y*root.zoom
            width: Math.max(24, w)*root.zoom; height: Math.max(8,h)*root.zoom

            Image {
                anchors.centerIn: parent
                source: IconResolver.forNode(kind, undefined)
                sourceSize.width: parent.width
                sourceSize.height: parent.height
                fillMode: Image.PreserveAspectFit
            }

            Text {
                anchors.top: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                text: label; color: Theme.muted; font.pixelSize: 11
            }

            MouseArea {
                anchors.fill: parent; onClicked: root.nodeClicked(id)
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    // pan/zoom interactions
    MouseArea {
        anchors.fill: parent
        onWheel: (w)=>{ root.zoom *= (w.angleDelta.y>0?1.1:0.9); }
        drag.target: null
        onPressed: { lastX = mouse.x; lastY = mouse.y }
        onPositionChanged: {
            if (mouse.buttons & Qt.RightButton) {
                root.panX += (mouse.x - lastX)
                root.panY += (mouse.y - lastY)
                lastX = mouse.x; lastY = mouse.y
            }
        }
        property real lastX: 0
        property real lastY: 0
    }
}
