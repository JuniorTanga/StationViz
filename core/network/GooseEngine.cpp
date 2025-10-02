// StationViz/core/network/GooseEngine.cpp
#include "GooseEngine.h"
#include <cstring>

namespace network {

GooseEngine::GooseEngine(EventBus& bus, StateStore& store)
: bus_(bus), store_(store) {}

GooseEngine::~GooseEngine() { stop(); }

bool GooseEngine::start(const std::string& ifaceName) {
    if (running_) return true;
    receiver_ = GooseReceiver_create();
    if (!receiver_) return false;
    GooseReceiver_setInterfaceId(receiver_, ifaceName.c_str());
    GooseReceiver_start(receiver_);
    running_ = GooseReceiver_isRunning(receiver_);
    return running_;
}

void GooseEngine::stop() {
    std::lock_guard<std::mutex> lk(mu_);
    if (receiver_) {
        GooseReceiver_stop(receiver_);
        GooseReceiver_destroy(receiver_);
        receiver_ = nullptr;
    }
    subs_.clear();
    running_ = false;
}

bool GooseEngine::isRunning() const { return running_; }

bool GooseEngine::subscribe(const std::string& goCbRef,
                            const std::string& macStr,
                            uint16_t appId,
                            const std::vector<std::string>& members)
{
    if (!receiver_) return false;

    // Crée un subscriber (API attend goCbRef en "MMS notation" - facultatif au filtrage niveau Ethernet)
    GooseSubscriber sub = GooseSubscriber_create(
        goCbRef.empty() ? const_cast<char*>("") : const_cast<char*>(goCbRef.c_str()),
        /*dataSetValues*/ nullptr
    );

    if (!sub) return false;

    // Filtre MAC + APPID
    auto mac = parseMac(macStr);
    if (!mac) { GooseSubscriber_destroy(sub); return false; }
    GooseSubscriber_setDstMac(sub, const_cast<uint8_t*>(mac->b.data()));
    GooseSubscriber_setAppId(sub, appId);

    // Callback
    GooseSubscriber_setListener(sub, &GooseEngine::onGoose, this);

    // Enregistrement dans le receiver (doit être fait AVANT start, mais on suppose start() déjà fait -> doc: éviter add/remove en cours de run)
    // Ici on prend le parti de stopper/ajouter/redémarrer si nécessaire.
    bool needRestart = false;
    if (running_) { GooseReceiver_stop(receiver_); needRestart = true; }

    GooseReceiver_addSubscriber(receiver_, sub);

    if (needRestart) GooseReceiver_start(receiver_);
    running_ = GooseReceiver_isRunning(receiver_);

    // Sauvegarde mapping membres (pour fallback si pas de dataRef)
    std::lock_guard<std::mutex> lk(mu_);
    subs_.push_back(Sub{ sub, members });
    return true;
}

void GooseEngine::setMetaResolver(std::function<std::optional<TagId>(const std::string&)> r) {
    resolveTagId_ = std::move(r);
}

/* static */ void GooseEngine::onGoose(GooseSubscriber subscriber, void* parameter) {
    auto* self = static_cast<GooseEngine*>(parameter);
    self->handleGoose(subscriber);
}

void GooseEngine::handleGoose(GooseSubscriber s) {
    // Standard: ignorer test=TRUE
    if (GooseSubscriber_isTest(s)) return;

    MmsValue* ds = GooseSubscriber_getDataSetValues(s);
    if (!ds) return;

    // Récupérer notre mapping par handle
    std::vector<std::string> members;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& sub : subs_) {
            if (sub.h == s) { members = sub.members; break; }
        }
    }
    const int n = MmsValue_getArraySize(ds);
    for (int i = 0; i < n; ++i) {
        MmsValue* v = MmsValue_getElement(ds, i);
        // Si on a un mapping index -> objectRef
        if (i < (int)members.size() && resolveTagId_) {
            auto id = resolveTagId_(members[i]);
            if (id) {
                TagValue tv = mmsToTagValue(v);
                store_.set(*id, tv);
                bus_.publish(*id, tv);
            }
        }
    }
}

// Conversion minimaliste MmsValue -> TagValue (mêmes choix que MmsSession)
TagValue GooseEngine::mmsToTagValue(MmsValue* v) {
    if (!v) return std::monostate{};
    switch (MmsValue_getType(v)) {
        case MMS_BOOLEAN:    return TagValue{ MmsValue_getBoolean(v) };
        case MMS_INTEGER:
        case MMS_UNSIGNED:   return TagValue{ (int64_t) MmsValue_toInt64(v) };
        case MMS_FLOAT:      return TagValue{ (double)  MmsValue_toDouble(v) };
        case MMS_VISIBLE_STRING:
        case MMS_STRING:     return TagValue{ std::string(MmsValue_toString(v)) };
        case MMS_OCTET_STRING: {
            ByteString bs;
            int size = MmsValue_getOctetStringSize(v);
            bs.resize(size);
            MmsValue_getOctetString(v, bs.data(), size);
            return TagValue{ std::move(bs) };
        }
        case MMS_BIT_STRING:
            return TagValue{ (int64_t) MmsValue_getBitStringAsInteger(v) };
        case MMS_ARRAY: {
            VectorValue vv;
            const int n = MmsValue_getArraySize(v);
            vv.data.reserve(n);
            for (int i=0;i<n;++i) {
                MmsValue* e = MmsValue_getElement(v, i);
                if (MmsValue_getType(e) == MMS_FLOAT)
                    vv.data.push_back((double) MmsValue_toDouble(e));
                else if (MmsValue_getType(e) == MMS_INTEGER || MmsValue_getType(e) == MMS_UNSIGNED)
                    vv.data.push_back((double) MmsValue_toInt64(e));
                else if (MmsValue_getType(e) == MMS_BOOLEAN)
                    vv.data.push_back(MmsValue_getBoolean(e) ? 1.0 : 0.0);
                else
                    vv.data.push_back(0.0);
            }
            return TagValue{ std::move(vv) };
        }
        default:
            return std::monostate{};
    }
}

} // namespace network
