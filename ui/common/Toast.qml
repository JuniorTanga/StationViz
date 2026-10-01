import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../qml/styles"

Popup {
  id: toast
  property alias text: label.text
  property color bg: "#10B981"      // vert
  property color fg: "#ffffff"
  modal: false
  focus: false
  x: parent ? parent.width - width - 16 : 16
  y: 16
  enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
  exit:  Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 } }

  background: Rectangle { color: toast.bg; radius: 8; opacity: 0.96 }
  contentItem: RowLayout {
    spacing: 8
    //padding: 10
    Label { text: "✓"; color: toast.fg; font.bold: true }
    Label { id: label; color: toast.fg }
  }

  function flash(msg, ok=true) {
    text = msg
    bg = ok ? "#10B981" : "#EF4444"
    open()
    timer.restart()
  }
  Timer { id: timer; interval: 2400; running: false; repeat: false; onTriggered: toast.close() }
}
