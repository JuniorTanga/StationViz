// StationViz/core/network/SvEngine.cpp
#include "SvEngine.h"

namespace network {

SvEngine::SvEngine(EventBus& bus, StateStore& store)
: bus_(bus), store_(store) {}

SvEngine::~SvEngine() { stop(); }

bool SvEngine::start(const std::string& ifaceName) {
    if (running_) return true;
    receiver_ = SVReceiver_create();
    if (!receiver_) return false;
    SVReceiver_setInterfaceId(receiver_, ifaceName.c_str());
    SVReceiver_start(receiver_);
    running_ = SVReceiver_isRunning(receiver_);
    return running_;
}

void SvEngine::stop() {
    std::lock_guard<std::mutex> lk(mu_);
    if (receiver_) {
        SVReceiver_stop(receiver_);
        SVReceiver_destroy(receiver_);
        receiver_ = nullptr;
    }
    subs_.clear();
    running_ = false;
}

bool SvEngine::isRunning() const { return running_; }

bool SvEngine::subscribe(const std::string& macStr,
                         uint16_t appId,
                         const std::vector<SvFieldSpec>& fields)
{
    if (!receiver_) return false;

    auto mac = parseMac(macStr);
    if (!mac) return false;

    SVSubscriber sub = SVSubscriber_create(mac->b.data(), appId);
    if (!sub) return false;

    SVSubscriber_setListener(sub, &SvEngine::onSv, this);

    bool needRestart = false;
    if (running_) { SVReceiver_stop(receiver_); needRestart = true; }

    SVReceiver_addSubscriber(receiver_, sub);

    if (needRestart) SVReceiver_start(receiver_);
    running_ = SVReceiver_isRunning(receiver_);

    std::lock_guard<std::mutex> lk(mu_);
    subs_.push_back(Sub{ sub, fields });
    return true;
}

void SvEngine::setMetaResolver(std::function<std::optional<TagId>(const std::string&)> r) {
    resolveTagId_ = std::move(r);
}

/* static */ void SvEngine::onSv(SVSubscriber subscriber, void* parameter, SVSubscriber_ASDU asdu) {
    auto* self = static_cast<SvEngine*>(parameter);
    self->handleSv(subscriber, asdu);
}

void SvEngine::handleSv(SVSubscriber subscriber, SVSubscriber_ASDU asdu) {
    // Récupère mapping pour ce subscriber
    std::vector<SvFieldSpec> fields;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& sub : subs_) {
            if (sub.h == subscriber) { fields = sub.fields; break; }
        }
    }
    if (fields.empty() || !resolveTagId_) return;

    for (const auto& f : fields) {
        auto id = resolveTagId_(f.objectRef);
        if (!id) continue;
        TagValue tv = readField(asdu, f);
        store_.set(*id, tv);
        bus_.publish(*id, tv);
    }
}

TagValue SvEngine::readField(SVSubscriber_ASDU asdu, const SvFieldSpec& spec) const {
    switch (spec.type) {
        case SvType::BOOL:        return TagValue{ SVSubscriber_ASDU_getINT8U(asdu, spec.index) ? true : false };
        case SvType::INT8:        return TagValue{ (int64_t) SVSubscriber_ASDU_getINT8(asdu, spec.index) };
        case SvType::INT16:       return TagValue{ (int64_t) SVSubscriber_ASDU_getINT16(asdu, spec.index) };
        case SvType::INT32:       return TagValue{ (int64_t) SVSubscriber_ASDU_getINT32(asdu, spec.index) };
        case SvType::INT64:       return TagValue{ (int64_t) SVSubscriber_ASDU_getINT64(asdu, spec.index) };
        case SvType::UINT8:       return TagValue{ (int64_t) SVSubscriber_ASDU_getINT8U(asdu, spec.index) };
        case SvType::UINT16:      return TagValue{ (int64_t) SVSubscriber_ASDU_getINT16U(asdu, spec.index) };
        case SvType::UINT24:      return TagValue{ (int64_t) (SVSubscriber_ASDU_getINT32U(asdu, spec.index) & 0xFFFFFFu) };
        case SvType::UINT32:      return TagValue{ (int64_t) SVSubscriber_ASDU_getINT32U(asdu, spec.index) };
        case SvType::UINT64:      return TagValue{ (int64_t) SVSubscriber_ASDU_getINT64U(asdu, spec.index) };
        case SvType::FLOAT32:     return TagValue{ (double)  SVSubscriber_ASDU_getFLOAT32(asdu, spec.index) };
        case SvType::FLOAT64:     return TagValue{ (double)  SVSubscriber_ASDU_getFLOAT64(asdu, spec.index) };
        case SvType::TIMESTAMP8:  return TagValue{ Timestamp{ /*ms?*/ 0 } };
        case SvType::BITSTRING4:  return TagValue{ (int64_t) 0 }; // exposer en int si besoin (à affiner)
        case SvType::QUALITY4:    return TagValue{ Quality{ /*4 bytes*/ 0 } };
        // Les suivants requièrent des règles spécifiques (strings/octets) — rarement utilisés en SV
        case SvType::ENUM4:
        case SvType::CODED_ENUM4:
        case SvType::OCTET20:
        case SvType::VSTRING35:
        case SvType::ENTRYTIME6:
            return std::monostate{};
    }
    return std::monostate{};
}

} // namespace network
