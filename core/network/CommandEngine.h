// StationViz/core/network/CommandEngine.h
#pragma once
#include "NetworkTypes.h"
#include <string>
#include <atomic>

extern "C" {
#include <iec61850_client.h>
#include <control.h>
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
    static MmsValue* makeMmsInteger(int64_t v);

private:
    std::atomic<Mode> mode_{Mode::ReadOnly};
};

} // namespace network
