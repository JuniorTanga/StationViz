// StationViz/core/network/ObjectRefMapper.cpp
#include "ObjectRefMapper.h"
#include <sstream>

namespace network {

static std::string buildLnName(const std::string& lnClass, const std::string& lnInst) {
    // IEC 61850: LNodeName = <lnClass><lnInst> (LLN0 reste "LLN0")
    if (lnClass == "LLN0") return "LLN0";
    return lnClass + lnInst;
}

std::string ObjectRefMapper::toMmsRef(const std::string& ldInst, const scl::FcdaRef& f) {
    // DO/DA peuvent être hiérarchiques: "Pos.stVal", "Mod.stVal", "Beh.stVal" etc.
    // ObjectRef attendu: "LD/LN.DO.component[FC]"
    std::ostringstream oss;
    const auto lnName = buildLnName(f.lnClass, f.lnInst);
    oss << ldInst << "/" << lnName << "." << f.doName;
    if (!f.daName.empty()) oss << "." << f.daName;
    if (!f.fc.empty())     oss << "[" << f.fc << "]";
    return oss.str();
}

} // namespace network
