#include "ValidationPolicy.hpp"

namespace monix::validation {

ValidationPolicy ValidationPolicy::fast() {
  ValidationPolicy p;
  p.profile = ValidationProfile::Fast;
  p.reject_unknown_schema = false;
  p.unknown_fields = DropPolicy::Allow;
  p.verify_integrity = false;
  p.verify_sequence = false;
  p.max_future_skew = std::chrono::seconds(5);
  p.max_past_skew = std::chrono::seconds(60);
  return p;
}

ValidationPolicy ValidationPolicy::standard() {
  ValidationPolicy p;
  p.profile = ValidationProfile::Standard;
  p.reject_unknown_schema = false;
  p.unknown_fields = DropPolicy::Warn;
  p.verify_integrity = false;
  p.verify_sequence = false;
  p.max_future_skew = std::chrono::seconds(30);
  p.max_past_skew = std::chrono::seconds(86400);
  return p;
}

ValidationPolicy ValidationPolicy::strict() {
  ValidationPolicy p;
  p.profile = ValidationProfile::Strict;
  p.reject_unknown_schema = true;
  p.unknown_fields = DropPolicy::Reject;
  p.verify_integrity = true;
  p.verify_sequence = true;
  p.max_future_skew = std::chrono::seconds(5);
  p.max_past_skew = std::chrono::seconds(3600);
  return p;
}

ValidationPolicy ValidationPolicy::forensic() {
  ValidationPolicy p;
  p.profile = ValidationProfile::Forensic;
  p.reject_unknown_schema = true;
  p.unknown_fields = DropPolicy::Reject;
  p.verify_integrity = true;
  p.verify_sequence = true;
  p.max_future_skew = std::chrono::seconds(1);
  p.max_past_skew = std::chrono::seconds(600);
  return p;
}

}  // namespace monix::validation
