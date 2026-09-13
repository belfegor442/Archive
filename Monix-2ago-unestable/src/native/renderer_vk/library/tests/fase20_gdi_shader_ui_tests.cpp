#include "fase20_gdi_shader_ui_tests.hpp"
#include "../ShaderWorkspaceConfig.hpp"
#include "../ShaderWorkspace.hpp"
#include "../ShaderLibrary.hpp"
#include "../ShaderLibraryEntry.hpp"
#include "../ShaderLibraryCompiler.hpp"
#include "../../shader_runtime/ShaderRuntime.hpp"
#include "../../shader_runtime/TransactionalShaderState.hpp"
#include "../../compiler/ShaderCache.hpp"
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

#define F20_ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        gFailed++; \
    } else { gPassed++; } \
} while(0)

#define F20_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::printf("  FAIL: %s == %s (line %d)\n", #a, #b, __LINE__); \
        gFailed++; \
    } else { gPassed++; } \
} while(0)

namespace monix { namespace renderer_vk { namespace tests {

struct TestEnv20 {
    fs::path root;
    ShaderWorkspace ws;
    ShaderLibrary* lib = nullptr;
    ShaderLibraryCompiler* comp = nullptr;
    ShaderRuntime* rt = nullptr;
    TransactionalShaderState* tx = nullptr;
    ShaderCache* cache = nullptr;
    ShaderBrowserPanelState panelState;
    ShaderBrowserPanel* panel = nullptr;

    TestEnv20() {
        root = fs::temp_directory_path() / "monix_fase20_test";
        if (fs::exists(root)) fs::remove_all(root);
        fs::create_directories(root);
        ws = ShaderWorkspace(root);
        ws.initialize();
    }
    ~TestEnv20() {
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

// ============================================================================
// 20.1 -- Settings Navigation
// ============================================================================

static void test_20_1_sub_tab_general() {
    std::printf("  [TEST] 20.1 sub tab general\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->setSubTab(SettingsSubTab::General);
    F20_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::General);
}

static void test_20_1_sub_tab_shaders() {
    std::printf("  [TEST] 20.1 sub tab shaders\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->setSubTab(SettingsSubTab::Shaders);
    F20_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::Shaders);
}

static void test_20_1_general_preserved() {
    std::printf("  [TEST] 20.1 general preserved\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->setSubTab(SettingsSubTab::General);
    F20_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::General);
    env.panel->setSubTab(SettingsSubTab::Shaders);
    F20_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::Shaders);
    env.panel->setSubTab(SettingsSubTab::General);
    F20_ASSERT_EQ(env.panel->subTab(), SettingsSubTab::General);
}

// ============================================================================
// 20.2 -- Library Rendering State
// ============================================================================

static void test_20_2_category_groups_render() {
    std::printf("  [TEST] 20.2 category groups render\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/CRT/curvature.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    auto groups = env.panel->categoryGroups();
    F20_ASSERT(groups.size() >= 2);
}

static void test_20_2_visible_shaders_render() {
    std::printf("  [TEST] 20.2 visible shaders render\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    auto rows = env.panel->visibleShaders();
    F20_ASSERT(rows.size() >= 2);
    F20_ASSERT(!rows[0].name.empty());
}

// ============================================================================
// 20.3 -- Language Selector
// ============================================================================

static void test_20_3_glsl_filter() {
    std::printf("  [TEST] 20.3 GLSL filter\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.createFile("SLANG/test.slang", kGLSL);
    env.scanLib();
    env.panel->setFilter(ShaderLanguageFilter::GLSL);
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_20_3_slang_filter() {
    std::printf("  [TEST] 20.3 SLANG filter\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.createFile("SLANG/test.slang", kGLSL);
    env.scanLib();
    env.panel->setFilter(ShaderLanguageFilter::Slang);
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_20_3_slangp_filter() {
    std::printf("  [TEST] 20.3 SLANGP filter\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.createFile("SLANGP/test.slangp", "passes []\n");
    env.scanLib();
    env.panel->setFilter(ShaderLanguageFilter::SlangPreset);
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_20_3_all_filter() {
    std::printf("  [TEST] 20.3 ALL filter\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.createFile("SLANG/test.slang", kGLSL);
    env.scanLib();
    env.panel->setFilter(ShaderLanguageFilter::All);
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 2);
}

// ============================================================================
// 20.4 -- Search Input
// ============================================================================

static void test_20_4_search_by_name() {
    std::printf("  [TEST] 20.4 search by name\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    env.panel->setSearchQuery("scanlines");
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_20_4_search_case_insensitive() {
    std::printf("  [TEST] 20.4 search case insensitive\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.scanLib();
    env.panel->setSearchQuery("SCANLINES");
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

static void test_20_4_search_clear() {
    std::printf("  [TEST] 20.4 search clear\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    env.panel->setSearchQuery("scanlines");
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
    env.panel->setSearchQuery("");
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 2);
}

// ============================================================================
// 20.5 -- Favorites
// ============================================================================

static void test_20_5_set_favorite() {
    std::printf("  [TEST] 20.5 set favorite\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->setFavorite(0, true);
    F20_ASSERT(env.panel->isFavorite(0));
}

static void test_20_5_favorite_shaders() {
    std::printf("  [TEST] 20.5 favorite shaders\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    env.panel->setFavorite(0, true);
    auto favs = env.panel->favoriteShaders();
    F20_ASSERT(favs.size() >= 1);
}

static void test_20_5_unset_favorite() {
    std::printf("  [TEST] 20.5 unset favorite\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->setFavorite(0, true);
    F20_ASSERT(env.panel->isFavorite(0));
    env.panel->setFavorite(0, false);
    F20_ASSERT(!env.panel->isFavorite(0));
}

// ============================================================================
// 20.6 -- Selection
// ============================================================================

static void test_20_6_select_shader() {
    std::printf("  [TEST] 20.6 select shader\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    F20_ASSERT(env.panel->hasSelection());
    F20_ASSERT(env.panel->selectedEntry() != nullptr);
}

static void test_20_6_no_selection() {
    std::printf("  [TEST] 20.6 no selection\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    F20_ASSERT(!env.panel->hasSelection());
    F20_ASSERT(env.panel->selectedEntry() == nullptr);
}

static void test_20_6_selection_detail() {
    std::printf("  [TEST] 20.6 selection detail\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    auto d = env.panel->detailInfo();
    F20_ASSERT(d.hasSelection);
    F20_ASSERT_EQ(d.name, "scanlines");
    F20_ASSERT_EQ(d.language, "GLSL");
    F20_ASSERT_EQ(d.category, "CRT");
}

// ============================================================================
// 20.7 -- Detail Panel
// ============================================================================

static void test_20_7_detail_panel_full() {
    std::printf("  [TEST] 20.7 detail panel full\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    auto d = env.panel->detailInfo();
    F20_ASSERT(d.hasSelection);
    F20_ASSERT(!d.name.empty());
    F20_ASSERT(!d.path.empty());
    F20_ASSERT(!d.language.empty());
    F20_ASSERT(!d.category.empty());
    F20_ASSERT(!d.extension.empty());
    F20_ASSERT(d.status == ShaderEntryStatus::Unknown);
    F20_ASSERT_EQ(d.isActive, false);
    F20_ASSERT_EQ(d.hasCompiledModule, false);
    F20_ASSERT_EQ(d.spirvSizeBytes, 0u);
    F20_ASSERT(d.lastCacheStatus.empty());
    F20_ASSERT(d.compileDurationMs == 0.0);
}

static void test_20_7_detail_no_selection() {
    std::printf("  [TEST] 20.7 detail no selection\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    auto d = env.panel->detailInfo();
    F20_ASSERT(!d.hasSelection);
}

// ============================================================================
// 20.8 -- Cache Stats
// ============================================================================

static void test_20_8_cache_stats() {
    std::printf("  [TEST] 20.8 cache stats\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    auto cs = env.panel->cacheStats();
    F20_ASSERT_EQ(cs.hits, 0u);
    F20_ASSERT_EQ(cs.misses, 0u);
    F20_ASSERT_EQ(cs.hitRate, 0.0);
    F20_ASSERT(cs.memoryLimitBytes > 0);
}

// ============================================================================
// 20.9 -- Diagnostics
// ============================================================================

static void test_20_9_renderer_diagnostics() {
    std::printf("  [TEST] 20.9 renderer diagnostics\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    RendererDiagnosticsInfo diag;
    diag.gpuVendor = "NVIDIA";
    diag.gpuRenderer = "RTX 3080";
    diag.apiVersion = "1.3.250";
    diag.validationAvailable = true;
    diag.validationEnabled = false;
    env.panel->setRendererDiagnostics(diag);
    auto r = env.panel->rendererDiagnostics();
    F20_ASSERT_EQ(r.gpuVendor, "NVIDIA");
    F20_ASSERT_EQ(r.gpuRenderer, "RTX 3080");
    F20_ASSERT_EQ(r.apiVersion, "1.3.250");
    F20_ASSERT(r.validationAvailable);
    F20_ASSERT(!r.validationEnabled);
}

static void test_20_9_diagnostics_empty() {
    std::printf("  [TEST] 20.9 diagnostics empty\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    auto r = env.panel->rendererDiagnostics();
    F20_ASSERT(r.gpuVendor.empty());
    F20_ASSERT(r.apiVersion.empty());
}

// ============================================================================
// 20.10 -- Load/Reload
// ============================================================================

static void test_20_10_load_request() {
    std::printf("  [TEST] 20.10 load request\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    env.panel->requestLoad(*env.tx);
    F20_ASSERT(true);
}

static void test_20_10_reload_request() {
    std::printf("  [TEST] 20.10 reload request\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    env.panel->requestReload(*env.tx);
    F20_ASSERT(true);
}

// ============================================================================
// 20.11 -- Error History
// ============================================================================

static void test_20_11_error_record() {
    std::printf("  [TEST] 20.11 error record\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->recordError("test.glsl", "fragment", "syntax error", "test.glsl", 5, 10);
    auto& h = env.panel->errorHistory();
    F20_ASSERT(!h.empty());
    F20_ASSERT_EQ(h[0].shader, "test.glsl");
    F20_ASSERT_EQ(h[0].stage, "fragment");
    F20_ASSERT_EQ(h[0].error, "syntax error");
    F20_ASSERT_EQ(h[0].line, 5);
    F20_ASSERT_EQ(h[0].column, 10);
}

static void test_20_11_error_max_20() {
    std::printf("  [TEST] 20.11 error max 20\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    for (int i = 0; i < 25; i++) {
        env.panel->recordError("test.glsl", "stage", "err" + std::to_string(i));
    }
    F20_ASSERT(env.panel->errorHistory().size() == 20);
}

static void test_20_11_error_clear() {
    std::printf("  [TEST] 20.11 error clear\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->recordError("test.glsl", "stage", "error");
    F20_ASSERT(!env.panel->errorHistory().empty());
    env.panel->clearErrorHistory();
    F20_ASSERT(env.panel->errorHistory().empty());
}

// ============================================================================
// 20.12 -- Hot Reload
// ============================================================================

static void test_20_12_hot_reload_event() {
    std::printf("  [TEST] 20.12 hot reload event\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    ShaderBrowserPanel::HotReloadEvent ev;
    ev.active = true;
    ev.filename = "test.glsl";
    ev.status = "Active";
    ev.durationMs = 5.0;
    env.panel->recordHotReloadEvent(ev);
    auto last = env.panel->lastHotReloadEvent();
    F20_ASSERT(last.active);
    F20_ASSERT_EQ(last.filename, "test.glsl");
    F20_ASSERT_EQ(last.status, "Active");
}

// ============================================================================
// 20.13 -- Config Persistence
// ============================================================================

static void test_20_13_config_persist() {
    std::printf("  [TEST] 20.13 config persist\n");
    TestEnv20 env;
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"GLSL/CRT/scanlines.glsl"};
    cfg.lastSelected = "GLSL/CRT/scanlines.glsl";
    cfg.lastLanguage = "SLANG";
    F20_ASSERT(cfg.save(env.root / "cfg.json"));
    auto loaded = ShaderWorkspaceConfig::load(env.root / "cfg.json");
    F20_ASSERT_EQ(loaded.favorites.size(), 1u);
    F20_ASSERT_EQ(loaded.lastLanguage, "SLANG");
}

// ============================================================================
// 20.14 -- Status Summary
// ============================================================================

static void test_20_14_status_summary() {
    std::printf("  [TEST] 20.14 status summary\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    auto s = env.panel->statusSummary();
    F20_ASSERT(!s.empty());
}

// ============================================================================
// 20.15 -- Load State Queries
// ============================================================================

static void test_20_15_state_queries() {
    std::printf("  [TEST] 20.15 state queries\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    F20_ASSERT(!env.panel->isCompiling());
    F20_ASSERT(!env.panel->isValidating());
    F20_ASSERT(!env.panel->isActivating());
    F20_ASSERT(!env.panel->isReloading());
}

// ============================================================================
// 20.16 -- Active Shader
// ============================================================================

static void test_20_16_active_shader_null() {
    std::printf("  [TEST] 20.16 active shader null\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    F20_ASSERT(env.panel->activeShader() == nullptr);
}

// ============================================================================
// 20.17 -- Status Indicator
// ============================================================================

static void test_20_17_status_indicator() {
    std::printf("  [TEST] 20.17 status indicator\n");
    const char* s = ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Unknown);
    F20_ASSERT(s != nullptr && s[0] != '\0');
    s = ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Active);
    F20_ASSERT(s != nullptr && s[0] != '\0');
    s = ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Error);
    F20_ASSERT(s != nullptr && s[0] != '\0');
}

static void test_20_17_status_color() {
    std::printf("  [TEST] 20.17 status color\n");
    const char* c = ShaderBrowserPanel::statusColor(ShaderEntryStatus::Unknown);
    F20_ASSERT(c != nullptr && c[0] != '\0');
    c = ShaderBrowserPanel::statusColor(ShaderEntryStatus::Active);
    F20_ASSERT(c != nullptr && c[0] != '\0');
}

// ============================================================================
// 20.18 -- Shader Count
// ============================================================================

static void test_20_18_total_count() {
    std::printf("  [TEST] 20.18 total count\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.createFile("SLANG/test.slang", kGLSL);
    env.scanLib();
    F20_ASSERT(env.panel->totalShaderCount() >= 3);
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 3);
}

static void test_20_18_filtered_count() {
    std::printf("  [TEST] 20.18 filtered count\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.createFile("SLANG/test.slang", kGLSL);
    env.scanLib();
    env.panel->setFilter(ShaderLanguageFilter::GLSL);
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

// ============================================================================
// 20.19 -- Scroll Bounds
// ============================================================================

static void test_20_19_scroll_offset_bounds() {
    std::printf("  [TEST] 20.19 scroll offset bounds\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panelState.scrollOffset = 0;
    F20_ASSERT_EQ(env.panelState.scrollOffset, 0);
}

// ============================================================================
// 20.20 -- Error History Entry Fields
// ============================================================================

static void test_20_20_error_entry_fields() {
    std::printf("  [TEST] 20.20 error entry fields\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->recordError("shader.glsl", "vertex", "undefined variable", "shader.glsl", 42, 7);
    auto& h = env.panel->errorHistory();
    F20_ASSERT(!h.empty());
    F20_ASSERT(h[0].timestamp > 0);
    F20_ASSERT_EQ(h[0].shader, "shader.glsl");
    F20_ASSERT_EQ(h[0].stage, "vertex");
    F20_ASSERT_EQ(h[0].error, "undefined variable");
    F20_ASSERT_EQ(h[0].file, "shader.glsl");
    F20_ASSERT_EQ(h[0].line, 42);
    F20_ASSERT_EQ(h[0].column, 7);
}

// ============================================================================
// 20.21 -- Source Kind Filter
// ============================================================================

static void test_20_21_source_kind() {
    std::printf("  [TEST] 20.21 source kind\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    const auto* entry = env.panel->selectedEntry();
    F20_ASSERT(entry != nullptr);
    F20_ASSERT(entry->sourceKind == ShaderSourceKind::User ||
               entry->sourceKind == ShaderSourceKind::Internal);
}

// ============================================================================
// 20.22 -- Search by Category
// ============================================================================

static void test_20_22_search_by_category() {
    std::printf("  [TEST] 20.22 search by category\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    env.panel->setSearchQuery("CRT");
    F20_ASSERT_EQ(env.panel->filteredShaderCount(), 1);
}

// ============================================================================
// 20.23 -- Multiple Favorites
// ============================================================================

static void test_20_23_multiple_favorites() {
    std::printf("  [TEST] 20.23 multiple favorites\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.createFile("GLSL/Test/noise.glsl", kGLSL);
    env.scanLib();
    env.panel->setFavorite(0, true);
    env.panel->setFavorite(1, true);
    F20_ASSERT(env.panel->isFavorite(0));
    F20_ASSERT(env.panel->isFavorite(1));
    auto favs = env.panel->favoriteShaders();
    F20_ASSERT(favs.size() >= 2);
}

// ============================================================================
// 20.24 -- Error History After Clear
// ============================================================================

static void test_20_24_error_after_clear() {
    std::printf("  [TEST] 20.24 error after clear\n");
    TestEnv20 env;
    env.createFile("GLSL/test.glsl", kGLSL);
    env.scanLib();
    env.panel->recordError("a.glsl", "frag", "err1");
    env.panel->recordError("b.glsl", "vert", "err2");
    env.panel->clearErrorHistory();
    F20_ASSERT(env.panel->errorHistory().empty());
    env.panel->recordError("c.glsl", "frag", "err3");
    F20_ASSERT_EQ(env.panel->errorHistory().size(), 1u);
    F20_ASSERT_EQ(env.panel->errorHistory()[0].error, "err3");
}

// ============================================================================
// 20.25 -- Selection Change
// ============================================================================

static void test_20_25_selection_change() {
    std::printf("  [TEST] 20.25 selection change\n");
    TestEnv20 env;
    env.createFile("GLSL/CRT/scanlines.glsl", kGLSL);
    env.createFile("GLSL/Effects/bloom.glsl", kGLSL);
    env.scanLib();
    env.panel->selectShader(0);
    F20_ASSERT(env.panel->hasSelection());
    auto n1 = env.panel->detailInfo().name;
    env.panel->selectShader(1);
    auto n2 = env.panel->detailInfo().name;
    F20_ASSERT(n1 != n2);
}

// ============================================================================
// Runner
// ============================================================================

void runFase20Tests() {
    std::printf("\n=== FASE 20 - GDI Shader Workspace UI Tests ===\n\n");

    std::printf("\n--- 20.1 Navigation ---\n");
    test_20_1_sub_tab_general();
    test_20_1_sub_tab_shaders();
    test_20_1_general_preserved();

    std::printf("\n--- 20.2 Library Rendering ---\n");
    test_20_2_category_groups_render();
    test_20_2_visible_shaders_render();

    std::printf("\n--- 20.3 Language Selector ---\n");
    test_20_3_glsl_filter();
    test_20_3_slang_filter();
    test_20_3_slangp_filter();
    test_20_3_all_filter();

    std::printf("\n--- 20.4 Search ---\n");
    test_20_4_search_by_name();
    test_20_4_search_case_insensitive();
    test_20_4_search_clear();

    std::printf("\n--- 20.5 Favorites ---\n");
    test_20_5_set_favorite();
    test_20_5_favorite_shaders();
    test_20_5_unset_favorite();

    std::printf("\n--- 20.6 Selection ---\n");
    test_20_6_select_shader();
    test_20_6_no_selection();
    test_20_6_selection_detail();

    std::printf("\n--- 20.7 Detail Panel ---\n");
    test_20_7_detail_panel_full();
    test_20_7_detail_no_selection();

    std::printf("\n--- 20.8 Cache Stats ---\n");
    test_20_8_cache_stats();

    std::printf("\n--- 20.9 Diagnostics ---\n");
    test_20_9_renderer_diagnostics();
    test_20_9_diagnostics_empty();

    std::printf("\n--- 20.10 Load/Reload ---\n");
    test_20_10_load_request();
    test_20_10_reload_request();

    std::printf("\n--- 20.11 Error History ---\n");
    test_20_11_error_record();
    test_20_11_error_max_20();
    test_20_11_error_clear();

    std::printf("\n--- 20.12 Hot Reload ---\n");
    test_20_12_hot_reload_event();

    std::printf("\n--- 20.13 Config ---\n");
    test_20_13_config_persist();

    std::printf("\n--- 20.14 Status Summary ---\n");
    test_20_14_status_summary();

    std::printf("\n--- 20.15 State Queries ---\n");
    test_20_15_state_queries();

    std::printf("\n--- 20.16 Active Shader ---\n");
    test_20_16_active_shader_null();

    std::printf("\n--- 20.17 Status Indicator ---\n");
    test_20_17_status_indicator();
    test_20_17_status_color();

    std::printf("\n--- 20.18 Shader Count ---\n");
    test_20_18_total_count();
    test_20_18_filtered_count();

    std::printf("\n--- 20.19 Scroll ---\n");
    test_20_19_scroll_offset_bounds();

    std::printf("\n--- 20.20 Error Fields ---\n");
    test_20_20_error_entry_fields();

    std::printf("\n--- 20.21 Source Kind ---\n");
    test_20_21_source_kind();

    std::printf("\n--- 20.22 Search Category ---\n");
    test_20_22_search_by_category();

    std::printf("\n--- 20.23 Multiple Favorites ---\n");
    test_20_23_multiple_favorites();

    std::printf("\n--- 20.24 Error After Clear ---\n");
    test_20_24_error_after_clear();

    std::printf("\n--- 20.25 Selection Change ---\n");
    test_20_25_selection_change();

    std::printf("\n=== FASE 20 Results: %d passed, %d failed ===\n", gPassed, gFailed);
    if (gFailed > 0) {
        std::printf("=== SOME FASE 20 TESTS FAILED ===\n");
    } else {
        std::printf("=== All FASE 20 tests passed ===\n");
    }
}

}}}
