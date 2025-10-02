// NodeModel.cpp

#include "NodeModel.h"
#include <QVariantMap>

QVariantMap NodeModel::get(int row) const {
    QVariantMap m;
    if (row < 0 || row >= static_cast<int>(nodes_.size()))
        return m;

    const auto& n = nodes_[row];
    m.insert(QStringLiteral("id"),    n.id);
    m.insert(QStringLiteral("kind"),  n.kind);
    m.insert(QStringLiteral("label"), n.label);
    m.insert(QStringLiteral("x"),     n.x);
    m.insert(QStringLiteral("y"),     n.y);
    m.insert(QStringLiteral("state"), n.state);
    return m;
}

