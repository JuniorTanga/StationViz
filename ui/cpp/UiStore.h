#pragma once
#include <QObject>
#include <QString>
#include <QSettings>

class UiStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentFile READ currentFile WRITE setCurrentFile NOTIFY currentFileChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged) // "sld","ieds","comms"
    Q_PROPERTY(QString selectionId READ selectionId WRITE setSelectionId NOTIFY selectionIdChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(qreal panX READ panX WRITE setPanX NOTIFY panChanged)
    Q_PROPERTY(qreal panY READ panY WRITE setPanY NOTIFY panChanged)



public:
    explicit UiStore(QObject* parent=nullptr) : QObject(parent) {}

    QString currentFile() const { return currentFile_; }
    void setCurrentFile(const QString& f) { if (currentFile_==f) return; currentFile_=f; emit currentFileChanged(); }

    QString viewMode() const { return viewMode_; }
    void setViewMode(const QString& m) { if (viewMode_==m) return; viewMode_=m; emit viewModeChanged(); }

    QString selectionId() const { return selectionId_; }
    void setSelectionId(const QString& id) { if (selectionId_==id) return; selectionId_=id; emit selectionIdChanged(); }

    qreal zoom() const { return zoom_; }
    void setZoom(qreal z) { if (qFuzzyCompare(zoom_, z)) return; zoom_=z; emit zoomChanged(); }

    qreal panX() const { return panX_; }
    void setPanX(qreal v) { if (qFuzzyCompare(panX_, v)) return; panX_=v; emit panChanged(); }

    qreal panY() const { return panY_; }
    void setPanY(qreal v) { if (qFuzzyCompare(panY_, v)) return; panY_=v; emit panChanged(); }

    Q_INVOKABLE void saveSettings() const {
        QSettings s;
        s.beginGroup("ui");
        s.setValue("currentFile", currentFile_);
        s.setValue("viewMode", viewMode_);
        s.setValue("selectionId", selectionId_);
        s.setValue("zoom", zoom_);
        s.setValue("panX", panX_);
        s.setValue("panY", panY_);
        s.endGroup();
    }

    Q_INVOKABLE void loadSettings() {
        QSettings s;
        s.beginGroup("ui");
        setCurrentFile(s.value("currentFile").toString());
        setViewMode(s.value("viewMode", "sld").toString());
        setSelectionId(s.value("selectionId").toString());
        setZoom(s.value("zoom", 1.0).toReal());
        setPanX(s.value("panX", 0.0).toReal());
        setPanY(s.value("panY", 0.0).toReal());
        s.endGroup();
    }

signals:
    void currentFileChanged();
    void viewModeChanged();
    void selectionIdChanged();
    void zoomChanged();
    void panChanged();

private:
    QString currentFile_;
    QString viewMode_{"sld"};
    QString selectionId_;
    qreal zoom_{1.0};
    qreal panX_{0.0};
    qreal panY_{0.0};
};
