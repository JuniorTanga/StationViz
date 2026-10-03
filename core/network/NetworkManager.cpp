// StationViz/core/network/NetworkManager.cpp
#include "NetworkManager.h"

#include <algorithm>
#include <chrono>
#include <map>

namespace network {

NetworkManager::NetworkManager(EventBus& bus, StateStore& store)
: bus_(bus), store_(store) {}

NetworkManager::~NetworkManager() { stop(); }

bool NetworkManager::start(const NetworkMap& map, const std::string& ifaceName) {
    iface_ = ifaceName;

    // 1) Enregistrer tous les objectRefs connus dans le TagRegistry
    for (const auto& g : map.gse) {
        for (const auto& ref : g.objectRefs) registry_.registerObjectRef(g.ied, ref);
    }
    for (const auto& s : map.sv) {
        for (const auto& ref : s.objectRefs) registry_.registerObjectRef(s.ied, ref);
    }
    for (const auto& kv : map.rcbByIed) {
        for (const auto& r : kv.second) {
            for (const auto& ref : r.dsMembers) registry_.registerObjectRef(kv.first, ref);
        }
    }

    // 2) Démarrer GooseEngine & SvEngine
    goose_ = std::make_unique<GooseEngine>(bus_, store_);
    sv_    = std::make_unique<SvEngine>(bus_, store_);

    goose_->setMetaResolver([this](const std::string& ref){ return registry_.resolve(ref); });
    sv_->setMetaResolver(   [this](const std::string& ref){ return registry_.resolve(ref); });

    if (!goose_->start(iface_)) return false;
    if (!sv_->start(iface_))    return false;

    // 3) Abonnements GOOSE
    for (const auto& g : map.gse) {
        goose_->subscribe(/*goCbRef*/ "", g.mac, g.appId, g.objectRefs);
    }

    // 4) Abonnements SV
    // Note: pour SV il faut fournir des SvFieldSpec (type + offset).
    // Ici on fait un mapping simple: on expose toutes les valeurs comme UINT32 à offset consécutifs (exemple).
    // => À ajuster selon vos ASDUs (IEC 61850-9-2): construire ce vecteur depuis SCL si vous avez la table.
    for (const auto& s : map.sv) {
        std::vector<SvFieldSpec> fields;
        int offset = 0;
        for (const auto& ref : s.objectRefs) {
            fields.push_back(SvFieldSpec{ ref, SvType::UINT32, offset });
            offset += 4;
        }
        sv_->subscribe(s.mac, s.appId, fields);
    }

    // 5) Start one MMS session per endpoint, plus a single supervisor thread.
    //
    // The previous code called spawnMmsWorker() *before* inserting the context
    // into the map, and the worker did `auto& ctx = mmsByIed_[ep.ied]` then
    // dereferenced it. operator[] default-constructs, so that was a guaranteed
    // null dereference on the first endpoint, followed by the map assignment
    // destroying what the thread had captured.
    for (const auto& m : map.mms) {
        auto ctx = std::make_shared<MmsCtx>();
        ctx->session = std::make_unique<MmsSession>(m.ied, m.ip, m.port, bus_, store_);
        ctx->session->setMetaResolver(
            [this](const std::string& ref){ return registry_.resolve(ref); });
        if (auto it = map.rcbByIed.find(m.ied); it != map.rcbByIed.end())
            ctx->rcbs = it->second;
        mmsByIed_[m.ied] = std::move(ctx);
    }

    mmsSupervisor_ = std::thread([this]{ mmsSupervisorLoop(); });

    return true;
}

// Polls every IED, reconnecting with exponential backoff and (re)configuring
// the report control blocks once a connection is up.
void NetworkManager::mmsSupervisorLoop() {
    const auto backoffMin = std::chrono::milliseconds(500);
    std::map<std::string, std::chrono::milliseconds> backoff;

    for (;;) {
        bool stopping = false;
        for (auto& kv : mmsByIed_) {
            auto& ctx = kv.second;
            if (ctx->stop.load()) { stopping = true; break; }

            // Zero means "never backed off", i.e. use the base interval.
            auto& wait = backoff[kv.first];
            if (wait.count() == 0) wait = backoffMin;

            if (!ctx->session->isUp()) {
                bool up = false;
                {
                    std::lock_guard<std::mutex> lk(connMu_);
                    up = ctx->session->connect(3000);
                }
                if (up) {
                    // The report handler must already be installed before RptEna,
                    // otherwise reports arriving in that window are dropped.
                    ctx->session->enableReports(ctx->rcbs);
                    wait = backoffMin;
                } else {
                    // Cap the doubling in one type: milliseconds throughout,
                    // otherwise std::min(milliseconds, seconds) does not compile.
                    auto next = wait * 2;
                    if (next > std::chrono::milliseconds(5000)) next = std::chrono::milliseconds(5000);
                    wait = next;
                    if (interruptibleWait(next)) return;   // stop requested
                    continue;
                }
            }
        }
        if (stopping) return;
        if (interruptibleWait(std::chrono::milliseconds(1000))) return;
    }
}

bool NetworkManager::interruptibleWait(std::chrono::milliseconds d) {
    std::unique_lock<std::mutex> lk(sleepMu_);
    for (auto& kv : mmsByIed_)
        if (kv.second->stop.load()) return true;
    sleepCv_.wait_for(lk, d, [this]{
        for (auto& kv : mmsByIed_)
            if (kv.second->stop.load()) return true;
        return false;
    });
    return false;
}

void NetworkManager::stop() {
    stopReplay();

    for (auto& kv : mmsByIed_) kv.second->stop.store(true);
    sleepCv_.notify_all();
    if (mmsSupervisor_.joinable()) mmsSupervisor_.join();
    for (auto& kv : mmsByIed_) {
        std::lock_guard<std::mutex> lk(connMu_);
        if (kv.second->session) kv.second->session->disconnect();
    }
    mmsByIed_.clear();

    if (goose_) { goose_->stop(); goose_.reset(); }
    if (sv_)    { sv_->stop();    sv_.reset(); }
}

bool NetworkManager::selectOperate(const std::string& ied,
                                   const std::string& ctlObjectRef,
                                   int64_t value, int timeoutMs) {
    auto it = mmsByIed_.find(ied);
    if (it == mmsByIed_.end()) return false;
    auto& ctx = it->second;
    if (!ctx->session || !ctx->session->isUp()) return false;
    // Same lock the supervisor takes around connect(): libiec61850 does not
    // serialise the request/response pair across threads by itself.
    std::lock_guard<std::mutex> lk(connMu_);
    return cmd_.operateSBOw(ctx->session->getConnection(), ctlObjectRef, value, timeoutMs);
}

std::function<std::optional<TagId>(const std::string&)> NetworkManager::resolver() const {
    return [this](const std::string& ref){ return registry_.resolve(ref); };
}

bool NetworkManager::replayPcap(const std::string& pcap, double speed) {
    if (!replayer_) replayer_ = std::make_unique<PcapReplayer>();
    if (replayer_->isRunning()) return false;
    return replayer_->start(pcap, iface_, speed, nullptr);
}

void NetworkManager::stopReplay() {
    if (replayer_) replayer_->stop();
}

} // namespace network
