#include "AppContext.h"
#include <QFileInfo>
#include <QDebug>
#include <QPointF>
#include <QHash>
#include <QMap>
#include <unordered_map>
#include <QStringList>
#include <algorithm>   // pour std::max
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPointer>
#include "nlohmann/json.hpp"


using json = nlohmann::json;

AppContext::AppContext(QObject* parent)
    : QObject(parent),
      scl_(std::make_unique<scl::SclManager>()),
      diagModel_(std::make_unique<DiagnosticModel>()),
      nodeModel_(std::make_unique<NodeModel>()),
      edgeModel_(std::make_unique<EdgeModel>()),
      uiStore_(std::make_unique<UiStore>())
{
    uiStore_->setViewMode("sld");
    iedModel_  = new IedModel(this);
}

//=========HELPERS=============

bool AppContext::openSclUrl(const QUrl& url)
{
    // Convertit proprement une URL (file://...) en chemin local
    return openSclFile(url.isLocalFile() ? url.toLocalFile()
                                         : url.toString());
}

bool AppContext::openSclFile(const QString& filePathIn)
{
    if (filePathIn.isEmpty()) return false;

    // Normalise : si on nous passe "file:///D:/...", on convertit en "D:/..."
    QString filePath = filePathIn;
    if (filePath.startsWith("file:", Qt::CaseInsensitive)) {
        filePath = QUrl(filePath).toLocalFile();
    }

    // 1) Charger SCL
    auto st = scl_->loadScl(filePath.toStdString());
    diagModel_->clear();

    if (!st) {
        diagModel_->append("error", "Échec du chargement du fichier SCL", filePath);
        hasScl_ = false;
        emit hasSclChanged();
        return false;
    }

    // 2) Diagnostics SCL réels
    fillDiagnosticsFromScl();

    // 3) Construire le plan SLD
    buildSldPlan();

    hasScl_ = true;
    uiStore_->setCurrentFile(filePath);
    emit hasSclChanged();
    return true;
}

void AppContext::loadSclAsync(const QUrl& url)
    {
        if (busy_) return;
        const QString filePathIn = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (filePathIn.isEmpty()) { emit fileLoaded(false); return; }

            // Normalise "file://"
            const QString filePath = filePathIn.startsWith("file:", Qt::CaseInsensitive)
                          ? QUrl(filePathIn).toLocalFile() : filePathIn;

            setBusy_(true);
        diagModel_->clear();

            // LOURD: parse SCL en tâche de fond
            QPointer<AppContext> self(this);
        auto future = QtConcurrent::run([self, filePath]() -> bool {
                if (!self) return false;
                // Uniquement le parse ici
                    auto st = self->scl_->loadScl(filePath.toStdString());
                return (bool)st;
            });

            auto* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, filePath](){
                const bool ok = watcher->result();
                watcher->deleteLater();

                    // Suite sur le thread UI
                    fillDiagnosticsFromScl();
                if (ok) {
                        buildSldPlan();
                        hasScl_ = true;
                        uiStore_->setCurrentFile(filePath);
                        emit hasSclChanged();
                    } else {
                        hasScl_ = false;
                        uiStore_->setCurrentFile(QString());
                        emit hasSclChanged();
                    }
                setBusy_(false);
                emit fileLoaded(ok);
            });
        watcher->setFuture(future);
    }

bool AppContext::parseAnchor(const QString& a, QString& ss, QString& vl, QString& bay)
{
    ss.clear(); vl.clear(); bay.clear();
    if (a.isEmpty()) return false;

    // Cas 1: canonique "SS:VL:BAY[:...]" (c’est ce que produit ton SclManager pour CN logiques)
    const auto partsColon = a.split(':');
    if (partsColon.size() >= 3) {
        ss  = partsColon[0].trimmed();
        vl  = partsColon[1].trimmed();
        bay = partsColon[2].trimmed();
        if (!ss.isEmpty() && !vl.isEmpty() && !bay.isEmpty()) return true;
    }

    // Cas 2: chemin "SS/VL/BAY/..." (fallback tolérant)
    const auto partsSlash = a.split('/');
    if (partsSlash.size() >= 3) {
        ss  = partsSlash[0].trimmed();
        vl  = partsSlash[1].trimmed();
        bay = partsSlash[2].trimmed();
        if (!ss.isEmpty() && !vl.isEmpty() && !bay.isEmpty()) return true;
    }

    return false;
}



void AppContext::fillDiagnosticsFromScl() {
    diagModel_->clear();
    const auto& diags = scl_->diagnostics();

    auto sev = [](scl::ErrorCode c)->QString {
        // Ton enum : None, FileNotFound, XmlParseError, ...
        return (c == scl::ErrorCode::None) ? "info" : "warn";
    };

    for (const auto& d : diags) {
        diagModel_->append(sev(d.code),
                           QString::fromStdString(d.message),
                           QString::fromStdString(d.location));
    }
    if (diags.empty())
        diagModel_->append("info", "Fichier SCL chargé", "");
}


void AppContext::fillIedsFromPlanJson() {
    iedModel_->beginReset();
    iedModel_->clearNoSignal();

    // Compteurs endpoints par IED (depuis SclManager)
    struct Cnt { int mms=0, gse=0, sv=0; };
    std::unordered_map<std::string, Cnt> cnts;

    if (scl_) {
        for (const auto& kv : scl_->mmsEndpoints()) {
            auto pos = kv.first.find('|'); std::string ied = (pos==std::string::npos)? kv.first : kv.first.substr(0,pos);
            cnts[ied].mms++;
        }
        for (const auto& kv : scl_->gseEndpoints()) {
            auto pos = kv.first.find('|'); std::string ied = (pos==std::string::npos)? kv.first : kv.first.substr(0,pos);
            cnts[ied].gse++;
        }
        for (const auto& kv : scl_->svEndpoints()) {
            auto pos = kv.first.find('|'); std::string ied = (pos==std::string::npos)? kv.first : kv.first.substr(0,pos);
            cnts[ied].sv++;
        }
    }

    // Lecture depuis le plan SLD (J["ieds"])
    json J;
    try { J = json::parse(sldMgr_->planJson(), nullptr, true); }
    catch (...) { J = json::object(); }

    if (J.contains("ieds") && J["ieds"].is_array()) {
        for (const auto& jIed : J["ieds"]) {
            IedModel::Item it;
            it.name = QString::fromStdString(jIed.value("name", std::string{}));
            const auto c = cnts[it.name.toStdString()];
            it.mms = c.mms; it.gse = c.gse; it.sv = c.sv;

            it.lds = QVariantList{};
            if (jIed.contains("lds") && jIed["lds"].is_array()) {
                for (const auto& jLd : jIed["lds"]) {
                    QVariantMap ld;
                    ld["inst"] = QString::fromStdString(jLd.value("inst", std::string{}));

                    QVariantList equips;
                    if (jLd.contains("equipments") && jLd["equipments"].is_array()) {
                        for (const auto& je : jLd["equipments"]) {
                            QVariantMap e;
                            const QString lnClass = QString::fromStdString(je.value("lnClass", std::string{}));
                            const QString lnInst  = QString::fromStdString(je.value("lnInst",  std::string{}));
                            const QString prefix  = QString::fromStdString(je.value("prefix",  std::string{}));
                            const QString label   = (prefix.isEmpty()? lnClass : prefix + "." + lnClass) + (lnInst.isEmpty()? "" : lnInst);
                            e["label"]  = label;
                            e["lnClass"]= lnClass;
                            e["lnInst"] = lnInst;
                            e["prefix"] = prefix;
                            // anchors (optionnel)
                            if (je.contains("anchors") && je["anchors"].is_array()) {
                                QVariantList anchors;
                                for (const auto& a : je["anchors"])
                                    anchors << QString::fromStdString(a.get<std::string>());
                                e["anchors"] = anchors;
                            }
                            equips << e;
                        }
                    }
                    ld["equipments"] = equips;
                    it.lds << ld;
                }
            }
            iedModel_->appendNoSignal(std::move(it));
        }
    }

    iedModel_->endReset();

    // --------------- [NOUVEAU] Construire iedGroups_ : SS/VL -> BAY -> IED -> LD -> equipments ---------------
    QVariantList groups; // liste de { ss, vl, bays:[ { name, ieds:[ { name, mms,gse,sv, lds:[{inst,equipments:[{label,lnClass,lnInst,prefix}]}] } ] } ] }

    // 2) Prépare index des compteurs endpoints par IED (déjà construit plus haut, on peut le reconstituer vite)
    if (scl_) {
        for (const auto& kv : scl_->mmsEndpoints()) { auto p=kv.first.find('|'); auto ied=(p==std::string::npos)? kv.first:kv.first.substr(0,p); cnts[ied].mms++; }
        for (const auto& kv : scl_->gseEndpoints()) { auto p=kv.first.find('|'); auto ied=(p==std::string::npos)? kv.first:kv.first.substr(0,p); cnts[ied].gse++; }
        for (const auto& kv : scl_->svEndpoints())  { auto p=kv.first.find('|'); auto ied=(p==std::string::npos)? kv.first:kv.first.substr(0,p); cnts[ied].sv++; }
    }

    // 3) Construire une structure hiérarchique en std::maps (tri stable pour l’affichage)
    struct Eq { QString label, lnClass, lnInst, prefix; };
    struct Ld { QString inst; std::vector<Eq> eqs; };
    struct Ied { QString name; int mms=0,gse=0,sv=0; std::map<QString, Ld> lds; };
    using Bay = std::map<QString, Ied>; // key=IED name
    using Group = std::map<QString, Bay>; // key=Bay name
    std::map<QString, Group> groupsMap; // key="SS|VL"

    if (J.contains("ieds") && J["ieds"].is_array()) {
        for (const auto& jIed : J["ieds"]) {
            const QString iedName = QString::fromStdString(jIed.value("name", std::string{}));
            const auto c = cnts[iedName.toStdString()];

            if (!(jIed.contains("lds") && jIed["lds"].is_array()))
                continue;

            for (const auto& jLd : jIed["lds"]) {
                const QString ldInst = QString::fromStdString(jLd.value("inst", std::string{}));
                if (!(jLd.contains("equipments") && jLd["equipments"].is_array()))
                    continue;

                for (const auto& je : jLd["equipments"]) {
                    const QString lnClass = QString::fromStdString(je.value("lnClass", std::string{}));
                    const QString lnInst  = QString::fromStdString(je.value("lnInst",  std::string{}));
                    const QString prefix  = QString::fromStdString(je.value("prefix",  std::string{}));
                    const QString label   = (prefix.isEmpty()? lnClass : prefix + "." + lnClass) + (lnInst.isEmpty()? "" : lnInst);

                    // anchors -> SS/VL/BAY
                    if (je.contains("anchors") && je["anchors"].is_array() && !je["anchors"].empty()) {
                        for (const auto& a : je["anchors"]) {
                            const QString anchor = QString::fromStdString(a.get<std::string>());
                            QString ss, vl, bay;
                            if (!parseAnchor(anchor, ss, vl, bay)) continue;

                            const QString gkey = ss + "|" + vl;          // groupe SS|VL
                            auto& group = groupsMap[gkey];
                            auto& bayMap = group[bay];
                            auto& ied = bayMap[iedName];
                            if (ied.name.isEmpty()) { ied.name=iedName; ied.mms=c.mms; ied.gse=c.gse; ied.sv=c.sv; }
                            auto& ld = ied.lds[ldInst];
                            if (ld.inst.isEmpty()) ld.inst = ldInst;
                            ld.eqs.push_back({label, lnClass, lnInst, prefix});
                        }
                    } else {
                        // Pas d’anchor → colonne “(Non assigné)”
                        const QString ss = "", vl = "";
                        const QString bay = "(Non assigné)";
                        const QString gkey = "—|—";
                        auto& group = groupsMap[gkey];
                        auto& bayMap = group[bay];
                        auto& ied = bayMap[iedName];
                        if (ied.name.isEmpty()) { ied.name=iedName; ied.mms=c.mms; ied.gse=c.gse; ied.sv=c.sv; }
                        auto& ld = ied.lds[ldInst];
                        if (ld.inst.isEmpty()) ld.inst = ldInst;
                        ld.eqs.push_back({label, lnClass, lnInst, prefix});
                    }
                }
            }
        }
    }

    // 4) Convertir en QVariant (QML-friendly)
    for (const auto& [gkey, group] : groupsMap) {
        QString ss="—", vl="—";
        const int sep = gkey.indexOf('|');
        if (sep > 0) { ss = gkey.left(sep); vl = gkey.mid(sep+1); }

        QVariantMap G; G["ss"]=ss; G["vl"]=vl;
        QVariantList bays;

        for (const auto& [bayName, bayMap] : group) {
            QVariantMap B; B["name"] = bayName;
            QVariantList ieds;

            for (const auto& [iedName, ied] : bayMap) {
                QVariantMap I; I["name"]=iedName; I["mms"]=ied.mms; I["gse"]=ied.gse; I["sv"]=ied.sv;
                QVariantList lds;

                for (const auto& [ldInst, ld] : ied.lds) {
                    QVariantMap L; L["inst"]=ldInst;
                    QVariantList equips;
                    for (const auto& eq : ld.eqs) {
                        QVariantMap E;
                        E["label"]=eq.label; E["lnClass"]=eq.lnClass; E["lnInst"]=eq.lnInst; E["prefix"]=eq.prefix;
                        equips << E;
                    }
                    L["equipments"] = equips;
                    lds << L;
                }
                I["lds"] = lds;
                ieds << I;
            }

            B["ieds"] = ieds;
            bays << B;
        }

        G["bays"] = bays;
        groups << G;
    }

    iedGroups_ = groups; // cache exposé à QML
}


void AppContext::buildSldPlan() {
    sldMgr_ = std::make_unique<sld::SldManager>(scl_.get(), sld::HeuristicsConfig{});
    auto st = sldMgr_->build();
    if (!st) {
        diagModel_->append("error", "Erreur lors de la construction du SLD", "");
        nodeModel_->clear();
        edgeModel_->clear();
        return;
    }

    fillModelsFromPlanJson();
    fillIedsFromPlanJson();
    emit iedsChanged();
}

void AppContext::fillModelsFromPlanJson() {
    nodeModel_->beginReset();
    nodeModel_->clearNoSignal();
    edgeModel_->beginReset();
    edgeModel_->clearNoSignal();

    const std::string planStr = sldMgr_->planJson();
    if (planStr.empty()) {
        diagModel_->append("warn", "Plan SLD vide (planJson())", "");
        nodeModel_->endReset();
        edgeModel_->endReset();
        return;
    }

    json j;
    try {
        j = json::parse(planStr, nullptr, true);
    } catch (const std::exception& e) {
        diagModel_->append("error", QString("JSON plan invalide: %1").arg(e.what()), "");
        nodeModel_->endReset();
        edgeModel_->endReset();
        return;
    }

    // ---- Paramètres de layout ----
    const double busY        = 80.0;   // base locale (restera constante)
    const double busXStep    = 360.0;
    const double feederTopDy = 80.0;   // espace Bus → 1er élément
    const double chainStepY  = 52.0;   // espace entre éléments d’un feeder
    const double laneStepX   = 90.0;
    const double laneStagger = 12.0;

    // Paramètres d’empilement vertical entre groupes
    const double groupGap       = 60.0; // marge entre groupes
    const double labelDy        = 18.0; // place pour le libellé
    const double groupBottomPad = 40.0; // marge basse sous le feeder le plus long

    // ---- Maps utilitaires ----
    QHash<QString, QPointF> pos;   pos.reserve(4096);
    QHash<QString, QString> label; label.reserve(4096);

    // 1) Pré-indexer les labels et kinds à partir du graphe condensé
    QHash<QString, QString> prettyLabelById;  // id -> label lisible (si dispo)
    QHash<QString, QString> kindById;         // id -> kind précis (CB/DS/Transformer/Bus/...)
    if (j.contains("graph") && j["graph"].is_object()) {
        const auto& g = j["graph"];
        if (g.contains("nodes") && g["nodes"].is_array()) {
            for (const auto& n : g["nodes"]) {
                const QString id  = QString::fromStdString(n.value("id", std::string{}));
                const QString lab = QString::fromStdString(n.value("label", std::string{}));
                const QString k   = QString::fromStdString(n.value("kind", std::string{}));
                // Certains exports mettent "eKind" séparément (CB/DS/...); on le privilégie si présent
                const QString ek  = QString::fromStdString(n.value("eKind", std::string{}));
                if (!lab.isEmpty()) prettyLabelById.insert(id, lab);
                if (!ek.isEmpty())  kindById.insert(id, ek);
                else if (!k.isEmpty()) kindById.insert(id, k);
            }
        }
    }


    // utilitaire pour un libellé "propre"
    auto prettyFromId = [](const QString& id)->QString {
        QString s = id;
        if (s.startsWith("CE:")) s = s.mid(3);
        int p = s.lastIndexOf('/');
        if (p >= 0 && p+1 < s.size()) s = s.mid(p+1);
        return s;
    };

    auto ensureNode = [&](const QString& id,
                          const QString& kind,      // "Bus" ou "Equipment" lors de l'appel
                          const QString& lbl,       // label explicite
                          double x, double y)
    {
        if (!pos.contains(id)) {
            pos.insert(id, QPointF(x,y));

            // label prioritaire: lbl -> prettyLabelById -> prettyFromId
            const QString nice = !lbl.isEmpty()
                                     ? lbl
                                     : (prettyLabelById.contains(id) ? prettyLabelById.value(id)
                                                                     : prettyFromId(id));
            label.insert(id, nice);

            NodeModel::Node n;
            n.id    = id;
            // CLÉ D’ICÔNE : si eKind est dispo dans kindById (CB/DS/CT/VT/Transformer...) on le prend,
            // sinon on garde 'kind' ("Bus" / "Equipment")
            n.kind  = kindById.contains(id) ? kindById.value(id) : kind;
            n.label = nice;
            n.x     = x;
            n.y     = y;
            n.state = "normal";
            nodeModel_->appendNoSignal(n);
        }
    };

    auto ensureBus = [&](const QString& busId, const QString& busLabel, double x, double y){
        ensureNode(busId, "Bus", busLabel, x, y);
    };
    auto pushEdge = [&](const QString& a, const QString& b, const QString& kind){
        EdgeModel::Edge e; e.fromId=a; e.toId=b; e.kind=kind;
        edgeModel_->appendNoSignal(e);
    };

    // ---- Index buses -> (ss, vl, label) et groupes ss:vl ----
    struct BusInfo { QString ss; QString vl; QString label; };
    QHash<QString, BusInfo> busInfo;
    QMap<QString, QStringList> groups; // key "SS:VL" -> liste busIds

    if (j.contains("buses") && j["buses"].is_array()) {
        for (const auto& b : j["buses"]) {
            const QString id  = QString::fromStdString(b.value("id", std::string{}));
            const QString ss  = QString::fromStdString(b.value("ss", std::string{}));
            const QString vl  = QString::fromStdString(b.value("vl", std::string{}));
            const QString lab = QString::fromStdString(b.value("label", std::string{}));
            busInfo.insert(id, {ss, vl, lab});
            const QString key = ss + ":" + vl;
            groups[key].push_back(id);
        }
    }

    // ---- Ordre des groupes ----
    QList<QString> orderedGroups; orderedGroups.reserve(groups.size());
    if (j.contains("ranks") && j["ranks"].contains("top") && j["ranks"]["top"].is_object()) {
        for (auto it = j["ranks"]["top"].begin(); it != j["ranks"]["top"].end(); ++it)
            orderedGroups.push_back(QString::fromStdString(it.key()));
    } else {
        orderedGroups = groups.keys();
    }

    // --- pré-collecte longueur max de feeder par BUS ---
    QHash<QString, int> maxChainByBus;        // busId -> longueur max
    if (j.contains("feeders") && j["feeders"].is_array()) {
        for (const auto& f : j["feeders"]) {
            const QString busId = QString::fromStdString(f.value("bus", std::string{}));
            const int len = f.contains("chain") && f["chain"].is_array() ? (int)f["chain"].size() : 0;
            maxChainByBus[busId] = std::max(maxChainByBus.value(busId, 0), len);
        }
    }

    // --- longueur max par GROUPE SS:VL ---
    QHash<QString, int> maxChainByGroup;      // "SS:VL" -> longueur max
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const QString key = it.key();                // "SS:VL"
        int maxLen = 0;
        for (const QString& busId : it.value()) {
            maxLen = std::max(maxLen, maxChainByBus.value(busId, 0));
        }
        maxChainByGroup.insert(key, maxLen);
    }

    // --- base Y par groupe avec empilement dynamique ---
    QHash<QString, double> groupBaseY;        // "SS:VL" -> y de la ligne Bus
    double currY = 0.0;
    //QList<QString> orderedGroups; orderedGroups.reserve(groups.size());
    if (j.contains("ranks") && j["ranks"].contains("top") && j["ranks"]["top"].is_object()) {
        for (auto it = j["ranks"]["top"].begin(); it != j["ranks"]["top"].end(); ++it)
            orderedGroups.push_back(QString::fromStdString(it.key()));
    } else {
        orderedGroups = groups.keys();
    }

    for (const QString& key : orderedGroups) {
        const int maxLen = maxChainByGroup.value(key, 0);
        // Hauteur nécessaire pour ce groupe :
        const double groupHeight = feederTopDy + std::max(0, maxLen - 1) * chainStepY
                                   + labelDy + groupBottomPad;
        const double baseY = currY + busY;       // position de la barre du bus
        groupBaseY.insert(key, baseY);
        currY += groupHeight + groupGap;         // empilement pour le prochain groupe
    }



    // ---- Placer les bus (y = groupBaseY[key]) ----
    for (const QString& key : orderedGroups) {
        QStringList busList = groups.value(key);
        if (j.contains("ranks") && j["ranks"].contains("top") && j["ranks"]["top"].contains(key.toStdString())) {
            busList.clear();
            for (const auto& b : j["ranks"]["top"][key.toStdString()])
                busList.push_back(QString::fromStdString(b.get<std::string>()));
        }

        const double y = groupBaseY.value(key, busY);
        for (int i = 0; i < busList.size(); ++i) {
            const QString busId = busList.at(i);
            const auto bi = busInfo.value(busId, BusInfo{});
            const double x = i * busXStep + 180.0;
            ensureBus(busId, bi.label, x, y);
        }
    }


    // ---- Pré-collecte des lanes par bus pour répartir horizontalement ----
    QHash<QString, int> laneMinByBus, laneMaxByBus;
    if (j.contains("feeders") && j["feeders"].is_array()) {
        for (const auto& f : j["feeders"]) {
            const QString busId = QString::fromStdString(f.value("bus", std::string{}));
            const int lane = f.value("lane", 0);
            if (!laneMinByBus.contains(busId) || lane < laneMinByBus[busId]) laneMinByBus[busId] = lane;
            if (!laneMaxByBus.contains(busId) || lane > laneMaxByBus[busId]) laneMaxByBus[busId] = lane;
        }
    }

    // ---- Pour fabriquer une barre de bus visible : xmin/xmax des feeders par bus ----
    QHash<QString, double> busSpanMinX, busSpanMaxX;

    // ---- Feeders (avec répartition horizontale par lane) ----
    if (j.contains("feeders") && j["feeders"].is_array()) {
        for (const auto& f : j["feeders"]) {
            const QString busId = QString::fromStdString(f.value("bus", std::string{}));
            const int lane = f.value("lane", 0);
            const QString endpoint = QString::fromStdString(f.value("endpoint", std::string("")));
            const auto& chain = f["chain"];

            // S'assurer que le bus existe
            if (!pos.contains(busId)) {
                // fallback : place ce bus dans le dernier groupe connu
                const auto bi = busInfo.value(busId, BusInfo{});
                const QString key = bi.ss + ":" + bi.vl;
                const double fbY = groupBaseY.isEmpty() ? busY : groupBaseY.constBegin().value();
                ensureBus(busId, bi.label, 180.0 + (double)pos.size() * 20.0, groupBaseY.value(key, fbY));
            }


            const QPointF bpos = pos.value(busId);
            const int laneMin = laneMinByBus.value(busId, lane);
            const int laneMax = laneMaxByBus.value(busId, lane);
            const int laneCount = (laneMax - laneMin + 1);
            const double totalWidth = (laneCount > 1) ? (laneCount - 1) * laneStepX : 0.0;
            const double x0 = bpos.x() - totalWidth * 0.5;
            const int laneIdx = lane - laneMin;
            const double nx = x0 + laneIdx * laneStepX + ((laneIdx & 1) ? laneStagger : 0.0);
            const double topY = bpos.y() + feederTopDy;

            // maj span bus
            if (!busSpanMinX.contains(busId) || nx < busSpanMinX[busId]) busSpanMinX[busId] = nx;
            if (!busSpanMaxX.contains(busId) || nx > busSpanMaxX[busId]) busSpanMaxX[busId] = nx;

            // Chaîne du feeder (verticale)
            QString prev = busId;
            int ci = 0;
            for (const auto& el : chain) {
                const QString nid = QString::fromStdString(el.get<std::string>());
                const double ny = topY + ci * chainStepY;
                ensureNode(nid, "Equipment", QString(), nx, ny);
                pushEdge(prev, nid, "FeederLink");
                prev = nid;
                ++ci;
            }
        }
    }

    // ---- Couplers ----
    if (j.contains("couplers") && j["couplers"].is_array()) {
        for (const auto& c : j["couplers"]) {
            const QString busA = QString::fromStdString(c.value("busA", std::string{}));
            const QString busB = QString::fromStdString(c.value("busB", std::string{}));

            if (!pos.contains(busA)) ensureBus(busA, busInfo.value(busA).label, 180.0, busY);
            if (!pos.contains(busB)) ensureBus(busB, busInfo.value(busB).label, 180.0 + busXStep, busY);

            pushEdge(busA, busB, "Coupler");
        }
    }

    // ---- Transformateurs ----
    if (j.contains("transformers") && j["transformers"].is_array()) {
        for (const auto& t : j["transformers"]) {
            const QString tr   = QString::fromStdString(t.value("tr", std::string{}));
            const QString busA = QString::fromStdString(t.value("busA", std::string{}));
            const QString busB = QString::fromStdString(t.value("busB", std::string{}));

            if (!pos.contains(busA)) ensureBus(busA, busInfo.value(busA).label, 180.0, busY);
            if (!pos.contains(busB)) ensureBus(busB, busInfo.value(busB).label, 180.0 + busXStep, busY);

            const QPointF a = pos.value(busA), b = pos.value(busB);
            const QPointF m = (a + b) / 2.0;
            const double y = std::max(a.y(), b.y()) + 60.0;
            ensureNode(tr, "Transformer", tr, m.x(), y);
            pushEdge(busA, tr, "TransformerLink");
            pushEdge(tr,  busB, "TransformerLink");
        }
    }

    // ---- Dessiner une barre de bus horizontale (span) pour chaque bus ayant des feeders ----
    for (auto it = busSpanMinX.constBegin(); it != busSpanMinX.constEnd(); ++it) {
        const QString busId = it.key();
        const double minX = it.value();
        const double maxX = busSpanMaxX.value(busId, minX);
        const double y = pos.value(busId).y();

        const QString leftId  = busId + "#L";
        const QString rightId = busId + "#R";
        ensureNode(leftId,  "Junction", "", minX, y);
        ensureNode(rightId, "Junction", "", maxX, y);
        pushEdge(leftId, rightId, "BusSpan");
    }

    nodeModel_->endReset();
    edgeModel_->endReset();

    diagModel_->append("info",
                       QString("Plan chargé: %1 nœuds / %2 arêtes")
                           .arg(nodeModel_->count())
                           .arg(edgeModel_->count()),
                       "");
}

void AppContext::setViewMode(const QString& mode) {
    uiStore_->setViewMode(mode);
}



void AppContext::clear() {
    scl_  = std::make_unique<scl::SclManager>();
    sldMgr_.reset();
    nodeModel_->clear();
    edgeModel_->clear();
    diagModel_->clear();
    hasScl_ = false;
    uiStore_->setCurrentFile({});
    emit hasSclChanged();
}
