#pragma once

#include "IEventValidator.hpp"

#include <string>
#include <vector>
#include <cstdint>

namespace monix::validation {

struct IntegrityHash {
  std::string algorithm;
  std::vector<std::uint8_t> digest;
  std::string hex() const;
  bool empty() const { return digest.empty(); }
};

class IntegrityValidator : public IEventValidator {
public:
  ValidationResult validate(const events::Event& event,
                            const ValidationContext& ctx) override;
  const char* name() const override { return "IntegrityValidator"; }
  const char* version() const override { return "3.0.0"; }

  static IntegrityHash computeHash(const events::Event& event);
  static std::string canonicalize(const events::Event& event);
};

}  // namespace monix::validation
