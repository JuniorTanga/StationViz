#pragma once
#include <string>

#include "SclManager.h"
#include "SldBuilder.h"
#include "SldTypes.h"

namespace sld {

class SldManager {
public:
    explicit SldManager(const scl::SclManager* sclMgr);

    scl::Status build();

    const SldPlan& plan() const { return plan_; }

    // Serialised for the QML layer. This is the only output the UI consumes.
    std::string planJson() const;

    const std::vector<std::string>& warnings() const { return plan_.warnings; }

private:
    const scl::SclManager* sclMgr_{nullptr};
    SldBuilder builder_;
    SldPlan plan_;
};

} // namespace sld
