#pragma once
#include "SldTypes.h"

namespace stationviz::sld {

class SldGraph {
public:
    Scene scene;

    const Node* findNode(const QString& id) const {
        for (const auto& n : scene.nodes) if (n.id.id == id) return &n;
        return nullptr;
    }
};

} // namespace stationviz::sld
