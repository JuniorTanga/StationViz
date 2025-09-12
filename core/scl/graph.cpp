#include "graph.h"

// ----------------- Gestion des noeuds -----------------
void Graph::addNode(const Node& node) {
    if (!m_nodes.contains(node.id)) {
        m_nodes.insert(node.id, node);
    }
}

void Graph::addNode(const QString& nodeId, const QString& nodeName) {
    if (!m_nodes.contains(nodeId)) {
        Node node;
        node.id = nodeId;
        node.name = nodeName;
        m_nodes.insert(nodeId, node);
    }
}

// ----------------- Gestion des arêtes -----------------
void Graph::addEdge(const Edge& edge) {
    if (!edge.node1 || !edge.node2) {
        qWarning() << "Invalid edge, null nodes.";
        return;
    }

    // Vérifier doublons
    for (const auto& e : m_edges) {
        if ((e.node1 == edge.node1 && e.node2 == edge.node2) ||
            (e.node1 == edge.node2 && e.node2 == edge.node1)) {
            qWarning() << "Edge already exists between"
                       << edge.node1->id << "and" << edge.node2->id;
            return;
        }
    }
    //QString edgeId = (edge.node1->id + "_" + edge.node2->id);
    m_edges.append(edge);
}

QList<const Node*> Graph::getNeighbors(const QString& nodeId) const {
    QList<const Node*> neighbors;
    for (const auto& edge : m_edges) {
        if (edge.node1->id == nodeId) neighbors.append(edge.node2);
        else if (edge.node2->id == nodeId) neighbors.append(edge.node1);
    }
    return neighbors;
}

void Graph::printGraph() const {
    qDebug() << "Nodes:";
    for (const auto& node : m_nodes) {
        qDebug() << " -" << node.id << "(" << node.name << ")";
    }
    qDebug() << "Edges:";
    for (const auto& edge : m_edges) {
        qDebug() << " -" << edge.node1->id << "<->" << edge.node2->id
                 << "[" << edge.name << "," << edge.type << "]";
    }
}



void Graph::addEdge(const QString& nodeId1, const QString& nodeId2,
                    const QString& label, const QString& type) {
    if (!m_nodes.contains(nodeId1) || !m_nodes.contains(nodeId2)) {
        qWarning() << "Cannot add edge, missing nodes:" << nodeId1 << nodeId2;
        return;
    }

    const Node* n1 = &m_nodes[nodeId1];
    const Node* n2 = &m_nodes[nodeId2];

    // Vérifier doublons
    for (const auto& e : m_edges) {
        if ((e.node1 == n1 && e.node2 == n2) ||
            (e.node1 == n2 && e.node2 == n1)) {
            qWarning() << "Edge already exists between" << nodeId1 << "and" << nodeId2;
            return;
        }
    }

    Edge edge;
    edge.node1 = n1;
    edge.node2 = n2;
    edge.name = label;
    edge.type = type;
    m_edges.append(edge);
}

// ----------------- Accesseurs -----------------
QList<Node> Graph::getNodes() const {
    return m_nodes.values();
}

QList<Edge> Graph::getEdges() const {
    return m_edges;
}

// ----------------- Outils d’analyse -----------------
int Graph::nodeDegree(const QString& nodeId) const {
    int degree = 0;
    for (const auto& edge : m_edges) {
        if (edge.node1->id == nodeId || edge.node2->id == nodeId) {
            degree++;
        }
    }
    return degree;
}

QList<const Edge*> Graph::edgesConnectedTo(const QString& nodeId) const {
    QList<const Edge*> connected;
    for (const auto& edge : m_edges) {
        if (edge.node1->id == nodeId || edge.node2->id == nodeId) {
            connected.append(&edge);
        }
    }
    return connected;
}

// ----------------- Parcours linéaire -----------------
QList<const Node*> Graph::linearNodeSequenceFrom(const QString& startNodeId) const {
    QList<const Node*> sequence;

    auto itStart = m_nodes.constFind(startNodeId);
    if (itStart == m_nodes.constEnd()) {
        qWarning() << "linearNodeSequenceFrom: node" << startNodeId << "inexistant.";
        return sequence;
    }

    QString current = startNodeId;
    QSet<QString> visited;

    while (true) {
        auto it = m_nodes.constFind(current);
        if (it == m_nodes.constEnd())
            break;

        const Node* currentNode = &it.value();
        sequence.append(currentNode);
        visited.insert(current);

        // Chercher les arêtes non encore explorées
        QList<const Edge*> connected;
        for (const auto& edge : m_edges) {
            if (edge.node1->id == current || edge.node2->id == current) {
                QString other = (edge.node1->id == current) ? edge.node2->id : edge.node1->id;
                if (!visited.contains(other)) {
                    connected.append(&edge);
                }
            }
        }

        if (connected.size() != 1) {
            // fin: soit cul-de-sac (0), soit carrefour (>1)
            break;
        }

        // Avancer vers le prochain noeud
        const Edge* nextEdge = connected.first();
        current = (nextEdge->node1->id == current) ? nextEdge->node2->id : nextEdge->node1->id;
    }

    return sequence;
}


QList<const Edge*> Graph::linearPathFrom(const QString& startNodeId) const {
    QList<const Edge*> path;
    if (!m_nodes.contains(startNodeId))
        return path;

    QString current = startNodeId;
    QSet<QString> visited;

    while (true) {
        visited.insert(current);

        QList<const Edge*> connected;
        for (const auto& edge : m_edges) {
            if (edge.node1->id == current || edge.node2->id == current) {
                QString other = (edge.node1->id == current) ? edge.node2->id : edge.node1->id;
                if (!visited.contains(other)) {
                    connected.append(&edge);
                }
            }
        }

        if (connected.size() != 1) {
            // Fin: sommet ou carrefour
            break;
        }

        const Edge* nextEdge = connected.first();
        path.append(nextEdge);

        // avancer vers le prochain noeud
        current = (nextEdge->node1->id == current) ? nextEdge->node2->id : nextEdge->node1->id;
    }

    return path;
}

QList<const Edge*> Graph::linearPathFromLeaf() const {
    for (const auto& node : m_nodes) {
        if (nodeDegree(node.id) == 1) {
            return linearPathFrom(node.id);
        }
    }
    return {};
}

QList<const Node*> Graph::getLeaves() const {
    QList<const Node*> leaves;
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (nodeDegree(it.key()) == 1) {
            leaves.append(&it.value());
        }
    }
    return leaves;
}
