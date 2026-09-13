#include "IntegrityValidator.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace monix::validation {

std::string IntegrityHash::hex() const {
  std::ostringstream oss;
  for (auto b : digest) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
  }
  return oss.str();
}

std::string IntegrityValidator::canonicalize(const events::Event& event) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"id\":\"" << event.id.toString() << "\"";
  oss << ",\"type\":\"" << event.type.qualifiedName() << "\"";
  oss << ",\"severity\":" << static_cast<int>(event.severity);
  oss << ",\"sequence\":" << event.sequence;
  oss << ",\"time\":{\"occurrence\":" << event.time.occurrence
      << ",\"ingestion\":" << event.time.ingestion << "}";
  oss << ",\"source\":{\"id\":\"" << event.source.id
      << "\",\"kind\":" << static_cast<int>(event.source.kind) << "}";

  if (event.hasActor()) {
    const auto& a = event.actor.value();
    oss << ",\"actor\":{\"id\":\"" << a.id << "\",\"kind\":" << static_cast<int>(a.kind);
    if (a.process_id.has_value()) oss << ",\"pid\":" << a.process_id.value();
    oss << "}";
  }

  if (event.hasEntity()) {
    const auto& e = event.entity.value();
    oss << ",\"entity\":{\"id\":\"" << e.id << "\",\"kind\":" << static_cast<int>(e.kind);
    if (e.name.has_value()) oss << ",\"name\":\"" << e.name.value() << "\"";
    oss << "}";
  }

  oss << ",\"provenance\":{\"source_id\":\"" << event.provenance.source_id
      << "\",\"schema_name\":\"" << event.provenance.schema_name
      << "\",\"schema_version\":" << event.provenance.schema_version
      << ",\"kind\":" << static_cast<int>(event.provenance.kind) << "}";

  oss << ",\"flags\":" << static_cast<std::uint32_t>(event.flags);
  oss << "}";

  return oss.str();
}

IntegrityHash IntegrityValidator::computeHash(const events::Event& event) {
  IntegrityHash result;
  result.algorithm = "SHA-256";

  std::string canonical = canonicalize(event);

  BCRYPT_ALG_HANDLE algHandle = nullptr;
  BCRYPT_HASH_HANDLE hashHandle = nullptr;

  NTSTATUS status = BCryptOpenAlgorithmProvider(&algHandle, BCRYPT_SHA256_ALGORITHM,
                                                  nullptr, 0);
  if (!BCRYPT_SUCCESS(status)) return result;

  status = BCryptCreateHash(algHandle, &hashHandle, nullptr, 0, nullptr, 0, 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return result;
  }

  status = BCryptHashData(hashHandle,
                          reinterpret_cast<PUCHAR>(const_cast<char*>(canonical.data())),
                          static_cast<ULONG>(canonical.size()), 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptDestroyHash(hashHandle);
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return result;
  }

  result.digest.resize(32);
  status = BCryptFinishHash(hashHandle, result.digest.data(), 32, 0);

  BCryptDestroyHash(hashHandle);
  BCryptCloseAlgorithmProvider(algHandle, 0);

  if (!BCRYPT_SUCCESS(status)) {
    result.digest.clear();
  }

  return result;
}

ValidationResult IntegrityValidator::validate(const events::Event& event,
                                               const ValidationContext& ctx) {
  auto start = std::chrono::steady_clock::now();
  ValidationResult result;

  if (!ctx.policy.verify_integrity) {
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return result;
  }

  IntegrityHash computed = computeHash(event);
  if (computed.empty()) {
    result.addIssue(ValidationIssueCode::IntegrityNotComputed,
                    ValidationSeverity::Warning, "integrity",
                    "Could not compute integrity hash");
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    result.status = ValidationStatus::ValidWithWarnings;
    return result;
  }

  auto end = std::chrono::steady_clock::now();
  result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  result.status = ValidationStatus::Valid;
  return result;
}

}  // namespace monix::validation
