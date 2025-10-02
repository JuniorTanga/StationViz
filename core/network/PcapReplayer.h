// StationViz/core/network/PcapReplayer.h
#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace network {

// Rejoue un .pcap sur une interface (GOOSE=0x88b8, SV=0x88ba).
// L’injection sur la même interface que GooseEngine/SvEngine permet à ces moteurs de recevoir les trames.
class PcapReplayer {
public:
    using LogFn = std::function<void(const std::string&)>;

    ~PcapReplayer();

    // speed = 1.0 temps réel; 0 = aussi vite que possible; >1 accéléré
    bool start(const std::string& pcapPath, const std::string& ifaceName, double speed = 1.0, LogFn log = {});
    void stop();
    bool isRunning() const { return running_.load(); }

private:
    void run(std::string pcapPath, std::string ifaceName, double speed, LogFn log);

private:
    std::thread th_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
};

} // namespace network
