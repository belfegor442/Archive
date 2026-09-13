#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::security {

enum class SecretType : std::uint8_t {
  Password,
  Token,
  ApiKey,
  PrivateKey,
  ConnectionString,
  Certificate,
  EmailAddress,
  CreditCard,
  SSN,
  IpAddress,
  FilePath
};

const char* SecretTypeName(SecretType st);

enum class RedactionLevel : std::uint8_t {
  None,
  Partial,
  Full,
  Hash
};

const char* RedactionLevelName(RedactionLevel rl);

struct SecretPattern {
  SecretType type = SecretType::Password;
  std::string name;
  std::string regex_pattern;
  std::string description;
  RedactionLevel default_redaction = RedactionLevel::Full;

  bool isValid() const;
};

struct DetectedSecret {
  SecretType type = SecretType::Password;
  std::string field_name;
  std::string original_value;
  std::string redacted_value;
  std::size_t position = 0;
  std::size_t length = 0;
  RedactionLevel redaction = RedactionLevel::Full;

  bool isValid() const;
};

struct PrivacyPolicy {
  bool redact_passwords = true;
  bool redact_tokens = true;
  bool redact_api_keys = true;
  bool redact_private_keys = true;
  bool redact_connection_strings = true;
  bool redact_certificates = true;
  bool redact_email_addresses = false;
  bool redact_credit_cards = true;
  bool redact_ssn = true;
  bool redact_ip_addresses = false;
  bool redact_file_paths = false;
  bool exclude_file_content = true;
  bool exclude_command_line_secrets = true;

  bool shouldRedact(SecretType type) const;
  bool isValid() const;
};

struct ScanResult {
  std::vector<DetectedSecret> secrets;
  std::size_t fields_scanned = 0;
  std::size_t secrets_found = 0;
  bool has_secrets = false;

  std::string summary() const;
};

class SecretDetector {
public:
  SecretDetector();
  ~SecretDetector();

  SecretDetector(const SecretDetector&) = delete;
  SecretDetector& operator=(const SecretDetector&) = delete;

  void setPolicy(const PrivacyPolicy& policy);
  PrivacyPolicy getPolicy() const;

  void addPattern(const SecretPattern& pattern);

  ScanResult scanValue(const std::string& field_name, const std::string& value) const;
  ScanResult scanFields(const std::unordered_map<std::string, std::string>& fields) const;
  ScanResult scanText(const std::string& text) const;

  std::string redact(const std::string& value, RedactionLevel level) const;
  std::string redactField(const std::string& field_name, const std::string& value) const;

  std::size_t totalScans() const;
  std::size_t totalSecretsFound() const;

  void clearPatterns();

private:
  bool matchesPattern(const std::string& value, const SecretPattern& pattern) const;

  mutable std::mutex mu_;
  PrivacyPolicy policy_;
  std::vector<SecretPattern> built_in_patterns_;
  std::vector<SecretPattern> custom_patterns_;
  mutable std::atomic<std::size_t> total_scans_{0};
  mutable std::atomic<std::size_t> total_secrets_{0};
};

}  // namespace monix::security
