#include "AppContext.h"
#include <QFileInfo>
#include <QDebug>
#include <QPointF>
#include <QHash>
#include <QMap>
#include <QStringList>
#include <algorithm>
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

    // Inventaires → listes vides (pas null)
    equipmentInventory_ = QVariantList{};
    inventoryIED_       = QVariantList{};

    uiStore_->setCurrentFile(QString());
}

// --------------------- OUVERTURE ---------------------

bool AppContext::openSclUrl(const QUrl& url)
{
    return openSclFile(url.isLocalFile() ? url.toLocalFile()
                                         : url.toString());
}

void AppContext::setViewMode(const QString& mode)
{
    if (uiStore_) {
        uiStore_->setViewMode(mode);
    }
}


bool AppContext::openSclFile(const QString& filePathIn)
{
    if (filePathIn.isEmpty()) return false;

    QString filePath = filePathIn;
    if (filePath.startsWith("file:", Qt::CaseInsensitive)) {
        filePath = QUrl(filePath).toLocalFile();
    }

    auto st = scl_->loadScl(filePath.toStdString());
    diagModel_->clear();

    if (!st) {
        diagModel_->append("error", "Échec du chargement du fichier SCL", filePath);
        hasScl_ = false; emit hasSclChanged();
        return false;
    }

    fillDiagnosticsFromScl();
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

    const QString filePath = filePathIn.startsWith("file:", Qt::CaseInsensitive)
                                 ? QUrl(filePathIn).toLocalFile() : filePathIn;

    setBusy_(true);
    diagModel_->clear();

    QPointer<AppContext> self(this);
    auto future = QtConcurrent::run([self, filePath]() -> bool {
        if (!self) return false;
        auto st = self->scl_->loadScl(filePath.toStdString());
        return (bool)st;
    });

    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, filePath](){
        const bool ok = watcher->result();
        watcher->deleteLater();

        fillDiagnosticsFromScl();
        if (ok) {
            buildSldPlan();
            hasScl_ = true;
            uiStore_->setCurrentFile(filePath);
        } else {
            hasScl_ = false;
            uiStore_->setCurrentFile(QString());
        }
        emit hasSclChanged();
        setBusy_(false);
        emit fileLoaded(ok);
    });
    watcher->setFuture(future);
}

// --------------------- DIAGNOSTICS ---------------------

void AppContext::fillDiagnosticsFromScl() {
    diagModel_->clear();
    const auto& diags = scl_->diagnostics();

    auto sev = [](scl::ErrorCode c)->QString {
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

// --------------------- IEDs (page IED) ---------------------

void AppContext::fillIedsFromPlanJson() {
    iedModel_->beginReset();
    iedModel_->clearNoSignal();

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
                            const QString label   = (prefix.isEmpty()? lnClass : prefix + "." + lnClass)
                                                  + (lnInst.isEmpty()? "" : lnInst);
                            e["label"]  = label;
                            e["lnClass"]= lnClass;
                            e["lnInst"] = lnInst;
                            e["prefix"] = prefix;
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

    // iedGroups_ si tu l’utilises déjà ailleurs (pas modifié ici)
}

// --------------------- BUILD SLD + MODELS ---------------------

void AppContext::buildSldPlan() {
    sldMgr_ = std::make_unique<sld::SldManager>(scl_.get());
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

void AppContext::fillModelsFromPlanJson()
{
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
    try { j = json::parse(planStr, nullptr, true); }
    catch (const std::exception& e) {
        diagModel_->append("error", QString("JSON plan invalide: %1").arg(e.what()), "");
        nodeModel_->endReset();
        edgeModel_->endReset();
        return;
    }

    // ---- Paramètres de layout (compact, ajustables) ----
    const double busY        = 80.0;      // base locale bus
    const double busXStep    = 260.0;     // écart entre bus d'un même groupe
    const double feederTopDy = 56.0;      // bus -> 1er élément du feeder
    const double chainStepY  = 44.0;      // écart vertical entre éléments du feeder
    const double laneStepX   = 72.0;      // écart horizontal entre feeders (lanes)
    const double laneStagger = 10.0;      // léger décalage +/- pour lisibilité
    const double groupGap       = 36.0;   // écart vertical entre groupes SS:VL
    const double labelDy        = 16.0;
    const double groupBottomPad = 24.0;

    // Décalage horizontal pour la branche VT par rapport au CT
    const double vtDx = 56.0;

    // ---- Maps utilitaires ----
    QHash<QString, QPointF> pos;   pos.reserve(4096);
    QHash<QString, QString> lab;   lab.reserve(4096);

    // 1) Pré-indexer labels/kinds depuis le graphe condensé
    QHash<QString, QString> prettyLabelById;
    QHash<QString, QString> kindById;
    if (j.contains("graph") && j["graph"].is_object()) {
        const auto& g = j["graph"];
        if (g.contains("nodes") && g["nodes"].is_array()) {
            for (const auto& n : g["nodes"]) {
                const QString id  = QString::fromStdString(n.value("id", std::string{}));
                const QString lab0= QString::fromStdString(n.value("label", std::string{}));
                const QString k   = QString::fromStdString(n.value("kind", std::string{}));
                const QString ek  = QString::fromStdString(n.value("eKind", std::string{}));
                if (!lab0.isEmpty()) prettyLabelById.insert(id, lab0);
                if (!ek.isEmpty())   kindById.insert(id, ek);
                else if (!k.isEmpty()) kindById.insert(id, k);
            }
        }
    }

    auto prettyFromId = [](const QString& id)->QString {
        QString s = id;
        if (s.startsWith("CE:")) s = s.mid(3);
        int p = s.lastIndexOf('/');
        if (p >= 0 && p+1 < s.size()) s = s.mid(p+1);
        return s;
    };

    auto ensureNode = [&](const QString& id, const QString& kind,
                          const QString& lbl, double x, double y)
    {
        if (!pos.contains(id)) {
            pos.insert(id, QPointF(x,y));
            const QString nice = !lbl.isEmpty() ? lbl
                                                : prettyLabelById.value(id, prettyFromId(id));
            lab.insert(id, nice);

            NodeModel::Node n;
            n.id = id; n.kind = kind; n.label = nice;
            n.x = x; n.y = y; n.state = "normal";
            nodeModel_->appendNoSignal(n);
        }
    };

    auto ensureBus = [&](const QString& busId, const QString& busLabel,
                         double x, double y)
    {
        ensureNode(busId, "Bus", busLabel, x, y);
    };

    // Two windings of the same transformer can legitimately reach the same
    // busbar, and sldLib reports each winding. The diagram wants one edge, so
    // identical (from, to, kind) triples are collapsed here.
    QSet<QString> edgeSeen;
    auto pushEdge = [&](const QString& a, const QString& b, const QString& kind){
        const QString k = a + QLatin1Char('\x1f') + b + QLatin1Char('\x1f') + kind;
        if (edgeSeen.contains(k)) return;
        edgeSeen.insert(k);
        EdgeModel::Edge e; e.fromId=a; e.toId=b; e.kind=kind;
        edgeModel_->appendNoSignal(e);
    };

    // ---- Index buses -> (ss, vl, label) et groupes ss:vl ----
    struct BusInfo { QString ss; QString vl; QString label; };
    QHash<QString, BusInfo> busInfo;
    QMap<QString, QStringList> groups; // "SS:VL" -> liste busIds

    if (j.contains("buses") && j["buses"].is_array()) {
        for (const auto& b : j["buses"]) {
            const QString id  = QString::fromStdString(b.value("id", std::string{}));
            const QString ss  = QString::fromStdString(b.value("ss", std::string{}));
            const QString vl  = QString::fromStdString(b.value("vl", std::string{}));
            const QString bl  = QString::fromStdString(b.value("label", std::string{}));
            busInfo.insert(id, {ss, vl, bl});
            const QString key = ss + ":" + vl;
            groups[key].push_back(id);
        }
    }

    // Ordre des groupes
    QList<QString> orderedGroups;
    if (j.contains("ranks") && j["ranks"].contains("top") && j["ranks"]["top"].is_object()) {
        for (auto it = j["ranks"]["top"].begin(); it != j["ranks"]["top"].end(); ++it)
            orderedGroups.push_back(QString::fromStdString(it.key()));
    } else {
        orderedGroups = groups.keys();
    }

    // Longueurs de chaînes
    QHash<QString, int> maxChainByBus;
    if (j.contains("feeders") && j["feeders"].is_array()) {
        for (const auto& f : j["feeders"]) {
            const QString busId = QString::fromStdString(f.value("bus", std::string{}));
            const int len = f.contains("chain") && f["chain"].is_array() ? (int)f["chain"].size() : 0;
            maxChainByBus[busId] = std::max(maxChainByBus.value(busId, 0), len);
        }
    }
    QHash<QString, int> maxChainByGroup;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const QString key = it.key();
        int maxLen = 0;
        for (const QString& busId : it.value())
            maxLen = std::max(maxLen, maxChainByBus.value(busId, 0));
        maxChainByGroup.insert(key, maxLen);
    }

    // Empilement vertical des groupes
    QHash<QString, double> groupBaseY;
    double currY = 0.0;
    for (const QString& key : orderedGroups) {
        const int maxLen = maxChainByGroup.value(key, 0);
        const double groupHeight =
            feederTopDy + std::max(0, maxLen - 1) * chainStepY + labelDy + groupBottomPad;
        const double baseY = currY + busY;
        groupBaseY.insert(key, baseY);
        currY += groupHeight + groupGap;
    }

    // Placer les bus
    // ---- Placer les bus (y = groupBaseY[key]) ----
    for (const QString& key : orderedGroups) {
        QStringList busList = groups.value(key);

        // ranks.top[key] peut ne pas exister OU ne pas être un array selon le plan
        if (j.contains("ranks")
            && j["ranks"].contains("top")
            && j["ranks"]["top"].is_object())
        {
            const auto keyStd = key.toStdString();
            auto itTop = j["ranks"]["top"].find(keyStd);
            if (itTop != j["ranks"]["top"].end() && itTop->is_array()) {
                busList.clear();
                for (const auto& b : *itTop)
                    if (b.is_string())
                        busList.push_back(QString::fromStdString(b.get<std::string>()));
            }
        }

        // si groupe vide, on passe
        if (busList.isEmpty())
            continue;

        const double y = groupBaseY.value(key, busY);
        for (int i = 0; i < busList.size(); ++i) {
            const QString busId = busList.at(i);
            const auto bi = busInfo.value(busId, BusInfo{});
            const double x = i * busXStep + 180.0;
            ensureBus(busId, bi.label, x, y);
        }
    }


    // Lanes min/max par bus
    QHash<QString, int> laneMinByBus, laneMaxByBus;
    if (j.contains("feeders") && j["feeders"].is_array()) {
        for (const auto& f : j["feeders"]) {
            const QString busId = QString::fromStdString(f.value("bus", std::string{}));
            const int lane = f.value("lane", 0);
            if (!laneMinByBus.contains(busId) || lane < laneMinByBus[busId]) laneMinByBus[busId] = lane;
            if (!laneMaxByBus.contains(busId) || lane > laneMaxByBus[busId]) laneMaxByBus[busId] = lane;
        }
    }

    // Pour tracer la barre de bus (span)
    QHash<QString, double> busSpanMinX, busSpanMaxX;

    // ---- Feeder ordering ----
    //
    // sldLib resolves the chain by walking the bay's own CE<->CN links outward
    // from its bus-side terminal, so f["chain"] is already in true electrical
    // order. The previous canonicalizeFeeder() re-sorted it by equipment kind,
    // which produced DS, CB, DS, CT, Line and placed the CT after the line-side
    // disconnector. Roles are read from the plan for the lateral VT branch and
    // for the endpoint marker; the vertical chain is used verbatim.
    auto roleOf = [](const nlohmann::json& f, const char* key) -> QString {
        if (!f.contains("roles")) return {};
        const auto& r = f["roles"];
        if (!r.contains(key) || !r[key].is_string()) return {};
        return QString::fromStdString(r[key].get<std::string>());
    };

    // ---- Feeders (placement + branche VT) ----
    // ---- Feeders (placement + branche VT) ----
    //QHash<QString, double> busSpanMinX, busSpanMaxX; // (re)déclaré ici, local au bloc
    if (j.contains("feeders") && j["feeders"].is_array()) {
        for (const auto& f : j["feeders"]) {
            if (!f.is_object()) continue;

            // busId
            QString busId;
            if (f.contains("bus") && f["bus"].is_string())
                busId = QString::fromStdString(f["bus"].get<std::string>());
            if (busId.isEmpty())
                continue;

            // lane
            int lane = 0;
            if (f.contains("lane") && f["lane"].is_number_integer())
                lane = f["lane"].get<int>();

            // s’assurer que le bus existe (fallback si nécessaire)
            if (!pos.contains(busId)) {
                const auto bi = busInfo.value(busId, BusInfo{});
                const QString key = bi.ss + ":" + bi.vl;
                const double fbY = groupBaseY.isEmpty() ? busY : groupBaseY.constBegin().value();
                ensureBus(busId, bi.label, 180.0 + (double)pos.size() * 20.0,
                          groupBaseY.value(key, fbY));
            }

            // chaîne brute
            QStringList rawChain;
            if (f.contains("chain") && f["chain"].is_array()) {
                for (const auto& el : f["chain"])
                    if (el.is_string())
                        rawChain << QString::fromStdString(el.get<std::string>());
            }
            if (rawChain.isEmpty()) {
                // rien à tracer pour ce feeder
                continue;
            }

            // lane min/max pour le bus
            const int laneMin = laneMinByBus.value(busId, lane);
            const int laneMax = laneMaxByBus.value(busId, lane);
            const int laneCount = (laneMax - laneMin + 1);

            const QPointF bpos = pos.value(busId);
            const double totalWidth = (laneCount > 1) ? (laneCount - 1) * laneStepX : 0.0;
            const double x0 = bpos.x() - totalWidth * 0.5;
            const int laneIdx = lane - laneMin;
            const double nx = x0 + laneIdx * laneStepX + ((laneIdx & 1) ? laneStagger : 0.0);
            const double topY = bpos.y() + feederTopDy;

            // maj span bus
            if (!busSpanMinX.contains(busId) || nx < busSpanMinX[busId]) busSpanMinX[busId] = nx;
            if (!busSpanMaxX.contains(busId) || nx > busSpanMaxX[busId]) busSpanMaxX[busId] = nx;

            // Chain in true electrical order, resolved by sldLib.
            const QStringList chain = rawChain;

            // placement vertical + arêtes
            QString prev = busId;
            int ci = 0;

            // CT/VT for the lateral branch come from the resolved roles.
            QString ctId = roleOf(f, "ct");
            QString vtId = roleOf(f, "vt");
            for (const auto& nid : rawChain) {
                if (ctId.isEmpty() && kindById.value(nid) == "CT") ctId = nid;
                if (vtId.isEmpty() && kindById.value(nid) == "VT") vtId = nid;
            }

            bool ctPlaced = false;

            for (const auto& nid : chain) {
                const double ny = topY + ci * chainStepY;
                const QString kind = kindById.value(nid, "Equipment");
                const QString lbl  = prettyLabelById.value(nid, prettyFromId(nid));

                ensureNode(nid, kind, lbl, nx, ny);
                pushEdge(prev, nid, "FeederLink");
                prev = nid;

                if (!ctPlaced && nid == ctId)
                    ctPlaced = true;

                ++ci;
            }

            // Si CT et VT *distincts* présents, créer une dérivation CT → VT
            if (ctPlaced && !vtId.isEmpty() && vtId != ctId) {
                const QPointF pct = pos.value(ctId);
                const double vx = pct.x() + vtDx;   // branche à droite
                const double vy = pct.y();

                const QString vKind = kindById.value(vtId, "VT");
                const QString vLbl  = prettyLabelById.value(vtId, prettyFromId(vtId));
                ensureNode(vtId, vKind, vLbl, vx, vy);
                pushEdge(ctId, vtId, "FeederBranch"); // arête spéciale ; routage en L
            }
        }
    }

    // ---- Dessiner la barre de bus (span) *uniquement* pour les bus qui en ont besoin ----
    for (auto it = busSpanMinX.constBegin(); it != busSpanMinX.constEnd(); ++it) {
        const QString busId = it.key();
        // sécurité : vérifier min/max présents et cohérents
        if (!busSpanMaxX.contains(busId)) continue;
        const double minX = it.value();
        const double maxX = busSpanMaxX.value(busId, minX);
        if (!(maxX > minX)) continue; // rien à relier

        const double y = pos.value(busId).y();
        const QString leftId  = busId + "#L";
        const QString rightId = busId + "#R";
        ensureNode(leftId,  "Junction", "", minX, y);
        ensureNode(rightId, "Junction", "", maxX, y);
        pushEdge(leftId, rightId, "BusSpan");
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

    // ---- Transformateurs inter-bus ----
    if (j.contains("transformers") && j["transformers"].is_array()) {
        // sldLib emits one entry per winding, each naming the busbar that
        // winding reaches. A three-winding transformer therefore appears three
        // times, which is correct: the old (busA, busB) pair model could not
        // represent one at all.
        for (const auto& t : j["transformers"]) {
            const QString tr  = QString::fromStdString(t.value("tr", std::string{}));
            const QString bus = QString::fromStdString(t.value("bus", std::string{}));
            if (tr.isEmpty() || bus.isEmpty()) continue;
            if (!pos.contains(bus))
                ensureBus(bus, busInfo.value(bus).label, 180.0, busY);
            const QPointF b = pos.value(bus);
            // Fan the windings out vertically so they do not stack on one point.
            ensureNode(tr, "Transformer", tr, b.x(), b.y() + 60.0);
            pushEdge(bus, tr, "TransformerLink");
        }
    }

    nodeModel_->endReset();
    edgeModel_->endReset();

    // ➕ Construire les inventaires pour la page Inventory
    buildEquipmentInventoryFromJson(j);  // Physique (SS/VL/Bay)
    buildInventoryFromIEDsJson(j);       // Depuis IEDs (LNs)

    diagModel_->append("info",
                       QString("Plan chargé: %1 nœuds / %2 arêtes")
                           .arg(nodeModel_->count()).arg(edgeModel_->count()), "");
}

// --------------------- CLEAR ---------------------

void AppContext::clear() {
    scl_  = std::make_unique<scl::SclManager>();
    sldMgr_.reset();
    nodeModel_->clear();
    edgeModel_->clear();
    diagModel_->clear();
    hasScl_ = false;
    uiStore_->setCurrentFile({});
    equipmentInventory_ = QVariantList{};
    inventoryIED_       = QVariantList{};
    emit hasSclChanged();
    emit inventoryChanged();
    emit inventoryIEDChanged();
}

// --------------------- ICONS (physique) ---------------------

QString AppContext::iconForKind(const QString& k) const
{
    const QString K = k.toUpper();
    if (K == "CB" || K == "CIRCUITBREAKER")        return ":/icons/equipment/cbr_opened.svg";
    if (K == "DS" || K == "DISCONNECTOR")          return ":/icons/equipment/ds_opened.svg";
    if (K == "CT")                                 return ":/icons/equipment/ct.svg";
    if (K == "VT")                                 return ":/icons/equipment/vt.svg";
    if (K == "TRANSFORMER" || K == "TRANSFORMER2W" || K == "TRANSFORMER_2W")
        return ":/icons/equipment/transformer_2w.svg";
    if (K == "LINE" || K.endsWith("_LINE"))        return ":/icons/equipment/line.svg";
    return ":/icons/equipment/unknown.svg";
}

// --------------------- ICONS (LNs) ---------------------

QString AppContext::friendlyKindFromLnClass(const QString& lnClass) const
{
    const QString L = lnClass.toUpper();
    if (L == "XCBR") return "CB";
    if (L == "XSWI") return "DS";
    if (L == "TCTR") return "CT";
    if (L == "TVTR") return "VT";
    if (L.startsWith("TTRF")) return "Transformer";
    return lnClass; // fallback
}

QString AppContext::iconForLnClass(const QString& lnClass) const
{
    return iconForKind(friendlyKindFromLnClass(lnClass));
}

// --------------------- INVENTAIRE PHYSIQUE ---------------------

void AppContext::buildEquipmentInventoryFromJson(const nlohmann::json& j)
{
    QHash<QString, QString> prettyLabelById;
    QHash<QString, QString> kindById;

    if (j.contains("graph") && j["graph"].is_object()) {
        const auto& g = j["graph"];
        if (g.contains("nodes") && g["nodes"].is_array()) {
            for (const auto& n : g["nodes"]) {
                const QString id  = QString::fromStdString(n.value("id", std::string{}));
                const QString lab = QString::fromStdString(n.value("label", std::string{}));
                const QString k   = QString::fromStdString(n.value("kind", std::string{}));
                const QString ek  = QString::fromStdString(n.value("eKind", std::string{}));
                if (!lab.isEmpty()) prettyLabelById.insert(id, lab);
                if (!ek.isEmpty())  kindById.insert(id, ek);
                else if (!k.isEmpty()) kindById.insert(id, k);
            }
        }
    }

    auto prettyFromId = [](const QString& id)->QString {
        QString s = id;
        if (s.startsWith("CE:")) s = s.mid(3);
        int p = s.lastIndexOf('/');
        if (p >= 0 && p+1 < s.size()) s = s.mid(p+1);
        return s;
    };

    // ss -> vl -> bay -> items[]
    QMap<QString, QMap<QString, QMap<QString, QList<QVariantMap>>>> tree;

    if (j.contains("graph") && j["graph"].is_object()) {
        const auto& g = j["graph"];
        if (g.contains("nodes") && g["nodes"].is_array()) {
            for (const auto& n : g["nodes"]) {
                const QString id   = QString::fromStdString(n.value("id",   std::string{}));
                const QString kind = QString::fromStdString(n.value("eKind",std::string{}));
                const QString kind2= QString::fromStdString(n.value("kind", std::string{}));
                const QString ss   = QString::fromStdString(n.value("ss",   std::string{}));
                const QString vl   = QString::fromStdString(n.value("vl",   std::string{}));
                const QString bay  = QString::fromStdString(n.value("bay",  std::string{}));
                const QString lbl0 = QString::fromStdString(n.value("label",std::string{}));

                const QString K = (!kind.isEmpty() ? kind : kind2);
                if (K.isEmpty())                    continue;
                if (K == "Bus" || K == "Junction")  continue;
                if (ss.isEmpty() || vl.isEmpty() || bay.isEmpty()) continue;

                const QString label = !lbl0.isEmpty()
                                          ? lbl0
                                          : prettyLabelById.value(id, prettyFromId(id));

                QVariantMap item;
                item["id"]    = id;
                item["label"] = label;
                item["kind"]  = K;
                item["icon"]  = iconForKind(K);

                tree[ss][vl][bay].push_back(item);
            }
        }
    }

    auto rankKind = [](const QString& K)->int {
        const QString k = K.toUpper();
        if (k == "DS" || k == "DISCONNECTOR")      return 10;
        if (k == "CB" || k == "CIRCUITBREAKER")    return 20;
        if (k == "CT")                              return 30;
        if (k == "VT")                              return 40;
        if (k.startsWith("TRANSFORMER"))            return 50;
        if (k == "LINE" || k.endsWith("_LINE"))     return 60;
        return 100;
    };
    for (auto& vlMap : tree) {
        for (auto& bayMap : vlMap) {
            for (auto it = bayMap.begin(); it != bayMap.end(); ++it) {
                auto& list = it.value();
                std::sort(list.begin(), list.end(), [&](const QVariantMap& a, const QVariantMap& b){
                    const int ra = rankKind(a.value("kind").toString());
                    const int rb = rankKind(b.value("kind").toString());
                    if (ra != rb) return ra < rb;
                    return a.value("label").toString().localeAwareCompare(b.value("label").toString()) < 0;
                });
            }
        }
    }

    QVariantList out;
    for (auto ssIt = tree.constBegin(); ssIt != tree.constEnd(); ++ssIt) {
        const QString ss = ssIt.key();
        QVariantList vls;
        const auto& vlMap = ssIt.value();

        for (auto vlIt = vlMap.constBegin(); vlIt != vlMap.constEnd(); ++vlIt) {
            const QString vl = vlIt.key();
            QVariantList bays;
            const auto& bayMap = vlIt.value();

            for (auto bayIt = bayMap.constBegin(); bayIt != bayMap.constEnd(); ++bayIt) {
                const QString bay = bayIt.key();
                QVariantList items;
                for (const auto& it : bayIt.value())
                    items.push_back(it);
                QVariantMap b; b["bay"] = bay; b["items"] = items;
                bays.push_back(b);
            }
            QVariantMap v; v["vl"] = vl; v["bays"] = bays;
            vls.push_back(v);
        }
        QVariantMap s; s["ss"] = ss; s["vls"] = vls;
        out.push_back(s);
    }

    equipmentInventory_ = out;
    emit inventoryChanged();
}

// --------------------- INVENTAIRE DEPUIS IEDs ---------------------

void AppContext::buildInventoryFromIEDsJson(const nlohmann::json& j)
{
    QVariantList out; // liste d’IEDs

    if (!j.contains("ieds") || !j["ieds"].is_array()) {
        inventoryIED_ = out;
        emit inventoryIEDChanged();
        return;
    }

    for (const auto& jIed : j["ieds"]) {
        QVariantMap ied;
        ied["name"] = QString::fromStdString(jIed.value("name", std::string{}));

        QVariantList ldsOut;
        if (jIed.contains("lds") && jIed["lds"].is_array()) {
            for (const auto& jLd : jIed["lds"]) {
                QVariantMap ld;
                ld["inst"] = QString::fromStdString(jLd.value("inst", std::string{}));

                QVariantList items;
                if (jLd.contains("equipments") && jLd["equipments"].is_array()) {
                    for (const auto& je : jLd["equipments"]) {
                        const QString lnClass = QString::fromStdString(je.value("lnClass", std::string{}));
                        const QString lnInst  = QString::fromStdString(je.value("lnInst",  std::string{}));
                        const QString prefix  = QString::fromStdString(je.value("prefix",  std::string{}));

                        QVariantMap it;
                        const QString label   = (prefix.isEmpty()? lnClass : prefix + "." + lnClass)
                                              + (lnInst.isEmpty()? "" : lnInst);
                        it["label"]   = label;
                        it["lnClass"] = lnClass;
                        it["icon"]    = iconForLnClass(lnClass);
                        it["kind"]    = friendlyKindFromLnClass(lnClass);

                        if (je.contains("anchors") && je["anchors"].is_array()) {
                            QVariantList anchors;
                            for (const auto& a : je["anchors"])
                                anchors << QString::fromStdString(a.get<std::string>());
                            it["anchors"] = anchors;
                        }
                        items << it;
                    }
                }
                ld["items"] = items;
                ldsOut << ld;
            }
        }
        ied["lds"] = ldsOut;
        out << ied;
    }

    inventoryIED_ = out;
    emit inventoryIEDChanged();
}
