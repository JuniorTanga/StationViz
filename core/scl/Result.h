#pragma once
#include <string>
#include <utility>
#include <optional>
#include <stdexcept>

namespace scl {

enum class ErrorCode {
    None = 0,
    FileNotFound,
    XmlParseError,
    SchemaNotSupported,
    MissingMandatoryField,
    InvalidPath,
    LogicError,

    DatasetNotFound,        // contrôle GSE/SV pointe un DataSet inexistant
    ControlBlockNotFound,   // GSEControl / SampledValueControl introuvable
    InvalidLdRef,           // ldInst ne correspond à aucun LDevice
    InvalidIedRef,          // ConnectedAP@iedName ne correspond à aucun IED
    BrokenConnectivityNode, // Terminal@connectivityNode vers CN inexistant
    DuplicateIEDName,       // IED@name en doublon
    DuplicateConnectivityNode, // ConnectivityNode@pathName en doublon
    DuplicateDataSetName,   // DataSet@name en doublon dans un même LN
    MissingSmpRate          // SV sans P[type="SmpRate"] (warning)
};

const char* to_string(ErrorCode c);

struct Error {
    ErrorCode code {ErrorCode::None};
    std::string message;
};

// Thrown by Result<T> accessors when the result holds an error. The accessors
// used to dereference a disengaged std::optional, which is undefined behaviour
// rather than a diagnosable failure.
struct BadResultAccess : std::logic_error {
    explicit BadResultAccess(const char* what_arg) : std::logic_error(what_arg) {}
};

template<typename T>
class Result {
public:
    Result(const T& value) : ok_(true), value_(value) {}
    Result(T&& value) : ok_(true), value_(std::move(value)) {}
    Result(Error e) : ok_(false), err_(std::move(e)) {}

    explicit operator bool() const { return ok_; }
    bool has_value() const { return ok_; }

    [[nodiscard]] const T& value() const& {
        if (!ok_) throw BadResultAccess("Result::value() on an error result");
        return *value_;
    }
    [[nodiscard]] T& value() & {
        if (!ok_) throw BadResultAccess("Result::value() on an error result");
        return *value_;
    }
    [[nodiscard]] T&& value() && {
        if (!ok_) throw BadResultAccess("Result::value() on an error result");
        return std::move(*value_);
    }
    [[nodiscard]] const T& operator*() const& { return value(); }
    [[nodiscard]] T&& operator*() && { return std::move(*this).value(); }

    [[nodiscard]] const Error& error() const {
        if (ok_) throw BadResultAccess("Result::error() on a successful result");
        return *err_;
    }

    [[nodiscard]] const T* operator->() const {
        if (!ok_) throw BadResultAccess("Result::operator-> on an error result");
        return &(*value_);
    }
    [[nodiscard]] T* operator->() {
        if (!ok_) throw BadResultAccess("Result::operator-> on an error result");
        return &(*value_);
    }

private:
    bool ok_ = false;
    std::optional<T> value_{};
    std::optional<Error> err_{};
};

class Status {
public:
    Status() = default; // OK
    explicit Status(Error e) : ok_(false), err_(std::move(e)) {}
    static Status Ok() { return Status(); }

    explicit operator bool() const { return ok_; }
    [[nodiscard]] const Error& error() const { return err_; }
private:
    bool ok_ = true;
    Error err_{};
};

} // namespace scl
