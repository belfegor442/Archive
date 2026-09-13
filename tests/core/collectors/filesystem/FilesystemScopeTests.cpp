#include "../../../core/collectors/filesystem/FilesystemScope.hpp"
#include "../../../core/collectors/filesystem/FilesystemTypes.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

using namespace monix::collectors::fs;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-62s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)
#define ASSERT_GE(a, b) do { if ((a) < (b)) { FAIL(#a " < " #b); return; } } while(0)

static std::filesystem::path testDir() {
  auto p = std::filesystem::temp_directory_path() / "monix_scope_test";
  std::error_code ec;
  std::filesystem::create_directories(p, ec);
  return p;
}

static void cleanupDir(const std::filesystem::path& p) {
  std::error_code ec;
  std::filesystem::remove_all(p, ec);
}

static void writeFile(const std::filesystem::path& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f.write(content.data(), static_cast<std::streamsize>(content.size()));
  f.close();
}

// ==================== HiddenFilePolicy Tests ====================

static void test_hidden_policy_names() {
  TEST("HiddenFilePolicy: all names");
  ASSERT_EQ(std::string(HiddenFilePolicyName(HiddenFilePolicy::Include)), "Include");
  ASSERT_EQ(std::string(HiddenFilePolicyName(HiddenFilePolicy::Exclude)), "Exclude");
  ASSERT_EQ(std::string(HiddenFilePolicyName(HiddenFilePolicy::Auto)), "Auto");
  PASS();
}

static void test_hidden_policy_from_string() {
  TEST("HiddenFilePolicy: fromString roundtrip");
  ASSERT_EQ(HiddenFilePolicyFromString("Include"), HiddenFilePolicy::Include);
  ASSERT_EQ(HiddenFilePolicyFromString("EXCLUDE"), HiddenFilePolicy::Exclude);
  ASSERT_EQ(HiddenFilePolicyFromString("auto"), HiddenFilePolicy::Auto);
  ASSERT_EQ(HiddenFilePolicyFromString("invalid"), HiddenFilePolicy::Exclude);
  PASS();
}

// ==================== ScopeConfig Tests ====================

static void test_scope_config_defaults() {
  TEST("FilesystemScopeConfig: defaults are safe");
  auto cfg = FilesystemScopeConfig::defaults();
  ASSERT_TRUE(cfg.include_paths.empty());
  ASSERT_TRUE(cfg.exclude_paths.empty());
  ASSERT_TRUE(cfg.include_extensions.empty());
  ASSERT_TRUE(cfg.exclude_extensions.empty());
  ASSERT_EQ(cfg.hidden_policy, HiddenFilePolicy::Exclude);
  ASSERT_TRUE(cfg.exclude_temp_dirs);
  ASSERT_TRUE(cfg.exclude_system_dirs);
  ASSERT_GE(cfg.max_recursion_depth, 1u);
  ASSERT_GE(cfg.max_entries_per_directory, 1000u);
  ASSERT_GE(cfg.max_total_entries, 10000u);
  PASS();
}

// ==================== Include Paths Tests ====================

static void test_include_path_match() {
  TEST("Scope: include path match");
  FilesystemScopeConfig cfg;
  cfg.include_paths.push_back("/data/projects");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/projects/test.txt"), ScopeDecision::Include);
  PASS();
}

static void test_include_path_no_match() {
  TEST("Scope: include path no match excludes");
  FilesystemScopeConfig cfg;
  cfg.include_paths.push_back("/data/projects");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/other/path/file.txt"), ScopeDecision::Exclude);
  PASS();
}

static void test_include_path_partial_match() {
  TEST("Scope: include path partial prefix match");
  FilesystemScopeConfig cfg;
  cfg.include_paths.push_back("/data/proj");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/projects/file.txt"), ScopeDecision::Exclude);
  PASS();
}

static void test_include_empty_accepts_all() {
  TEST("Scope: empty include paths accepts all");
  FilesystemScopeConfig cfg;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/any/path/file.txt"), ScopeDecision::Include);
  PASS();
}

// ==================== Exclude Paths Tests ====================

static void test_exclude_path_match() {
  TEST("Scope: exclude path match");
  FilesystemScopeConfig cfg;
  cfg.exclude_paths.push_back("/data/secret");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/secret/file.txt"), ScopeDecision::Exclude);
  PASS();
}

static void test_exclude_path_no_match() {
  TEST("Scope: exclude path no match allows");
  FilesystemScopeConfig cfg;
  cfg.exclude_paths.push_back("/data/secret");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/public/file.txt"), ScopeDecision::Include);
  PASS();
}

static void test_exclude_overrides_include() {
  TEST("Scope: exclude overrides include");
  FilesystemScopeConfig cfg;
  cfg.include_paths.push_back("/data");
  cfg.exclude_paths.push_back("/data/secret");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/public.txt"), ScopeDecision::Include);
  ASSERT_EQ(scope.evaluate("/data/secret/file.txt"), ScopeDecision::Exclude);
  PASS();
}

// ==================== Extension Tests ====================

static void test_include_extension_match() {
  TEST("Scope: include extension match");
  FilesystemScopeConfig cfg;
  cfg.include_extensions.push_back(".cpp");
  cfg.include_extensions.push_back(".hpp");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/src/main.cpp"), ScopeDecision::Include);
  PASS();
}

static void test_include_extension_no_match() {
  TEST("Scope: include extension no match excludes");
  FilesystemScopeConfig cfg;
  cfg.include_extensions.push_back(".cpp");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/src/main.txt"), ScopeDecision::Exclude);
  PASS();
}

static void test_exclude_extension_match() {
  TEST("Scope: exclude extension match");
  FilesystemScopeConfig cfg;
  cfg.exclude_extensions.push_back(".log");
  cfg.exclude_extensions.push_back(".tmp");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/app.log"), ScopeDecision::Exclude);
  PASS();
}

static void test_exclude_extension_no_match() {
  TEST("Scope: exclude extension no match allows");
  FilesystemScopeConfig cfg;
  cfg.exclude_extensions.push_back(".log");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/app.cpp"), ScopeDecision::Include);
  PASS();
}

static void test_extension_case_insensitive() {
  TEST("Scope: extension matching is case-insensitive");
  FilesystemScopeConfig cfg;
  cfg.include_extensions.push_back(".CPP");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/src/main.cpp"), ScopeDecision::Include);
  ASSERT_EQ(scope.evaluate("/src/main.CPP"), ScopeDecision::Include);
  PASS();
}

static void test_directory_bypasses_extension_filter() {
  TEST("Scope: directories bypass extension filter");
  FilesystemScopeConfig cfg;
  cfg.include_extensions.push_back(".cpp");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/src/subdir"), ScopeDecision::Include);
  PASS();
}

static void test_exclude_ext_overrides_include_ext() {
  TEST("Scope: exclude extension overrides include");
  FilesystemScopeConfig cfg;
  cfg.include_extensions.push_back(".cpp");
  cfg.exclude_extensions.push_back(".bak");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/src/main.cpp"), ScopeDecision::Include);
  ASSERT_EQ(scope.evaluate("/src/main.cpp.bak"), ScopeDecision::Exclude);
  PASS();
}

// ==================== Hidden/System Policy Tests ====================

static void test_hidden_exclude_policy() {
  TEST("Scope: hidden file exclude policy");
  FilesystemScopeConfig cfg;
  cfg.hidden_policy = HiddenFilePolicy::Exclude;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/.hidden"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/data/normal.txt"), ScopeDecision::Include);
  PASS();
}

static void test_hidden_include_policy() {
  TEST("Scope: hidden file include policy");
  FilesystemScopeConfig cfg;
  cfg.hidden_policy = HiddenFilePolicy::Include;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/.hidden"), ScopeDecision::Include);
  PASS();
}

static void test_hidden_auto_policy() {
  TEST("Scope: hidden file auto policy");
  FilesystemScopeConfig cfg;
  cfg.hidden_policy = HiddenFilePolicy::Auto;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/.hidden"), ScopeDecision::Include);
  PASS();
}

static void test_system_file_excluded() {
  TEST("Scope: system files excluded by default");
  auto cfg = FilesystemScopeConfig::defaults();
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/desktop.ini"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/data/Thumbs.db"), ScopeDecision::Exclude);
  PASS();
}

static void test_dotfile_parent_dir() {
  TEST("Scope: .dotfile directories excluded");
  FilesystemScopeConfig cfg;
  cfg.hidden_policy = HiddenFilePolicy::Exclude;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/.config/file.txt"), ScopeDecision::Exclude);
  PASS();
}

// ==================== Temp Dir Tests ====================

static void test_temp_dir_excluded() {
  TEST("Scope: temp directory excluded by default");
  auto cfg = FilesystemScopeConfig::defaults();
  FilesystemScope scope(cfg);

  auto tempDir = std::filesystem::temp_directory_path();
  ASSERT_EQ(scope.evaluate(tempDir / "test.txt"), ScopeDecision::Exclude);
  PASS();
}

static void test_temp_dir_included_when_disabled() {
  TEST("Scope: temp directory included when exclude_temp_dirs=false");
  FilesystemScopeConfig cfg;
  cfg.exclude_temp_dirs = false;
  FilesystemScope scope(cfg);

  auto tempDir = std::filesystem::temp_directory_path();
  ASSERT_EQ(scope.evaluate(tempDir / "test.txt"), ScopeDecision::Include);
  PASS();
}

// ==================== System Dir Tests ====================

static void test_system_dir_excluded() {
  TEST("Scope: system directory excluded by default");
  auto cfg = FilesystemScopeConfig::defaults();
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/proc/cpuinfo"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/sys/module/test"), ScopeDecision::Exclude);
  PASS();
}

static void test_system_dir_included_when_disabled() {
  TEST("Scope: system directory included when exclude_system_dirs=false");
  FilesystemScopeConfig cfg;
  cfg.exclude_system_dirs = false;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/proc/cpuinfo"), ScopeDecision::Include);
  PASS();
}

// ==================== Loop Detection Tests ====================

static void test_loop_detection_first_visit() {
  TEST("Scope: first visit is not a loop");
  FilesystemScope scope;
  ASSERT_TRUE(scope.trackVisit("/data/file.txt"));
  PASS();
}

static void test_loop_detection_duplicate_visit() {
  TEST("Scope: duplicate visit is a loop");
  FilesystemScope scope;
  scope.trackVisit("/data/file.txt");
  ASSERT_FALSE(scope.trackVisit("/data/file.txt"));
  PASS();
}

static void test_loop_detection_different_path() {
  TEST("Scope: different path is not a loop");
  FilesystemScope scope;
  scope.trackVisit("/data/file1.txt");
  ASSERT_TRUE(scope.trackVisit("/data/file2.txt"));
  PASS();
}

static void test_loop_detection_reset() {
  TEST("Scope: reset clears loop state");
  FilesystemScope scope;
  scope.trackVisit("/data/file.txt");
  scope.reset();
  ASSERT_TRUE(scope.trackVisit("/data/file.txt"));
  PASS();
}

static void test_isLoop_check() {
  TEST("Scope: isLoop check works");
  FilesystemScope scope;
  ASSERT_FALSE(scope.isLoop("/data/file.txt"));
  scope.trackVisit("/data/file.txt");
  ASSERT_TRUE(scope.isLoop("/data/file.txt"));
  PASS();
}

// ==================== Recursion Depth Tests ====================

static void test_recursion_depth_limit() {
  TEST("Scope: recursion depth limit");
  FilesystemScopeConfig cfg;
  cfg.max_recursion_depth = 3;
  FilesystemScope scope(cfg);

  ASSERT_TRUE(scope.shouldRecurse("/a/b", 0));
  ASSERT_TRUE(scope.shouldRecurse("/a/b/c", 2));
  ASSERT_FALSE(scope.shouldRecurse("/a/b/c/d", 3));
  ASSERT_FALSE(scope.shouldRecurse("/a/b/c/d/e", 4));
  PASS();
}

static void test_recursion_depth_with_loop() {
  TEST("Scope: recursion stops at loop even within depth");
  FilesystemScopeConfig cfg;
  cfg.max_recursion_depth = 100;
  FilesystemScope scope(cfg);

  scope.trackVisit("/data/dir1");
  ASSERT_FALSE(scope.shouldRecurse("/data/dir1", 0));
  PASS();
}

// ==================== Entry Limits Tests ====================

static void test_total_entries_limit() {
  TEST("Scope: total entries limit");
  FilesystemScopeConfig cfg;
  cfg.max_total_entries = 3;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/file1.txt"), ScopeDecision::Include);
  scope.trackVisit("/file1.txt");
  ASSERT_EQ(scope.evaluate("/file2.txt"), ScopeDecision::Include);
  scope.trackVisit("/file2.txt");
  ASSERT_EQ(scope.evaluate("/file3.txt"), ScopeDecision::Include);
  scope.trackVisit("/file3.txt");
  PASS();
}

static void test_directory_count_tracking() {
  TEST("Scope: directory count tracking");
  FilesystemScope scope;
  ASSERT_EQ(scope.directoryCount(), 0u);

  scope.trackVisit("/a/dir1/file.txt");
  scope.trackVisit("/b/dir2/file.txt");
  ASSERT_EQ(scope.directoryCount(), 2u);
  PASS();
}

// ==================== Combined Filter Tests ====================

static void test_combined_include_exclude_paths() {
  TEST("Scope: combined include + exclude paths");
  FilesystemScopeConfig cfg;
  cfg.include_paths.push_back("/data");
  cfg.exclude_paths.push_back("/data/secret");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/public.txt"), ScopeDecision::Include);
  ASSERT_EQ(scope.evaluate("/data/secret/file.txt"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/other/file.txt"), ScopeDecision::Exclude);
  PASS();
}

static void test_combined_extension_and_hidden() {
  TEST("Scope: combined extension + hidden policy");
  FilesystemScopeConfig cfg;
  cfg.include_extensions.push_back(".log");
  cfg.hidden_policy = HiddenFilePolicy::Exclude;
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("/data/app.log"), ScopeDecision::Include);
  ASSERT_EQ(scope.evaluate("/data/app.txt"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/data/.hidden.log"), ScopeDecision::Exclude);
  PASS();
}

static void test_scope_config_getter() {
  TEST("Scope: config getter returns current config");
  FilesystemScopeConfig cfg;
  cfg.include_paths.push_back("/test");
  cfg.max_recursion_depth = 5;
  FilesystemScope scope(cfg);

  const auto& got = scope.config();
  ASSERT_EQ(got.include_paths.size(), 1u);
  ASSERT_EQ(got.max_recursion_depth, 5u);
  PASS();
}

static void test_scope_config_setter() {
  TEST("Scope: config setter updates config");
  FilesystemScope scope;
  FilesystemScopeConfig cfg;
  cfg.exclude_paths.push_back("/blocked");
  scope.setConfig(cfg);

  ASSERT_EQ(scope.evaluate("/blocked/file.txt"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/allowed/file.txt"), ScopeDecision::Include);
  PASS();
}

// ==================== Integration Test ====================

static void test_scope_with_collector_config() {
  TEST("Scope: integrates with FilesystemConfig");
  FilesystemScopeConfig scopeCfg;
  scopeCfg.include_paths.push_back("/data");
  scopeCfg.exclude_extensions.push_back(".log");
  scopeCfg.hidden_policy = HiddenFilePolicy::Exclude;

  FilesystemConfig fsCfg = FilesystemConfig::defaults();
  fsCfg.scope = scopeCfg;

  FilesystemScope scope(fsCfg.scope);

  ASSERT_EQ(scope.evaluate("/data/test.cpp"), ScopeDecision::Include);
  ASSERT_EQ(scope.evaluate("/data/test.log"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/data/.hidden"), ScopeDecision::Exclude);
  ASSERT_EQ(scope.evaluate("/other/file.txt"), ScopeDecision::Exclude);
  PASS();
}

// ==================== Large Tree Protection Test ====================

static void test_large_tree_depth_protection() {
  TEST("Scope: large tree depth protection");
  FilesystemScopeConfig cfg;
  cfg.max_recursion_depth = 5;
  FilesystemScope scope(cfg);

  for (uint32_t i = 0; i < 10; i++) {
    std::string deepPath = "/root";
    for (uint32_t j = 0; j <= i; j++) {
      deepPath += "/level" + std::to_string(j);
    }

    if (i < 5) {
      ASSERT_TRUE(scope.shouldRecurse(deepPath, i));
    } else {
      ASSERT_FALSE(scope.shouldRecurse(deepPath, i));
    }
  }
  PASS();
}

// ==================== Symlink/Junction Behavior Test ====================

static void test_symlink_same_inode_loop_detection() {
  TEST("Scope: symlink to already-visited path detected as loop");
  FilesystemScope scope;

  ASSERT_TRUE(scope.trackVisit("/data/link_target"));
  ASSERT_TRUE(scope.isLoop("/data/link_target"));
  ASSERT_FALSE(scope.trackVisit("/data/link_target"));
  PASS();
}

// ==================== Windows Path Handling Test ====================

static void test_windows_path_handling() {
  TEST("Scope: Windows-style paths work");
  FilesystemScopeConfig cfg;
  cfg.include_paths.push_back("C:\\data\\projects");
  cfg.exclude_paths.push_back("C:\\data\\projects\\secret");
  FilesystemScope scope(cfg);

  ASSERT_EQ(scope.evaluate("C:\\data\\projects\\test.txt"), ScopeDecision::Include);
  ASSERT_EQ(scope.evaluate("C:\\data\\projects\\secret\\file.txt"), ScopeDecision::Exclude);
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Filesystem Scope Tests ===\n\n");

  printf("[HiddenFilePolicy Names]\n");
  test_hidden_policy_names();

  printf("[HiddenFilePolicy String]\n");
  test_hidden_policy_from_string();

  printf("[ScopeConfig]\n");
  test_scope_config_defaults();

  printf("[Include Paths]\n");
  test_include_path_match();
  test_include_path_no_match();
  test_include_path_partial_match();
  test_include_empty_accepts_all();

  printf("[Exclude Paths]\n");
  test_exclude_path_match();
  test_exclude_path_no_match();
  test_exclude_overrides_include();

  printf("[Include Extensions]\n");
  test_include_extension_match();
  test_include_extension_no_match();

  printf("[Exclude Extensions]\n");
  test_exclude_extension_match();
  test_exclude_extension_no_match();
  test_extension_case_insensitive();
  test_directory_bypasses_extension_filter();
  test_exclude_ext_overrides_include_ext();

  printf("[Hidden/System Policy]\n");
  test_hidden_exclude_policy();
  test_hidden_include_policy();
  test_hidden_auto_policy();
  test_system_file_excluded();
  test_dotfile_parent_dir();

  printf("[Temp Dirs]\n");
  test_temp_dir_excluded();
  test_temp_dir_included_when_disabled();

  printf("[System Dirs]\n");
  test_system_dir_excluded();
  test_system_dir_included_when_disabled();

  printf("[Loop Detection]\n");
  test_loop_detection_first_visit();
  test_loop_detection_duplicate_visit();
  test_loop_detection_different_path();
  test_loop_detection_reset();
  test_isLoop_check();

  printf("[Recursion Depth]\n");
  test_recursion_depth_limit();
  test_recursion_depth_with_loop();

  printf("[Entry Limits]\n");
  test_total_entries_limit();
  test_directory_count_tracking();

  printf("[Combined Filters]\n");
  test_combined_include_exclude_paths();
  test_combined_extension_and_hidden();
  test_scope_config_getter();
  test_scope_config_setter();

  printf("[Integration]\n");
  test_scope_with_collector_config();

  printf("[Protection]\n");
  test_large_tree_depth_protection();
  test_symlink_same_inode_loop_detection();
  test_windows_path_handling();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_FilesystemScopeTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"HiddenFilePolicy: names", test_hidden_policy_names},
  {"HiddenFilePolicy: fromString", test_hidden_policy_from_string},
  {"FilesystemScopeConfig: defaults", test_scope_config_defaults},
  {"Scope: include path match", test_include_path_match},
  {"Scope: include path no match", test_include_path_no_match},
  {"Scope: include path partial", test_include_path_partial_match},
  {"Scope: empty include accepts all", test_include_empty_accepts_all},
  {"Scope: exclude path match", test_exclude_path_match},
  {"Scope: exclude path no match", test_exclude_path_no_match},
  {"Scope: exclude overrides include", test_exclude_overrides_include},
  {"Scope: include extension match", test_include_extension_match},
  {"Scope: include extension no match", test_include_extension_no_match},
  {"Scope: exclude extension match", test_exclude_extension_match},
  {"Scope: exclude extension no match", test_exclude_extension_no_match},
  {"Scope: extension case insensitive", test_extension_case_insensitive},
  {"Scope: directory bypasses extension", test_directory_bypasses_extension_filter},
  {"Scope: exclude ext overrides include", test_exclude_ext_overrides_include_ext},
  {"Scope: hidden exclude", test_hidden_exclude_policy},
  {"Scope: hidden include", test_hidden_include_policy},
  {"Scope: hidden auto", test_hidden_auto_policy},
  {"Scope: system files excluded", test_system_file_excluded},
  {"Scope: dotfile parent dir", test_dotfile_parent_dir},
  {"Scope: temp dir excluded", test_temp_dir_excluded},
  {"Scope: temp dir included when disabled", test_temp_dir_included_when_disabled},
  {"Scope: system dir excluded", test_system_dir_excluded},
  {"Scope: system dir included when disabled", test_system_dir_included_when_disabled},
  {"Scope: loop detection first visit", test_loop_detection_first_visit},
  {"Scope: loop detection duplicate visit", test_loop_detection_duplicate_visit},
  {"Scope: loop detection different path", test_loop_detection_different_path},
  {"Scope: loop detection reset", test_loop_detection_reset},
  {"Scope: isLoop check", test_isLoop_check},
  {"Scope: recursion depth limit", test_recursion_depth_limit},
  {"Scope: recursion with loop", test_recursion_depth_with_loop},
  {"Scope: total entries limit", test_total_entries_limit},
  {"Scope: directory count tracking", test_directory_count_tracking},
  {"Scope: combined include exclude paths", test_combined_include_exclude_paths},
  {"Scope: combined extension hidden", test_combined_extension_and_hidden},
  {"Scope: config getter", test_scope_config_getter},
  {"Scope: config setter", test_scope_config_setter},
  {"Scope: integrates with FilesystemConfig", test_scope_with_collector_config},
  {"Scope: large tree depth protection", test_large_tree_depth_protection},
  {"Scope: symlink loop detection", test_symlink_same_inode_loop_detection},
  {"Scope: Windows paths", test_windows_path_handling},
};

const KTestEntry* GetKTests_FilesystemScope() { return s_ktests; }
std::size_t GetKTestCount_FilesystemScope() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
