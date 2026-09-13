#pragma once

#include "IEventValidator.hpp"
#include "ValidationPolicy.hpp"
#include "ValidationContext.hpp"
#include "EventSchemaRegistry.hpp"
#include "SourceRegistry.hpp"

#include <memory>
#include <vector>

namespace monix::validation {

class EventValidationPipeline {
public:
  explicit EventValidationPipeline(ValidationPolicy policy = ValidationPolicy::standard());

  void setSchemaRegistry(const EventSchemaRegistry* schemas);
  void setSourceRegistry(const SourceRegistry* sources);
  void setPolicy(const ValidationPolicy& policy);

  ValidationResult validate(const events::Event& event);
  ValidationReport toReport(const events::Event& event,
                            const ValidationResult& result) const;

private:
  ValidationPolicy policy_;
  EventSchemaRegistry defaultSchemas_;
  SourceRegistry defaultSources_;
  const EventSchemaRegistry* schemas_ = nullptr;
  const SourceRegistry* sources_ = nullptr;

  std::vector<std::unique_ptr<IEventValidator>> validators_;
};

}  // namespace monix::validation
