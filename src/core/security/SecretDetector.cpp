#include "SecretDetector.hpp"

#include <algorithm>
#include <regex>

namespace monix::security {

const char* SecretTypeName(SecretType st) {
  switch (st) {
    case SecretType::Password:          return "Password";
    case SecretType::Token:             return "Token";
    case SecretType::ApiKey:            return "ApiKey";
    case SecretType::PrivateKey:        return "PrivateKey";
    case SecretType::ConnectionString:  return "ConnectionString";
    case SecretType::Certificate:       return "Certificate";
    case SecretType::EmailAddress:      return "EmailAddress";
    case SecretType::CreditCard:        return "CreditCard";
    case SecretType::SSN:               return "SSN";
    case SecretType::IpAddress:         return "IpAddress";
    case SecretType::FilePath:          return "FilePath";
  }
  return "Unknown";
}

const char* RedactionLevelName(RedactionLevel rl) {
  switch (rl) {
    case RedactionLevel::None:    return "None";
    case RedactionLevel::Partial: return "Partial";
    case RedactionLevel::Full:    return "Full";
    case RedactionLevel::Hash:    return "Hash";
  }
  return "Unknown";
}

bool SecretPattern::isValid() const {
  return !name.empty() && !regex_pattern.empty();
}

bool DetectedSecret::isValid() const {
  return !field_name.empty() && length > 0;
}

bool PrivacyPolicy::shouldRedact(SecretType type) const {
  switch (type) {
    case SecretType::Password:          return redact_passwords;
    case SecretType::Token:             return redact_tokens;
    case SecretType::ApiKey:            return redact_api_keys;
    case SecretType::PrivateKey:        return redact_private_keys;
    case SecretType::ConnectionString:  return redact_connection_strings;
    case SecretType::Certificate:       return redact_certificates;
    case SecretType::EmailAddress:      return redact_email_addresses;
    case SecretType::CreditCard:        return redact_credit_cards;
    case SecretType::SSN:               return redact_ssn;
    case SecretType::IpAddress:         return redact_ip_addresses;
    case SecretType::FilePath:          return redact_file_paths;
  }
  return false;
}

bool PrivacyPolicy::isValid() const {
  return true;
}

std::string ScanResult::summary() const {
  return "scanned=" + std::to_string(fields_scanned) +
    " found=" + std::to_string(secrets_found);
}

SecretDetector::SecretDetector() {
  built_in_patterns_.push_back({SecretType::Password, "password",
    "(password|passwd|pwd)\\s*[=:]\\s*\\S+", "Password in config", RedactionLevel::Full});
  built_in_patterns_.push_back({SecretType::Token, "bearer_token",
    "(bearer|token|auth)\\s*[=:]\\s*[A-Za-z0-9\\-._~+/]+=*", "Auth token", RedactionLevel::Full});
  built_in_patterns_.push_back({SecretType::ApiKey, "api_key",
    "(api[_-]?key|apikey)\\s*[=:]\\s*\\S+", "API key", RedactionLevel::Full});
  built_in_patterns_.push_back({SecretType::ConnectionString, "connection_string",
    "(connection[_-]?string|conn[_-]?str)\\s*[=:]\\s*\\S+", "Connection string", RedactionLevel::Partial});
  built_in_patterns_.push_back({SecretType::PrivateKey, "private_key",
    "-----BEGIN (RSA |EC |DSA )?PRIVATE KEY-----", "Private key", RedactionLevel::Full});
  built_in_patterns_.push_back({SecretType::CreditCard, "credit_card",
    "\\b[0-9]{4}[- ]?[0-9]{4}[- ]?[0-9]{4}[- ]?[0-9]{4}\\b", "Credit card number", RedactionLevel::Partial});
  built_in_patterns_.push_back({SecretType::SSN, "ssn",
    "\\b[0-9]{3}-[0-9]{2}-[0-9]{4}\\b", "Social Security Number", RedactionLevel::Full});
}

SecretDetector::~SecretDetector() {}

void SecretDetector::setPolicy(const PrivacyPolicy& policy) {
  std::lock_guard<std::mutex> lock(mu_);
  policy_ = policy;
}

PrivacyPolicy SecretDetector::getPolicy() const {
  std::lock_guard<std::mutex> lock(mu_);
  return policy_;
}

void SecretDetector::addPattern(const SecretPattern& pattern) {
  std::lock_guard<std::mutex> lock(mu_);
  if (pattern.isValid()) {
    custom_patterns_.push_back(pattern);
  }
}

ScanResult SecretDetector::scanValue(const std::string& field_name, const std::string& value) const {
  std::lock_guard<std::mutex> lock(mu_);
  ScanResult result;
  result.fields_scanned = 1;
  total_scans_++;

  for (const auto& pattern : built_in_patterns_) {
    if (!policy_.shouldRedact(pattern.type)) continue;
    if (matchesPattern(value, pattern)) {
      DetectedSecret secret;
      secret.type = pattern.type;
      secret.field_name = field_name;
      secret.original_value = value;
      secret.redacted_value = redact(value, pattern.default_redaction);
      secret.position = 0;
      secret.length = value.size();
      secret.redaction = pattern.default_redaction;
      result.secrets.push_back(secret);
      result.secrets_found++;
      total_secrets_++;
    }
  }

  for (const auto& pattern : custom_patterns_) {
    if (!policy_.shouldRedact(pattern.type)) continue;
    if (matchesPattern(value, pattern)) {
      DetectedSecret secret;
      secret.type = pattern.type;
      secret.field_name = field_name;
      secret.original_value = value;
      secret.redacted_value = redact(value, pattern.default_redaction);
      secret.position = 0;
      secret.length = value.size();
      secret.redaction = pattern.default_redaction;
      result.secrets.push_back(secret);
      result.secrets_found++;
      total_secrets_++;
    }
  }

  result.has_secrets = !result.secrets.empty();
  return result;
}

ScanResult SecretDetector::scanFields(const std::unordered_map<std::string, std::string>& fields) const {
  ScanResult total;
  for (const auto& [key, value] : fields) {
    auto result = scanValue(key, value);
    total.fields_scanned += result.fields_scanned;
    total.secrets.insert(total.secrets.end(), result.secrets.begin(), result.secrets.end());
  }
  total.secrets_found = total.secrets.size();
  total.has_secrets = !total.secrets.empty();
  return total;
}

ScanResult SecretDetector::scanText(const std::string& text) const {
  return scanValue("text", text);
}

std::string SecretDetector::redact(const std::string& value, RedactionLevel level) const {
  if (value.empty()) return value;
  switch (level) {
    case RedactionLevel::None:
      return value;
    case RedactionLevel::Partial:
      if (value.size() <= 4) return "****";
      return value.substr(0, 2) + std::string(value.size() - 4, '*') + value.substr(value.size() - 2);
    case RedactionLevel::Full:
      return std::string(value.size(), '*');
    case RedactionLevel::Hash:
      return "[HASH:" + std::to_string(std::hash<std::string>{}(value)) + "]";
  }
  return value;
}

std::string SecretDetector::redactField(const std::string& field_name, const std::string& value) const {
  auto result = scanValue(field_name, value);
  if (!result.has_secrets) return value;
  return redact(value, result.secrets[0].redaction);
}

std::size_t SecretDetector::totalScans() const {
  return total_scans_;
}

std::size_t SecretDetector::totalSecretsFound() const {
  return total_secrets_;
}

void SecretDetector::clearPatterns() {
  std::lock_guard<std::mutex> lock(mu_);
  custom_patterns_.clear();
}

bool SecretDetector::matchesPattern(const std::string& value, const SecretPattern& pattern) const {
  try {
    std::regex re(pattern.regex_pattern, std::regex::icase);
    return std::regex_search(value, re);
  } catch (...) {
    return false;
  }
}

}  // namespace monix::security
