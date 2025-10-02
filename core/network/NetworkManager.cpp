// StationViz/core/network/NetworkManager.cpp
#include "NetworkManager.h"
#include <chrono>

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

    // 5) Démarrer les sessions MMS + workers de reconnexion
    for (const auto& m : map.mms) {
        std::vector<RcbConfig> rcbs;
        if (auto it = map.rcbByIed.find(m.ied); it != map.rcbByIed.end()) rcbs = it->second;
        auto ctx = std::make_unique<MmsCtx>();
        ctx->session = std::make_unique<MmsSession>(m.ied, m.ip, m.port, bus_, store_);
        ctx->session->setMetaResolver([this](const std::string& ref){ return registry_.resolve(ref); });
        ctx->rcbs = rcbs;
        spawnMmsWorker(m, ctx->rcbs);
        mmsByIed_[m.ied] = std::move(ctx);
    }

    return true;
}

void NetworkManager::stop() {
    // stop replay
    stopReplay();

    // stop MMS workers
    for (auto& kv : mmsByIed_) {
        auto& ctx = kv.second;
        ctx->stop.store(true);
        if (ctx->th.joinable()) ctx->th.join();
        if (ctx->session) ctx->session->disconnect();
    }
    mmsByIed_.clear();

    if (goose_) { goose_->stop(); goose_.reset(); }
    if (sv_)    { sv_->stop();    sv_.reset(); }
}

void NetworkManager::spawnMmsWorker(const EndpointMms& ep, const std::vector<RcbConfig>& rcbs) {
    auto& ctx = mmsByIed_[ep.ied];
    ctx->th = std::thread([this, ep, rcbs, ctxPtr=ctx.get()](){
        const auto backoffMin = std::chrono::milliseconds(500);
        const auto backoffMax = std::chrono::seconds(5);
        auto backoff = backoffMin;

        while (!ctxPtr->stop.load()) {
            if (!ctxPtr->session->isUp()) {
                if (ctxPtr->session->connect(3000)) {
                    // (Re)configure RCB + GI
                    ctxPtr->session->enableReports(rcbs);
                    backoff = backoffMin; // reset
                } else {
                    std::this_thread::sleep_for(backoff);
                    backoff = std::min(backoff * 2, backoffMax);
                    continue;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

bool NetworkManager::selectOperate(const std::string& ied, const std::string& ctlObjectRef, int64_t value, int timeoutMs) {
    auto it = mmsByIed_.find(ied);
    if (it == mmsByIed_.end()) return false;
    auto& sess = it->second->session;
    if (!sess || !sess->isUp()) return false;
    return cmd_.operateSBOw(it->second->session->getConnection(), ctlObjectRef, value, timeoutMs);
    );
}

// Pour ne pas exposer les entrailles de MmsSession, on peut surcharger MmsSession
// avec un getter 'IedConnection native()' si nécessaire.
// Ici, on modifie plutôt MmsSession (ajoute getConnection()):

// ==> À AJOUTER dans MmsSession.h (public):
// IedConnection getConnection() const { return con_; }

// ==> Puis on appelle:
bool NetworkManager::selectOperate(const std::string& ied, const std::string& ctlObjectRef, int64_t value, int timeoutMs);

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
