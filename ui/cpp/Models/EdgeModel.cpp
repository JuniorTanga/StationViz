
#include "EdgeModel.h"

QVariantMap EdgeModel::get(int row) const {
    QVariantMap m;
    if (row < 0 || row >= (int)edges_.size()) return m;
    const auto& e = edges_[row];
    m["fromId"] = e.fromId;
    m["toId"]   = e.toId;
    m["kind"]   = e.kind;
    return m;
}
