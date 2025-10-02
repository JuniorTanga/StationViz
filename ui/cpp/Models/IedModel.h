#pragma once
#include <QAbstractListModel>
#include <QVariant>
#include <vector>

class IedModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    struct Item {
        QString name;
        QVariantList lds; // liste de { inst, equipments:[{label, lnClass, lnInst, prefix, anchors}] }
        int mms{0};
        int gse{0};
        int sv{0};
    };

    enum Roles { NameRole = Qt::UserRole + 1, LdsRole, MmsRole, GseRole, SvRole };

    explicit IedModel(QObject* parent=nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return static_cast<int>(items_.size());
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() < 0 || idx.row() >= (int)items_.size()) return {};
        const Item& it = items_[idx.row()];
        switch (role) {
            case NameRole: return it.name;
            case LdsRole:  return it.lds;
            case MmsRole:  return it.mms;
            case GseRole:  return it.gse;
            case SvRole:   return it.sv;
        }
        return {};
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            { NameRole, "name" },
            { LdsRole,  "lds"  },
            { MmsRole,  "mms"  },
            { GseRole,  "gse"  },
            { SvRole,   "sv"   }
        };
    }

    // utils
    void beginReset(){ beginResetModel(); }
    void endReset(){ endResetModel(); emit countChanged(); }
    void clearNoSignal(){ items_.clear(); }
    void appendNoSignal(Item it){ items_.push_back(std::move(it)); }
    Q_INVOKABLE int count() const { return (int)items_.size(); }

    Q_INVOKABLE QVariantMap get(int row) const {
        QVariantMap m;
        if (row < 0 || row >= (int)items_.size()) return m;
        const auto& it = items_[row];
        m["name"] = it.name; m["lds"] = it.lds;
        m["mms"] = it.mms; m["gse"] = it.gse; m["sv"] = it.sv;
        return m;
    }

signals:
    void countChanged();

private:
    std::vector<Item> items_;
};
