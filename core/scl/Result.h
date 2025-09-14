#pragma once
#include <string>
#include <utility>

namespace scl {

enum class ErrorCode {
    None = 0,
    FileNotFound,
    XmlParseError,
    SchemaNotSupported,
    MissingMandatoryField,
    InvalidPath,
    LogicError,
};

struct Error {
    ErrorCode code {ErrorCode::None};
    std::string message;
};

template<typename T>
class Result {
public:
    Result(const T& value) : ok_(true), value_(value) {}
    Result(T&& value) : ok_(true), value_(std::move(value)) {}
    Result(Error e) : ok_(false), err_(std::move(e)) {}

    explicit operator bool() const { return ok_; }
    const T& value() const { return value_; }
    T& value() { return value_; }
    const Error& error() const { return err_; }

    const T* operator->() const { return &value_; }
    T* operator->() { return &value_; }

private:
    bool ok_ = false;
    T value_{};
    Error err_{};
};

class Status {
public:
    Status() = default; // OK
    explicit Status(Error e) : ok_(false), err_(std::move(e)) {}
    static Status Ok() { return Status(); }

    explicit operator bool() const { return ok_; }
    const Error& error() const { return err_; }
private:
    bool ok_ = true;
    Error err_{};
};

} // namespace scl
