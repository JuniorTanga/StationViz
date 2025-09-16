// ========================= Main.qml =========================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: win
    visible: true
    width: 1200
    height: 800
    title: "StationViz — SLD Viewer (vertical VL + scroll)"

    // fournie via main.cpp
    property string sclPath: initialSclPath || ""

    // modèles non nuls
    property var planObj:  ({ buses: [], feeders: [], couplers: [], transformers: [] })
    property var graphObj: ({ nodes: [], edges: [] })

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        //padding: 8

        // --------- barre de contrôle
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: pathField
                text: win.sclPath
                placeholderText: "Chemin du fichier .scd/.scl/.icd"
                Layout.fillWidth: true
                onAccepted: btnLoad.clicked()
            }

            Button {
                id: btnLoad
                text: "Charger SCL"
                onClicked: {
                    const err = sldFacade.loadScl(pathField.text)
                    if (err.length) {
                        console.error("SCL error:", err)
                        statusLabel.text = "Erreur: " + err
                        return
                    }
                    statusLabel.text = "SCL chargé"
                }
            }

            Button {
                text: "Construire SLD"
                onClicked: {
                    const err = sldFacade.buildSld()
                    if (err.length) {
                        console.error("SLD error:", err)
                        statusLabel.text = "Erreur: " + err
                        return
                    }
                    function safeParse(txt, tag) {
                        if (!txt || typeof txt !== "string" || txt.length === 0) {
                            console.error(tag, "vide"); return null
                        }
                        try {
                            const o = JSON.parse(txt)
                            if (!o || typeof o !== "object") {
                                console.error(tag, "non-objet:", txt.slice(0,160))
                                return null
                            }
                            return o
                        } catch (e) {
                            console.error("Parse", tag, "failed:", e, txt.slice(0,200))
                            return null
                        }
                    }
                    const p = safeParse(sldFacade.planJson(), "planJson")
                    const g = safeParse(sldFacade.condensedJson(), "condensedJson")
                    if (!p || !Array.isArray(p.buses) || !Array.isArray(p.feeders)) {
                        statusLabel.text = "Plan invalide"; return
                    }
                    if (!g || !Array.isArray(g.nodes) || !Array.isArray(g.edges)) {
                        statusLabel.text = "Graphe invalide"; return
                    }
                    planObj = p; graphObj = g
                    statusLabel.text = "SLD prêt"
                    canvas.plan = planObj
                    canvas.graph = graphObj
                    canvas.requestPaint()
                }
            }

            Button {
                text: "Reset"
                onClicked: {
                    sldFacade.reset()
                    statusLabel.text = "Réinitialisé"
                    planObj  = ({ buses: [], feeders: [], couplers: [], transformers: [] })
                    graphObj = ({ nodes: [], edges: [] })
                    canvas.plan  = planObj
                    canvas.graph = graphObj
                    canvas.requestPaint()
                }
            }

            Label {
                id: statusLabel
                text: sldFacade.ready ? "SLD prêt" : "Prêt à charger"
            }
        }

        // --------- zone d'affichage : scroll molette (Flickable)
        Flickable {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth:  canvas.implicitWidth
            contentHeight: canvas.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            interactive: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

            WheelHandler {
                target: scroller
                orientation: Qt.Vertical
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (ev)=> {
                    scroller.contentY = Math.max(0, Math.min(scroller.contentHeight - scroller.height,
                                             scroller.contentY - ev.angleDelta.y))
                    ev.accepted = true
                }
            }

            Canvas {
                id: canvas

                // tailles implicites => activent le scroll
                property int contentWidth:  1200
                property int contentHeight: 1200
                implicitWidth:  Math.max(1200, contentWidth)
                implicitHeight: Math.max(800,  contentHeight)

                // modèles non nuls
                property var plan:  ({ buses: [], feeders: [], couplers: [], transformers: [] })
                property var graph: ({ nodes: [], edges: [] })

                // ---------- helpers
                function labelFor(id) {
                    for (var i=0; i<graph.nodes.length; ++i)
                        if (graph.nodes[i].id === id)
                            return graph.nodes[i].label || id
                    return id
                }
                function kindFor(id) {
                    for (var i=0; i<graph.nodes.length; ++i)
                        if (graph.nodes[i].id === id)
                            return (graph.nodes[i].eKind || "Unknown")
                    return "Unknown"
                }
                function colorFor(kind) {
                    switch(kind) {
                    case "CB": return "#d9534f";              // rouge
                    case "DS": return "#f0ad4e";              // orange
                    case "CT": return "#5bc0de";              // cyan
                    case "VT": return "#0275d8";              // bleu
                    case "Transformer": return "#28a745";     // vert
                    case "Line": return "#6f42c1";            // violet
                    default: return "#444";
                    }
                }
                function ellipsize(ctx, text, maxW) {
                    var t = text || ""
                    if (ctx.measureText(t).width <= maxW) return t
                    var ell = "…", lo=0, hi=t.length
                    while (lo < hi) {
                        var mid = ((lo+hi)/2)|0
                        var s = t.slice(0, mid) + ell
                        if (ctx.measureText(s).width <= maxW) lo = mid+1
                        else hi = mid
                    }
                    return t.slice(0, Math.max(0, lo-1)) + ell
                }
                function drawSymbol(ctx, kind, cx, cy, size) {
                    var s = size|0; if (s<8) s=8
                    ctx.save()
                    ctx.fillStyle = colorFor(kind)
                    ctx.strokeStyle = "#111"; ctx.lineWidth = 1
                    switch(kind) {
                    case "CB": // carré
                        ctx.fillRect(cx - s/2, cy - s/2, s, s)
                        ctx.strokeRect(cx - s/2, cy - s/2, s, s); break
                    case "DS": // losange
                        ctx.beginPath()
                        ctx.moveTo(cx, cy - s/2); ctx.lineTo(cx + s/2, cy)
                        ctx.lineTo(cx, cy + s/2); ctx.lineTo(cx - s/2, cy)
                        ctx.closePath(); ctx.fill(); ctx.stroke(); break
                    case "CT": // triangle
                        ctx.beginPath()
                        ctx.moveTo(cx, cy - s/2); ctx.lineTo(cx + s/2, cy + s/2)
                        ctx.lineTo(cx - s/2, cy + s/2); ctx.closePath()
                        ctx.fill(); ctx.stroke(); break
                    case "VT": // cercle
                        ctx.beginPath(); ctx.arc(cx, cy, s/2, 0, Math.PI*2)
                        ctx.closePath(); ctx.fill(); ctx.stroke(); break
                    case "Transformer": // rect arrondi
                        var r=4, x=cx-s/2, y=cy-s/2, w=s, h=s
                        ctx.beginPath()
                        ctx.moveTo(x+r,y); ctx.arcTo(x+w,y, x+w,y+h, r)
                        ctx.arcTo(x+w,y+h, x,y+h, r); ctx.arcTo(x,y+h, x,y, r)
                        ctx.arcTo(x,y, x+w,y, r); ctx.closePath()
                        ctx.fill(); ctx.stroke(); break
                    case "Line": // point
                        ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI*2)
                        ctx.closePath(); ctx.fill(); break
                    default:
                        ctx.fillStyle = "#555"
                        ctx.fillRect(cx - s/2, cy - s/2, s, s)
                        ctx.strokeRect(cx - s/2, cy - s/2, s, s); break
                    }
                    ctx.restore()
                }

                // --------- mise à jour taille contenu
                function recomputeContentSize(vlBlocks) {
                    var totalH = 40
                    for (var b=0; b<vlBlocks.length; ++b)
                        totalH += vlBlocks[b].height + 40
                    contentHeight = Math.max(800, totalH)

                    var maxW = 0
                    for (b=0; b<vlBlocks.length; ++b)
                        if (vlBlocks[b].width > maxW) maxW = vlBlocks[b].width
                    contentWidth = Math.max(1200, maxW + 80)

                    // notifier le Flickable
                    canvas.implicitWidth  = Math.max(1200, contentWidth)
                    canvas.implicitHeight = Math.max(800,  contentHeight)
                }

                onPlanChanged: requestPaint()
                onGraphChanged: requestPaint()

                // ================== Rendu ==================
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.fillStyle = "white"
                    ctx.fillRect(0,0,width,height)

                    if (!plan || !plan.buses || plan.buses.length === 0) {
                        ctx.fillStyle = "#777"
                        ctx.font = "16px sans-serif"
                        ctx.fillText("Charge un SCL puis Construis le SLD", 30, 40)
                        return
                    }

                    // --- constantes UI
                    var VL_MARGIN_X = 40, VL_MARGIN_Y = 40, VL_TITLE_H = 22
                    var BUS_W = 260, BUS_H = 42, BUS_SPACING = 60
                    var GROUP_PAD_X = 24, GROUP_TOP = 10, GROUP_MIN_H = 120
                    var COUPLER_Y_OFF = 4, TAP_DROP = 8
                    var CHAIN_BOX = { w: 18, h: 18 }, SEG_GAP_Y = 10
                    var LANE_BASE = 26

                    // --- bucket VL -> bus[]
                    var vlToBuses = {}
                    for (var i=0; i<plan.buses.length; ++i) {
                        var bus = plan.buses[i]
                        if (!vlToBuses[bus.vl]) vlToBuses[bus.vl] = []
                        vlToBuses[bus.vl].push(bus)
                    }
                    var vlList = []
                    for (var k in vlToBuses) if (vlToBuses.hasOwnProperty(k)) vlList.push(k)
                    vlList.sort()

                    // feeders par bus
                    var feedersByBus = {}
                    for (i=0; i<plan.feeders.length; ++i) {
                        var f = plan.feeders[i]
                        if (!feedersByBus[f.bus]) feedersByBus[f.bus] = []
                        feedersByBus[f.bus].push(f)
                    }
                    // couplers par VL
                    var couplersByVl = {}
                    for (i=0; i<(plan.couplers||[]).length; ++i) {
                        var c = plan.couplers[i]
                        var keyVl = (c.vl || "")
                        if (!couplersByVl[keyVl]) couplersByVl[keyVl] = []
                        couplersByVl[keyVl].push(c)
                    }

                    // pré-mesure
                    ctx.font = "10px sans-serif"
                    var busLanePitch = {}, busMaxChain = {}
                    for (i=0; i<plan.buses.length; ++i) {
                        var b = plan.buses[i]
                        var list = (feedersByBus[b.id] || []).slice()
                        var maxW = 0, maxChain = 1
                        for (var t=0; t<list.length; ++t) {
                            var ff = list[t]
                            if (ff.chain && ff.chain.length) {
                                if (ff.chain.length > maxChain) maxChain = ff.chain.length
                                for (var j=0; j<ff.chain.length; ++j) {
                                    var ww = ctx.measureText(labelFor(ff.chain[j]) || "").width
                                    if (ww > maxW) maxW = ww
                                }
                            }
                            if (ff.endpoint && ff.endpoint !== "Unknown") {
                                var we = ctx.measureText(ff.endpoint).width
                                if (we > maxW) maxW = we
                            }
                        }
                        busLanePitch[b.id] = Math.max(LANE_BASE, 18 + 14 + Math.ceil(maxW))
                        busMaxChain[b.id] = maxChain
                    }

                    // layout vertical
                    var vlBlocks = []
                    var curY = VL_MARGIN_Y
                    var allBusPos = {}, allLanePitch = {}

                    for (var v=0; v<vlList.length; ++v) {
                        var vl = vlList[v]
                        var buses = vlToBuses[vl]
                        var blockW = VL_MARGIN_X + buses.length * (BUS_W + BUS_SPACING) - BUS_SPACING + VL_MARGIN_X
                        var blockH = VL_TITLE_H + GROUP_TOP + Math.max(GROUP_MIN_H, 160)

                        var x = VL_MARGIN_X
                        var busPos = {}
                        for (i=0; i<buses.length; ++i) {
                            var bb = buses[i]
                            busPos[bb.id] = { x: x, y: curY + VL_TITLE_H + 8 }
                            allBusPos[bb.id] = busPos[bb.id]
                            allLanePitch[bb.id] = busLanePitch[bb.id]
                            x += BUS_W + BUS_SPACING
                            var estH = (CHAIN_BOX.h + SEG_GAP_Y) * busMaxChain[bb.id] + 60
                            if (VL_TITLE_H + GROUP_TOP + estH > blockH)
                                blockH = VL_TITLE_H + GROUP_TOP + estH
                        }

                        // titre VL
                        ctx.fillStyle = "#444"
                        ctx.font = "bold 16px sans-serif"
                        ctx.fillText(vl, VL_MARGIN_X, curY)

                        // fond bloc VL
                        ctx.fillStyle = "#f7f9fc"
                        ctx.fillRect(VL_MARGIN_X - 12, curY + VL_TITLE_H,
                                     blockW - (VL_MARGIN_X - 12)*2, blockH - VL_TITLE_H + 8)
                        ctx.strokeStyle = "#e0e6ef"; ctx.lineWidth = 1
                        ctx.strokeRect(VL_MARGIN_X - 12, curY + VL_TITLE_H,
                                       blockW - (VL_MARGIN_X - 12)*2, blockH - VL_TITLE_H + 8)

                        // bus
                        for (i=0; i<buses.length; ++i) {
                            var b2 = buses[i]; var bp = busPos[b2.id]
                            ctx.fillStyle = "#0a84ff"
                            ctx.fillRect(bp.x, bp.y, BUS_W, BUS_H)
                            ctx.fillStyle = "white"; ctx.font = "bold 12px sans-serif"
                            ctx.fillText(b2.label || (b2.vl + "-BUS"), bp.x + 10, bp.y + 26)
                        }

                        vlBlocks.push({ vl: vl, x: VL_MARGIN_X, y: curY, width: blockW, height: blockH, busPos: busPos })
                        curY += blockH + VL_MARGIN_Y
                    }

                    // taille canevas
                    recomputeContentSize(vlBlocks)

                    // feeders
                    ctx.font = "10px sans-serif"
                    for (var vb=0; vb<vlBlocks.length; ++vb) {
                        var blk = vlBlocks[vb]
                        var busesVL = vlToBuses[blk.vl]
                        for (i=0; i<busesVL.length; ++i) {
                            var b3 = busesVL[i]
                            var bp2 = allBusPos[b3.id]
                            var list2 = (feedersByBus[b3.id] || []).slice()
                            list2.sort(function(a,b){ var la=(a.laneIndex|0), lb=(b.laneIndex|0); if (la!==lb) return la-lb; return a.id<b.id?-1:(a.id>b.id?1:0) })
                            var lanePitch = allLanePitch[b3.id]
                            var tapY = bp2.y + BUS_H + 4 + TAP_DROP
                            var chainStartY = tapY + 12
                            var MAX_LABEL_W = lanePitch - (CHAIN_BOX.w/2 + 6 + 4)

                            for (var m=0; m<list2.length; ++m) {
                                var f1 = list2[m]
                                var lane = f1.laneIndex || m
                                var bx = bp2.x + 24 + lane * lanePitch

                                // ↓ → ↓
                                ctx.strokeStyle = "#333"; ctx.lineWidth = 2
                                ctx.beginPath(); ctx.moveTo(bp2.x + BUS_W/2, bp2.y + BUS_H); ctx.lineTo(bp2.x + BUS_W/2, tapY); ctx.stroke()
                                ctx.beginPath(); ctx.moveTo(bp2.x + BUS_W/2, tapY); ctx.lineTo(bx, tapY); ctx.stroke()
                                ctx.beginPath(); ctx.moveTo(bx, tapY); ctx.lineTo(bx, chainStartY); ctx.stroke()

                                // chaîne
                                var cy = chainStartY
                                for (var j=0; j<f1.chain.length; ++j) {
                                    var idn = f1.chain[j], kind = kindFor(idn)
                                    drawSymbol(ctx, kind, bx, cy + CHAIN_BOX.h/2, CHAIN_BOX.w)
                                    ctx.fillStyle = "#555"; ctx.font = "10px sans-serif"
                                    var lbl = ellipsize(ctx, labelFor(idn), MAX_LABEL_W)
                                    ctx.fillText(lbl, bx + 14, cy + 12)
                                    ctx.strokeStyle = "#888"; ctx.lineWidth = 1
                                    ctx.beginPath(); ctx.moveTo(bx, cy + CHAIN_BOX.h); ctx.lineTo(bx, cy + CHAIN_BOX.h + SEG_GAP_Y); ctx.stroke()
                                    cy += CHAIN_BOX.h + SEG_GAP_Y
                                }
                                if (f1.endpoint && f1.endpoint !== "Unknown") {
                                    drawSymbol(ctx, f1.endpoint, bx, cy + 3, 12)
                                    ctx.fillStyle = (f1.endpoint === "Transformer") ? "#1f7a33" : "#090"
                                    ctx.font = "10px sans-serif"
                                    var ep = ellipsize(ctx, f1.endpoint, MAX_LABEL_W)
                                    ctx.fillText(ep, bx + 10, cy + 6)
                                }
                            }
                        }

                        // couplers du VL (EN DERNIER => au-dessus)
                        var cpls = (couplersByVl[blk.vl] || [])
                        for (i=0; i<cpls.length; ++i) {
                            var cpl = cpls[i]
                            var a = allBusPos[cpl.busA], bpos = allBusPos[cpl.busB]
                            if (!a || !bpos) continue
                            var yCoupler = blk.busPos[vlToBuses[blk.vl][0].id].y + BUS_H + 4
                            ctx.strokeStyle = (cpl.type === "CB") ? "#cc0000" : "#ffaa00"
                            ctx.lineWidth = 3
                            ctx.beginPath(); ctx.moveTo(a.x + BUS_W/2, yCoupler); ctx.lineTo(bpos.x + BUS_W/2, yCoupler); ctx.stroke()
                            ctx.fillStyle = "#333"; ctx.font = "10px sans-serif"
                            var mid = (a.x + bpos.x)/2 + BUS_W/2
                            var lab = (cpl.equip && cpl.equip.length) ? cpl.equip : cpl.type
                            ctx.fillText(lab, mid + 6, yCoupler - 4)
                        }
                    }

                    // mini-légende
                    var pad=12, boxW=220, boxH=120
                    var lx = width - boxW - pad, ly = height - boxH - pad
                    ctx.fillStyle = "rgba(255,255,255,0.92)"
                    ctx.fillRect(lx, ly, boxW, boxH)
                    ctx.strokeStyle = "#e0e6ef"; ctx.lineWidth = 1
                    ctx.strokeRect(lx, ly, boxW, boxH)
                    ctx.fillStyle = "#333"; ctx.font = "bold 12px sans-serif"
                    ctx.fillText("Légende", lx + 10, ly + 18)
                    var kinds = ["CB","DS","CT","VT","Transformer","Line"], yy = ly + 36
                    for (var q=0; q<kinds.length; ++q) {
                        drawSymbol(ctx, kinds[q], lx + 16, yy - 4, 14)
                        ctx.fillStyle = "#333"; ctx.font = "11px sans-serif"
                        ctx.fillText(kinds[q], lx + 30, yy)
                        yy += 18
                    }
                } // onPaint
            } // Canvas
        } // Flickable
    } // ColumnLayout
}
