#pragma once

#include <string>
#include <utility>

namespace monix::renderer_vk {

struct Status {
    bool ok = true;
    std::string message;

    static Status success() { return {}; }
    static Status failure(std::string msg) { return Status{false, std::move(msg)}; }
    explicit operator bool() const { return ok; }
};

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), status_(Status::success()) {}
    Result(Status status) : status_(std::move(status)) {}

    bool ok() const { return status_.ok; }
    explicit operator bool() const { return ok(); }
    const Status& status() const { return status_; }
    const std::string& error() const { return status_.message; }

    T& value() { return value_; }
    const T& value() const { return value_; }

private:
    T value_{};
    Status status_;
};

}  // namespace monix::renderer_vk
