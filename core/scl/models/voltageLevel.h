#pragma once
#include <QString>
#include <QList>
#include "bay.h"

class VoltageLevel {
public:
    QString name;
    QString desc;

    double voltage = 0.0;           //Voltage in volt
    double nominalVoltage = 0.0;    //as stored in the scl file
    QString unit;                  // ex : "V"
    QString multiplier;            // ex : "k"
    int nomFreq = 0;               // fréquence nominale en Hz
    int numPhases = 0;             // nombre de phases

    QList<Bay> bays;

    VoltageLevel() = default;
};
