// StationViz/core/network/TagRegistry.cpp
#include "TagRegistry.h"
#include <regex>

namespace network {

std::pair<std::string,std::string> TagRegistry::splitOnce(const std::string& s, char d) {
    auto pos = s.find(d);
    if (pos == std::string::npos) return {s, ""};
    return { s.substr(0, pos), s.substr(pos+1) };
}

std::optional<ObjectRefParts> TagRegistry::parseObjectRef(const std::string& ref) {
    // Format attendu: LD/LN.DO(.COMP)* (.DA)* [FC]
    // Exemple: "LD0/XCBR1.Pos.stVal[ST]" ou "LD0/LLN0.Mod.stVal[ST]"
    ObjectRefParts out;

    // séparer FC
    auto lbr = ref.rfind('[');
    auto rbr = ref.rfind(']');
    if (lbr == std::string::npos || rbr == std::string::npos || rbr < lbr) return std::nullopt;
    out.fc = ref.substr(lbr+1, rbr-lbr-1);

    // tronc sans [FC]
    const std::string base = ref.substr(0, lbr);

    // LD / reste
    auto slash = base.find('/');
    if (slash == std::string::npos) return std::nullopt;
    out.ldInst = base.substr(0, slash);
    const std::string ln_do = base.substr(slash+1);

    // LN . (DO.DA…)
    auto dot = ln_do.find('.');
    if (dot == std::string::npos) return std::nullopt;
    out.lnName = ln_do.substr(0, dot);
    const std::string rest = ln_do.substr(dot+1);

    // DO(.DA…)
    // on coupe doName et daName au premier '.' : doName = premier token, daName = le reste
    auto firstDot = rest.find('.');
    if (firstDot == std::string::npos) {
        out.doName = rest;
        out.daName.clear();
    } else {
        out.doName = rest.substr(0, firstDot);
        out.daName = rest.substr(firstDot+1);
    }

    return out;
}

TagId TagRegistry::registerObjectRef(const std::string& ied, const std::string& objectRef) {
    auto parts = parseObjectRef(objectRef);
    // En cas de parsing impossible, on hash seulement la ref complète (fallback)
    TagId id;
    if (!parts) {
        id = TagId{ std::hash<std::string>{}(objectRef) };
    } else {
        // lnClass/lnInst : on n’essaie pas de les séparer ici (lnName déjà concat)
        id = TagId::fromParts(
            ied,
            parts->ldInst,
            parts->lnName,
            parts->doName,
            parts->daName,
            parts->fc
        );
    }

    std::unique_lock lk(m_);
    idByRef_[objectRef] = id;
    refById_[id.value] = objectRef;
    return id;
}

std::optional<TagId> TagRegistry::getIdByObjectRef(const std::string& objectRef) const {
    std::shared_lock lk(m_);
    auto it = idByRef_.find(objectRef);
    if (it == idByRef_.end()) return std::nullopt;
    return it->second;
}

std::optional<std::string> TagRegistry::getObjectRefById(TagId id) const {
    std::shared_lock lk(m_);
    auto it = refById_.find(id.value);
    if (it == refById_.end()) return std::nullopt;
    return it->second;
}

} // namespace network
