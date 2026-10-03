// StationViz/core/network/MmsSession.cpp
#include "MmsSession.h"
#include <cstring>

namespace network {

MmsSession::MmsSession(std::string ied, std::string ip, uint16_t port,
                       EventBus& bus, StateStore& store)
: ied_(std::move(ied)), ip_(std::move(ip)), port_(port), bus_(bus), store_(store)
{
    con_ = IedConnection_create(); // thread mode par défaut
    // handler close
    IedConnection_installConnectionClosedHandler(con_, &MmsSession::onConnectionClosed, this);
}

MmsSession::~MmsSession() {
    disconnect();
    if (con_) { IedConnection_destroy(con_); con_ = nullptr; }
}

bool MmsSession::connect(uint32_t timeoutMs) {
    if (!con_) return false;
    IedClientError err = IED_ERROR_OK;
    IedConnection_setConnectTimeout(con_, timeoutMs);
    IedConnection_connect(con_, &err, ip_.c_str(), port_);
    if (err == IED_ERROR_OK) {
        up_.store(true);
        return true;
    }
    up_.store(false);
    return false;
}

void MmsSession::disconnect() {
    if (!con_) return;
    if (up_.load()) {
        IedConnection_close(con_);
        up_.store(false);
    }
}

bool MmsSession::enableReports(const std::vector<RcbConfig>& rcbs) {
    if (!up_.load()) return false;
    IedClientError err = IED_ERROR_OK;

    for (const auto& cfg : rcbs) {
        // 1) Lire RCB
        ClientReportControlBlock rcb = IedConnection_getRCBValues(con_, &err, cfg.rcbRef.c_str(), NULL);
        if (err != IED_ERROR_OK || rcb == NULL) continue;

        // 2) Configurer DataSet si nécessaire
        if (!cfg.dataSetRef.empty())
            ClientReportControlBlock_setDataSetReference(rcb, cfg.dataSetRef.c_str());

        // 3) Activer RptEna
        ClientReportControlBlock_setRptEna(rcb, true);

        // 4) Appliquer (mask champs modifiés)
        uint32_t mask = RCB_ELEMENT_DATSET | RCB_ELEMENT_RPT_ENA;
        IedConnection_setRCBValues(con_, &err, rcb, mask, true);

        // 5) Installer handler de report sur ce RCB
        IedConnection_installReportHandler(con_, cfg.rcbRef.c_str(), nullptr, &MmsSession::onReport, this);

        // 6) Sauver mapping membres (fallback si DataReference absent)
        if (!cfg.dsMembers.empty())
            dsMembersByRcb_[cfg.rcbRef] = cfg.dsMembers;

        // 7) Option GI
        if (cfg.giAtStartup) {
            IedConnection_triggerGIReport(con_, &err, cfg.rcbRef.c_str());
        }

        ClientReportControlBlock_destroy(rcb);
    }

    return true;
}

bool MmsSession::triggerGI(const std::string& rcbRef) {
    if (!up_.load()) return false;
    IedClientError err = IED_ERROR_OK;
    IedConnection_triggerGIReport(con_, &err, rcbRef.c_str());
    return (err == IED_ERROR_OK);
}

void MmsSession::setMetaResolver(std::function<std::optional<TagId>(const std::string&)> r) {
    resolveTagId_ = std::move(r);
}

void MmsSession::onConnectionClosed(void* param, IedConnection) {
    auto* self = static_cast<MmsSession*>(param);
    self->up_.store(false);
    // Le reconnect/backoff sera géré par NetworkManager (prochaine partie)
}

void MmsSession::onReport(void* param, ClientReport report) {
    auto* self = static_cast<MmsSession*>(param);
    self->handleReport(report);
}

void MmsSession::handleReport(ClientReport report) {
    if (!report) return;

    // 1) Essayer DataReferences (si OptFlds inclut dataRef)
    const bool hasDataRef = ClientReport_hasDataReference(report);
    MmsValue* values = ClientReport_getDataSetValues(report); // array
    if (!values) return;

    const int numElems = MmsValue_getArraySize(values);
    for (int i = 0; i < numElems; ++i) {
        MmsValue* v = MmsValue_getElement(values, i);
        if (hasDataRef) {
            const char* dr = ClientReport_getDataReference(report, i);
            if (dr && *dr) { publishByDataRef(dr, v); continue; }
        }
        // Fallback par index à partir du RCB
        const char* rcbRef = ClientReport_getRcbReference(report);
        if (rcbRef) publishByIndex(rcbRef, i, v);
    }

    // (Optionnel) timestamps/confRev/seqNum disponibles si besoin
    // ClientReport_hasTimestamp/report_getTimestamp(), _hasSeqNum/_getSeqNum, etc.
}

TagValue MmsSession::mmsToTagValue(MmsValue* v) {
    if (!v) return std::monostate{};
    const auto t = MmsValue_getType(v);
    switch (t) {
        case MMS_BOOLEAN:    return TagValue{ MmsValue_getBoolean(v) };
        case MMS_INTEGER:
        case MMS_UNSIGNED:   return TagValue{ (int64_t) MmsValue_toInt64(v) };
        case MMS_FLOAT:      return TagValue{ (double)  MmsValue_toDouble(v) };
        case MMS_VISIBLE_STRING:
        case MMS_STRING:     return TagValue{ std::string(MmsValue_toString(v)) };
        case MMS_UTC_TIME:   {
            // UTCTime -> microseconds (libIEC61850 retourne 64-bit epoch? vérifier docs svt version)
            // Ici on expose epochMs si conversion dispo, sinon brut
            return TagValue{ Timestamp{ /*approx*/ 0 } };
        }
        case MMS_OCTET_STRING: {
            // MmsValue_getOctetString was removed in libiec61850 1.5: the buffer
            // is now exposed directly and must be copied by the caller.
            const int size = MmsValue_getOctetStringSize(v);
            ByteString bs;
            bs.resize(size);
            const uint8_t* buf = MmsValue_getOctetStringBuffer(v);
            if (buf && size > 0) memcpy(bs.data(), buf, static_cast<size_t>(size));
            return TagValue{ std::move(bs) };
        }
        case MMS_BIT_STRING: {
            // beaucoup de DO/DA statut (DPS/DPI) sont BIT_STRING. On renvoie un int64_t.
            return TagValue{ (int64_t) MmsValue_getBitStringAsInteger(v) };
        }
        case MMS_ARRAY: {
            VectorValue vv;
            const int n = MmsValue_getArraySize(v);
            vv.data.reserve(n);
            for (int i=0;i<n;++i) {
                MmsValue* e = MmsValue_getElement(v, i);
                if (MmsValue_getType(e) == MMS_FLOAT)
                    vv.data.push_back((double) MmsValue_toDouble(e));
                else if (MmsValue_getType(e) == MMS_INTEGER || MmsValue_getType(e) == MMS_UNSIGNED)
                    vv.data.push_back((double) MmsValue_toInt64(e));
                else if (MmsValue_getType(e) == MMS_BOOLEAN)
                    vv.data.push_back(MmsValue_getBoolean(e) ? 1.0 : 0.0);
                else
                    vv.data.push_back(0.0);
            }
            return TagValue{ std::move(vv) };
        }
        default:
            return std::monostate{};
    }
}

void MmsSession::publishByDataRef(const std::string& dataRef, MmsValue* v) {
    if (!resolveTagId_) return;
    auto id = resolveTagId_(dataRef);
    if (!id) return;
    TagValue tv = mmsToTagValue(v);
    store_.set(*id, tv);
    bus_.publish(*id, tv);
}

void MmsSession::publishByIndex(const std::string& rcbRef, int idx, MmsValue* v) {
    if (!resolveTagId_) return;
    auto it = dsMembersByRcb_.find(rcbRef);
    if (it == dsMembersByRcb_.end()) return;
    const auto& list = it->second;
    if (idx < 0 || idx >= (int)list.size()) return;
    const std::string& dataRef = list[idx];
    publishByDataRef(dataRef, v);
}

} // namespace network
