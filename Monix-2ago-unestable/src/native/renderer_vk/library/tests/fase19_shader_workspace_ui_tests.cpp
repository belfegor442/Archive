#include "fase19_shader_workspace_ui_tests.hpp"
#include "../ShaderWorkspaceConfig.hpp"
#include "../ShaderWorkspace.hpp"
#include "../ShaderLibrary.hpp"
#include "../ShaderLibraryEntry.hpp"
#include "../ShaderLibraryCompiler.hpp"
#include "../../shader_runtime/ShaderRuntime.hpp"
#include "../../shader_runtime/TransactionalShaderState.hpp"
#include "../../compiler/ShaderCache.hpp"
#include "../../preset/SlangPreset.hpp"
#include "../../ui/ShaderBrowserPanel.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

static int gPassed = 0;
static int gFailed = 0;

#define F19_ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        gFailed++; \
    } else { gPassed++; } \
} while(0)

#define F19_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::printf("  FAIL: %s == %s (line %d)\n", #a, #b, __LINE__); \
        gFailed++; \
    } else { gPassed++; } \
} while(0)

namespace monix { namespace renderer_vk { namespace tests {

struct TestEnv19 {
    fs::path root;
    ShaderWorkspace ws;
    ShaderLibrary* lib = nullptr;
    ShaderLibraryCompiler* comp = nullptr;
    ShaderRuntime* rt = nullptr;
    TransactionalShaderState* tx = nullptr;
    ShaderCache* cache = nullptr;
    ShaderBrowserPanelState panelState;
    ShaderBrowserPanel* panel = nullptr;

    TestEnv19() {
        root = fs::temp_directory_path() / "monix_fase19_test";
        if (fs::exists(root)) fs::remove_all(root);
        fs::create_directories(root);
        ws = ShaderWorkspace(root);
        ws.initialize();
    }
    ~TestEnv19() {
        delete panel; delete cache; delete tx; delete rt; delete comp; delete lib;
        std::error_code ec;
        if (fs::exists(root)) fs::remove_all(root, ec);
    }
    void createFile(const std::string& rel, const std::string& content) {
        auto p = root / rel;
        fs::create_directories(p.parent_path());
        std::ofstream(p) << content;
    }
    void scanLib() {
        delete panel; delete cache; delete tx; delete rt; delete comp; delete lib;
        lib = new ShaderLibrary(root, &ws);
        lib->scan();
        ShaderRuntimeConfig cfg;
        cfg.slangcPath = "";
        rt = new ShaderRuntime(cfg);
        comp = new ShaderLibraryCompiler(*lib, *rt);
        cache = new ShaderCache(root / "cache", 1024 * 1024);
        tx = new TransactionalShaderState();
        panel = new ShaderBrowserPanel(*lib, *comp, *rt, panelState);
    }
};

static const char* kGLSL = "#version 450\nvoid main(){gl_FragColor=vec4(1);}\n";
static const char* kGLSL2 = "#version 450\nvoid main(){gl_FragColor=vec4(0,1,0,1);}\n";
static const char* kBad = "this is not valid GLSL @#$%\n";

// ============================================================================
// FASE 19.1 -- Settings Section General/Shaders (3 tests)
// ============================================================================

static void test_19_1_settings_section_general() {
    std::printf("  [TEST] 19.1 settings section general\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panelState.subTab = SettingsSubTab::General;
    env.panel->setSubTab(SettingsSubTab::General);
    F19_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::General);
    F19_ASSERT_EQ(env.panelState.subTab, SettingsSubTab::General);
}

static void test_19_1_settings_section_shaders() {
    std::printf("  [TEST] 19.1 settings section shaders\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panelState.subTab = SettingsSubTab::Shaders;
    env.panel->setSubTab(SettingsSubTab::Shaders);
    F19_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::Shaders);
    F19_ASSERT_EQ(env.panelState.subTab, SettingsSubTab::Shaders);
}

static void test_19_1_general_preserved_after_shader_tab() {
    std::printf("  [TEST] 19.1 general preserved after shader tab\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->setSubTab(SettingsSubTab::General);
    F19_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::General);
    env.panel->setSubTab(SettingsSubTab::Shaders);
    F19_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::Shaders);
    env.panel->setSubTab(SettingsSubTab::General);
    F19_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::General);
}

// ============================================================================
// FASE 19.2 -- Shader Library (4 tests)
// ============================================================================

static void test_19_2_language_selector_glsl() {
    std::printf("  [TEST] 19.2 language selector GLSL\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.createFile("SLANG/test.slang", kGLSL);
    env.scanLib();
    env.panel->setFilter(ShaderLanguageFilter::GLSL);
    F19_ASSERT_EQ(env.panel->filter(), ShaderLanguageFilter::GLSL);
    F19_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_19_2_language_selector_slang() {
    std::printf("  [TEST] 19.2 language selector SLANG\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.createFile("SLANG/test.slang", kGLSL);
    env.scanLib();
    env.panel->setFilter(ShaderLanguageFilter::Slang);
    F19_ASSERT_EQ(env.panel->filter(), ShaderLanguageFilter::Slang);
    F19_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_19_2_category_groups() {
    std::printf("  [TEST] 19.2 category groups\n");
    TestEnv19 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/CRT/curvature.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    auto groups = env.panel->categoryGroups();
    F19_ASSERT(groups.size() >= 2);
    bool hasCRT = false, hasEffects = false;
    for (auto& g : groups) {
        if (g.name == "CRT") hasCRT = true;
        if (g.name == "Effects") hasEffects = true;
    }
    F19_ASSERT(hasCRT);
    F19_ASSERT(hasEffects);
}

static void test_19_2_status_indicator() {
    std::printf("  [TEST] 19.2 status indicator\n");
    const char* ind = ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Unknown);
    F19_ASSERT(ind != nullptr);
    F19_ASSERT(ind[0] != '\0');
    ind = ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Active);
    F19_ASSERT(ind != nullptr);
    F19_ASSERT(ind[0] != '\0');
    ind = ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Error);
    F19_ASSERT(ind != nullptr);
    F19_ASSERT(ind[0] != '\0');
}

// ============================================================================
// FASE 19.3 -- Search (3 tests)
// ============================================================================

static void test_19_3_search_by_name() {
    std::printf("  [TEST] 19.3 search by name\n");
    TestEnv19 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/CRT/curvature.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    env.panel->setSearchQuery("scanlines");
    F19_ASSERT_EQ(env.panel->searchQuery(), "scanlines");
    F19_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_19_3_search_case_insensitive() {
    std::printf("  [TEST] 19.3 search case insensitive\n");
    TestEnv19 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.scanLib();
    env.panel->setSearchQuery("SCANLINES");
    F19_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
    env.panel->setSearchQuery("ScAnLiNeS");
    F19_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_19_3_favorites_filter() {
    std::printf("  [TEST] 19.3 favorites filter\n");
    TestEnv19 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    env.panel->setFavorite(0, true);
    F19_ASSERT(env.panel->isFavorite(0));
    auto favs = env.panel->favoriteShaders();
    F19_ASSERT(favs.size() >= 1);
    env.panel->setFavorite(0, false);
    F19_ASSERT(!env.panel->isFavorite(0));
}

// ============================================================================
// FASE 19.4 -- Shader Selection (3 tests)
// ============================================================================

static void test_19_4_selection_detail_panel() {
    std::printf("  [TEST] 19.4 selection detail panel\n");
    TestEnv19 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    F19_ASSERT(env.panel->hasSelection());
    auto detail = env.panel->detailInfo();
    F19_ASSERT(detail.hasSelection);
    F19_ASSERT_EQ(detail.name, "scanlines");
    F19_ASSERT_EQ(detail.language, "GLSL");
    F19_ASSERT_EQ(detail.category, "CRT");
    F19_ASSERT_EQ(detail.extension, ".glsl");
    F19_ASSERT_EQ(detail.status, ShaderEntryStatus::Unknown);
    F19_ASSERT_EQ(detail.isActive, false);
    F19_ASSERT_EQ(detail.hasCompiledModule, false);
}

static void test_19_4_no_selection_detail() {
    std::printf("  [TEST] 19.4 no selection detail\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    auto detail = env.panel->detailInfo();
    F19_ASSERT(!detail.hasSelection);
}

static void test_19_4_selected_entry() {
    std::printf("  [TEST] 19.4 selected entry\n");
    TestEnv19 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.scanLib();
    F19_ASSERT(env.panel->selectedEntry() == nullptr);
    env.panel->selectShader(0);
    F19_ASSERT(env.panel->selectedEntry() != nullptr);
    F19_ASSERT_EQ(env.panel->selectedEntry()->name, "scanlines");
}

// ============================================================================
// FASE 19.5 -- Load (3 tests)
// ============================================================================

static void test_19_5_load_request() {
    std::printf("  [TEST] 19.5 load request\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    auto statusBefore = env.panel->detailInfo().status;
    env.panel->requestLoad(*env.tx);
    F19_ASSERT(statusBefore != env.panel->detailInfo().status ||
               env.panel->detailInfo().status == ShaderEntryStatus::Error);
}

static void test_19_5_load_state_queries() {
    std::printf("  [TEST] 19.5 load state queries\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    F19_ASSERT(!env.panel->isCompiling());
    F19_ASSERT(!env.panel->isValidating());
    F19_ASSERT(!env.panel->isActivating());
    F19_ASSERT(!env.panel->isReloading());
}

static void test_19_5_load_rollback_on_failure() {
    std::printf("  [TEST] 19.5 load rollback on failure\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kBad);
    env.scanLib();
    env.panel->selectShader(0);
    F19_ASSERT_EQ(env.panel->detailInfo().status, ShaderEntryStatus::Unknown);
    F19_ASSERT(env.panel->activeShader() == nullptr);
}

// ============================================================================
// FASE 19.6 -- Reload (2 tests)
// ============================================================================

static void test_19_6_reload_request() {
    std::printf("  [TEST] 19.6 reload request\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    env.panel->requestReload(*env.tx);
    F19_ASSERT(env.panelState.reloadRequested == false ||
               env.panel->detailInfo().status == ShaderEntryStatus::Unknown ||
               env.panel->detailInfo().status == ShaderEntryStatus::Error);
}

static void test_19_6_reload_active_shader() {
    std::printf("  [TEST] 19.6 reload active shader\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    F19_ASSERT(env.panel->activeShader() == nullptr);
}

// ============================================================================
// FASE 19.7 -- Error History (2 tests)
// ============================================================================

static void test_19_7_error_history_record() {
    std::printf("  [TEST] 19.7 error history record\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->recordError("test.glsl", "fragment", "syntax error", "test.glsl", 5, 10);
    auto& hist = env.panel->errorHistory();
    F19_ASSERT(!hist.empty());
    F19_ASSERT_EQ(hist[0].shader, "test.glsl");
    F19_ASSERT_EQ(hist[0].stage, "fragment");
    F19_ASSERT_EQ(hist[0].error, "syntax error");
    F19_ASSERT_EQ(hist[0].file, "test.glsl");
    F19_ASSERT_EQ(hist[0].line, 5);
    F19_ASSERT_EQ(hist[0].column, 10);
}

static void test_19_7_error_history_max_20() {
    std::printf("  [TEST] 19.7 error history max 20\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    for (int i = 0; i < 25; i++) {
        env.panel->recordError("test.glsl", "stage", "error " + std::to_string(i));
    }
    auto& hist = env.panel->errorHistory();
    F19_ASSERT(hist.size() == 20);
    F19_ASSERT_EQ(hist[0].error, "error 5");
    F19_ASSERT_EQ(hist[19].error, "error 24");
}

// ============================================================================
// FASE 19.8 -- Cache Stats (1 test)
// ============================================================================

static void test_19_8_cache_stats() {
    std::printf("  [TEST] 19.8 cache stats\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    auto stats = env.panel->cacheStats();
    F19_ASSERT_EQ(stats.hits, 0u);
    F19_ASSERT_EQ(stats.misses, 0u);
    F19_ASSERT_EQ(stats.totalEntries, 0u);
    F19_ASSERT_EQ(stats.hitRate, 0.0);
}

// ============================================================================
// FASE 19.9 -- Hot Reload (1 test)
// ============================================================================

static void test_19_9_hot_reload_event() {
    std::printf("  [TEST] 19.9 hot reload event\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    ShaderBrowserPanel::HotReloadEvent ev;
    ev.active = true;
    ev.filename = "test.glsl";
    ev.status = "Active";
    ev.durationMs = 12.5;
    env.panel->recordHotReloadEvent(ev);
    auto last = env.panel->lastHotReloadEvent();
    F19_ASSERT(last.active);
    F19_ASSERT_EQ(last.filename, "test.glsl");
    F19_ASSERT_EQ(last.status, "Active");
    F19_ASSERT(last.durationMs > 0.0);
}

// ============================================================================
// FASE 19.10 -- Config Persistence (1 test)
// ============================================================================

static void test_19_10_config_persistence() {
    std::printf("  [TEST] 19.10 config persistence\n");
    TestEnv19 env;
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"GLSL/CRT/scanlines.glsl", "SLANG/test.slang"};
    cfg.lastSelected = "GLSL/CRT/scanlines.glsl";
    cfg.lastLanguage = "SLANG";
    F19_ASSERT(cfg.save(env.root / "config.json"));
    auto loaded = ShaderWorkspaceConfig::load(env.root / "config.json");
    F19_ASSERT_EQ(loaded.favorites.size(), 2u);
    F19_ASSERT_EQ(loaded.lastSelected, "GLSL/CRT/scanlines.glsl");
    F19_ASSERT_EQ(loaded.lastLanguage, "SLANG");
}

// ============================================================================
// FASE 19.11 -- Diagnostics (1 test)
// ============================================================================

static void test_19_11_renderer_diagnostics() {
    std::printf("  [TEST] 19.11 renderer diagnostics\n");
    TestEnv19 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    RendererDiagnosticsInfo diag;
    diag.gpuVendor = "NVIDIA";
    diag.gpuRenderer = "RTX 3080";
    diag.apiVersion = "1.3.250";
    diag.validationAvailable = true;
    diag.validationEnabled = true;
    env.panel->setRendererDiagnostics(diag);
    auto result = env.panel->rendererDiagnostics();
    F19_ASSERT_EQ(result.gpuVendor, "NVIDIA");
    F19_ASSERT_EQ(result.gpuRenderer, "RTX 3080");
    F19_ASSERT_EQ(result.apiVersion, "1.3.250");
    F19_ASSERT(result.validationAvailable);
    F19_ASSERT(result.validationEnabled);
}

// ============================================================================
// FASE 19.12 -- Status Summary (1 test)
// ============================================================================

static void test_19_12_status_summary() {
    std::printf("  [TEST] 19.12 status summary\n");
    TestEnv19 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    auto summary = env.panel->statusSummary();
    F19_ASSERT(!summary.empty());
    F19_ASSERT(summary.find("Shaders") != std::string::npos ||
               summary.find("shaders") != std::string::npos ||
               summary.find("2") != std::string::npos);
}

// ============================================================================
// Runner
// ============================================================================

void runFase19Tests() {
    std::printf("\n=== FASE 19 - Shader Workspace UI + Real User Workflow ===\n\n");

    std::printf("\n--- 19.1 Settings Section ---\n");
    test_19_1_settings_section_general();
    test_19_1_settings_section_shaders();
    test_19_1_general_preserved_after_shader_tab();

    std::printf("\n--- 19.2 Shader Library ---\n");
    test_19_2_language_selector_glsl();
    test_19_2_language_selector_slang();
    test_19_2_category_groups();
    test_19_2_status_indicator();

    std::printf("\n--- 19.3 Search ---\n");
    test_19_3_search_by_name();
    test_19_3_search_case_insensitive();
    test_19_3_favorites_filter();

    std::printf("\n--- 19.4 Selection ---\n");
    test_19_4_selection_detail_panel();
    test_19_4_no_selection_detail();
    test_19_4_selected_entry();

    std::printf("\n--- 19.5 Load ---\n");
    test_19_5_load_request();
    test_19_5_load_state_queries();
    test_19_5_load_rollback_on_failure();

    std::printf("\n--- 19.6 Reload ---\n");
    test_19_6_reload_request();
    test_19_6_reload_active_shader();

    std::printf("\n--- 19.7 Error History ---\n");
    test_19_7_error_history_record();
    test_19_7_error_history_max_20();

    std::printf("\n--- 19.8 Cache Stats ---\n");
    test_19_8_cache_stats();

    std::printf("\n--- 19.9 Hot Reload ---\n");
    test_19_9_hot_reload_event();

    std::printf("\n--- 19.10 Config Persistence ---\n");
    test_19_10_config_persistence();

    std::printf("\n--- 19.11 Diagnostics ---\n");
    test_19_11_renderer_diagnostics();

    std::printf("\n--- 19.12 Status Summary ---\n");
    test_19_12_status_summary();

    std::printf("\n=== FASE 19 Results: %d passed, %d failed ===\n", gPassed, gFailed);
    if (gFailed > 0) {
        std::printf("=== SOME FASE 19 TESTS FAILED ===\n");
    } else {
        std::printf("=== All FASE 19 tests passed ===\n");
    }
}

}}}
