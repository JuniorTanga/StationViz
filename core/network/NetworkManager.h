// StationViz/core/network/NetworkManager.h
#pragma once
#include "NetworkTypes.h"
#include "EventBus.h"
#include "StateStore.h"
#include "TagRegistry.h"
#include "MmsSession.h"
#include "GooseEngine.h"
#include "SvEngine.h"
#include "CommandEngine.h"
#include "PcapReplayer.h"

#include <string>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <memory>

namespace network {

class NetworkManager {
public:
    NetworkManager(EventBus& bus, StateStore& store);
    ~NetworkManager();

    // Démarrage global : lance MMS, GOOSE et SV selon NetworkMap
    bool start(const NetworkMap& map, const std::string& ifaceName);

    // Arrêt propre (sessions, threads, engines)
    void stop();

    // Commandes (protégées par Mode ReadOnly/Test)
    void setTestMode(bool enabled) { cmd_.setMode(enabled ? CommandEngine::Mode::Test
                                                          : CommandEngine::Mode::ReadOnly); }

    // Sélection/Opération (SBOw) sur un objet contrôle
    bool selectOperate(const std::string& ied, const std::string& ctlObjectRef, int64_t value, int timeoutMs);

    // Expose TagRegistry resolver aux moteurs
    std::function<std::optional<TagId>(const std::string&)> resolver() const;

    // Simulation
    bool replayPcap(const std::string& pcap, double speed=1.0);
    void stopReplay();

private:
    void spawnMmsWorker(const EndpointMms& ep, const std::vector<RcbConfig>& rcbs);

private:
    EventBus& bus_;
    StateStore& store_;

    TagRegistry registry_;

    // Engines
    std::unique_ptr<GooseEngine> goose_;
    std::unique_ptr<SvEngine>    sv_;
    std::unique_ptr<PcapReplayer> replayer_;

    // MMS sessions & threads
    struct MmsCtx {
        std::unique_ptr<MmsSession> session;
        std::thread th;
        std::atomic<bool> stop{false};
        std::vector<RcbConfig> rcbs;
    };
    std::unordered_map<std::string, std::unique_ptr<MmsCtx>> mmsByIed_; // key = IED

    // iface
    std::string iface_;

    // commandes
    CommandEngine cmd_;
};

} // namespace network
