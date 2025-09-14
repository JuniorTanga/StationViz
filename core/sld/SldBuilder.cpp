#include "SldBuilder.h"
#include "sclNodes/substation.h" // core/scl/
#include "sclNodes/voltage_level.h" // core/scl/
#include "sclNodes/bay.h" // core/scl/
#include "sclNodes/conducting_equipment.h" // core/scl/

using namespace stationviz::sld;
using namespace stationviz::scl;

static NodeKind mapCe(const CeType t) {
    switch (t) {
        case CeType::CBR: return NodeKind::Breaker;
        case CeType::DIS: return NodeKind::Disconnector;
        default: return NodeKind::Unknown;
    }
}

SldBuilder::SldBuilder(std::shared_ptr<stationviz::scl::SclManager> mgr, QObject* parent)
: QObject(parent), mgr_(std::move(mgr)) {}

SldGraph SldBuilder::build() {
    SldGraph g;

    const auto& sList = mgr_->substations();
    if (sList.isEmpty()) return g;

    const auto& S = sList.first();

    // Layout constants (px)
    const double MARGIN = 40;
    const double VL_SPACING_Y = 220;    // distance entre niveaux
    const double BAY_SPACING_X = 220;   // distance entre bays
    const double BUS_LEN = 1800;        // longueur bus par défaut

    double currentY = MARGIN;
    double maxWidth = BUS_LEN + 2*MARGIN;

    for (const auto& VL : S.voltageLevels) {
        // Busbar node
        Node bus;
        bus.id.id = S.name + "/" + VL.name + "/BUS";
        bus.kind = NodeKind::Busbar;
        bus.label = VL.name;
        bus.pos = {MARGIN, currentY};
        bus.style = {BUS_LEN, 8, false};
        g.scene.nodes.push_back(bus);

        // Level label
        Node lab;
        lab.id.id = S.name + "/" + VL.name + "/LBL";
        lab.kind = NodeKind::VoltageLevelLabel;
        lab.label = VL.name;
        lab.pos = {MARGIN - 30, currentY - 24};
        lab.style = {120, 24, false};
        g.scene.nodes.push_back(lab);

        // Bays
        double x = MARGIN + 60;
        for (const auto& B : VL.bays) {
            Node bay;
            bay.id.id = S.name + "/" + VL.name + "/" + B.name;
            bay.kind = NodeKind::Bay;
            bay.label = B.name;
            bay.pos = {x, currentY};
            bay.style = {1,1,false};
            g.scene.nodes.push_back(bay);

            // Equipments below the bus
            double yDown = currentY + 60;
            for (const auto& CE : B.equipments) {
                Node n;
                n.id.id = bay.id.id + "/" + CE.name;
                n.kind = mapCe(CE.type);
                n.label = CE.name;
                n.style = {24, 24, false};
                n.pos = {x, yDown};
                g.scene.nodes.push_back(n);

                // Edge vertical from bus to equipment
                Edge e; e.from = bus.id.id; e.to = n.id.id;
                e.polyline = { {x, currentY}, {x, yDown - n.style.h/2} };
                g.scene.edges.push_back(e);

                yDown += 60;
            }

            x += BAY_SPACING_X;
            if (x + MARGIN > maxWidth) maxWidth = x + MARGIN;
        }

        currentY += VL_SPACING_Y;
    }

    g.scene.bounds = {0,0, maxWidth, currentY + MARGIN};
    return g;
}
