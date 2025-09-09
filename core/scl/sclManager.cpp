#include "sclParser.h"
#include "sclManager.h"
#include <QDebug>

bool SclManager::loadSclFile(const QString& filePath) {
    SclParser parser(filePath);

    if (!parser.loadFile()) {
        qWarning() << "Impossible de charger le fichier SCL:" << filePath;
        return false;
    }

    m_substations = parser.parseSubstations();
    m_ieds = parser.parseIeds();

    if (m_substations.isEmpty() && m_ieds.isEmpty()) {
        qWarning() << "Fichier SCL chargé mais aucune donnée trouvée.";
        return false;
    }

    qDebug() << "Chargement réussi :"
             << m_substations.size() << "sous-stations,"
             << m_ieds.size() << "IEDs.";
    return true;
}

QList<Substation> SclManager::getSubstations() const {
    return m_substations;
}

QList<Ied> SclManager::getIeds() const {
    return m_ieds;
}

void SclManager::printIeds() const
{
    for (const auto &ied : m_ieds) {
        qDebug().noquote() << "IED:" << ied.name
                 << "Type:" << ied.type
                 << "Manufacturer:" << ied.manufacturer
                 << "ConfigVersion:" << ied.configVersion
                 << "Owner:" << ied.owner;

        for (const auto &ap : ied.accessPoints) {
            qDebug().noquote() << "  AccessPoint:" << ap.name;
        }
    }
}

void SclManager::printSubstations() const
{
    qDebug().noquote() << "=== Liste des Substations ===";
    if (m_substations.isEmpty()) {
        qDebug().noquote() << "  Aucune substation trouvée.";
        return;
    }
    for (const auto& sub : m_substations) {
        qDebug().noquote() << "Substation:" << sub.name << "| Desc:" << sub.desc;

        // VoltageLevels
        for (const auto& vl : sub.voltageLevels) {
            qDebug().noquote() << "  VoltageLevel:" << vl.name
                               << "| Vnom:" << vl.nominalVoltage
                               << vl.multiplier + vl.unit
                               << "| Freq:" << vl.nomFreq << "Hz"
                               << "| Phases:" << vl.numPhases;

            // Bays
            for (const auto& bay : vl.bays) {
                qDebug().noquote() << "    Bay:" << bay.name
                                   << "| Desc:" << bay.desc;

                // Equipements
                for (const auto& eq : bay.equipments) {
                    qDebug().noquote() << "      Equipment:" << eq.type
                                       << "| Name:" << eq.name;
                }
            }
        }

        // Transformers
        for (const auto& tr : sub.transformers) {
            qDebug().noquote() << "  Transformer:" << tr.name
                               << "| Desc:" << tr.desc;
        }
    }
}
