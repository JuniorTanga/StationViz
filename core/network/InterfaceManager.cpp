#include "InterfaceManager.h"

// libpcap is only needed to enumerate interfaces and to sanity-check one with
// pcap_open_live. GOOSE and SV on Linux go through libiec61850's own AF_PACKET
// HAL and do not need it, so the build must not depend on it.
#if defined(STATIONVIZ_NO_PCAP)
  #define STATIONVIZ_HAVE_PCAP 0
#elif defined(_WIN32)
  #include <pcap.h>
  #define STATIONVIZ_HAVE_PCAP 1
#else
  #include <pcap/pcap.h>
  #define STATIONVIZ_HAVE_PCAP 1
#endif

#if !defined(_WIN32)
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/if_ether.h>
  #include <linux/if_packet.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <errno.h>
#endif

#include <cstring>
#include <string>
#if !defined(_WIN32)
  #include <dirent.h>
#endif

namespace network {

std::vector<std::string> InterfaceManager::listInterfaces() {
    std::vector<std::string> out;
#if !STATIONVIZ_HAVE_PCAP
    // Fall back to the kernel: /sys/class/net is always present on Linux.
    if (DIR* d = ::opendir("/sys/class/net")) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            out.push_back(n);
        }
        ::closedir(d);
    }
    return out;
#else
    char errbuf[PCAP_ERRBUF_SIZE] = {0};
    pcap_if_t* alldevs = nullptr;
    if (pcap_findalldevs(&alldevs, errbuf) == -1 || !alldevs) {
        return out;
    }
    for (pcap_if_t* d = alldevs; d; d = d->next) {
        if (d->name) out.emplace_back(d->name);
    }
    pcap_freealldevs(alldevs);
    return out;
#endif
}

bool InterfaceManager::validateInterface(const std::string& ifaceName, std::string* whyNot) {
#if !STATIONVIZ_HAVE_PCAP
    // Without libpcap, check the interface exists and that a raw socket opens on
    // it, which is what GOOSE and SV will actually require.
    (void)ifaceName;
    std::string ignored;
    return hasRawSocketPrivilege(whyNot ? whyNot : &ignored);
#else
    char errbuf[PCAP_ERRBUF_SIZE] = {0};
    pcap_t* h = pcap_open_live(ifaceName.c_str(), 65535, 1, 1, errbuf);
    if (!h) {
        if (whyNot) *whyNot = errbuf;
        return false;
    }
    pcap_close(h);
    return true;
#endif
}

bool InterfaceManager::hasRawSocketPrivilege(std::string* whyNot) {
#if defined(_WIN32) && STATIONVIZ_HAVE_PCAP
    // Sur Windows, la présence/usage de NPcap via pcap_open_live est l’indicateur
    char errbuf[PCAP_ERRBUF_SIZE] = {0};
    pcap_if_t* alldevs = nullptr;
    if (pcap_findalldevs(&alldevs, errbuf) == -1 || !alldevs) {
        if (whyNot) *whyNot = errbuf;
        return false;
    }
    pcap_freealldevs(alldevs);
    return true;
#else
    // Linux : test d’ouverture d’un RAW socket (requiert root ou CAP_NET_RAW)
    int fd = ::socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        if (whyNot) *whyNot = std::string("socket(AF_PACKET,SOCK_RAW) failed: ") + std::strerror(errno);
        return false;
    }
    ::close(fd);
    return true;
#endif
}

std::optional<std::string> InterfaceManager::chooseInterfacePrefer(const std::string& preferred) {
    std::string why;
    if (!preferred.empty() && validateInterface(preferred, &why)) return preferred;

    auto all = listInterfaces();
    for (const auto& n : all) {
        if (validateInterface(n)) return n;
    }
    return std::nullopt;
}

} // namespace network
