#pragma once

#include "ValidationResult.hpp"
#include "ValidationContext.hpp"
#include "../events/Event.hpp"

namespace monix::validation {

class IEventValidator {
public:
  virtual ~IEventValidator() = default;
  virtual ValidationResult validate(const events::Event& event,
                                    const ValidationContext& ctx) = 0;
  virtual const char* name() const = 0;
  virtual const char* version() const = 0;
};

}  // namespace monix::validation
