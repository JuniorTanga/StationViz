// StationViz/core/network/CommandEngine.cpp
#include "CommandEngine.h"

namespace network {

MmsValue* CommandEngine::makeMmsInteger(int64_t v) {
    return MmsValue_newIntegerFromInt64(v);
}

bool CommandEngine::operateSBOw(IedConnection con, const std::string& ctlRef, int64_t val, int timeoutMs) {
    if (mode_.load() != Mode::Test) return false; // verrou FAT

    if (!con) return false;

    IedClientError err = IED_ERROR_OK;

    // Crée un client de contrôle pour l’objet (SBOw)
    ControlObjectClient ctrl = ControlObjectClient_create(con, ctlRef.c_str());

    if (!ctrl) return false;

    // Origin facultatif (catégorie "StationViz", id 1)
    ControlObjectClient_setOrigin(ctrl, 1, 1);

    // Valeur de commande
    MmsValue* ctlVal = makeMmsInteger(val);

    // SELECT with value (SBOw)
    ControlObjectClient_selectWithValue(ctrl, &err, ctlVal, timeoutMs);
    if (err != IED_ERROR_OK) {
        MmsValue_delete(ctlVal);
        ControlObjectClient_destroy(ctrl);
        return false;
    }

    // OPERATE
    ControlObjectClient_operate(ctrl, &err, ctlVal, timeoutMs);
    MmsValue_delete(ctlVal);

    bool ok = (err == IED_ERROR_OK);

    // Toujours cancel (cleanup) si modèle l’exige — optionnel:
    ControlObjectClient_cancel(ctrl, &err);

    ControlObjectClient_destroy(ctrl);
    return ok;
}

} // namespace network
