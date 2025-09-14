#pragma once
#include <QString>
#include <QMap>
#include <QList>
#include <QSet>
#include <QDebug>

struct Node {
    QString id;       // unique ID
    QString name;     // local name (optional)
};

struct Edge {
    const Node* node1 {nullptr};
    const Node* node2 {nullptr};
    QString id;
    QString name;
    QString type;
};

class Graph {
public:
    // Ajout de noeuds
    void addNode(const Node& node);
    void addNode(const QString& nodeId, const QString& nodeName = "");

    // Ajout d'arêtes
    void createEdge(const QString& nodeId1, const QString& nodeId2,
                 const QString& label = "", const QString& type = "");
    void addEdge(const QString& nodeId1, const QString& nodeId2,
                    const QString& label = "", const QString& type = "");
    void addEdge(const Edge& edge); // version directe (rarement utile)

    // Accesseurs
    QList<Node> getNodes() const;
    QList<Edge> getEdges() const;

    // Outils d'analyse
    int nodeDegree(const QString& nodeId) const;
    QList<const Edge*> edgesConnectedTo(const QString& nodeId) const;
    QList<const Node*> getNeighbors(const QString& nodeId) const;

    // Parcours linéaire
    QList<const Edge*> linearPathFrom(const QString& startNodeId) const;
    QList<const Edge*> linearPathFromLeaf() const;
    QList<const Node*> linearNodeSequenceFrom(const QString& startNodeId) const;
    QList<const Node*> getLeaves() const;
    void printGraph() const;

private:
    QMap<QString, Node> m_nodes;   // id -> Node
    QList<Edge> m_edges;           // liste d’arêtes
};
