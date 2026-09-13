#include "FilesystemScope.hpp"

#include <algorithm>
#include <cctype>

namespace monix::collectors::fs {

static std::string toLower(const std::string& s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

static bool matchesGlob(const std::string& pattern, const std::string& text) {
  std::string patLower = toLower(pattern);
  std::string txtLower = toLower(text);

  std::size_t pi = 0, ti = 0;
  std::size_t starPi = std::string::npos, starTi = 0;

  while (ti < txtLower.size()) {
    if (pi < patLower.size() && (patLower[pi] == '?' || patLower[pi] == txtLower[ti])) {
      pi++;
      ti++;
    } else if (pi < patLower.size() && patLower[pi] == '*') {
      starPi = pi++;
      starTi = ti;
    } else if (starPi != std::string::npos) {
      pi = starPi + 1;
      ti = ++starTi;
    } else {
      return false;
    }
  }

  while (pi < patLower.size() && patLower[pi] == '*') pi++;

  return pi == patLower.size();
}

const char* HiddenFilePolicyName(HiddenFilePolicy p) {
  switch (p) {
    case HiddenFilePolicy::Include: return "Include";
    case HiddenFilePolicy::Exclude: return "Exclude";
    case HiddenFilePolicy::Auto:    return "Auto";
  }
  return "Unknown";
}

HiddenFilePolicy HiddenFilePolicyFromString(const char* s) {
  std::string lower = toLower(s);
  if (lower == "include") return HiddenFilePolicy::Include;
  if (lower == "exclude") return HiddenFilePolicy::Exclude;
  if (lower == "auto")    return HiddenFilePolicy::Auto;
  return HiddenFilePolicy::Exclude;
}

FilesystemScopeConfig FilesystemScopeConfig::defaults() {
  FilesystemScopeConfig cfg;
  cfg.include_paths = {};
  cfg.exclude_paths = {};
  cfg.include_extensions = {};
  cfg.exclude_extensions = {};
  cfg.hidden_policy = HiddenFilePolicy::Exclude;
  cfg.exclude_temp_dirs = true;
  cfg.exclude_system_dirs = true;
  cfg.max_recursion_depth = 20;
  cfg.max_entries_per_directory = 100000;
  cfg.max_total_entries = 1000000;
  return cfg;
}

FilesystemScope::FilesystemScope(FilesystemScopeConfig config)
  : config_(std::move(config)) {}

ScopeDecision FilesystemScope::evaluate(const std::filesystem::path& path) const {
  auto r = checkIncludePaths(path);
  if (r != ScopeDecision::Include) return r;

  r = checkExcludePaths(path);
  if (r != ScopeDecision::Include) return r;

  r = checkExtensions(path);
  if (r != ScopeDecision::Include) return r;

  r = checkHiddenSystem(path);
  if (r != ScopeDecision::Include) return r;

  r = checkTempDirs(path);
  if (r != ScopeDecision::Include) return r;

  r = checkSystemDirs(path);
  if (r != ScopeDecision::Include) return r;

  r = checkLimits();
  if (r != ScopeDecision::Include) return r;

  return ScopeDecision::Include;
}

bool FilesystemScope::shouldRecurse(const std::filesystem::path& path, std::uint32_t current_depth) const {
  if (current_depth >= config_.max_recursion_depth) {
    return false;
  }

  if (isLoop(path)) {
    return false;
  }

  return true;
}

bool FilesystemScope::trackVisit(const std::filesystem::path& path) {
  auto key = path.lexically_normal().string();
  auto result = visited_.insert(key);

  auto parentKey = path.parent_path().lexically_normal().string();
  dir_paths_.insert(parentKey);
  dir_entry_counts_[parentKey]++;

  total_entries_++;

  return result.second;
}

bool FilesystemScope::isLoop(const std::filesystem::path& path) const {
  auto key = path.lexically_normal().string();
  return visited_.find(key) != visited_.end();
}

void FilesystemScope::reset() {
  visited_.clear();
  total_entries_ = 0;
  dir_paths_.clear();
  dir_entry_counts_.clear();
}

std::size_t FilesystemScope::totalEntries() const {
  return total_entries_;
}

std::size_t FilesystemScope::directoryEntries(const std::filesystem::path& dir) const {
  auto key = dir.lexically_normal().string();
  auto it = dir_entry_counts_.find(key);
  return it != dir_entry_counts_.end() ? it->second : 0;
}

std::size_t FilesystemScope::directoryCount() const {
  return dir_paths_.size();
}

const FilesystemScopeConfig& FilesystemScope::config() const {
  return config_;
}

void FilesystemScope::setConfig(FilesystemScopeConfig config) {
  config_ = std::move(config);
}

ScopeDecision FilesystemScope::checkIncludePaths(const std::filesystem::path& path) const {
  if (config_.include_paths.empty()) {
    return ScopeDecision::Include;
  }

  auto pathStr = path.lexically_normal().string();
  for (const auto& inc : config_.include_paths) {
    auto incNorm = std::filesystem::path(inc).lexically_normal().string();
    if (pathStr == incNorm || pathStr.find(incNorm + "/") == 0 ||
        pathStr.find(incNorm + "\\") == 0) {
      return ScopeDecision::Include;
    }
  }

  return ScopeDecision::Exclude;
}

ScopeDecision FilesystemScope::checkExcludePaths(const std::filesystem::path& path) const {
  auto pathStr = path.lexically_normal().string();
  for (const auto& exc : config_.exclude_paths) {
    auto excNorm = std::filesystem::path(exc).lexically_normal().string();
    if (pathStr == excNorm || pathStr.find(excNorm + "/") == 0 ||
        pathStr.find(excNorm + "\\") == 0) {
      return ScopeDecision::Exclude;
    }
  }

  return ScopeDecision::Include;
}

ScopeDecision FilesystemScope::checkExtensions(const std::filesystem::path& path) const {
  if (path.extension().empty()) {
    return ScopeDecision::Include;
  }

  auto ext = toLower(path.extension().string());

  if (!config_.include_extensions.empty()) {
    bool found = false;
    for (const auto& incExt : config_.include_extensions) {
      if (ext == toLower(incExt)) {
        found = true;
        break;
      }
    }
    if (!found) return ScopeDecision::Exclude;
  }

  for (const auto& excExt : config_.exclude_extensions) {
    if (ext == toLower(excExt)) {
      return ScopeDecision::Exclude;
    }
  }

  return ScopeDecision::Include;
}

ScopeDecision FilesystemScope::checkHiddenSystem(const std::filesystem::path& path) const {
  if (config_.hidden_policy == HiddenFilePolicy::Include) {
    return ScopeDecision::Include;
  }

  if (config_.hidden_policy == HiddenFilePolicy::Auto) {
    return ScopeDecision::Include;
  }

  if (isHiddenOrSystem(path)) {
    return ScopeDecision::Exclude;
  }

  return ScopeDecision::Include;
}

ScopeDecision FilesystemScope::checkTempDirs(const std::filesystem::path& path) const {
  if (!config_.exclude_temp_dirs) {
    return ScopeDecision::Include;
  }

  if (isTempPath(path)) {
    return ScopeDecision::Exclude;
  }

  return ScopeDecision::Include;
}

ScopeDecision FilesystemScope::checkSystemDirs(const std::filesystem::path& path) const {
  if (!config_.exclude_system_dirs) {
    return ScopeDecision::Include;
  }

  if (isSystemPath(path)) {
    return ScopeDecision::Exclude;
  }

  return ScopeDecision::Include;
}

ScopeDecision FilesystemScope::checkLimits() const {
  if (total_entries_ >= config_.max_total_entries) {
    return ScopeDecision::Exclude;
  }
  return ScopeDecision::Include;
}

bool FilesystemScope::isHiddenOrSystem(const std::filesystem::path& path) {
  for (const auto& part : path) {
    auto name = part.string();
    if (name.empty() || name == "." || name == "..") continue;

    if (name[0] == '.') return true;

    auto nameLower = toLower(name);
    static const std::vector<std::string> systemNames = {
      "desktop.ini", "thumbs.db", ".ds_store",
      "$recycle.bin", "system volume information"
    };
    for (const auto& sn : systemNames) {
      if (nameLower == sn) return true;
    }
  }

  return false;
}

bool FilesystemScope::isTempPath(const std::filesystem::path& path) {
  auto pathStr = path.lexically_normal().string();
  auto pathLower = toLower(pathStr);

  static const std::vector<std::string> tempPrefixes = {
    "/tmp/", "\\tmp\\",
    "/temp/", "\\temp\\",
    "/var/tmp/", "\\var\\tmp\\",
    "/private/tmp/", "\\private\\tmp\\"
  };

  for (const auto& prefix : tempPrefixes) {
    if (pathLower.find(prefix) == 0) return true;
  }

  auto tempDir = std::filesystem::temp_directory_path().lexically_normal().string();
  auto tempLower = toLower(tempDir);
  if (pathLower.find(tempLower) == 0) return true;

  return false;
}

bool FilesystemScope::isSystemPath(const std::filesystem::path& path) {
  auto pathStr = path.lexically_normal().string();
  auto pathLower = toLower(pathStr);

  static const std::vector<std::string> systemPrefixes = {
    "/proc/", "\\proc\\",
    "/sys/", "\\sys\\",
    "/dev/", "\\dev\\",
    "/etc/ssl/", "\\etc\\ssl\\"
  };

  for (const auto& prefix : systemPrefixes) {
    if (pathLower.find(prefix) == 0) return true;
  }

  return false;
}

}  // namespace monix::collectors::fs
