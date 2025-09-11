#pragma once
#include <QString>

class Terminal {
public:
    QString name;             // nom du terminal (facultatif)
    QString cNodeName;        // nom du ConnectivityNode local
    QString connectivityNode; // identifiant complet du nœud

    Terminal() = default;
};
