// StationViz/core/network/StateStore.cpp
#include "StateStore.h"

namespace network {

bool StateStore::get(TagId id, TagValue& out) const {
    std::shared_lock lk(m_);
    auto it = map_.find(id);
    if (it == map_.end()) return false;
    out = it->second;
    return true;
}

void StateStore::set(TagId id, const TagValue& v) {
    std::unique_lock lk(m_);
    map_[id] = v;
}

} // namespace network
