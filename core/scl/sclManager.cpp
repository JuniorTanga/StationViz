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

void SclManager::buildBayGraphs()
{
    bayGraphs.clear();

    // Parcours de toutes les Substations
    for (const auto& substation : m_substations)
    {
        // Parcours des VoltageLevel
        for (const auto& vl : substation.voltageLevels)
        {
            // Parcours des Bay
            for (const auto& bay : vl.bays)
            {
                Graph g;

                // --- Étape 1 : Ajouter les noeuds (ConnectivityNodes) ---
                for (const auto& node : bay.connectivityNodes)
                {
                    g.addNode(node.pathName,node.name); //pathName is the Id
                }

                // --- Étape 2 : Ajouter les arêtes (liaisons ConductingEquipment-Terminals) ---
                for (const auto& ce : bay.equipments)
                {
                    const auto& terminals = ce.terminals;
                    if (terminals.size() == 2) // Hypothèse : 2 terminaux
                    {
                        QString n1 = terminals[0].cNodeName;
                        QString n2 = terminals[1].cNodeName;

                        g.addEdge(n1,n2,ce.name,ce.type); //NB: Utiliser g.addEdge({id,name,type..}) pour aller plus vite
                        // On met aussi le nom de l’équipement comme label de l’arête
                    }
                }

                // --- Étape 3 : (optionnel) organiser le graphe ---
                g.layoutLinear(); // méthode à coder plus tard pour arranger les noeuds

                // --- Étape 4 : enregistrement dans la Map ---
                bayGraphs.insert(&bay, g);
            }
        }
    }
}

void SclManager::printBays() const
{
    if (bayGraphs.isEmpty()) {
        qWarning() << "No Bay graphs available. Call buildBayGraphs() first.";
        return;
    }

    for (auto it = bayGraphs.cbegin(); it != bayGraphs.cend(); ++it) {
        const Bay* bay = it.key();
        const Graph& graph = it.value();

        qDebug() << "===== Bay:" << bay->name << "=====";

        const auto& nodes = graph.getLinearSequenceNodes();
        const auto& edges = graph.getLinearSequenceEdges();

        if (nodes.isEmpty()) {
            qWarning() << "Bay" << bay->name << "has no nodes.";
            continue;
        }
        if (edges.isEmpty()) {
            qWarning() << "Bay" << bay->name << "has no edges.";
            continue;
        }

        // On imprime la séquence nœuds -> équipements
        qDebug() << "Linear sequence for Bay" << bay->name << ":";

        for (int i = 0; i < edges.size(); ++i) {
            const Edge& edge = edges[i];
            QString startNode = edge.node1;
            QString endNode = edge.node2;

            // Affiche le noeud de départ, le nom du CE (étiquette arête) et le noeud suivant
            qDebug() << startNode << " --[" << edge.name << "]--> " << endNode;
        }

        // Affiche les nœuds isolés s’il y en a
        for (const auto& node : nodes) {
            bool connected = false;
            for (const auto& edge : edges) {
                if (edge.node1 == node || edge.node2 == node) {
                    connected = true;
                    break;
                }
            }
            if (!connected) {
                qDebug() << node << "(isolated node)";
            }
        }

        qDebug() << "\n"; // séparation entre les Bays
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
