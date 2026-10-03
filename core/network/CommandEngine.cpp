// StationViz/core/network/CommandEngine.cpp
#include "CommandEngine.h"

#include <cstring>

namespace network {

// Builds a control value of the type the IED actually declares.
//
// The previous version always built an integer. A real IED rejects that: an
// SPCSO control takes a boolean, and an APC takes an AnalogueValue *structure*.
// ControlObjectClient_getCtlValType is the only reliable source for the type.
//
// A structure-typed control (APC) cannot be built from the client API alone:
// libiec61850 exposes no accessor for the control object's
// MmsVariableSpecification, and MmsValue_newStructure() needs one. Rather than
// send a malformed control value, return null and let the caller fail.
MmsValue* CommandEngine::makeCtlValue(ControlObjectClient ctrl, int64_t val) {
    switch (ControlObjectClient_getCtlValType(ctrl)) {
        case MMS_BOOLEAN:
            return MmsValue_newBoolean(val != 0);
        case MMS_INTEGER:
        case MMS_UNSIGNED:
            return MmsValue_newIntegerFromInt64(val);
        case MMS_FLOAT:
            return MmsValue_newFloat(static_cast<float>(val));
        case MMS_VISIBLE_STRING: {
            const std::string s = std::to_string(val);
            return MmsValue_newVisibleString(s.c_str());
        }
        default:
            return nullptr;
    }
}

void CommandEngine::setTestBit(MmsValue* ctlVal, bool on) {
    if (!ctlVal || MmsValue_getType(ctlVal) != MMS_BIT_STRING) return;
    MmsValue_setBitStringBit(ctlVal, 1, on);
}

bool CommandEngine::operateSBOw(IedConnection con, const std::string& ctlRef,
                                int64_t val, int timeoutMs) {
    (void)timeoutMs;   // the synchronous API has no timeout; see operateSBOwAsync
    if (mode_.load() != Mode::Test) return false;   // FAT gate
    if (!con) return false;

    // ControlObjectClient_create takes (objectReference, connection). The
    // previous call had them the other way round.
    ControlObjectClient ctrl = ControlObjectClient_create(ctlRef.c_str(), con);
    if (!ctrl) return false;

    // origin: identifier and category. The previous call passed two ints.
    ControlObjectClient_setOrigin(ctrl, "StationViz", 1);

    // Control model: direct-with-enhanced-security or sbo-with-enhanced-security
    // need Operate directly; status-only needs SBOw. Always-SBOw fails against a
    // DIRECT control, which is the most common interop failure in control.
    const ControlModel model = ControlObjectClient_getControlModel(ctrl);
    const bool needsSelect =
        model == CONTROL_MODEL_SBO_ENHANCED ||
        model == CONTROL_MODEL_DIRECT_ENHANCED;

    MmsValue* ctlVal = makeCtlValue(ctrl, val);
    if (!ctlVal) { ControlObjectClient_destroy(ctrl); return false; }

    bool ok = false;
    if (needsSelect) {
        // selectWithValue(self, ctlVal) returns bool; there is no error
        // out-parameter and no timeout on the synchronous variant.
        if (!ControlObjectClient_selectWithValue(ctrl, ctlVal)) {
            MmsValue_delete(ctlVal);
            ControlObjectClient_destroy(ctrl);
            return false;
        }
        // operate(self, ctlVal, operTime). The third argument is the operate
        // timestamp in milliseconds, not a timeout.
        ok = ControlObjectClient_operate(ctrl, ctlVal,
                                         (uint64_t)0x0000FFFFFFFFFFULL);
    } else {
        ok = ControlObjectClient_operate(ctrl, ctlVal,
                                         (uint64_t)0x0000FFFFFFFFFFULL);
    }

    MmsValue_delete(ctlVal);

    // Cancel only on the failure path: after a successful operate the select is
    // already consumed, and cancelling there discards it for nothing.
    if (!ok) ControlObjectClient_cancel(ctrl);

    ControlObjectClient_destroy(ctrl);
    return ok;
}

} // namespace network