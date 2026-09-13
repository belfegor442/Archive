#pragma once

#include "IEventValidator.hpp"

#include <unordered_set>
#include <mutex>

namespace monix::validation {

class SemanticValidator : public IEventValidator {
public:
  ValidationResult validate(const events::Event& event,
                            const ValidationContext& ctx) override;
  const char* name() const override { return "SemanticValidator"; }
  const char* version() const override { return "3.0.0"; }

  void clearSeenIds();
  std::size_t seenIdCount() const;

private:
  mutable std::mutex mu_;
  std::unordered_set<std::string> seenIds_;
};

}  // namespace monix::validation
