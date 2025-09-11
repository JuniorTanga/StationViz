// LogicalNode.h
#pragma once
#include <QString>
#include <QList>

class DAI {
public:
    QString name;
    QString value;
};

class DOI {
public:
    QString name;
    QList<DAI> dais;
};

class LogicalNode {
public:
    QString lnClass;
    QString inst;
    QString lnType;
    QString prefix;

    QList<DOI> dois;
};
