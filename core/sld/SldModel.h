#pragma once
#include "SldGraph.h"
#include <QAbstractListModel>

namespace stationviz::sld {

class SldNodeModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit SldNodeModel(QObject *parent = nullptr)
        : QAbstractListModel(parent) {} // <-- ajout
    enum Roles {
        IdRole = Qt::UserRole + 1,
        KindRole,
        LabelRole,
        XRole,
        YRole,
        WRole,
        HRole
    };

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &idx, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setNodes(const QVector<Node> &ns);

private:
    QVector<Node> nodes_;
};

class SldEdgeModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit SldEdgeModel(QObject *parent = nullptr)
        : QAbstractListModel(parent) {} // <-- ajout
    enum Roles { FromRole = Qt::UserRole + 1, ToRole, PointsRole };

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &idx, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEdges(const QVector<Edge> &es);

private:
    QVector<Edge> edges_;
};

} // namespace stationviz::sld
