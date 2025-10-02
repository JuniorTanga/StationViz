// StationViz/core/network/MmsSession.h
#pragma once
#include "NetworkTypes.h"
#include "EventBus.h"
#include "StateStore.h"
#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <unordered_map>

extern "C" {
#include <iec61850_client.h>
}

namespace network {

// Une session par IED
class MmsSession {
public:
    MmsSession(std::string ied, std::string ip, uint16_t port,
               EventBus& bus, StateStore& store);
    ~MmsSession();

    bool connect(uint32_t timeoutMs = 3000);
    void disconnect();
    bool isUp() const { return up_.load(); }

    // Reports (RCB)
    bool enableReports(const std::vector<RcbConfig>& rcbs);

    // GI manuel si besoin
    bool triggerGI(const std::string& rcbRef);

    // Conversion TagMeta (optionnel: injecté par TagRegistry)
    void setMetaResolver(std::function<std::optional<TagId>(const std::string& objectRef)> r);
	// public:
	IedConnection getConnection() const { return con_; }


private:
    static void onConnectionClosed(void* param, IedConnection con);
    static void onReport(void* param, ClientReport report);

    void handleReport(ClientReport report);

    // Helpers
    static TagValue mmsToTagValue(MmsValue* v);
    void publishByDataRef(const std::string& dataRef, MmsValue* v);
    void publishByIndex(const std::string& rcbRef, int idx, MmsValue* v);

private:
    std::string ied_, ip_;
    uint16_t port_;

    EventBus& bus_;
    StateStore& store_;
    std::function<std::optional<TagId>(const std::string&)> resolveTagId_;

    IedConnection con_{nullptr};
    std::atomic<bool> up_{false};
    std::mutex mu_;

    // Pour re-mapper si le report ne contient pas dataRef
    std::unordered_map<std::string, std::vector<std::string>> dsMembersByRcb_; // rcbRef -> members
};

} // namespace network
