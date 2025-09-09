#pragma once
#include <QString>
#include <QList>
#include "logicalNode.h"

#pragma once
#include <QString>
#include <QList>

class AccessPoint {
public:
    QString name;
    //QString servers;
};

class Ied {
public:
    QString name;
    QString desc;
    QString type;
    QString manufacturer;
    QString configVersion;
    QString owner;

    QList<AccessPoint> accessPoints;

    // Pour extension future : AccessPoint, LogicalDevice, etc.
    Ied() = default;
};
