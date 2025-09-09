#pragma once
#include <QString>
#include <QList>
#include "voltageLevel.h"
#include "powerTransformer.h"

class Substation {
public:
    QString name;
    QString desc;
    QList<VoltageLevel> voltageLevels;

    //transformers
    QList<PowerTransformer> transformers;
    Substation() = default;
};
