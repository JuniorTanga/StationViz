// StationViz/core/network/PcapReplayer.cpp
#include "PcapReplayer.h"

#include <chrono>
#include <cstring>

// Replaying a capture onto a live interface needs libpcap on both sides (read
// the file, inject the frames). Without it the replayer is simply unavailable;
// GOOSE and SV reception do not depend on it. See docs/PLAN.md section 10.
#if defined(STATIONVIZ_NO_PCAP)
#  define STATIONVIZ_HAVE_PCAP 0
#elif defined(_WIN32)
#  include <pcap.h>
#  define STATIONVIZ_HAVE_PCAP 1
#else
#  include <pcap/pcap.h>
#  define STATIONVIZ_HAVE_PCAP 1
#endif

namespace network {

#if !STATIONVIZ_HAVE_PCAP

PcapReplayer::~PcapReplayer() = default;

bool PcapReplayer::start(const std::string&, const std::string&, double, LogFn log) {
    if (log) log("PcapReplayer unavailable: built without libpcap");
    return false;
}

void PcapReplayer::stop() {}
void PcapReplayer::run(std::string, std::string, double, LogFn) {}

#else

PcapReplayer::~PcapReplayer() { stop(); }

bool PcapReplayer::start(const std::string& pcapPath, const std::string& ifaceName,
                         double speed, LogFn log) {
    if (running_.load()) return false;
    stop_.store(false);
    th_ = std::thread(&PcapReplayer::run, this, pcapPath, ifaceName, speed, log);
    running_.store(true);
    return true;
}

void PcapReplayer::stop() {
    if (!running_.load()) return;
    stop_.store(true);
    if (th_.joinable()) th_.join();
    running_.store(false);
}

void PcapReplayer::run(std::string pcapPath, std::string ifaceName, double speed, LogFn log) {
    char errbuf[PCAP_ERRBUF_SIZE] = {0};

    pcap_t* pcap_reader = pcap_open_offline(pcapPath.c_str(), errbuf);
    if (!pcap_reader) {
        if (log) log(std::string("pcap_open_offline failed: ") + errbuf);
        running_.store(false);
        return;
    }

    pcap_t* pcap_writer = pcap_open_live(ifaceName.c_str(), 65535, 1, 1, errbuf);
    if (!pcap_writer) {
        if (log) log(std::string("pcap_open_live failed: ") + errbuf);
        pcap_close(pcap_reader);
        running_.store(false);
        return;
    }

    pcap_pkthdr* hdr = nullptr;
    const u_char* data = nullptr;

    // Replay timing is the *interval* between consecutive packets. The previous
    // version measured from the first packet, so each wait grew cumulatively and
    // the replay was progressively slower and asymptotically wrong.
    timeval prev_ts{0, 0};
    bool have_prev = false;

    while (!stop_.load()) {
        const int r = pcap_next_ex(pcap_reader, &hdr, &data);
        if (r == 1) {
            if (hdr->caplen >= 14) {
                const uint16_t ethertype =
                    static_cast<uint16_t>((data[12] << 8) | data[13]);
                if (ethertype == 0x88b8 || ethertype == 0x88ba) {
                    if (speed > 0 && have_prev) {
                        const double dt =
                            (hdr->ts.tv_sec - prev_ts.tv_sec) +
                            (hdr->ts.tv_usec - prev_ts.tv_usec) / 1e6;
                        if (dt > 0)
                            std::this_thread::sleep_for(
                                std::chrono::microseconds(
                                    static_cast<int64_t>(1e6 * dt / speed)));
                    }
                    prev_ts = hdr->ts;
                    have_prev = true;
                    pcap_inject(pcap_writer, data, hdr->caplen);
                }
            }
        } else if (r == 0) {
            continue;
        } else {
            break;   // EOF or error
        }
    }

    pcap_close(pcap_writer);
    pcap_close(pcap_reader);
    running_.store(false);
}

#endif  // STATIONVIZ_HAVE_PCAP

} // namespace network