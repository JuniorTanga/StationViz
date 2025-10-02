#pragma once
#include <QObject>
#include <memory>
#include <QString>
#include <QUrl>

#include "UiStore.h"
#include "Models/DiagnosticModel.h"
#include "Models/NodeModel.h"
#include "Models/EdgeModel.h"

// Backends (cohérents avec tes modules)
#include "SclManager.h"
#include "SldManager.h"
#include "Models/IedModel.h"
#include "SldTypes.h"   // pour sld::SldPlan

class AppContext : public QObject {
    Q_OBJECT
    //Q_PROPERTY(bool hasScl READ hasScl NOTIFY hasSclChanged)
    Q_PROPERTY(DiagnosticModel* diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(NodeModel* nodes READ nodes CONSTANT)
    Q_PROPERTY(EdgeModel* edges READ edges CONSTANT)
    Q_PROPERTY(UiStore* uiStore READ uiStore CONSTANT)
    Q_PROPERTY(IedModel* ieds READ ieds NOTIFY iedsChanged)
    Q_PROPERTY(QVariant iedGroups READ iedGroups NOTIFY iedsChanged)
    Q_PROPERTY(bool hasScl READ hasScl NOTIFY hasSclChanged)
    Q_PROPERTY(bool busy   READ busy   NOTIFY busyChanged)

public:
    explicit AppContext(QObject* parent=nullptr);

    Q_INVOKABLE bool openSclFile(const QString& filePath);
    Q_INVOKABLE void setViewMode(const QString& mode); // "sld","ieds","comms"
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool openSclUrl(const QUrl& url);
    Q_INVOKABLE void loadSclAsync(const QUrl& url);

    //bool busy() const;
    bool hasScl() const { return hasScl_; }
    bool busy() const   { return busy_; }
    DiagnosticModel* diagnostics() const { return diagModel_.get(); }
    NodeModel* nodes() const { return nodeModel_.get(); }
    EdgeModel* edges() const { return edgeModel_.get(); }
    UiStore* uiStore() const { return uiStore_.get(); }
    IedModel* ieds() const { return iedModel_; }
    QVariant  iedGroups() const { return iedGroups_; }

signals:
    void hasSclChanged();
    void iedsChanged();
    void busyChanged();
    void fileLoaded(bool ok);   // pour le toast

private:
	
	void fillDiagnosticsFromScl();
	
	void buildSldPlan();
	void fillModelsFromPlanJson();
    bool hasScl_{false};
    void fillIedsFromPlanJson();
    QVariant iedGroups_;               // ← cache pour QML
    static bool parseAnchor(const QString& a, QString& ss, QString& vl, QString& bay);
    bool busy_{false};
    void setBusy_(bool b){ if (busy_==b) return; busy_=b; emit busyChanged(); }

    IedModel* iedModel_{nullptr};
    std::unique_ptr<scl::SclManager> scl_;
    std::unique_ptr<sld::SldManager> sldMgr_;
    std::unique_ptr<DiagnosticModel> diagModel_;
    std::unique_ptr<NodeModel> nodeModel_;
    std::unique_ptr<EdgeModel> edgeModel_;
    std::unique_ptr<UiStore> uiStore_;
};
