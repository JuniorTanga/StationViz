// StationViz/core/network/SvEngine.h
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
#include <sv_subscriber.h>
}

namespace network {

enum class SvType {
    BOOL, INT8, INT16, INT32, INT64, UINT8, UINT16, UINT24, UINT32, UINT64,
    FLOAT32, FLOAT64, ENUM4, CODED_ENUM4, OCTET20, VSTRING35, TIMESTAMP8, ENTRYTIME6,
    BITSTRING4, QUALITY4
};

struct SvFieldSpec {
    std::string objectRef; // ex: "LD/LN.DO.DA[MX]"
    SvType type;
    int index;             // offset (en octets) dans l’ASDU
};

class SvEngine {
public:
    explicit SvEngine(EventBus& bus, StateStore& store);
    ~SvEngine();

    bool start(const std::string& ifaceName);
    void stop();
    bool isRunning() const;

    // Abonnement à un flux SV
    // eth MAC + appId identifient le flux; 'fields' décrit comment décoder l’ASDU
    bool subscribe(const std::string& macStr,
                   uint16_t appId,
                   const std::vector<SvFieldSpec>& fields);

    void setMetaResolver(std::function<std::optional<TagId>(const std::string& objectRef)> r);

private:
    static void onSv(SVSubscriber subscriber, void* parameter, SVSubscriber_ASDU asdu);
    void handleSv(SVSubscriber subscriber, SVSubscriber_ASDU asdu);

    TagValue readField(SVSubscriber_ASDU asdu, const SvFieldSpec& spec) const;

private:
    EventBus& bus_;
    StateStore& store_;
    std::function<std::optional<TagId>(const std::string&)> resolveTagId_;

    SVReceiver receiver_{nullptr};

    struct Sub {
        SVSubscriber h{nullptr};
        std::vector<SvFieldSpec> fields;
    };

    std::mutex mu_;
    std::vector<Sub> subs_;
    bool running_{false};
};

} // namespace network
