// NodeModel.h
#pragma once
#include <QAbstractListModel>
#include <QString>
#include <vector>

class NodeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int count() const { return static_cast<int>(nodes_.size()); }



    struct Node { QString id; QString kind; QString label; double x{0}, y{0}; QString state; };
    enum Roles { IdRole=Qt::UserRole+1, KindRole, LabelRole, XRole, YRole, StateRole };

    explicit NodeModel(QObject* parent=nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& p=QModelIndex()) const override { return p.isValid()?0:(int)nodes_.size(); }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid()) return {};
        const auto& n = nodes_[idx.row()];
        switch(role){
            case IdRole:    return n.id;
            case KindRole:  return n.kind;
            case LabelRole: return n.label;
            case XRole:     return n.x;
            case YRole:     return n.y;
            case StateRole: return n.state;
        }
        return {};
    }
    QHash<int,QByteArray> roleNames() const override {
        return {{IdRole,"id"},{KindRole,"kind"},{LabelRole,"label"},{XRole,"x"},{YRole,"y"},{StateRole,"state"}};
    }
	


    // Batch reset helpers
    void beginReset(){ beginResetModel(); }
    void endReset(){ endResetModel(); emit countChanged(); }
    void clear(){ beginResetModel(); nodes_.clear(); endResetModel(); }
    void clearNoSignal(){ nodes_.clear(); }
    void appendNoSignal(const Node& n){ nodes_.push_back(n); }

    // utilitaires
    const std::vector<Node>& items() const { return nodes_; }

signals:
    void countChanged();

private:
    std::vector<Node> nodes_;
};
