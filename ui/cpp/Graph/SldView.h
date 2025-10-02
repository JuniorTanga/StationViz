#pragma once
#include <QQuickItem>
#include <QPointer>
#include "SldIconAtlas.h"

class NodeModel;
class EdgeModel;

class SldView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(bool iconsEnabled READ iconsEnabled WRITE setIconsEnabled NOTIFY iconsEnabledChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(qreal panX READ panX WRITE setPanX NOTIFY panChanged)
    Q_PROPERTY(qreal panY READ panY WRITE setPanY NOTIFY panChanged)
    Q_PROPERTY(NodeModel* nodes READ nodes WRITE setNodes NOTIFY nodesChanged)
    Q_PROPERTY(EdgeModel* edges READ edges WRITE setEdges NOTIFY edgesChanged)
    Q_PROPERTY(QString selectionId READ selectionId WRITE setSelectionId NOTIFY selectionChanged)
    Q_PROPERTY(QColor edgeColor READ edgeColor WRITE setEdgeColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor nodeColor READ nodeColor WRITE setNodeColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY colorsChanged)

public:
    SldView();

    qreal zoom() const { return zoom_; }
    void setZoom(qreal z);

    qreal panX() const { return panX_; }
    void setPanX(qreal v);

    qreal panY() const { return panY_; }
    void setPanY(qreal v);

    NodeModel* nodes() const { return nodes_; }
    void setNodes(NodeModel* m);

    EdgeModel* edges() const { return edges_; }
    void setEdges(EdgeModel* m);

    QString selectionId() const { return selectionId_; }
    void setSelectionId(const QString &id) {
        if (selectionId_ == id)
            return;
        selectionId_ = id;
        update();
        emit selectionChanged();
    }

    QColor edgeColor() const { return edgeColor_; }
    void setEdgeColor(const QColor& c){ if (edgeColor_==c) return; edgeColor_=c; emit colorsChanged(); update(); }

    QColor nodeColor() const { return nodeColor_; }
    void setNodeColor(const QColor& c){ if (nodeColor_==c) return; nodeColor_=c; emit colorsChanged(); update(); }

    QColor selectionColor() const { return selectionColor_; }
    void setSelectionColor(const QColor& c){ if (selectionColor_==c) return; selectionColor_=c; emit colorsChanged(); update(); }

    bool iconsEnabled() const { return iconsEnabled_; }
    void setIconsEnabled(bool v){ if (iconsEnabled_==v) return; iconsEnabled_=v; update(); emit iconsEnabledChanged(); }

    Q_INVOKABLE void fitToContent();
    Q_INVOKABLE void centerOn(const QString& nodeId);

signals:
    void zoomChanged();
    void panChanged();
    void nodesChanged();
    void edgesChanged();
    void nodeClicked(QString id);
    void selectionChanged();
    void colorsChanged();
    void iconsEnabledChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    QRectF computeContentBBox() const;
    QString hitTestNodeId(const QPointF& viewPt) const;

    QString selectionId_;
    QColor edgeColor_{QColor("#9E9E9E")};
    QColor nodeColor_{QColor("#263238")};
    QColor selectionColor_{QColor("#2E7D32")}; // vert

    qreal zoom_{1.0};
    qreal panX_{0.0}, panY_{0.0};
    qreal iconZoomThreshold_{0.7};
    QPointer<NodeModel> nodes_;
    QPointer<EdgeModel> edges_;

    QPointF lastDrag_;
    bool dragging_{false};

    SldIconAtlas atlas_;
    bool atlasBuilt_{false};
    void ensureAtlas_();
    QHash<QString, QString> iconMap_; // kind -> qrc path

    QSGTransformNode* rootNode_{nullptr};
    QSGGeometryNode*  busNode_{nullptr};   // triangles (bus épais)
    QSGGeometryNode*  edgeNode_{nullptr};  // lignes orthogonales
    QSGGeometryNode*  nodeNode_{nullptr};  // rectangles filaires
    QSGGeometryNode*  selNode_{nullptr};   // cadre sélection
    QSGGeometryNode*  iconNode_{nullptr};  // triangles texturés (désactivé par défaut)

    bool iconsEnabled_{false};
};
