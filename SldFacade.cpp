#include "SldFacade.h"
#include <iostream>

QString SldFacade::loadScl(const QString& path) {
    ready_ = false; emit readyChanged();
    sldMgr_.reset();

    sclMgr_ = std::make_unique<scl::SclManager>();
    auto st = sclMgr_->loadScl(path.toStdString());
    if (!st) {
        const QString msg = QString::fromStdString(st.error().message);
        emit errorOccurred(msg);
        return msg;
    }
    return {};
}

QString SldFacade::buildSld() {
    if (!sclMgr_ || !sclMgr_->model()) {
        const QString msg = "SCL not loaded";
        emit errorOccurred(msg);
        return msg;
    }

    sld::HeuristicsConfig cfg; // par défaut; peut être exposé plus tard si besoin
    sldMgr_ = std::make_unique<sld::SldManager>(sclMgr_->model(), cfg);

    auto st = sldMgr_->build();
    if (!st) {
        const QString msg = QString::fromStdString(st.error().message);
        emit errorOccurred(msg);
        return msg;
    }

    ready_ = true;
    emit readyChanged();
    //pour debuggage
    sldMgr_->printCondensed();
    sldMgr_->printCouplers();
    sldMgr_->printFeeders();
    std::cout << "\n PlanJson log : \n" << sldMgr_->planJson();
    return {};
}

QString SldFacade::reload(const QString& path) {
    const auto e1 = loadScl(path);
    if (!e1.isEmpty()) return e1;
    return buildSld();
}

void SldFacade::reset() {
    sldMgr_.reset();
    sclMgr_.reset();
    if (ready_) { ready_ = false; emit readyChanged(); }
}

QString SldFacade::rawJson() const {
    if (!sldMgr_) return "{}";
    return QString::fromStdString(sldMgr_->rawJson());
}

QString SldFacade::condensedJson() const {
    if (!sldMgr_)
        return QStringLiteral("{\"nodes\":[],\"edges\":[]}");
    auto s = sldMgr_->condensedJson();
    if (s.empty())
        return QStringLiteral("{\"nodes\":[],\"edges\":[]}");
    return QString::fromStdString(s);
}

QString SldFacade::planJson() const {
    if (!sldMgr_)
        return QStringLiteral(
            "{\"buses\":[],\"feeders\":[],\"couplers\":[],\"transformers\":[]}");
    auto s = sldMgr_->planJson();
    if (s.empty())
        return QStringLiteral(
            "{\"buses\":[],\"feeders\":[],\"couplers\":[],\"transformers\":[]}");
    return QString::fromStdString(s);
}
