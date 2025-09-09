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
    QList<LogicalNode> lNodes;

    QList<ConductingEquipment> equipments;
    QList<ConnectivityNode> connectivityNodes;

    Bay() = default;
};
