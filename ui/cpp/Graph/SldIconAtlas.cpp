#include "SldIconAtlas.h"
//#include <QSvgRenderer>
#include <QtSvg/QSvgRenderer>
#include <QPainter>
#include <QSGTexture>

SldIconAtlas::SldIconAtlas(QObject* parent) : QObject(parent) {}

bool SldIconAtlas::build(const QMap<QString, QString>& idToResource,
                         int iconPxSize, int spacing)
{
    if (idToResource.isEmpty()) {
        atlas_ = QImage();
        entries_.clear();
        texPerWin_.clear();
        return true;
    }

    const int count = idToResource.size();
    const int cell  = iconPxSize + spacing;
    const int cols  = qMin(count, 16);                 // simple packing en grille
    const int rows  = (count + cols - 1) / cols;
    const int W = cols * cell + spacing;
    const int H = rows * cell + spacing;

    QImage img(W, H, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    int i = 0;
    entries_.clear();

    for (auto it = idToResource.constBegin(); it != idToResource.constEnd(); ++it, ++i) {
        const QString id  = it.key();
        const QString res = it.value();

        const int cx = (i % cols) * cell + spacing;
        const int cy = (i / cols) * cell + spacing;
        const QRect dst(cx, cy, iconPxSize, iconPxSize);

        QSvgRenderer svg(res);
        if (svg.isValid()) {
            svg.render(&p, dst);
        } else {
            // fallback: croix
            p.setPen(QPen(Qt::red, 2));
            p.drawRect(dst.adjusted(1,1,-1,-1));
            p.drawLine(dst.topLeft(), dst.bottomRight());
            p.drawLine(dst.topRight(), dst.bottomLeft());
        }

        // UV
        QRectF uv((double)dst.x() / W, (double)dst.y() / H,
                  (double)dst.width() / W, (double)dst.height() / H);
        SldIconAtlas::Entry e;
        e.uv = uv;
        e.pxSize = QSize(iconPxSize, iconPxSize);
        entries_.insert(id, e);
    }

    p.end();
    atlas_ = img;
    texPerWin_.clear(); // invalider textures
    return true;
}

QSGTexture* SldIconAtlas::textureFor(QQuickWindow* win) const {
    if (!win || atlas_.isNull()) return nullptr;
    if (texPerWin_.contains(win)) return texPerWin_.value(win).data();
    QSharedPointer<QSGTexture> t(win->createTextureFromImage(atlas_));
    t->setFiltering(QSGTexture::Linear);
    texPerWin_.insert(win, t);
    return t.data();
}
