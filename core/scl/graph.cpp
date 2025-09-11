#include "graph.h"
#include <QSet>

int Graph::nodeDegree(const QString& nodeId) const {
    int degree = 0;
    for (const auto& edge : m_edges) {
        if (edge.node1 == nodeId || edge.node2 == nodeId) {
            degree++;
        }
    }
    return degree;
}

QList<Edge> Graph::edgesConnectedTo(const QString& nodeId) const {
    QList<Edge> connected;
    for (const auto& edge : m_edges) {
        if (edge.node1 == nodeId || edge.node2 == nodeId) {
            connected.append(edge);
        }
    }
    return connected;
}

void Graph::addNode(const Node& node) {
    if (!m_nodes.contains(node.id)) {
        m_nodes.insert(node.id, node);
    }
}

void Graph::addEdge(const Edge& edge) {
    // optionnel : vérifier si les noeuds existent
    if (!m_nodes.contains(edge.node1) || !m_nodes.contains(edge.node2)) {
        //qWarning() << "Edge nodes not found:" << edge.node1 << edge.node2;
        return;
    }
    m_edges.append(edge);
}

void Graph::addNode(const QString& nodeId, const QString& nodeName) {
    Node node;
    node.id = nodeId;
    addNode(node); // réutilise la version existante
}

void Graph::addEdge(const QString& node1, const QString& node2, const QString& label,const QString type) {
    Edge edge;
    edge.node1 = node1;
    edge.node2 = node2;
    edge.name = label;
    edge.type = type;
    addEdge(edge); // réutilise la version existante
}

void Graph::layoutLinear() {
    m_linearSequenceNodes.clear();
    m_linearSequenceEdges.clear();

    if (m_nodes.isEmpty() || m_edges.isEmpty()) return;

    // 1. Identifier les noeuds extrêmes (degré 1)
    QString startNodeId;
    for (const auto& node : m_nodes) {
        if (nodeDegree(node.id) == 1) {
            startNodeId = node.id;
            break;
        }
    }
    if (startNodeId.isEmpty()) {
        startNodeId = m_nodes.begin()->id; // graphe cyclique ou aucun degré 1
    }

    // 2. BFS pour ordonner les noeuds
    QSet<QString> visited;
    QList<QString> queue;
    queue.append(startNodeId);
    visited.insert(startNodeId);

    while (!queue.isEmpty()) {
        QString current = queue.takeFirst();

        // parcourir toutes les arêtes connectées
        for (const auto& edge : edgesConnectedTo(current)) {
            QString neighbor = (edge.node1 == current) ? edge.node2 : edge.node1;

            if (!visited.contains(neighbor)) {
                m_linearSequenceEdges.append(edge);
                queue.append(neighbor);
                visited.insert(neighbor);
            }
        }

        if (!m_linearSequenceNodes.contains(current))
            m_linearSequenceNodes.append(current);
    }

    // 3. Ajouter les noeuds restants isolés (au cas où)
    for (const auto& node : m_nodes) {
        if (!m_linearSequenceNodes.contains(node.id)) {
            m_linearSequenceNodes.append(node.id);
        }
    }
}

QList<QString> Graph::toLinearSequence() const {
    QList<QString> sequence;

    // Construire l’adjacence
    QMultiMap<QString, Edge> adjacency;
    for (const auto& e : m_edges) {
        adjacency.insert(e.node1, e);
        adjacency.insert(e.node2, e);
    }

    // Trouver une extrémité (degré == 1)
    QString startNode;
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (adjacency.count(it.key()) == 1) {
            startNode = it.key();
            break;
        }
    }
    if (startNode.isEmpty()) return sequence; // cas pathologique

    // Parcours linéaire
    QString current = startNode;
    QString prev = "";

    while (true) {
        // Ajouter le Node courant
        sequence.append("Node: " + m_nodes[current].name);

        const auto edgesForNode = adjacency.values(current);
        Edge nextEdge;
        bool found = false;

        // Choisir l’arête qui ne mène pas vers prev
        for (const auto& e : edgesForNode) {
            QString other = (e.node1 == current) ? e.node2 : e.node1;
            if (other != prev) {
                nextEdge = e;
                found = true;
                break;
            }
        }

        if (!found) break; // extrémité atteinte

        // Ajouter l’équipement (edge)
        sequence.append("Equip: " + nextEdge.type + " " + nextEdge.name);

        prev = current;
        current = (nextEdge.node1 == current) ? nextEdge.node2 : nextEdge.node1;
    }

    return sequence;
}


