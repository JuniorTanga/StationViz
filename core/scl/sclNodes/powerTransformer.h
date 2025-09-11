#pragma once
#include <QString>
#include <QList>
#include "transformerWinding.h"

class PowerTransformer {
public:
    QString name;
    QString desc;

    QList<TransformerWinding> windings;

    PowerTransformer() = default;
};
