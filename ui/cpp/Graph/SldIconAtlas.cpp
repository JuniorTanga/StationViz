#include "SldIconAtlas.h"
#include <QPainter>
#include <QSGTexture>
#include <QQuickWindow>
#include <cmath>

#ifdef STATIONVIZ_HAVE_SVG
#include <QtSvg/QSvgRenderer>
#endif

bool SldIconAtlas::build(const QMap<QString, QString>& idToResource,
                         int iconPxSize, int spacing)
{
    entries_.clear();
    texPerWin_.clear();

#ifndef STATIONVIZ_HAVE_SVG
    // Without Qt6Svg the equipment symbols cannot be rasterised. Publish an
    // empty atlas so SldView draws no icons instead of drawing broken ones.
    Q_UNUSED(idToResource); Q_UNUSED(iconPxSize); Q_UNUSED(spacing);
    atlas_ = QImage(2, 2, QImage::Format_RGBA8888_Premultiplied);
    atlas_.fill(Qt::transparent);
    return false;
#else
    const int count = idToResource.size();
    if (count <= 0) {
        atlas_ = QImage(2, 2, QImage::Format_RGBA8888_Premultiplied);
        atlas_.fill(Qt::transparent);
        return true;
    }

    const int cell = iconPxSize + spacing * 2;
    const int cols = std::ceil(std::sqrt((double)count));
    const int rows = std::ceil((double)count / (double)cols);
    const int W = cols * cell + spacing;
    const int H = rows * cell + spacing;

    QImage img(W, H, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    int i = 0;
    for (auto it = idToResource.constBegin(); it != idToResource.constEnd(); ++it, ++i) {
        const QString id  = it.key();
        const QString res = it.value();

        const int cx = (i % cols) * cell + spacing;
        const int cy = (i / cols) * cell + spacing;
        const QRectF cellRect(cx, cy, iconPxSize, iconPxSize);

        QSvgRenderer svg(res);
        Entry e;

        if (svg.isValid()) {
            const QRectF vb = svg.viewBoxF();
            const double aspect = (vb.height() > 0.0) ? (vb.width() / vb.height()) : 1.0;

            // Rectangle de destination avec ratio conservé, centré dans la cellule
            QRectF dst = cellRect;
            if (aspect >= 1.0) {
                const double h = cellRect.width() / aspect;
                dst = QRectF(cellRect.x(), cellRect.y() + (cellRect.height() - h) * 0.5,
                             cellRect.width(), h);
            } else {
                const double w = cellRect.height() * aspect;
                dst = QRectF(cellRect.x() + (cellRect.width() - w) * 0.5,
                             cellRect.y(), w, cellRect.height());
            }

            svg.render(&p, dst);

            // UV serrées sur la zone réellement peinte
            e.uv = QRectF(dst.x() / W, dst.y() / H, dst.width() / W, dst.height() / H);
            e.pxSize = dst.size().toSize();
            e.aspect = (float)aspect;
        } else {
            // Fallback : croix rouge dans la cellule complète
            p.setPen(QPen(Qt::red, 2));
            p.drawRect(cellRect.adjusted(1, 1, -1, -1));
            p.drawLine(cellRect.topLeft(), cellRect.bottomRight());
            p.drawLine(cellRect.topRight(), cellRect.bottomLeft());

            e.uv = QRectF(cellRect.x() / W, cellRect.y() / H,
                          cellRect.width() / W, cellRect.height() / H);
            e.pxSize = cellRect.size().toSize();
            e.aspect = 1.f;
        }

        entries_.insert(id, e);
    }

    p.end();
    atlas_ = std::move(img);
    return true;
#endif
}

QSGTexture* SldIconAtlas::textureFor(QQuickWindow* win) const
{
    if (!win) return nullptr;
    auto it = texPerWin_.find(win);
    if (it != texPerWin_.end() && !it.value().isNull())
        return it.value().data();

    QSharedPointer<QSGTexture> tex(win->createTextureFromImage(atlas_));
    if (!tex.isNull()) {
        tex->setFiltering(QSGTexture::Linear);
        tex->setMipmapFiltering(QSGTexture::Linear);
        tex->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        tex->setVerticalWrapMode(QSGTexture::ClampToEdge);
        texPerWin_.insert(win, tex);
        return tex.data();
    }
    return nullptr;
}
