#pragma once
#include <string>
#include <vector>

namespace sld {

enum class TopologyHint {
    Auto,
    SingleBus,
    DoubleBus,
    RingBus,
    BreakerAndHalf
};

struct HeuristicsConfig {
    // --- Bus detection
    int busDegreeThreshold {3};
    std::vector<std::string> busNameHints {"BUS","BUSBAR","BBS","BARRE","BAR"};

    // --- Feeder growth
    int feederMaxDepth {24};

    // kinds traversables en série (continuer à « pousser » le feeder)
    std::vector<int> seriesPassKinds; // remplie depuis map EquipmentKind → int

    // kinds de terminaison de feeder
    std::vector<int> endpointKinds;

    // --- High-level topology hint
    TopologyHint hint {TopologyHint::Auto};
};

} // namespace sld
