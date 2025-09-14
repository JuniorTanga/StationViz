#pragma once
#include <string>
#include "Result.h"
#include "SclTypes.h"

namespace scl {

class SclParser {
public:
    SclParser();
    Result<SclModel> parseFile(const std::string& path);
    Result<SclModel> parseString(const std::string& xml);
};

} // namespace scl
