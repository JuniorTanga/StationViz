#pragma once
#include <QString>
#include <QMap>
#include <QList>


struct Node {
    QString id;       // unique ID
    QString name;     // local name (optional)
};


struct Edge {
    QString id;
    QString name;
    QString type;
    QString node1;
    QString node2;
};

class Graph {
public:
    void addNode(const Node& node);
    void addEdge(const Edge& edge);

    void addNode(const QString& nodeId = "",const QString& nodeName = "");
    void addEdge(const QString& node1, const QString& node2, const QString& label = "", const QString type = "");

    QList<Node> getNodes() const;
    QList<Edge> getEdges() const;

    QList<QString> toLinearSequence() const;

    const QList<QString>& getLinearSequenceNodes() const { return m_linearSequenceNodes; }
    const QList<Edge>& getLinearSequenceEdges() const { return m_linearSequenceEdges; }

    void layoutLinear(); // <-- méthode à implémenter

private:
    QMap<QString, Node> m_nodes;   // id -> Node
    QList<Edge> m_edges;
    QList<QString> m_linearSequenceNodes;
    QList<Edge> m_linearSequenceEdges;

    int nodeDegree(const QString& nodeId) const;
    QList<Edge> edgesConnectedTo(const QString& nodeId) const;
};

