// StationViz/core/network/StateStore.h
#pragma once
#include "NetworkTypes.h"
#include <shared_mutex>
#include <unordered_map>

namespace network {

class StateStore {
public:
    bool get(TagId id, TagValue& out) const;
    void set(TagId id, const TagValue& v);
private:
    mutable std::shared_mutex m_;
    std::unordered_map<TagId, TagValue, TagId::Hash> map_;
};

} // namespace network
