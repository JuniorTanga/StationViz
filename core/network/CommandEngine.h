// StationViz/core/network/CommandEngine.h
#pragma once
#include "NetworkTypes.h"
#include <string>
#include <atomic>

extern "C" {
#include <iec61850_client.h>
// control.h is server-private (src/iec61850/inc_private) and pulls in
// mms_server_libinternal.h. Everything this client needs is declared by
// iec61850_client.h.
}

namespace network {

class CommandEngine {
public:
    enum class Mode { ReadOnly, Test };

    void setMode(Mode m) { mode_.store(m); }

    // ctlRef = "LD/LN.DO.Oper" (ou objet de contrôle)
    // val    = valeur entière (ex: 1=On/Close, 0=Off/Open) à adapter selon DO
    // timeoutMs pour select/operate
    bool operateSBOw(IedConnection con, const std::string& ctlRef, int64_t val, int timeoutMs);

private:
    // Builds a ctlVal of the type the IED declares, via
    // ControlObjectClient_getCtlValType.
    static MmsValue* makeCtlValue(ControlObjectClient ctrl, int64_t val);
    // Sets the IEC 61850 `test` bit on a structure-typed control value.
    static void setTestBit(MmsValue* ctlVal, bool on);

private:
    std::atomic<Mode> mode_{Mode::ReadOnly};
};

} // namespace network
