// StationViz/core/network/TagRegistry.h
#pragma once
#include "NetworkTypes.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <shared_mutex>

namespace network {

struct ObjectRefParts {
    std::string ldInst;    // LD
    std::string lnName;    // LN = lnClass+lnInst (ex: XCBR1 ou LLN0)
    std::string doName;    // DO (évent. hiérarchie "Pos")
    std::string daName;    // DA (évent. hiérarchie "stVal")
    std::string fc;        // FC entre []
};

class TagRegistry {
public:
    // Enregistre un objectRef (LD/LN.DO.DA[FC]) et retourne le TagId
    TagId registerObjectRef(const std::string& ied, const std::string& objectRef);

    // Recherche
    std::optional<TagId> getIdByObjectRef(const std::string& objectRef) const;
    std::optional<std::string> getObjectRefById(TagId id) const;

    // Helper pour engines
    std::optional<TagId> resolve(const std::string& objectRef) const {
        return getIdByObjectRef(objectRef);
    }

private:
    static std::optional<ObjectRefParts> parseObjectRef(const std::string& ref);

    static std::pair<std::string,std::string> splitOnce(const std::string& s, char delim);

private:
    mutable std::shared_mutex m_;
    std::unordered_map<std::string, TagId> idByRef_;        // "LD/LN.DO.DA[FC]" -> TagId
    std::unordered_map<uint64_t, std::string> refById_;     // TagId.value -> ref
};

} // namespace network
