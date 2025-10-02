#include "InterfaceManager.h"

#if defined(_WIN32)
  #include <pcap.h>
#else
  #include <pcap/pcap.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/if_ether.h>
  #include <linux/if_packet.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <errno.h>
#endif

#include <cstring>

namespace network {

std::vector<std::string> InterfaceManager::listInterfaces() {
    std::vector<std::string> out;

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
}

bool InterfaceManager::validateInterface(const std::string& ifaceName, std::string* whyNot) {
    char errbuf[PCAP_ERRBUF_SIZE] = {0};
    pcap_t* h = pcap_open_live(ifaceName.c_str(), 65535, 1, 1, errbuf);
    if (!h) {
        if (whyNot) *whyNot = errbuf;
        return false;
    }
    pcap_close(h);
    return true;
}

bool InterfaceManager::hasRawSocketPrivilege(std::string* whyNot) {
#if defined(_WIN32)
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
