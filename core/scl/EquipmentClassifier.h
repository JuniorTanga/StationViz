#pragma once
#include <string_view>
#include <unordered_set>

namespace scl {

// Liste minimale d'exemples (à étendre depuis un fichier de config si besoin)
inline bool is_physical_equipment_ln(std::string_view lnClass) {
    static const std::unordered_set<std::string_view> kEquip{
        "XCBR","XSWI","YEFN","PTRC","CSWI","CILO","RREC",
        "PDIS","PTOC","PTEF","PTUV","PTOV","PXBR"
    };
    return kEquip.count(lnClass) > 0;
}

inline bool is_excluded_ln(std::string_view lnClass) {
    return lnClass == "LLN0" || lnClass == "LPHD" || lnClass == "MMXU";
}

} // namespace scl
