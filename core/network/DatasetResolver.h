// StationViz/core/network/DatasetResolver.h
#pragma once
#include <optional>
#include <string>
#include <vector>
#include "../scl/SclManager.h"
#include "NetworkTypes.h"
#include "ObjectRefMapper.h"

namespace network {

class DatasetResolver {
public:
    struct ResolvedDS {
        std::string dataSetRef;            // "LD/LLN0.myDS"
        std::vector<std::string> members;  // object refs
    };

    static std::optional<ResolvedDS>
    resolveLn0(const scl::SclManager& sm,
               const std::string& ied,
               const std::string& ldInst,
               const std::string& dsName);
};

} // namespace network
