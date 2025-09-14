// ============================================================================
// StationViz/core/scl/sclNodes/scl_common.h
// ============================================================================
#pragma once
#include <QString>


namespace stationviz::scl {


struct SclId { QString id; QString mrid; };


enum class CeType { CBR, DIS, BUSBAR, VTR, PTR, VSC, Unknown };


enum class Unit { V, A, Hz, VA, W, Var, None };


enum class Mult { p=-12, n=-9, u=-6, m=-3, none=0, k=3, M=6, G=9 };


struct Quantity {
double si = 0.0; // valeur normalisée SI
Unit unit = Unit::None; // unité déclarée
Mult mult = Mult::none; // multiplicateur tel que SCL
double original = 0.0; // valeur telle que lue
};


inline double applyMultiplier(double value, Mult m) {
const int exp = static_cast<int>(m);
double scale = 1.0;
for (int i=0;i< (exp<0?-exp:exp); ++i) scale *= 10.0;
return exp<0 ? value/scale : value*scale;
}


}
