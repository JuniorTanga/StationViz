// StationViz/core/network/NetworkTypes.h
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <optional>
#include <unordered_map>
#include <functional>

// TagId stable (hash 64-bit) = hash("IED|LD|LN|DO|DA|FC")
namespace network {

struct TagId {
    uint64_t value {0};
    static TagId fromParts(std::string_view ied,
                           std::string_view ld,
                           std::string_view ln,
                           std::string_view doName,
                           std::string_view daName,
                           std::string_view fc) {
        auto h = [](uint64_t s, char c){ return (s ^ (uint64_t)(unsigned char)c) * 1099511628211ull; };
        uint64_t v = 1469598103934665603ull; // FNV-1a 64
        auto feed = [&](std::string_view x){
            for (char c: x) v = h(v, c);
            v = h(v, '|');
        };
        feed(ied); feed(ld); feed(ln); feed(doName); feed(daName); feed(fc);
        return TagId{v};
    }
    bool operator==(const TagId& o) const { return value == o.value; }
    struct Hash { size_t operator()(const TagId& t) const { return (size_t)t.value; } };
};

struct Timestamp {
    uint64_t epochMs{0}; // millisecondes depuis epoch
};

struct Quality {
    uint32_t q{0}; // bitmap IEC61850/Quality si disponible
};

using ByteString = std::vector<uint8_t>;

struct VectorValue {
    std::vector<double> data; // pour SV ou tableaux simples
};

using TagValue = std::variant<std::monostate, bool, int64_t, double, std::string,
                              Quality, Timestamp, VectorValue, ByteString>;

struct TagMeta {
    std::string ied, ldInst, lnClass, lnInst;
    std::string doName, daName, fc;   // FC = ST/MX/CO/SE/etc.
    std::string mmsRef;               // "LD/LN.item(component)[FC]"
};

// Endpoints/minimas pour NetworkMap
struct EndpointMms { std::string ied, ip; uint16_t port{102}; std::string ap; };
struct EndpointGse { std::string ied, ldInst, cbName; std::string mac; uint16_t appId{0}; int vlanId{-1}; int vlanPrio{-1}; std::vector<std::string> objectRefs; };
struct EndpointSv  { std::string ied, ldInst, cbName; std::string mac; uint16_t appId{0}; int vlanId{-1}; int vlanPrio{-1}; std::vector<std::string> objectRefs; };

struct RcbConfig {
    std::string rcbRef;               // "LD0/LLN0.RP01" ou "LD0/LLN0.BRxx"
    std::string dataSetRef;           // "LD0/LLN0.myDS"
    bool buffered{false};
    bool giAtStartup{true};
    std::vector<std::string> dsMembers; // ordre des membres (object refs) si dataRef absent en report
};

struct NetworkMap {
    std::vector<EndpointMms> mms;
    std::vector<EndpointGse> gse;
    std::vector<EndpointSv>  sv;
    // RCBs par IED
    std::unordered_map<std::string, std::vector<RcbConfig>> rcbByIed;
};

} // namespace network
