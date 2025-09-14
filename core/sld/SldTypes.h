#pragma once
#include <QString>
#include <QPointF>
#include <QVector>
#include <QRectF>

namespace stationviz::sld {

enum class NodeKind { Busbar, Bay, Breaker, Disconnector, Transformer, VoltageLevelLabel, Unknown };

struct NodeId { QString id; };

struct NodeStyle { double w=32, h=24; bool rotate=false; };

struct Node {
    NodeId id;            // unique, e.g. "Sub/VL/Bay/CBR1"
    NodeKind kind = NodeKind::Unknown;
    QString label;        // e.g. CBR1
    QPointF pos;          // layout position (scene coords, px)
    NodeStyle style;      // size/orientation
};

struct Edge { QString from; QString to; QVector<QPointF> polyline; };

struct Scene { QVector<Node> nodes; QVector<Edge> edges; QRectF bounds; };

} // namespace stationviz::sld
