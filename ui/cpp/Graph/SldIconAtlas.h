#pragma once
#include <QObject>
#include <QImage>
#include <QMap>
#include <QRectF>
#include <QSize>
#include <QHash>
#include <QQuickWindow>
#include <QSGTexture>

/**
 * Atlas d’icônes SVG → texture QSG.
 * - Conserve le ratio (aspect) du viewBox du SVG
 * - Enregistre une UV serrée sur la zone réellement dessinée
 */
class SldIconAtlas : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QRectF uv;        // UV [0..1] dans l’atlas (zone utile)
        QSize  pxSize;    // taille réellement dessinée dans l’atlas (px)
        float  aspect = 1.f; // largeur/hauteur du viewBox
        bool isNull() const { return uv.isNull(); }
    };

    explicit SldIconAtlas(QObject* parent=nullptr) : QObject(parent) {}

    // idToResource: ex.  "CB"->":/icons/equipment/cbr_opened.svg"
    bool build(const QMap<QString, QString>& idToResource,
               int iconPxSize = 64,
               int spacing    = 4);

    Entry entry(const QString& id) const { return entries_.value(id); }
    QImage image() const { return atlas_; }

    // Texture (mise en cache par QQuickWindow)
    QSGTexture* textureFor(QQuickWindow* win) const;

private:
    QImage atlas_;                        // RGBA premultiplied
    QMap<QString, Entry> entries_;        // id → entry
    mutable QHash<QQuickWindow*, QSharedPointer<QSGTexture>> texPerWin_;
};
