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
void SclManager::buildBayGraphs() {
    m_bayGraphs.clear();

    for (const auto& sub : m_substations) {
        for (const auto& vl : sub.voltageLevels) {
            for (const auto& bay : vl.bays) {
                Graph graph;

                // === Ajouter les ConnectivityNodes comme noeuds ===
                for (const auto& cn : bay.connectivityNodes) {
                    graph.addNode(cn.pathName, cn.name);
                }

                // === Ajouter les ConductingEquipments comme noeuds + arêtes ===
                for (const auto& eq : bay.equipments) {
                    QString eqId = bay.name + "/" + eq.name; // identifiant unique
                    graph.addNode(eqId, eq.name);

                    if (eq.terminals.size() == 2) {
                        QString n1 = eq.terminals[0].connectivityNode;
                        QString n2 = eq.terminals[1].connectivityNode;

                        // relier les CN au node équipement
                        graph.addEdge(eqId, n1, eq.name, eq.type);
                        graph.addEdge(eqId, n2, eq.name, eq.type);
                    }
                    else if (eq.terminals.size() == 1) {
                        QString n1 = eq.terminals[0].connectivityNode;
                        graph.addEdge(eqId, n1, eq.name, eq.type);
                    }
                    else {
                        qWarning() << "Equipement" << eq.name
                                   << "dans Bay" << bay.name
                                   << "a un nombre inattendu de terminaux:"
                                   << eq.terminals.size();
                    }
                }

                QString bayId = sub.name + "/" + vl.name + "/" + bay.name;
                m_bayGraphs.insert(bayId, graph);
            }
        }
    }
}

void SclManager::printBays() const {
    if (m_bayGraphs.isEmpty()) {
        qWarning() << "Aucun graphe de Bay disponible. As-tu appelé buildBayGraphs() ?";
        return;
    }

    for (auto it = m_bayGraphs.begin(); it != m_bayGraphs.end(); ++it) {
        const QString& bayId = it.key();
        const Graph& graph = it.value();

        qDebug() << "\n=== Bay:" << bayId << "===";

        auto leaves = graph.getLeaves();
        if (leaves.isEmpty()) {
            qWarning() << "Bay" << bayId << "n’a pas de leaf nodes.";
            continue;
        }

        const Node* start = leaves.first();
        QList<const Node*> sequence = graph.linearNodeSequenceFrom(start->id);

        if (sequence.isEmpty()) {
            qWarning() << "Bay" << bayId << "séquence vide.";
            continue;
        }

        QStringList seqStr;
        for (const Node* node : sequence) {
            seqStr << (node->name.isEmpty() ? node->id : node->name);
        }
        qDebug() << "Sequence:" << seqStr.join(" -> ");
    }
}

QList<Substation> SclManager::getSubstations() const {
    return m_substations;
}

QList<Ied> SclManager::getIeds() const {
    return m_ieds;
}

void SclManager::printIeds() const
{
    qDebug().noquote() << "=== List of IEDs ===";

    if (m_ieds.isEmpty()) {
        qDebug().noquote() << "  Aucun IED trouvée.";
        return;
    }

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
    qDebug().noquote() << "=== List of Substations ===";
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
