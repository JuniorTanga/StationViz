// StationViz/core/network/GooseEngine.h
#pragma once
#include "NetworkTypes.h"
#include "EventBus.h"
#include "StateStore.h"
#include "NetUtils.h"
#include <vector>
#include <string>
#include <functional>
#include <mutex>

extern "C" {
#include <goose_receiver.h>
#include <goose_subscriber.h>
}

namespace network {

class GooseEngine {
public:
    explicit GooseEngine(EventBus& bus, StateStore& store);
    ~GooseEngine();

    // Démarre un receiver lié à une interface (ex: "eth0").
    bool start(const std::string& ifaceName);
    void stop();
    bool isRunning() const;

    // S'abonner à un flux GOOSE sur cette interface.
    // 'members' = object refs (ordre du DataSet) pour mapping index→Tag
    // goCbRef: si connu (ex: "LD0/LLN0$GO$gcbName"), sinon laisser vide.
    bool subscribe(const std::string& goCbRef,
                   const std::string& macStr,
                   uint16_t appId,
                   const std::vector<std::string>& members);

    // Résolution TagId depuis objectRef (injecté par TagRegistry/NetworkManager)
    void setMetaResolver(std::function<std::optional<TagId>(const std::string& objectRef)> r);

private:
    static void onGoose(GooseSubscriber subscriber, void* parameter);

    void handleGoose(GooseSubscriber s);

    static TagValue mmsToTagValue(MmsValue* v);

private:
    EventBus& bus_;
    StateStore& store_;
    std::function<std::optional<TagId>(const std::string&)> resolveTagId_;

    GooseReceiver receiver_{nullptr};

    struct Sub {
        GooseSubscriber h{nullptr};
        std::vector<std::string> members; // object refs (index → ref)
    };

    std::mutex mu_;
    std::vector<Sub> subs_;
    bool running_{false};
};

} // namespace network
