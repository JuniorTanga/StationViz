#pragma once
#include "SldTypes.hpp"

namespace stationviz::sld {

class SldLayout {
public:
    static void autoRoute(Scene& scene) {
        // Extension point for routing and layout refinements.
        (void)scene; // no-op for v1
    }
};

} // namespace stationviz::sld
