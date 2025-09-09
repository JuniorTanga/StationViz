#pragma once
#include <QString>

class ConnectivityNode {
public:
    QString name;      // ex : "L1"
    QString pathName;  // ex : "S1 380kV/E1/A"

    ConnectivityNode() = default;
};
