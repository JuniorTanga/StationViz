// =============================================================
// StationViz / frontend/ — Demo Qt Quick pour affichage SLD
// Monofolder (frontend/) + sous-dossier qml/
// Dépend de sclLib + sldLib
// Qt6 (Quick)
// =============================================================
// CONTENU
//   frontend/CMakeLists.txt
//   frontend/SldFacade.h
//   frontend/SldFacade.cpp
//   frontend/main.cpp
//   frontend/qml/Main.qml
// =============================================================

// =============================
// File: frontend/CMakeLists.txt
// =============================
cmake_minimum_required(VERSION 3.20)
project(stationviz_frontend LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 COMPONENTS Quick REQUIRED)

add_executable(stationviz_demo
    main.cpp
    SldFacade.cpp
    SldFacade.h
)

target_link_libraries(stationviz_demo PRIVATE
    Qt6::Quick
    sldLib
    sclLib
)

# Inclure ce dossier et hériter des include dirs exportés par sclLib/sldLib
target_include_directories(stationviz_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(stationviz_demo PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# Copier le dossier QML au runtime
add_custom_command(TARGET stationviz_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/qml
            $<TARGET_FILE_DIR:stationviz_demo>/qml)

// =============================
// File: frontend/SldFacade.h
// =============================
#pragma once
#include <QObject>
#include <QString>
#include <memory>

#include "SclManager.h"  // sclLib
#include "SldManager.h"  // sldLib

class SldFacade : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
public:
    explicit SldFacade(const QString& scdPath, QObject* parent=nullptr);

    Q_INVOKABLE QString planJson() const { return m_planJson; }
    Q_INVOKABLE QString condensedJson() const { return m_condensedJson; }
    Q_INVOKABLE QString error() const { return m_error; }
    Q_INVOKABLE bool ready() const { return m_ready; }

signals:
    void changed();

private:
    void build(const QString& scdPath);

    QString m_planJson;
    QString m_condensedJson;
    QString m_error;
    bool m_ready {false};

    std::unique_ptr<scl::SclManager> m_scl;
    std::unique_ptr<sld::SldManager> m_sld;
};

// =============================
// File: frontend/SldFacade.cpp
// =============================
#include "SldFacade.h"
#include <QDebug>

SldFacade::SldFacade(const QString& scdPath, QObject* parent)
    : QObject(parent)
{
    build(scdPath);
}

void SldFacade::build(const QString& scdPath)
{
    m_error.clear(); m_ready = false; emit changed();

    m_scl = std::make_unique<scl::SclManager>();
    auto st = m_scl->loadScl(scdPath.toStdString());
    if (!st) {
        m_error = QString::fromStdString(st.error().message);
        emit changed();
        return;
    }

    sld::HeuristicsConfig cfg; // valeurs par défaut OK pour la démo
    m_sld = std::make_unique<sld::SldManager>(m_scl->model(), cfg);
    st = m_sld->build();
    if (!st) {
        m_error = QString::fromStdString(st.error().message);
        emit changed();
        return;
    }

    m_planJson      = QString::fromStdString(m_sld->planJson());
    m_condensedJson = QString::fromStdString(m_sld->condensedJson());
    m_ready = true;
    emit changed();
}

// =============================
// File: frontend/main.cpp
// =============================
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "SldFacade.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // *** DÉFINIR ICI VOTRE CHEMIN SCD ***
    // REMPLACEZ par un chemin absolu valide vers votre fichier .scd/.scl/.icd.
    const QString kSclPath = QString::fromUtf8("/ABSOLUTE/PATH/TO/poste.scd");

    SldFacade backend(kSclPath);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl::fromLocalFile(QStringLiteral("qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return -1;

    return app.exec();
}

// =============================
// File: frontend/qml/Main.qml
// =============================
import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: win
    width: 1200
    height: 800
    visible: true
    color: "white"
    title: "StationViz — Démo SLD"

    property var plan: ({})
    property var graph: ({})         // condensed graph
    property var nodesById: ({})
    property var positions: ({})     // id -> {x,y,w,h,label,eKind,kind}
    property var edges: []           // [{x1,y1,x2,y2}]

    // Layout params (simple & visibles)
    property int margin: 40
    property int vlGap: 220
    property int busWidth: 160
    property int busHeight: 18
    property int busGapX: 220
    property int laneGapX: 120
    property int rowGapY: 80
    property int equipWidth: 90
    property int equipHeight: 28

    function colorForEquip(kind) {
        switch (kind) {
        case "CB": return "#1976d2";
        case "DS": return "#455a64";
        case "ES": return "#6d4c41";
        case "CT": return "#00897b";
        case "VT": return "#26a69a";
        case "PT": return "#26a69a";
        case "Transformer": return "#f57c00";
        case "Line": return "#2e7d32";
        case "Cable": return "#558b2f";
        default: return "#607d8b";
        }
    }

    function rebuild() {
        if (!backend.ready()) {
            console.log("backend not ready: ", backend.error());
            return;
        }
        try {
            plan = JSON.parse(backend.planJson());
            graph = JSON.parse(backend.condensedJson());
        } catch(e) {
            console.log("JSON parse error", e);
            return;
        }
        // Build nodesById
        nodesById = {};
        for (var i=0; i<graph.nodes.length; ++i) {
            var n = graph.nodes[i];
            nodesById[n.id] = n;
        }
        // Compute layout per VL groups from buses
        positions = {}; edges = [];
        var vlGroups = {};
        for (var b=0; b<plan.buses.length; ++b) {
            var bus = plan.buses[b];
            var key = bus.ss + ":" + bus.vl;
            if (!vlGroups[key]) vlGroups[key] = [];
            vlGroups[key].push(bus);
        }
        var vlKeys = Object.keys(vlGroups);
        for (var vi=0; vi<vlKeys.length; ++vi) {
            var key = vlKeys[vi];
            var buses = vlGroups[key];
            var baseY = margin + vi*vlGap;
            for (var bi=0; bi<buses.length; ++bi) {
                var bus = buses[bi];
                var bx = margin + bi*busGapX;
                var by = baseY;
                positions[bus.busNodeId] = { x: bx, y: by, w: busWidth, h: busHeight, label: bus.label, kind: "Bus" };
            }
        }
        // Place couplers (entre 2 bus) : au milieu
        for (var c=0; c<plan.couplers.length; ++c) {
            var cp = plan.couplers[c];
            var a = positions[cp.busA]; var b = positions[cp.busB];
            if (!a || !b) continue;
            var x = Math.min(a.x, b.x) + Math.abs(a.x-b.x)/2 - equipWidth/2;
            var y = Math.min(a.y, b.y) + busHeight + 12;
            var n = nodesById[cp.equip];
            var eKind = n && n.eKind ? n.eKind : (cp.type==="CB"?"CB":"DS");
            positions[cp.equip] = { x:x, y:y, w:equipWidth, h:equipHeight, label:(n?n.label:cp.equip), eKind:eKind, kind:"Equipment" };
            // edges to both buses
            edges.push({ x1:x+equipWidth/2, y1:y, x2:a.x+busWidth/2, y2:a.y+busHeight });
            edges.push({ x1:x+equipWidth/2, y1:y, x2:b.x+busWidth/2, y2:b.y+busHeight });
        }
        // Place feeders sous chaque bus par laneIndex
        for (var fi=0; fi<plan.feeders.length; ++fi) {
            var f = plan.feeders[fi];
            var busPos = positions[f.busId]; if (!busPos) continue;
            for (var si=0; si<f.chain.length; ++si) {
                var nid = f.chain[si];
                var nx = busPos.x + f.lane*laneGapX;
                var ny = busPos.y + busHeight + 40 + si*rowGapY;
                var nfo = nodesById[nid] || { label:nid, eKind:"Unknown" };
                positions[nid] = { x:nx, y:ny, w:equipWidth, h:equipHeight, label:nfo.label, eKind:nfo.eKind||"Unknown", kind:"Equipment" };
                // wire: from previous or bus
                if (si === 0) {
                    edges.push({ x1:nx+equipWidth/2, y1:ny, x2:busPos.x+busWidth/2, y2:busPos.y+busHeight });
                } else {
                    var prev = positions[f.chain[si-1]];
                    if (prev) edges.push({ x1:nx+equipWidth/2, y1:ny, x2:prev.x+equipWidth/2, y2:prev.y+equipHeight });
                }
            }
        }
        canvas.requestPaint();
        nodeRepeater.model = Object.keys(positions);
    }

    Component.onCompleted: rebuild()

    // Toolbar minimal (info + refresh)
    header: ToolBar {
        Row {
            spacing: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            Label { text: backend.ready() ? "OK" : ("Erreur: "+backend.error()); color: backend.ready()?"#2e7d32":"#c62828" }
            Button { text: "Rafraîchir"; onClicked: rebuild() }
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0,0,width,height);
            ctx.strokeStyle = "#444";
            ctx.lineWidth = 2;
            for (var i=0; i<edges.length; ++i) {
                var e = edges[i];
                ctx.beginPath();
                ctx.moveTo(e.x1, e.y1);
                ctx.lineTo(e.x2, e.y2);
                ctx.stroke();
            }
        }
    }

    Repeater {
        id: nodeRepeater
        model: []
        delegate: Item {
            readonly property var n: positions[modelData]
            x: n ? n.x : 0
            y: n ? n.y : 0
            width: n ? n.w : 0
            height: n ? n.h : 0
            Rectangle {
                anchors.fill: parent
                radius: n && n.kind==="Bus" ? 2 : 4
                color: n && n.kind==="Bus" ? "#212121" : colorForEquip(n.eKind)
                border.color: "#111"
            }
            Text {
                anchors.centerIn: parent
                text: n ? n.label : "?"
                color: n && n.kind==="Bus" ? "#fafafa" : "white"
                font.pointSize: 9
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: parent.width - 6
                wrapMode: Text.NoWrap
            }
        }
    }
}
