#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "SclManager.h"
#include "SldTypes.h"

namespace sld {

class SldBuilder {
public:
    explicit SldBuilder(const scl::SclModel* model);

    // Derives the whole single-line diagram from the SCL Bay structure.
    scl::Status build(SldPlan& out) const;

private:
    const scl::SclModel* model_{nullptr};

    // Ordered role chain of one feeder bay, resolved by walking the bay's own
    // CE<->CN links outward from the bus-side terminal.
    void resolveRoles(const scl::Bay& bay, const std::string& ss,
                      const std::string& vl, Feeder& f) const;

    static std::string key(const std::string& ss, const std::string& vl,
                           const std::string& bay);
};

} // namespace sld