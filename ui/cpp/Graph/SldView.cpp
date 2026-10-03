#include <QSGGeometryNode>
#include <QMatrix4x4>
#include <QSGFlatColorMaterial>
#include <QSGTransformNode>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QSGTextureMaterial>
#include <QSGGeometry>
#include <algorithm>

#include "../Models/NodeModel.h"
#include "../Models/EdgeModel.h"
#include "SldView.h"

SldView::SldView() {
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    iconMap_.insert("CB",          ":/icons/equipment/cbr_opened.svg");
    iconMap_.insert("DS",          ":/icons/equipment/ds_opened.svg");
    iconMap_.insert("CT",          ":/icons/equipment/ct.svg");
    iconMap_.insert("VT",          ":/icons/equipment/vt.svg");
    iconMap_.insert("Transformer", ":/icons/equipment/transformer_2w.svg");
    iconMap_.insert("Unknown",     ":/icons/equipment/unknown.svg");
    // On n’affiche pas d’icône pour la barre (bus dessiné en épais)
    // iconMap_.insert("Bus", ":/icons/equipment/busbar.svg");
}

void SldView::ensureAtlas_() {
    if (atlasBuilt_) return;
    QMap<QString, QString> res;
    for (auto it = iconMap_.cbegin(); it != iconMap_.cend(); ++it)
        res.insert(it.key(), it.value());
    atlas_.build(res, 96, 2);   // 96px/sprite pour netteté au zoom
    atlasBuilt_ = true;
}

void SldView::setZoom(qreal z){ if (z<0.05) z=0.05; if (z>20.0) z=20.0; if (qFuzzyCompare(zoom_,z)) return; zoom_=z; emit zoomChanged(); update(); }
void SldView::setPanX(qreal v){ if (qFuzzyCompare(panX_,v)) return; panX_=v; emit panChanged(); update(); }
void SldView::setPanY(qreal v){ if (qFuzzyCompare(panY_,v)) return; panY_=v; emit panChanged(); update(); }

void SldView::setNodes(NodeModel* m){
    if (nodes_ == m) return;
    // The model is usually the same object across reloads; only its contents
    // change. Without these connections updatePaintNode was never called after
    // App.openSclFile(), so the canvas stayed blank until the user panned.
    if (nodes_) disconnect(nodes_, nullptr, this, nullptr);
    nodes_ = m;
    if (nodes_) {
        connect(nodes_, &QAbstractItemModel::modelReset, this, &SldView::update);
        connect(nodes_, &QAbstractItemModel::dataChanged, this, [this]{ update(); });
        connect(nodes_, &QAbstractItemModel::rowsInserted, this, [this]{ update(); });
        connect(nodes_, &QAbstractItemModel::rowsRemoved,  this, [this]{ update(); });
    }
    emit nodesChanged();
    update();
}

void SldView::setEdges(EdgeModel* m){
    if (edges_ == m) return;
    if (edges_) disconnect(edges_, nullptr, this, nullptr);
    edges_ = m;
    if (edges_) {
        connect(edges_, &QAbstractItemModel::modelReset, this, &SldView::update);
        connect(edges_, &QAbstractItemModel::dataChanged, this, [this]{ update(); });
        connect(edges_, &QAbstractItemModel::rowsInserted, this, [this]{ update(); });
        connect(edges_, &QAbstractItemModel::rowsRemoved,  this, [this]{ update(); });
    }
    emit edgesChanged();
    update();
}

QRectF SldView::computeContentBBox() const {
    if (!nodes_) return {};
    QRectF bbox; bool first=true;
    for (const auto& n : nodes_->items()){
        QRectF r(n.x-8, n.y-8, 16, 16);
        if (first){ bbox = r; first=false; }
        else bbox = bbox.united(r);
    }
    return bbox;
}

void SldView::fitToContent() {
    QRectF bb = computeContentBBox();
    if (bb.isEmpty()) return;
    const qreal w = width(), h = height();
    if (w<=0 || h<=0) return;
    const qreal zx = (w * 0.9) / bb.width();
    const qreal zy = (h * 0.9) / bb.height();
    setZoom(qMin(zx, zy));
    setPanX(-(bb.center().x()*zoom_) + w/2.0);
    setPanY(-(bb.center().y()*zoom_) + h/2.0);
}

void SldView::centerOn(const QString& nodeId){
    if (!nodes_) return;
    for (const auto& n : nodes_->items()){
        if (n.id == nodeId){
            const qreal w = width(), h = height();
            setPanX(-(n.x*zoom_) + w/2.0);
            setPanY(-(n.y*zoom_) + h/2.0);
            break;
        }
    }
}

QSGNode* SldView::updatePaintNode(QSGNode* old, UpdatePaintNodeData*)
{
    // Root + transform (pan/zoom)
    QSGTransformNode* root = dynamic_cast<QSGTransformNode*>(old);
    if (!root) root = new QSGTransformNode;

    QMatrix4x4 m;
    m.translate(panX_, panY_);
    m.scale(zoom_);
    root->setMatrix(m);

    // Helpers: allocation géométrie
    auto ensureLinesGeom = [](QSGNode* parent, int idx)->QSGGeometryNode*{
        QSGGeometryNode* n = nullptr;
        if (parent->childCount() > idx) n = static_cast<QSGGeometryNode*>(parent->childAtIndex(idx));
        else { n = new QSGGeometryNode(); parent->appendChildNode(n); }
        if (!n->geometry()) {
            auto* g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
            g->setDrawingMode(QSGGeometry::DrawLines);
            n->setGeometry(g);
            n->setFlag(QSGNode::OwnsGeometry, true);
            auto* mat = new QSGFlatColorMaterial();
            mat->setColor(Qt::black);
            n->setMaterial(mat);
            n->setFlag(QSGNode::OwnsMaterial, true);
        } else {
            n->geometry()->setDrawingMode(QSGGeometry::DrawLines);
        }
        return n;
    };

    auto ensureTrianglesGeom = [](QSGNode* parent, int idx)->QSGGeometryNode*{
        QSGGeometryNode* n = nullptr;
        if (parent->childCount() > idx) n = static_cast<QSGGeometryNode*>(parent->childAtIndex(idx));
        else { n = new QSGGeometryNode(); parent->appendChildNode(n); }
        if (!n->geometry()) {
            auto* g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
            g->setDrawingMode(QSGGeometry::DrawTriangles);
            n->setGeometry(g);
            n->setFlag(QSGNode::OwnsGeometry, true);
            auto* mat = new QSGFlatColorMaterial();
            mat->setColor(Qt::black);
            n->setMaterial(mat);
            n->setFlag(QSGNode::OwnsMaterial, true);
        } else {
            n->geometry()->setDrawingMode(QSGGeometry::DrawTriangles);
        }
        return n;
    };

    auto pushSeg = [](QVector<QPointF>& buf, const QPointF& a, const QPointF& b){
        buf.push_back(a); buf.push_back(b);
    };

    auto pushThickRect = [&](QVector<QPointF>& buf, const QPointF& a, const QPointF& b, float pxThickness){
        const float half = pxThickness / std::max((float)zoom_, 0.001f) * 0.5f;
        const float dx = float(b.x() - a.x());
        const float dy = float(b.y() - a.y());
        const float len = std::max(1e-6f, std::sqrt(dx*dx + dy*dy));
        const float nx = -dy / len, ny = dx / len;
        const QPointF p1(a.x() + nx*half, a.y() + ny*half);
        const QPointF p2(a.x() - nx*half, a.y() - ny*half);
        const QPointF p3(b.x() - nx*half, b.y() - ny*half);
        const QPointF p4(b.x() + nx*half, b.y() + ny*half);
        buf.push_back(p1); buf.push_back(p2); buf.push_back(p3);
        buf.push_back(p1); buf.push_back(p3); buf.push_back(p4);
    };

    auto orthPolyline = [](const QPointF& a, const QPointF& b,
                           const QString& kindA, const QString& kindB,
                           const QString& edgeKind)->QVector<QPointF>
    {
        QVector<QPointF> P;
        const bool nearX = qAbs(a.x() - b.x()) < 0.5;
        const bool nearY = qAbs(a.y() - b.y()) < 0.5;
        if (edgeKind == "BusSpan") { P << a << b; return P; }
        if (nearX || nearY) { P << a << b; return P; }

        const bool busToEquip = (kindA == "Bus" && kindB != "Bus");
        const bool equipToBus = (kindB == "Bus" && kindA != "Bus");
        if (busToEquip || equipToBus) {
            const QPointF src = busToEquip ? a : b;
            const QPointF dst = busToEquip ? b : a;
            const float dy = float(dst.y() - src.y());
            const float jog = std::clamp(dy * 0.5f, 32.0f, 140.0f);
            const float ytap = float(src.y()) + jog;
            if (busToEquip) { P << a << QPointF(a.x(), ytap) << QPointF(b.x(), ytap) << b; }
            else            { P << b << QPointF(b.x(), ytap) << QPointF(a.x(), ytap) << a; }
            return P;
        }

        if (edgeKind == "FeederBranch") {
            // depuis CT (vertical) vers VT (à droite) : petit L propre
            P << a << QPointF(b.x(), a.y()) << b;
            return P;
        }

        P << a << QPointF(a.x(), b.y()) << b;
        return P;
    };

    // Prépare positions & kinds
    QHash<QString,QPointF> pos;
    QHash<QString,QString> kind;
    if (nodes_) {
        pos.reserve((int)nodes_->items().size());
        kind.reserve((int)nodes_->items().size());
        for (const auto& n : nodes_->items()) {
            pos.insert(n.id, QPointF(n.x, n.y));
            kind.insert(n.id, n.kind);
        }
    }

    // 0) BUS épais (triangles)
    {
        const float busPx = 3.0f;
        auto* g = ensureTrianglesGeom(root, 0);
        QVector<QPointF> tris;

        if (edges_) {
            for (const auto& e : edges_->items()) {
                if (e.kind != "BusSpan") continue;
                const QPointF a = pos.value(e.fromId, {});
                const QPointF b = pos.value(e.toId,   {});
                pushThickRect(tris, a, b, busPx);
            }
        }

        auto* geom = g->geometry();
        geom->allocate(tris.size());
        auto* v = geom->vertexDataAsPoint2D();
        for (int i=0;i<tris.size();++i) v[i].set(tris[i].x(), tris[i].y());
        static_cast<QSGFlatColorMaterial*>(g->material())->setColor(edgeColor_);
        g->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

    // 1) Arêtes orthogonales (hors BusSpan)
    {
        auto* g = ensureLinesGeom(root, 1);
        QVector<QPointF> segs;

        if (edges_) {
            for (const auto& e : edges_->items()) {
                if (e.kind == "BusSpan") continue;
                const QPointF a = pos.value(e.fromId, {});
                const QPointF b = pos.value(e.toId,   {});
                const QString ka = kind.value(e.fromId);
                const QString kb = kind.value(e.toId);

                auto poly = orthPolyline(a, b, ka, kb, e.kind);
                for (int i=0; i+1<poly.size(); ++i) pushSeg(segs, poly[i], poly[i+1]);
            }
        }

        auto* geom = g->geometry();
        geom->allocate(segs.size());
        auto* v = geom->vertexDataAsPoint2D();
        for (int i=0;i<segs.size();++i) v[i].set(segs[i].x(), segs[i].y());
        static_cast<QSGFlatColorMaterial*>(g->material())->setColor(edgeColor_);
        g->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

    // 2) Nœuds (rectangles filaires)
    {
        auto* g = ensureLinesGeom(root, 2);
        auto* geom = g->geometry();
        const int count = (nodes_ ? (int)nodes_->items().size() : 0) * 8;
        geom->allocate(count);
        auto* v = geom->vertexDataAsPoint2D();
        int k = 0;

        if (nodes_) {
            for (const auto& n : nodes_->items()) {
                // Only equipment gets a marker box. Junction nodes are the
                // endpoints of a bus span and a busbar is drawn as the bar
                // itself; drawing a 12x12 box on every node put a square on the
                // middle of each busbar and on all 12 span endpoints, which read
                // as "the busbars are small squares".
                if (n.kind != QLatin1String("Equipment")) continue;
                const float r = 6.f;
                const float x = (float)n.x, y = (float)n.y;
                v[k++].set(x-r, y-r); v[k++].set(x+r, y-r);
                v[k++].set(x+r, y-r); v[k++].set(x+r, y+r);
                v[k++].set(x+r, y+r); v[k++].set(x-r, y+r);
                v[k++].set(x-r, y+r); v[k++].set(x-r, y-r);
            }
        }
        static_cast<QSGFlatColorMaterial*>(g->material())->setColor(nodeColor_);
        g->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

    // 3) Sélection
    {
        auto* g = ensureLinesGeom(root, 3);
        auto* geom = g->geometry();
        int count = (selectionId_.isEmpty() ? 0 : 8);
        geom->allocate(count);
        auto* v = geom->vertexDataAsPoint2D();

        if (count && nodes_) {
            for (const auto& n : nodes_->items()){
                if (n.id == selectionId_) {
                    const float r = 9.f;
                    const float x = (float)n.x, y = (float)n.y;
                    int k = 0;
                    v[k++].set(x-r, y-r); v[k++].set(x+r, y-r);
                    v[k++].set(x+r, y-r); v[k++].set(x+r, y+r);
                    v[k++].set(x+r, y+r); v[k++].set(x-r, y+r);
                    v[k++].set(x-r, y+r); v[k++].set(x-r, y-r);
                    break;
                }
            }
        }
        static_cast<QSGFlatColorMaterial*>(g->material())->setColor(selectionColor_);
        g->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

    // 4) Icônes batchées (préserve le ratio)
    {
        ensureAtlas_();
        if (!iconsEnabled_ || zoom_ < iconZoomThreshold_ || !nodes_) {
            // on s’assure tout de même d’avoir un node pour garder l’ordre des layers
            QSGGeometryNode* g = (root->childCount() > 4)
                                     ? static_cast<QSGGeometryNode*>(root->childAtIndex(4))
                                     : nullptr;
            if (!g) {
                g = new QSGGeometryNode();
                root->appendChildNode(g);
                auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
                geom->setDrawingMode(QSGGeometry::DrawTriangles);
                g->setGeometry(geom);
                g->setFlag(QSGNode::OwnsGeometry, true);
                auto* mat = new QSGTextureMaterial();
                g->setMaterial(mat);
                g->setFlag(QSGNode::OwnsMaterial, true);
            }
            g->geometry()->allocate(0);
            g->markDirty(QSGNode::DirtyGeometry);
        } else {
            QSGTexture* tex = atlas_.textureFor(window());
            QSGGeometryNode* g = (root->childCount() > 4)
                                     ? static_cast<QSGGeometryNode*>(root->childAtIndex(4))
                                     : nullptr;
            if (!g) {
                g = new QSGGeometryNode();
                root->appendChildNode(g);
                auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
                geom->setDrawingMode(QSGGeometry::DrawTriangles);
                g->setGeometry(geom);
                g->setFlag(QSGNode::OwnsGeometry, true);
                auto* mat = new QSGTextureMaterial();
                g->setMaterial(mat);
                g->setFlag(QSGNode::OwnsMaterial, true);
            }
            auto* geom = g->geometry();

            // Compte les icônes affichables
            int iconCount = 0;
            for (const auto& n : nodes_->items()) {
                if (iconMap_.contains(n.kind) && !atlas_.entry(n.kind).uv.isNull())
                    ++iconCount;
            }
            geom->allocate(iconCount * 6);

            auto* mat = static_cast<QSGTextureMaterial*>(g->material());
            mat->setTexture(tex);

            auto* verts = reinterpret_cast<QSGGeometry::TexturedPoint2D*>(geom->vertexData());
            int k = 0;

            // Respecte la propriété QML iconWorldHeight. La valeur codée en dur
            // (46) dépassait le pas vertical du layout (44) et, pour un VT
            // d'aspect ~1.7,-envoyait l'icône recouvrir la voie voisine.
            const float iconH = iconWorldHeight_;

            for (const auto& n : nodes_->items()) {
                if (!iconMap_.contains(n.kind)) continue;
                const auto e = atlas_.entry(n.kind);
                if (e.uv.isNull()) continue;

                const float aspect = (e.aspect > 0.f ? e.aspect : 1.f);
                const float w = iconH * aspect;

                const float x = static_cast<float>(n.x);
                const float y = static_cast<float>(n.y);
                const float x0 = x - w * 0.5f, x1 = x + w * 0.5f;
                const float y0 = y - iconH * 0.5f, y1 = y + iconH * 0.5f;

                const float u0 = e.uv.left();
                const float v0 = e.uv.top();
                const float u1 = e.uv.right();
                const float v1 = e.uv.bottom();

                verts[k++].set(x0, y0, u0, v0);
                verts[k++].set(x1, y0, u1, v0);
                verts[k++].set(x1, y1, u1, v1);

                verts[k++].set(x0, y0, u0, v0);
                verts[k++].set(x1, y1, u1, v1);
                verts[k++].set(x0, y1, u0, v1);
            }
            g->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
        }
    }

    return root;
}

void SldView::wheelEvent(QWheelEvent* ev) {
    const QPointF p = ev->position();
    const qreal oldZ = zoom_;
    const qreal factor = ev->angleDelta().y() > 0 ? 1.1 : 0.9;
    setZoom(zoom_ * factor);
    const qreal sx = (p.x() - panX_) / oldZ;
    const qreal sy = (p.y() - panY_) / oldZ;
    setPanX(p.x() - sx*zoom_);
    setPanY(p.y() - sy*zoom_);
    ev->accept();
}

void SldView::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        const QPointF pView = ev->position();
        const QPointF pWorld = QPointF((pView.x()-panX_)/zoom_, (pView.y()-panY_)/zoom_);
        const QString id = hitTestNodeId(pWorld);
        if (!id.isEmpty()) emit nodeClicked(id);
        else emit nodeClicked(QString());   // clic dans le vide : désélectionne
    }
    if (ev->button() == Qt::MiddleButton || (ev->button()==Qt::LeftButton && ev->modifiers() & Qt::AltModifier)) {
        dragging_ = true;
        lastDrag_ = ev->position();
        ev->accept();
        return;
    }
    if (ev->button() == Qt::LeftButton) ev->accept();
}

void SldView::mouseMoveEvent(QMouseEvent* ev) {
    if (dragging_) {
        const QPointF d = ev->position() - lastDrag_;
        setPanX(panX_ + d.x());
        setPanY(panY_ + d.y());
        lastDrag_ = ev->position();
        ev->accept();
    }
}

// Sans ceci dragging_ restait vrai après le premier glissement : le moindre
// déplacement de souris ensuite déplaçait le diagramme au lieu de sélectionner.
void SldView::mouseReleaseEvent(QMouseEvent* ev) {
    if (dragging_) {
        dragging_ = false;
        ev->accept();
    }
}

QString SldView::hitTestNodeId(const QPointF& worldPt) const {
    if (!nodes_) return {};
    for (const auto& n : nodes_->items()){
        QRectF r(n.x-8, n.y-8, 16,16);
        if (r.contains(worldPt)) return n.id;
    }
    return {};
}
