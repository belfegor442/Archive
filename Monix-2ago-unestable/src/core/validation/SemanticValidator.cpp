#include "SemanticValidator.hpp"

#include "../events/EventTime.hpp"

namespace monix::validation {

ValidationResult SemanticValidator::validate(const events::Event& event,
                                             const ValidationContext& ctx) {
  auto start = std::chrono::steady_clock::now();
  ValidationResult result;

  if (event.time.occurrence > 0 && ctx.now > 0) {
    auto futureMs = event.time.occurrence - ctx.now;
    if (futureMs > ctx.policy.max_future_skew.count() * 1000) {
      result.addIssue(ValidationIssueCode::FutureTimestamp,
                      ValidationSeverity::Warning, "time.occurrence",
                      "Timestamp is significantly in the future");
    }

    auto pastMs = ctx.now - event.time.occurrence;
    if (pastMs > ctx.policy.max_past_skew.count() * 1000) {
      result.addIssue(ValidationIssueCode::InvalidTimestamp,
                      ValidationSeverity::Warning, "time.occurrence",
                      "Timestamp is significantly in the past");
    }

    if (event.time.ingestion > 0) {
      auto skew = event.time.ingestion - event.time.occurrence;
      if (skew > ctx.policy.max_future_skew.count() * 1000) {
        result.addIssue(ValidationIssueCode::ClockSkewDetected,
                        ValidationSeverity::Warning, "time",
                        "Ingestion timestamp ahead of occurrence by significant margin");
      }
    }
  }

  if (event.hasActor()) {
    const auto& actor = event.actor.value();
    if (actor.kind == events::ActorKind::Process && actor.process_id.has_value()) {
      if (actor.process_id.value() == 0) {
        result.addIssue(ValidationIssueCode::InvalidActor,
                        ValidationSeverity::Warning, "actor.process_id",
                        "Process ID is zero (kernel idle)");
      }
    }
    if (actor.kind == events::ActorKind::User && !actor.username.has_value()) {
      if (ctx.policy.profile != ValidationProfile::Fast) {
        result.addIssue(ValidationIssueCode::InvalidActor,
                        ValidationSeverity::Notice, "actor.username",
                        "User actor without username");
      }
    }
  }

  if (event.hasEntity()) {
    const auto& entity = event.entity.value();
    if (entity.kind == events::EntityKind::File &&
        entity.name.has_value() && entity.name->empty()) {
      result.addIssue(ValidationIssueCode::InvalidEntity,
                      ValidationSeverity::Error, "entity.name",
                      "File entity has empty name");
    }
    if (entity.kind == events::EntityKind::Socket &&
        entity.name.has_value() && entity.name->find("socket:") == std::string::npos &&
        entity.name->find(":") != std::string::npos) {
      result.addIssue(ValidationIssueCode::InvalidEntity,
                      ValidationSeverity::Warning, "entity.name",
                      "Socket entity name does not match expected format");
    }
  }

  if (event.hasActor() && event.hasEntity()) {
    const auto& actor = event.actor.value();
    const auto& entity = event.entity.value();
    if (event.type.namespace_name == "filesystem") {
      if (actor.kind == events::ActorKind::Unknown &&
          entity.kind == events::EntityKind::Process) {
        result.addIssue(ValidationIssueCode::ActorEntityMismatch,
                        ValidationSeverity::Warning, "actor,entity",
                        "Filesystem event has Process entity");
      }
    }
  }

  if (event.action.has_value() && event.type.namespace_name == "filesystem") {
    const auto& action = event.action.value();
    if (event.type.name == "created" &&
        (action.name == "delete" || action.name == "removed")) {
      result.addIssue(ValidationIssueCode::ActionTypeMismatch,
                      ValidationSeverity::Error, "action",
                      "Action contradicts event type");
    }
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    std::string idStr = event.id.toString();
    if (seenIds_.count(idStr) > 0) {
      result.addIssue(ValidationIssueCode::DuplicateEventId,
                      ValidationSeverity::Warning, "id",
                      "Duplicate event ID detected");
    }
    seenIds_.insert(idStr);
  }

  auto end = std::chrono::steady_clock::now();
  result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  bool hasError = false;
  for (const auto& issue : result.issues) {
    if (issue.severity == ValidationSeverity::Error ||
        issue.severity == ValidationSeverity::Fatal) {
      hasError = true;
      break;
    }
  }
  result.status = hasError ? ValidationStatus::Invalid
                           : (result.issues.empty() ? ValidationStatus::Valid
                                                     : ValidationStatus::ValidWithWarnings);
  return result;
}

void SemanticValidator::clearSeenIds() {
  std::lock_guard<std::mutex> lock(mu_);
  seenIds_.clear();
}

std::size_t SemanticValidator::seenIdCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return seenIds_.size();
}

}  // namespace monix::validation
