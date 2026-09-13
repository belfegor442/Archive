#include "ProcessIdentity.hpp"

#include <sstream>
#include <iomanip>

namespace monix::collectors::proc {

ProcessIdentity::ProcessIdentity(ProcessId pid, std::int64_t creation_time_ms)
  : pid_(pid), creation_time_ms_(creation_time_ms) {
  id_ = compute(pid, creation_time_ms);
}

ProcessInstanceId ProcessIdentity::compute(ProcessId pid, std::int64_t creation_time_ms) {
  if (pid == 0) return 0;

  std::uint64_t upper = static_cast<std::uint64_t>(creation_time_ms) & 0xFFFFFFFFFFFFULL;
  std::uint64_t lower = static_cast<std::uint64_t>(pid);
  return (upper << 32) | lower;
}

ProcessInstanceId ProcessIdentity::invalid() {
  return 0;
}

ProcessInstanceId ProcessIdentity::id() const {
  return id_;
}

ProcessId ProcessIdentity::pid() const {
  return pid_;
}

std::int64_t ProcessIdentity::creationTimeMs() const {
  return creation_time_ms_;
}

bool ProcessIdentity::isValid() const {
  return id_ != 0 && pid_ != 0;
}

bool ProcessIdentity::operator==(const ProcessIdentity& o) const {
  return id_ == o.id_;
}

bool ProcessIdentity::operator!=(const ProcessIdentity& o) const {
  return id_ != o.id_;
}

bool ProcessIdentity::operator<(const ProcessIdentity& o) const {
  return id_ < o.id_;
}

std::string ProcessIdentity::toString() const {
  std::ostringstream oss;
  oss << "PID:" << pid_ << "@" << creation_time_ms_;
  return oss.str();
}

}  // namespace monix::collectors::proc
