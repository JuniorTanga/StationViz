#pragma once
#include <QObject>
#include <memory>
#include "SldGraph.h"
#include "SclManager.h"

namespace stationviz::sld {

class SldBuilder : public QObject {
    Q_OBJECT
public:
    explicit SldBuilder(std::shared_ptr<stationviz::scl::SclManager> mgr, QObject* parent=nullptr);
    SldGraph build();

private:
    std::shared_ptr<stationviz::scl::SclManager> mgr_;
};

} // namespace stationviz::sld
