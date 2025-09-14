#include <iostream>
#include <QDebug>
#include "core/scl/SclManager.h"

int main(int argc, char **argv) {

    const std::string path = "test.scd";

    scl::SclManager mgr;
    auto st = mgr.loadScl(path);
    if (!st) {
        std::cerr << "Failed to load SCL: " << st.error().message << "";
        return 2;
    }

    std::cout << "=== Substations ===\n";
    mgr.printSubstations();
    std::cout << "=== IEDs ===\n";
    mgr.printIEDs();
    std::cout << "=== Communication ===\n";
    mgr.printCommunication();
    std::cout << "=== Topology (CE -> CN) ===\n";
    mgr.printTopology();

    std::cout << "=== JSON (Substations) ===\n" << mgr.toJsonSubstations() << "";
    std::cout << "=== JSON (Network) ===\n" << mgr.toJsonNetwork() << "";

    // Exemple: résolution d'un LNodeRef venu d'un CE
    if (auto ssRes = mgr.findSubstation("SS1")) {
        // rien de spécifique ici; juste démonstration d'appel
    }

    return 0;
}
