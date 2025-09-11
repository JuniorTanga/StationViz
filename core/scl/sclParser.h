#pragma once

#include <QString>
#include <QList>
#include "sclNodes/substation.h"
#include "sclNodes/ied.h"

// pugixml
#include "pugixml/pugixml.hpp"

/**
 * @brief La classe SclParser est responsable de :
 *  - Charger un fichier SCL (.scd, .icd, .cid…)
 *  - Parser et extraire les informations de base :
 *      * Substations, VoltageLevels, Bays, Equipements
 *      * IEDs et leurs attributs
 */
class SclParser {
public:
    /**
     * @brief Constructeur du parser
     * @param filePath chemin du fichier SCL à parser
     */
    explicit SclParser(const QString& filePath);

    /**
     * @brief Charge le fichier XML dans m_doc (via pugixml)
     * @return true si succès, false sinon
     */
    bool loadFile();

    /**
     * @brief Parse les sous-stations <Substation>
     * @return Liste de Substation
     */
    QList<Substation> parseSubstations();

    /**
     * @brief Parse les IEDs <IED>
     * @return Liste de Ied
     */
    QList<Ied> parseIeds();

private:
    QString m_filePath;       ///< chemin vers le fichier SCL
    pugi::xml_document m_doc; ///< document XML chargé
};
