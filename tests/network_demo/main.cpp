#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <variant>

#include "scl/SclManager.h"
#include "network/NetworkMapBuilder.h"
#include "network/NetworkManager.h"
#include "network/InterfaceManager.h"
#include "network/EventBus.h"
#include "network/StateStore.h"
#include "network/NetworkTypes.h"

using namespace network;

static std::atomic<bool> g_stop{false};
static void onSig(int){ g_stop.store(true); }

static std::string toString(const TagValue& v) {
    if (std::holds_alternative<std::monostate>(v)) return "null";
    if (auto p = std::get_if<bool>(&v)) return *p ? "true" : "false";
    if (auto p = std::get_if<int64_t>(&v)) return std::to_string(*p);
    if (auto p = std::get_if<double>(&v)) return std::to_string(*p);
    if (auto p = std::get_if<std::string>(&v)) return *p;
    if (auto p = std::get_if<Quality>(&v)) return "Quality(" + std::to_string(p->q) + ")";
    if (auto p = std::get_if<Timestamp>(&v)) return "Ts(ms)=" + std::to_string(p->epochMs);
    if (auto p = std::get_if<VectorValue>(&v)) {
        std::string s = "[";
        for (size_t i=0;i<p->data.size();++i) { s += std::to_string(p->data[i]); if (i+1<p->data.size()) s += ","; }
        s += "]";
        return s;
    }
    if (auto p = std::get_if<ByteString>(&v)) return "Octets(" + std::to_string(p->size()) + ")";
    return "?";
}

int main(int argc, char** argv) {
    std::string sclPath;
    std::string iface;
    std::string pcapPath;
    bool testMode = false;

    // parse args (ultra simple)
    for (int i=1;i<argc;++i) {
        std::string a = argv[i];
        auto next = [&](std::string& out){ if (i+1<argc) out = argv[++i]; };
        if (a == "--scl") next(sclPath);
        else if (a == "--iface") next(iface);
        else if (a == "--pcap") next(pcapPath);
        else if (a == "--test") testMode = true;
        else if (a == "-h" || a == "--help") {
            std::cout << "Usage: " << argv[0] << " --scl <file.icd/scd> [--iface eth0] [--pcap capture.pcap] [--test]\n";
            return 0;
        }
    }

    if (sclPath.empty()) {
        std::cerr << "Missing --scl <file>\n";
        return 1;
    }

    std::string why;
    if (!InterfaceManager::hasRawSocketPrivilege(&why)) {
        std::cerr << "[WARN] Raw socket/pcap privilege check failed: " << why << "\n";
        std::cerr << "       (Linux: sudo or setcap cap_net_raw+ep on the binary; Windows: install NPcap)\n";
    }

    auto chosen = InterfaceManager::chooseInterfacePrefer(iface);
    if (!chosen) {
        std::cerr << "No usable interface found.\n";
        return 2;
    }
    iface = *chosen;
    std::cout << "[INFO] Using interface: " << iface << "\n";

    // Charge SCL
    scl::SclManager sm;
    if (!sm.loadFromFile(sclPath)) {
        std::cerr << "Failed to load SCL: " << sclPath << "\n";
        return 3;
    }

    // Build NetworkMap
    NetworkMap map = NetworkMapBuilder::fromScl(sm);

    // Infra runtime
    EventBus bus;
    StateStore store;
    NetworkManager nm(bus, store);

    // Log des updates
    bus.subscribe([&](TagId id, const TagValue& v){
        std::cout << "[TagChanged] id=" << id.value << " value=" << toString(v) << "\n";
    });

    // Mode test (commandes SBOw)
    nm.setTestMode(testMode);

    // Start
    if (!nm.start(map, iface)) {
        std::cerr << "NetworkManager start failed\n";
        return 4;
    }

    // Option: rejouer un PCAP pour tester GOOSE/SV sans réseau
    if (!pcapPath.empty()) {
        if (!nm.replayPcap(pcapPath, 1.0)) {
            std::cerr << "[WARN] Failed to start PCAP replay\n";
        } else {
            std::cout << "[INFO] Replaying PCAP: " << pcapPath << "\n";
        }
    }

    // Loop jusqu'à Ctrl+C
    std::signal(SIGINT, onSig);
#if defined(SIGTERM)
    std::signal(SIGTERM, onSig);
#endif
    std::cout << "[INFO] Running. Press Ctrl+C to stop...\n";

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    nm.stop();
    std::cout << "[INFO] Stopped.\n";
    return 0;
}
