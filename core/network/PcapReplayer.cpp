// StationViz/core/network/PcapReplayer.cpp
#include "PcapReplayer.h"
#include <chrono>
#include <cstring>

#if defined(_WIN32)
#  include <pcap.h>
#else
#  include <pcap/pcap.h>
#endif

namespace network {

PcapReplayer::~PcapReplayer() { stop(); }

bool PcapReplayer::start(const std::string& pcapPath, const std::string& ifaceName, double speed, LogFn log) {
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

    // lecture offline
    pcap_t* pcap_reader = pcap_open_offline(pcapPath.c_str(), errbuf);
    if (!pcap_reader) {
        if (log) log(std::string("pcap_open_offline failed: ") + errbuf);
        running_.store(false);
        return;
    }

    // injection live
    // snaplen 65535, promisc 1, timeout 1ms
    pcap_t* pcap_writer = pcap_open_live(ifaceName.c_str(), 65535, 1, 1, errbuf);
    if (!pcap_writer) {
        if (log) log(std::string("pcap_open_live failed: ") + errbuf);
        pcap_close(pcap_reader);
        running_.store(false);
        return;
    }

    pcap_pkthdr* hdr = nullptr;
    const u_char* data = nullptr;

    timeval first_ts{0,0};
    bool first = true;

    while (!stop_.load()) {
        int r = pcap_next_ex(pcap_reader, &hdr, &data);
        if (r == 1) {
            // Filter EtherType: 0x88b8 (GOOSE), 0x88ba (SV)
            if (hdr->caplen >= 14) {
                uint16_t ethertype = (uint16_t)((data[12] << 8) | data[13]);
                if (ethertype == 0x88b8 || ethertype == 0x88ba) {
                    // timing
                    if (speed > 0 && !first) {
                        double dt_src = (hdr->ts.tv_sec - first_ts.tv_sec) + (hdr->ts.tv_usec - first_ts.tv_usec)/1e6;
                        if (dt_src > 0) std::this_thread::sleep_for(std::chrono::microseconds((int64_t)(1e6 * dt_src / speed)));
                    }
                    if (first) {
                        first_ts = hdr->ts;
                        first = false;
                    }
                    // inject
                    int rc = pcap_inject(pcap_writer, data, hdr->caplen);
                    (void)rc;
                }
            }
        }
        else if (r == 0) {
            // timeout (offline: rare)
            continue;
        }
        else {
            // EOF ou erreur
            break;
        }
    }

    pcap_close(pcap_writer);
    pcap_close(pcap_reader);
    running_.store(false);
}

} // namespace network
