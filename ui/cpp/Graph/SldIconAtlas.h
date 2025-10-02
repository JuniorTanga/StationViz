#pragma once
#include <QObject>
#include <QImage>
#include <QHash>
#include <QQuickWindow>
#include <QRectF>
#include <QSize>
#include <QSharedPointer>
#include <QSGTexture>

class SldIconAtlas : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QRectF uv;     // UV en [0,1] dans l'atlas
        QSize  pxSize; // taille pixel rendue (avant upscale world)
    };

    explicit SldIconAtlas(QObject* parent=nullptr);

    // Construit l’atlas à partir d’un mapping id->ressource (qrc:/… .svg)
    // iconPxSize = taille de rendu de chaque icône dans l’atlas (carré)
    bool build(const QMap<QString, QString>& idToResource,
               int iconPxSize = 48, int spacing = 2);

    // Récupère (ou crée) une texture pour une fenêtre Quick donnée
    QSGTexture* textureFor(QQuickWindow* win) const;

    // Entrée pour un id d’icône (ex: "Bus", "Transformer")
    Entry entry(const QString& id) const { return entries_.value(id); }

    // Dimensions de l’atlas (pixels)
    QSize atlasSizePx() const { return atlas_.size(); }

private:
    QImage atlas_;                        // RGBA32 premultiplied
    QMap<QString, Entry> entries_;        // id -> entry
    mutable QHash<QQuickWindow*, QSharedPointer<QSGTexture>> texPerWin_;
};
