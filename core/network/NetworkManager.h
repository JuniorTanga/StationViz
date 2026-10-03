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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
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
    EventBus& bus_;
    StateStore& store_;

    TagRegistry registry_;

    // Engines
    std::unique_ptr<GooseEngine> goose_;
    std::unique_ptr<SvEngine>    sv_;
    std::unique_ptr<PcapReplayer> replayer_;

    // MMS sessions.
    //
    // Held by shared_ptr and owned by a single supervisor thread rather than by
    // one thread per IED: IedConnection_create is already thread mode, so the
    // old design paid two threads per IED just to poll and reconnect. The
    // shared_ptr also fixes a null dereference, see NetworkManager.cpp.
    // MmsSession holds reference members, a mutex and an atomic, so it is not
    // assignable; it is held by pointer rather than by value.
    struct MmsCtx {
        std::unique_ptr<MmsSession> session;
        std::atomic<bool> stop{false};
        std::vector<RcbConfig> rcbs;
    };
    std::unordered_map<std::string, std::shared_ptr<MmsCtx>> mmsByIed_; // key = IED

    // Serialises every use of an IedConnection. libiec61850 guards individual
    // structures but not the request/response pair, so a connect() in the
    // supervisor racing an operate() from the GUI could corrupt the association.
    mutable std::mutex connMu_;

    // One supervisor for all IEDs, woken by cv_ so stop() is immediate rather
    // than blocking for up to one backoff interval per IED in turn.
    std::thread mmsSupervisor_;
    std::mutex sleepMu_;
    std::condition_variable sleepCv_;

    void mmsSupervisorLoop();
    // Waits up to `d`, returning false if stop was requested.
    bool interruptibleWait(std::chrono::milliseconds d);

    // iface
    std::string iface_;

    // commandes
    CommandEngine cmd_;
};

} // namespace network
