#pragma once

#include <QString>
#include <QList>
#include "sclNodes/ied.h"
#include "sclNodes/substation.h"
#include "graph.h"


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

    void buildBayGraphs();
    const Graph& getBayGraph(const Bay* bay) const;
    const QMap<const Bay*, Graph>& getBayGraphs() const;

    void printIeds() const;
    void printSubstations() const;
    void printBays() const;

private:
    QList<Substation> m_substations;
    QMap<QString, Graph> m_bayGraphs;
    QList<Ied> m_ieds;
};
