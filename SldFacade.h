#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "SclManager.h"  // sclLib
#include "SldManager.h"  // sldLib (version avancée)

class SldFacade : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
public:
    explicit SldFacade(QObject* parent = nullptr)
        : QObject(parent) {}

    // Charge un fichier SCL. Retourne "" si OK, sinon un message d'erreur.
    Q_INVOKABLE QString loadScl(const QString& path);

    // Construit le SLD à partir du SCL chargé. Retourne "" si OK, sinon message d'erreur.
    Q_INVOKABLE QString buildSld();

    // Raccourci pratique: recharge + reconstruit.
    Q_INVOKABLE QString reload(const QString& path);

    // Remet l'état à zéro (libère SLD, garde ou non le SCL — ici on libère tout)
    Q_INVOKABLE void reset();

    // Getter Q_PROPERTY
    bool isReady() const { return ready_; }

    // Exports JSON pour le frontend
    Q_INVOKABLE QString rawJson() const;        // graphe brut (debug)
    Q_INVOKABLE QString condensedJson() const;  // graphe condensé Bus+Equip
    Q_INVOKABLE QString planJson() const;       // JSON enrichi (buses, feeders, couplers, transformers)

signals:
    void readyChanged();
    void errorOccurred(const QString& message);

private:
    bool ready_ = false;
    std::unique_ptr<scl::SclManager> sclMgr_;
    std::unique_ptr<sld::SldManager> sldMgr_;
};
