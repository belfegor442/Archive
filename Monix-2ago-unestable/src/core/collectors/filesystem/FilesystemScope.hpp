#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <filesystem>

namespace monix::collectors::fs {

enum class HiddenFilePolicy : std::uint8_t {
  Include,
  Exclude,
  Auto
};

const char* HiddenFilePolicyName(HiddenFilePolicy p);
HiddenFilePolicy HiddenFilePolicyFromString(const char* s);

enum class ScopeDecision : std::uint8_t {
  Include,
  Exclude,
  SkipSubtree
};

struct FilesystemScopeConfig {
  std::vector<std::string> include_paths;
  std::vector<std::string> exclude_paths;
  std::vector<std::string> include_extensions;
  std::vector<std::string> exclude_extensions;
  HiddenFilePolicy hidden_policy = HiddenFilePolicy::Exclude;
  bool exclude_temp_dirs = true;
  bool exclude_system_dirs = true;

  std::uint32_t max_recursion_depth = 20;
  std::size_t max_entries_per_directory = 100000;
  std::size_t max_total_entries = 1000000;

  static FilesystemScopeConfig defaults();
};

class FilesystemScope {
public:
  explicit FilesystemScope(FilesystemScopeConfig config = FilesystemScopeConfig::defaults());

  ScopeDecision evaluate(const std::filesystem::path& path) const;
  bool shouldRecurse(const std::filesystem::path& path, std::uint32_t current_depth) const;
  bool trackVisit(const std::filesystem::path& path);
  bool isLoop(const std::filesystem::path& path) const;
  void reset();

  std::size_t totalEntries() const;
  std::size_t directoryEntries(const std::filesystem::path& dir) const;
  std::size_t directoryCount() const;

  const FilesystemScopeConfig& config() const;
  void setConfig(FilesystemScopeConfig config);

private:
  ScopeDecision checkIncludePaths(const std::filesystem::path& path) const;
  ScopeDecision checkExcludePaths(const std::filesystem::path& path) const;
  ScopeDecision checkExtensions(const std::filesystem::path& path) const;
  ScopeDecision checkHiddenSystem(const std::filesystem::path& path) const;
  ScopeDecision checkTempDirs(const std::filesystem::path& path) const;
  ScopeDecision checkSystemDirs(const std::filesystem::path& path) const;
  ScopeDecision checkLimits() const;

  static bool isHiddenOrSystem(const std::filesystem::path& path);
  static bool isTempPath(const std::filesystem::path& path);
  static bool isSystemPath(const std::filesystem::path& path);

  FilesystemScopeConfig config_;
  mutable std::set<std::string> visited_;
  mutable std::size_t total_entries_ = 0;
  mutable std::set<std::string> dir_paths_;
  mutable std::map<std::string, std::size_t> dir_entry_counts_;
};

}  // namespace monix::collectors::fs
