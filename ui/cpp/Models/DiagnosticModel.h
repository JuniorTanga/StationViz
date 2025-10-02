// DiagnosticModel.h
#pragma once
#include <QAbstractListModel>
#include <QString>
#include <vector>

class DiagnosticModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { SeverityRole=Qt::UserRole+1, MessageRole, LocationRole };

    struct Row { QString severity; QString message; QString location; };

    explicit DiagnosticModel(QObject* parent=nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& p=QModelIndex()) const override { return p.isValid()?0:(int)rows_.size(); }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid()) return {};
        const auto& r = rows_[idx.row()];
        switch(role){
            case SeverityRole: return r.severity;
            case MessageRole:  return r.message;
            case LocationRole: return r.location;
        }
        return {};
    }
    QHash<int,QByteArray> roleNames() const override {
        return {{SeverityRole,"severity"},{MessageRole,"message"},{LocationRole,"location"}};
    }

    void clear() { beginResetModel(); rows_.clear(); endResetModel(); }
    void append(const QString& sev, const QString& msg, const QString& loc) {
        beginInsertRows({}, (int)rows_.size(), (int)rows_.size());
        rows_.push_back({sev,msg,loc});
        endInsertRows();
    }

private:
    std::vector<Row> rows_;
};
