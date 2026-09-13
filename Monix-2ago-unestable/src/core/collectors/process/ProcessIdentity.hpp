#pragma once

#include "ProcessTypes.hpp"

#include <cstdint>
#include <string>
#include <functional>

namespace monix::collectors::proc {

class ProcessIdentity {
public:
  ProcessIdentity() = default;
  ProcessIdentity(ProcessId pid, std::int64_t creation_time_ms);

  static ProcessInstanceId compute(ProcessId pid, std::int64_t creation_time_ms);
  static ProcessInstanceId invalid();

  ProcessInstanceId id() const;
  ProcessId pid() const;
  std::int64_t creationTimeMs() const;
  bool isValid() const;

  bool operator==(const ProcessIdentity& o) const;
  bool operator!=(const ProcessIdentity& o) const;
  bool operator<(const ProcessIdentity& o) const;

  std::string toString() const;

private:
  ProcessInstanceId id_ = 0;
  ProcessId pid_ = 0;
  std::int64_t creation_time_ms_ = 0;
};

}  // namespace monix::collectors::proc

namespace std {
template<>
struct hash<monix::collectors::proc::ProcessIdentity> {
  std::size_t operator()(const monix::collectors::proc::ProcessIdentity& id) const {
    return std::hash<monix::collectors::proc::ProcessInstanceId>{}(id.id());
  }
};
}
