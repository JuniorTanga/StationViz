#pragma once
#include <QString>
#include <QList>
#include "bay.h"

class VoltageLevel {
public:
    QString name;
    QString desc;

    double nominalVoltage = 0.0;   // en volts
    QString unit;                  // ex : "V"
    QString multiplier;            // ex : "k"
    int nomFreq = 0;               // fréquence nominale en Hz
    int numPhases = 0;             // nombre de phases

    QList<Bay> bays;

    VoltageLevel() = default;
};
