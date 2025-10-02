// StationViz/core/network/EventBus.cpp
#include "EventBus.h"

namespace network {

void EventBus::subscribe(TagChanged cb) {
    std::lock_guard<std::mutex> lk(m_);
    subs_.push_back(std::move(cb));
}

void EventBus::publish(TagId id, const TagValue& v) {
    std::vector<TagChanged> local;
    {
        std::lock_guard<std::mutex> lk(m_);
        local = subs_; // copie pour éviter deadlocks si un sub re-souscrit
    }
    for (auto& s : local) s(id, v);
}

} // namespace network
