#pragma once
#include <QString>
#include <QList>
#include "conductingEquipment.h"
#include "connectivityNode.h"
#include "logicalNode.h"

class Bay {
public:
    QString name;
    QString desc;

    QList<ConductingEquipment> equipments; //CBRs , DIS ...
    QList<ConnectivityNode> connectivityNodes;

    QList<LogicalNode> lNodes; //A revoir après

    Bay() = default;   
};
