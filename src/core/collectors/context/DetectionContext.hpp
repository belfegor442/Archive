#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::context {

enum class ContextSource : std::uint8_t {
  Direct,
  Derived,
  Inferred,
  External,
  Cached
};

const char* ContextSourceName(ContextSource src);

enum class ContextConfidence : std::uint8_t {
  Certain,
  High,
  Medium,
  Low,
  Speculative
};

const char* ContextConfidenceName(ContextConfidence conf);

struct ContextEntry {
  std::string key;
  std::string value;
  ContextSource source = ContextSource::Direct;
  ContextConfidence confidence = ContextConfidence::Certain;
  std::int64_t obtained_ms = 0;
  std::int64_t expires_ms = 0;
  std::string provenance;

  bool isExpired(std::int64_t now_ms) const;
  bool isValid() const;
};

struct ProcessContext {
  std::string process_name;
  std::string process_path;
  std::uint64_t pid = 0;
  std::string reputation;
  std::string file_hash;
  std::string digital_signature;
  std::string publisher;
  bool is_signed = false;
  bool is_known_good = false;
  bool is_elevated = false;

  bool isValid() const;
};

struct FileContext {
  std::string file_path;
  std::string file_type;
  std::string extension;
  std::uint64_t size_bytes = 0;
  std::string hash_sha256;
  bool is_executable = false;
  bool is_temporary = false;
  bool is_hidden = false;
  std::string mime_type;

  bool isValid() const;
};

struct DeviceContext {
  std::string device_id;
  std::string device_type;
  std::string device_name;
  std::string vendor;
  std::string trust_level;
  bool is_external = false;
  bool is_known = false;
  std::string classification;

  bool isValid() const;
};

struct NetworkContext {
  std::string source_ip;
  std::string dest_ip;
  std::uint16_t source_port = 0;
  std::uint16_t dest_port = 0;
  std::string protocol;
  bool is_internal = false;
  bool is_encrypted = false;
  std::string geo_region;
  std::string threat_intel;

  bool isValid() const;
};

struct UserContext {
  std::string user_id;
  std::string user_name;
  std::string domain;
  bool is_admin = false;
  bool is_service = false;
  std::string risk_level;
  std::string department;

  bool isValid() const;
};

struct DetectionContext {
  ProcessContext process;
  FileContext file;
  DeviceContext device;
  NetworkContext network;
  UserContext user;
  std::unordered_map<std::string, ContextEntry> extra;
  std::int64_t assembled_ms = 0;

  std::string getExtra(const std::string& key) const;
  bool hasExtra(const std::string& key) const;
  void setExtra(const std::string& key, const std::string& value,
                ContextSource src = ContextSource::Direct,
                ContextConfidence conf = ContextConfidence::Certain);
  bool isValid() const;
  std::size_t entryCount() const;
};

class DetectionContextBuilder {
public:
  DetectionContextBuilder();
  ~DetectionContextBuilder();

  DetectionContextBuilder(const DetectionContextBuilder&) = delete;
  DetectionContextBuilder& operator=(const DetectionContextBuilder&) = delete;

  void setProcess(const ProcessContext& ctx);
  void setFile(const FileContext& ctx);
  void setDevice(const DeviceContext& ctx);
  void setNetwork(const NetworkContext& ctx);
  void setUser(const UserContext& ctx);
  void addExtra(const std::string& key, const std::string& value,
                ContextSource src = ContextSource::Direct,
                ContextConfidence conf = ContextConfidence::Certain);

  DetectionContext build();

  void clear();
  std::size_t totalBuilt() const;

private:
  DetectionContext current_;
  std::atomic<std::size_t> total_built_{0};
};

}  // namespace monix::collectors::context
