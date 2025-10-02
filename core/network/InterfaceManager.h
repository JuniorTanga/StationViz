#pragma once
#include <string>
#include <vector>
#include <optional>

namespace network {

// Outils pour choisir une interface et vérifier les permissions nécessaires
class InterfaceManager {
public:
    // Liste les interfaces réseau disponibles (via pcap)
    static std::vector<std::string> listInterfaces();

    // Valide qu'on peut capturer/injecter sur cette interface (pcap_open_live)
    static bool validateInterface(const std::string& ifaceName, std::string* whyNot = nullptr);

    // Détermine si le processus possède les privilèges raw sockets (Linux) / pcap opérationnel (Windows)
    static bool hasRawSocketPrivilege(std::string* whyNot = nullptr);

    // Retourne ifaceName si valide, sinon la première interface utilisable (ou std::nullopt)
    static std::optional<std::string> chooseInterfacePrefer(const std::string& preferred);
};

} // namespace network
