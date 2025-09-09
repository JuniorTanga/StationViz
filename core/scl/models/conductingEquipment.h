#pragma once
#include <QString>
#include <QList>
#include "terminal.h"

class ConductingEquipment {
public:
    QString name;
    QString desc;
    QString type;  // CBR, DIS, VTR, CTR...

    QList<Terminal> terminals;

    ConductingEquipment() = default;
};
