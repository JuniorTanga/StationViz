#include "Result.h"

namespace scl {

const char* to_string(ErrorCode c) {
    switch (c) {
        case ErrorCode::None:                     return "None";
        case ErrorCode::FileNotFound:             return "FileNotFound";
        case ErrorCode::XmlParseError:            return "XmlParseError";
        case ErrorCode::SchemaNotSupported:       return "SchemaNotSupported";
        case ErrorCode::MissingMandatoryField:     return "MissingMandatoryField";
        case ErrorCode::InvalidPath:              return "InvalidPath";
        case ErrorCode::LogicError:               return "LogicError";
        case ErrorCode::DatasetNotFound:          return "DatasetNotFound";
        case ErrorCode::ControlBlockNotFound:     return "ControlBlockNotFound";
        case ErrorCode::InvalidLdRef:             return "InvalidLdRef";
        case ErrorCode::InvalidIedRef:            return "InvalidIedRef";
        case ErrorCode::BrokenConnectivityNode:   return "BrokenConnectivityNode";
        case ErrorCode::DuplicateIEDName:         return "DuplicateIEDName";
        case ErrorCode::DuplicateConnectivityNode:return "DuplicateConnectivityNode";
        case ErrorCode::DuplicateDataSetName:     return "DuplicateDataSetName";
        case ErrorCode::MissingSmpRate:           return "MissingSmpRate";
        case ErrorCode::AppIdMismatch:            return "AppIdMismatch";
    }
    return "Unknown";
}

} // namespace scl
