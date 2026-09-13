#pragma once

#include "IEventValidator.hpp"
#include "EventSchemaRegistry.hpp"

namespace monix::validation {

class SchemaValidator : public IEventValidator {
public:
  ValidationResult validate(const events::Event& event,
                            const ValidationContext& ctx) override;
  const char* name() const override { return "SchemaValidator"; }
  const char* version() const override { return "3.0.0"; }
};

}  // namespace monix::validation
