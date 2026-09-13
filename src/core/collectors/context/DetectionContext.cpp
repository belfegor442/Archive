#include "DetectionContext.hpp"

#include <algorithm>

namespace monix::collectors::context {

const char* ContextSourceName(ContextSource src) {
  switch (src) {
    case ContextSource::Direct:    return "Direct";
    case ContextSource::Derived:   return "Derived";
    case ContextSource::Inferred:  return "Inferred";
    case ContextSource::External:  return "External";
    case ContextSource::Cached:    return "Cached";
  }
  return "Unknown";
}

const char* ContextConfidenceName(ContextConfidence conf) {
  switch (conf) {
    case ContextConfidence::Certain:     return "Certain";
    case ContextConfidence::High:        return "High";
    case ContextConfidence::Medium:      return "Medium";
    case ContextConfidence::Low:         return "Low";
    case ContextConfidence::Speculative: return "Speculative";
  }
  return "Unknown";
}

bool ContextEntry::isExpired(std::int64_t now_ms) const {
  if (expires_ms <= 0) return false;
  return now_ms > expires_ms;
}

bool ContextEntry::isValid() const {
  return !key.empty() && !value.empty();
}

bool ProcessContext::isValid() const {
  return !process_name.empty() || pid > 0;
}

bool FileContext::isValid() const {
  return !file_path.empty();
}

bool DeviceContext::isValid() const {
  return !device_id.empty();
}

bool NetworkContext::isValid() const {
  return !source_ip.empty() || !dest_ip.empty();
}

bool UserContext::isValid() const {
  return !user_id.empty() || !user_name.empty();
}

std::string DetectionContext::getExtra(const std::string& key) const {
  auto it = extra.find(key);
  if (it != extra.end()) return it->second.value;
  return "";
}

bool DetectionContext::hasExtra(const std::string& key) const {
  return extra.find(key) != extra.end();
}

void DetectionContext::setExtra(const std::string& key, const std::string& value,
                                ContextSource src, ContextConfidence conf) {
  ContextEntry entry;
  entry.key = key;
  entry.value = value;
  entry.source = src;
  entry.confidence = conf;
  entry.obtained_ms = 0;
  extra[key] = entry;
}

bool DetectionContext::isValid() const {
  return process.isValid() || file.isValid() || device.isValid() ||
         network.isValid() || user.isValid() || !extra.empty();
}

std::size_t DetectionContext::entryCount() const {
  std::size_t count = extra.size();
  if (process.isValid()) count++;
  if (file.isValid()) count++;
  if (device.isValid()) count++;
  if (network.isValid()) count++;
  if (user.isValid()) count++;
  return count;
}

DetectionContextBuilder::DetectionContextBuilder() {}
DetectionContextBuilder::~DetectionContextBuilder() {}

void DetectionContextBuilder::setProcess(const ProcessContext& ctx) {
  current_.process = ctx;
}

void DetectionContextBuilder::setFile(const FileContext& ctx) {
  current_.file = ctx;
}

void DetectionContextBuilder::setDevice(const DeviceContext& ctx) {
  current_.device = ctx;
}

void DetectionContextBuilder::setNetwork(const NetworkContext& ctx) {
  current_.network = ctx;
}

void DetectionContextBuilder::setUser(const UserContext& ctx) {
  current_.user = ctx;
}

void DetectionContextBuilder::addExtra(const std::string& key, const std::string& value,
                                       ContextSource src, ContextConfidence conf) {
  current_.setExtra(key, value, src, conf);
}

DetectionContext DetectionContextBuilder::build() {
  DetectionContext result = current_;
  result.assembled_ms = 0;
  total_built_++;
  return result;
}

void DetectionContextBuilder::clear() {
  current_ = DetectionContext{};
}

std::size_t DetectionContextBuilder::totalBuilt() const {
  return total_built_;
}

}  // namespace monix::collectors::context
