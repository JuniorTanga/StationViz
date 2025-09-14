#include "SldModel.h"

using namespace stationviz::sld;

int SldNodeModel::rowCount(const QModelIndex& parent) const { (void)parent; return nodes_.size(); }

QVariant SldNodeModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row()<0 || idx.row()>=nodes_.size()) return {};
    const auto& n = nodes_[idx.row()];
    switch (role) {
        case IdRole: return n.id.id;
        case KindRole: return static_cast<int>(n.kind);
        case LabelRole: return n.label;
        case XRole: return n.pos.x();
        case YRole: return n.pos.y();
        case WRole: return n.style.w;
        case HRole: return n.style.h;
    }
    return {};
}

QHash<int,QByteArray> SldNodeModel::roleNames() const {
    return {{IdRole,"id"},{KindRole,"kind"},{LabelRole,"label"},{XRole,"x"},{YRole,"y"},{WRole,"w"},{HRole,"h"}};
}

void SldNodeModel::setNodes(const QVector<Node>& ns) { beginResetModel(); nodes_ = ns; endResetModel(); }

int SldEdgeModel::rowCount(const QModelIndex& parent) const { (void)parent; return edges_.size(); }

QVariant SldEdgeModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row()<0 || idx.row()>=edges_.size()) return {};
    const auto& e = edges_[idx.row()];
    switch (role) {
        case FromRole: return e.from;
        case ToRole: return e.to;
        case PointsRole: {
            QVariantList pts; pts.reserve(e.polyline.size());
            for (const auto& p : e.polyline) pts << QVariant::fromValue(QPointF(p));
            return pts;
        }
    }
    return {};
}

QHash<int,QByteArray> SldEdgeModel::roleNames() const {
    return {{FromRole,"from"},{ToRole,"to"},{PointsRole,"points"}};
}

void SldEdgeModel::setEdges(const QVector<Edge>& es) { beginResetModel(); edges_ = es; endResetModel(); }
