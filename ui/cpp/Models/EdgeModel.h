// EdgeModel.h
#pragma once
#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>
#include <vector>

class EdgeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    struct Edge { QString fromId; QString toId; QString kind; };
    enum Roles { FromRole=Qt::UserRole+1, ToRole, KindRole };
    Q_INVOKABLE int count() const { return static_cast<int>(edges_.size()); }
    Q_INVOKABLE QVariantMap get(int row) const;


    explicit EdgeModel(QObject* parent=nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& p=QModelIndex()) const override { return p.isValid()?0:(int)edges_.size(); }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid()) return {};
        const auto& e = edges_[idx.row()];
        switch(role){
            case FromRole: return e.fromId;
            case ToRole:   return e.toId;
            case KindRole: return e.kind;
        }
        return {};
    }
    QHash<int,QByteArray> roleNames() const override {
        return {{FromRole,"fromId"},{ToRole,"toId"},{KindRole,"kind"}};
    }

    void beginReset(){ beginResetModel(); }
    void endReset(){ endResetModel(); emit countChanged(); }
    void clear(){ beginResetModel(); edges_.clear(); endResetModel(); }
    void clearNoSignal(){ edges_.clear(); }
    void appendNoSignal(const Edge& e){ edges_.push_back(e); }

    const std::vector<Edge>& items() const { return edges_; }


signals:
    void countChanged();

private:
    std::vector<Edge> edges_;
};
