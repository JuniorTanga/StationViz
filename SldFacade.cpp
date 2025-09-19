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

    std::cout << "\n\n SUBSTATION : \n\n";
    sclMgr_->printSubstations();
    std::cout << "\n\n Communications : \n\n";
    sclMgr_->printCommunication();
    std::cout << "\n\n IEDs : \n\n" ;
    sclMgr_->printIEDs();
    std::cout << "\n\n Topology : \n\n";
    sclMgr_->printTopology();

    return {};
}

QString SldFacade::buildSld() {
    try {
        if (!sclMgr_ || !sclMgr_->model()) {
            emit errorOccurred("SCL not loaded");
            return "SCL not loaded";
        }
        sld::HeuristicsConfig cfg;
        sldMgr_ = std::make_unique<sld::SldManager>(sclMgr_->model(), cfg);
        auto st = sldMgr_->build();
        if (!st) {
            const QString msg = QString::fromStdString(st.error().message);
            emit errorOccurred(msg);
            return msg;
        }
        std::cout <<  "\n Log PlanJson \n" << sldMgr_->planJson();
        ready_ = true;
        emit readyChanged();
        return {};
    } catch (const std::exception &e) {
        const QString msg = QString("Exception: ") + e.what();
        emit errorOccurred(msg);
        return msg;
    } catch (...) {
        emit errorOccurred("Unknown exception in buildSld()");
        return "Unknown exception";
    }
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
