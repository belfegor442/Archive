#include "EventValidationPipeline.hpp"
#include "StructuralValidator.hpp"
#include "SchemaValidator.hpp"
#include "SemanticValidator.hpp"
#include "IntegrityValidator.hpp"

#include <chrono>

namespace monix::validation {

EventValidationPipeline::EventValidationPipeline(ValidationPolicy policy)
  : policy_(policy) {
  validators_.push_back(std::make_unique<StructuralValidator>());
  validators_.push_back(std::make_unique<SchemaValidator>());
  validators_.push_back(std::make_unique<SemanticValidator>());
  validators_.push_back(std::make_unique<IntegrityValidator>());
}

void EventValidationPipeline::setSchemaRegistry(const EventSchemaRegistry* schemas) {
  schemas_ = schemas;
}

void EventValidationPipeline::setSourceRegistry(const SourceRegistry* sources) {
  sources_ = sources;
}

void EventValidationPipeline::setPolicy(const ValidationPolicy& policy) {
  policy_ = policy;
}

ValidationResult EventValidationPipeline::validate(const events::Event& event) {
  auto start = std::chrono::steady_clock::now();
  ValidationContext ctx;
  ctx.profile = policy_.profile;
  ctx.policy = policy_;
  ctx.now = events::currentSystemTimeMs();
  ctx.schemas = schemas_ ? schemas_ : &defaultSchemas_;
  ctx.sources = sources_ ? sources_ : &defaultSources_;

  ValidationResult combined;

  for (auto& validator : validators_) {
    ValidationResult r = validator->validate(event, ctx);
    combined.mergeFrom(r);

    bool hasFatal = false;
    for (const auto& issue : r.issues) {
      if (issue.severity == ValidationSeverity::Fatal) {
        hasFatal = true;
        break;
      }
    }
    if (hasFatal) break;
  }

  auto end = std::chrono::steady_clock::now();
  combined.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return combined;
}

ValidationReport EventValidationPipeline::toReport(const events::Event& event,
                                                    const ValidationResult& result) const {
  ValidationReport report;
  report.event_id = event.id.toString();
  report.status = result.status;
  report.issues = result.issues;
  report.validator_version = result.validator_version;
  report.schema_version = std::to_string(event.provenance.schema_version);
  report.duration = result.duration;
  return report;
}

}  // namespace monix::validation
