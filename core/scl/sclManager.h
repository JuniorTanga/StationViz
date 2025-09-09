#pragma once

#include <QString>
#include <QList>
#include "ied.h"
#include "substation.h"


/**
 * @brief SclManager est la façade qui gère le chargement et l’accès
 * aux données extraites d’un fichier SCL (SCD, ICD, etc.)
 */
class SclManager {
public:
    SclManager() = default;

    /**
     * @brief load and parse the scl file
     * @param filePath : path to scl file
     * @return true if succeed, false if failed
     */
    bool loadSclFile(const QString& filePath);

    QList<Substation> getSubstations() const;
    QList<Ied> getIeds() const;

    void printIeds() const;
    void printSubstations() const;

private:
    QList<Substation> m_substations;
    QList<Ied> m_ieds;
};
