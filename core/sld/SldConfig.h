#pragma once
// EquipmentKind is declared in SldTypes.h, which includes this file, so it
// cannot be included here without a circular dependency. The default
// initialisers that depend on it live in sldConfigDefaults() below, which
// SldBuilder calls after both headers are parsed.
#include <string>
#include <vector>
#include <initializer_list>

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
    // "BB" was present in the pre-Boost implementation and was lost in the
    // rewrite, which is why BBS-named and BB2-named busbars stopped being
    // recognised (and took their explicit couplers with them).
    std::vector<std::string> busNameHints {"BUS","BUSBAR","BBS","BB","BARRE","BAR"};

    // --- Feeder growth
    int feederMaxDepth {24};

    // kinds traversables en série (continuer à « pousser » le feeder)
    std::vector<int> seriesPassKinds;

    // kinds de terminaison de feeder
    std::vector<int> endpointKinds;

    // --- High-level topology hint
    TopologyHint hint {TopologyHint::Auto};
};

} // namespace sld
