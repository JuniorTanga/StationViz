// StationViz/core/network/EventBus.h
#pragma once
#include "NetworkTypes.h"
#include <mutex>
#include <vector>

namespace network {

class EventBus {
public:
    using TagChanged = std::function<void(TagId, const TagValue&)>;
    void subscribe(TagChanged cb);
    void publish(TagId id, const TagValue& v);
private:
    std::mutex m_;
    std::vector<TagChanged> subs_;
};

} // namespace network
