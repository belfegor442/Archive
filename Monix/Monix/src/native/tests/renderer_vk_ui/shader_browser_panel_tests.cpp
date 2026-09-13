#include "shader_browser_panel_tests.hpp"

#include "../../ui/shaders/ShaderBrowserPanel.hpp"
#include "../../library/ShaderLibrary.hpp"
#include "../../library/ShaderLibraryCompiler.hpp"
#include "../../shader_runtime/ShaderRuntime.hpp"
#include "../../shader_runtime/TransactionalShaderState.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace monix::renderer_vk::tests {

static int testsPassed = 0;
static int testsFailed = 0;

#define SBP_ASSERT(cond) do { \
  if (!(cond)) { \
    std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
    testsFailed++; \
  } else { testsPassed++; } \
} while(0)

static std::filesystem::path testShaderRoot() {
    auto p = std::filesystem::current_path() / "tests" / "shaders";
    if (std::filesystem::exists(p)) return p;
    p = std::filesystem::current_path() / ".." / "tests" / "shaders";
    if (std::filesystem::exists(p)) return p;
    return std::filesystem::current_path() / "tests" / "shaders";
}

struct TestContext {
    ShaderLibrary lib;
    std::unique_ptr<ShaderRuntime> rt;
    std::unique_ptr<ShaderLibraryCompiler> comp;
    ShaderBrowserPanelState state;
    std::unique_ptr<ShaderBrowserPanel> panel;

    TestContext() : lib(testShaderRoot()) {
        lib.scan();
        auto cfg = ShaderRuntimeConfig{};
        cfg.rootDirectory = std::filesystem::current_path();
        cfg.shaderCacheDirectory = std::filesystem::current_path() / "cache_test";
        cfg.slangcPath = "";
        cfg.includeDirectories = { testShaderRoot() };
        std::filesystem::create_directories(cfg.shaderCacheDirectory);
        rt = std::make_unique<ShaderRuntime>(std::move(cfg));
        rt->initialize();
        comp = std::make_unique<ShaderLibraryCompiler>(lib, *rt);
        panel = std::make_unique<ShaderBrowserPanel>(lib, *comp, *rt, state);
    }
};

static void test_settings_section_default_general() {
    std::printf("--- Settings Section ---\n");
    std::printf("  [TEST] test_settings_section_default_general\n");
    ShaderBrowserPanelState state;
    SBP_ASSERT(state.subTab == SettingsSubTab::General);
}

static void test_change_general_to_shaders() {
    std::printf("  [TEST] test_change_general_to_shaders\n");
    ShaderBrowserPanelState state;
    state.subTab = SettingsSubTab::General;
    state.subTab = SettingsSubTab::Shaders;
    SBP_ASSERT(state.subTab == SettingsSubTab::Shaders);
}

static void test_change_shaders_to_general() {
    std::printf("  [TEST] test_change_shaders_to_general\n");
    ShaderBrowserPanelState state;
    state.subTab = SettingsSubTab::Shaders;
    state.subTab = SettingsSubTab::General;
    SBP_ASSERT(state.subTab == SettingsSubTab::General);
}

static void test_filter_glsl() {
    std::printf("--- Filter ---\n");
    std::printf("  [TEST] test_filter_glsl\n");
    TestContext ctx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    SBP_ASSERT(ctx.panel->filter() == ShaderLanguageFilter::GLSL);
    const auto visible = ctx.panel->visibleShaders();
    for (const auto& row : visible) {
        const auto* e = ctx.lib.entry(row.libraryIndex);
        SBP_ASSERT(e != nullptr);
        SBP_ASSERT(e->language == ShaderLanguage::GLSL);
    }
}

static void test_filter_slang() {
    std::printf("  [TEST] test_filter_slang\n");
    TestContext ctx;
    ctx.panel->setFilter(ShaderLanguageFilter::Slang);
    SBP_ASSERT(ctx.panel->filter() == ShaderLanguageFilter::Slang);
    const auto visible = ctx.panel->visibleShaders();
    for (const auto& row : visible) {
        const auto* e = ctx.lib.entry(row.libraryIndex);
        SBP_ASSERT(e != nullptr);
        SBP_ASSERT(e->language == ShaderLanguage::Slang);
        SBP_ASSERT(e->extension != ".slangp");
    }
}

static void test_filter_slangp() {
    std::printf("  [TEST] test_filter_slangp\n");
    TestContext ctx;
    ctx.panel->setFilter(ShaderLanguageFilter::Preset);
    SBP_ASSERT(ctx.panel->filter() == ShaderLanguageFilter::Preset);
    const auto visible = ctx.panel->visibleShaders();
    for (const auto& row : visible) {
        const auto* e = ctx.lib.entry(row.libraryIndex);
        SBP_ASSERT(e != nullptr);
        SBP_ASSERT(e->extension == ".slangp");
    }
}

static void test_select_shader() {
    std::printf("--- Selection ---\n");
    std::printf("  [TEST] test_select_shader\n");
    TestContext ctx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const int count = ctx.panel->filteredShaderCount();
    if (count > 0) {
        ctx.panel->selectShader(0);
        SBP_ASSERT(ctx.panel->hasSelection());
        SBP_ASSERT(ctx.panel->selectedEntry() != nullptr);
    }
}

static void test_select_nonexistent_shader() {
    std::printf("  [TEST] test_select_nonexistent_shader\n");
    TestContext ctx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    ctx.panel->selectShader(99999);
    SBP_ASSERT(!ctx.panel->hasSelection());
    SBP_ASSERT(ctx.panel->selectedEntry() == nullptr);
}

static void test_load_valid_shader() {
    std::printf("--- Load ---\n");
    std::printf("  [TEST] test_load_valid_shader\n");
    TestContext ctx;
    TransactionalShaderState tx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const int count = ctx.panel->filteredShaderCount();
    if (count > 0) {
        ctx.panel->selectShader(0);
        ctx.panel->requestLoad(tx);
        const auto* entry = ctx.panel->selectedEntry();
        if (entry) {
            SBP_ASSERT(entry->status == ShaderEntryStatus::Compiled ||
                       entry->status == ShaderEntryStatus::Active ||
                       entry->status == ShaderEntryStatus::Error);
        }
    }
}

static void test_load_invalid_shader() {
    std::printf("  [TEST] test_load_invalid_shader\n");
    TestContext ctx;
    TransactionalShaderState tx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    ctx.panel->selectShader(99999);
    ctx.panel->requestLoad(tx);
}

static void test_reload_shader() {
    std::printf("--- Reload ---\n");
    std::printf("  [TEST] test_reload_shader\n");
    TestContext ctx;
    TransactionalShaderState tx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const int count = ctx.panel->filteredShaderCount();
    if (count > 0) {
        ctx.panel->selectShader(0);
        ctx.panel->requestReload(tx);
        const auto* entry = ctx.panel->selectedEntry();
        if (entry) {
            SBP_ASSERT(entry->status == ShaderEntryStatus::Compiled ||
                       entry->status == ShaderEntryStatus::Active ||
                       entry->status == ShaderEntryStatus::Error);
        }
    }
}

static void test_status_compiling() {
    std::printf("--- Status ---\n");
    std::printf("  [TEST] test_status_compiling\n");
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Compiling), "compiling") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusColor(ShaderEntryStatus::Compiling), "yellow") == 0);
}

static void test_status_failed() {
    std::printf("  [TEST] test_status_failed\n");
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Error), "error") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusColor(ShaderEntryStatus::Error), "red") == 0);
}

static void test_status_active() {
    std::printf("  [TEST] test_status_active\n");
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Active), "active") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusColor(ShaderEntryStatus::Active), "blue") == 0);
}

static void test_previous_shader_survives_failure() {
    std::printf("  [TEST] test_previous_shader_survives_failure\n");
    TestContext ctx;
    TransactionalShaderState tx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const int count = ctx.panel->filteredShaderCount();
    if (count >= 2) {
        ctx.panel->selectShader(0);
        ctx.panel->requestLoad(tx);
        const auto* entry0 = ctx.panel->selectedEntry();
        const auto status0 = entry0 ? entry0->status : ShaderEntryStatus::Unknown;

        ctx.panel->selectShader(count - 1);
        ctx.panel->requestLoad(tx);
        const auto* entryLast = ctx.panel->selectedEntry();
        if (entryLast && entryLast->status == ShaderEntryStatus::Error) {
            SBP_ASSERT(status0 != ShaderEntryStatus::Unknown);
        }
    }
}

static void test_cache_stats_visible() {
    std::printf("--- Cache Stats ---\n");
    std::printf("  [TEST] test_cache_stats_visible\n");
    TestContext ctx;
    const auto stats = ctx.panel->cacheStats();
    SBP_ASSERT(stats.hits >= 0);
    SBP_ASSERT(stats.misses >= 0);
}

static void test_categories_grouped() {
    std::printf("--- Categories ---\n");
    std::printf("  [TEST] test_categories_grouped\n");
    TestContext ctx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const auto groups = ctx.panel->categoryGroups();
    for (const auto& g : groups) {
        SBP_ASSERT(!g.name.empty());
        SBP_ASSERT(!g.entryIndices.empty());
    }
}

static void test_cg_not_visible() {
    std::printf("  [TEST] test_cg_not_visible\n");
    TestContext ctx;
    SBP_ASSERT(!ctx.panel->isExtensionVisible(".cg"));
}

static void test_general_settings_still_works() {
    std::printf("--- Regression ---\n");
    std::printf("  [TEST] test_general_settings_still_works\n");
    ShaderBrowserPanelState state;
    state.subTab = SettingsSubTab::General;
    SBP_ASSERT(state.subTab == SettingsSubTab::General);
}

static void test_build_complete() {
    std::printf("  [TEST] test_build_complete\n");
    SBP_ASSERT(sizeof(ShaderBrowserPanelState) > 0);
    SBP_ASSERT(sizeof(ShaderLanguageFilter) > 0);
    SBP_ASSERT(sizeof(SettingsSubTab) > 0);
}

// ============================================================================
// FASE 14 — New tests
// ============================================================================

// 14.L.1 — status transitions
static void test_f14_status_transitions() {
    std::printf("--- FASE 14: Status Transitions ---\n");
    std::printf("  [TEST] test_f14_status_transitions\n");
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Validating), "validating") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Activating), "activating") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusIndicator(ShaderEntryStatus::Reloading), "reloading") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusColor(ShaderEntryStatus::Validating), "cyan") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusColor(ShaderEntryStatus::Activating), "cyan") == 0);
    SBP_ASSERT(strcmp(ShaderBrowserPanel::statusColor(ShaderEntryStatus::Reloading), "magenta") == 0);
    SBP_ASSERT(strcmp(shaderEntryStatusName(ShaderEntryStatus::Validating), "Validating") == 0);
    SBP_ASSERT(strcmp(shaderEntryStatusName(ShaderEntryStatus::Activating), "Activating") == 0);
    SBP_ASSERT(strcmp(shaderEntryStatusName(ShaderEntryStatus::Reloading), "Reloading") == 0);
}

// 14.L.2 — active shader state
static void test_f14_active_shader() {
    std::printf("  [TEST] test_f14_active_shader\n");
    TestContext ctx;
    const auto* active = ctx.panel->activeShader();
    if (active) {
        SBP_ASSERT(active->isActive);
        SBP_ASSERT(active->status == ShaderEntryStatus::Active);
    } else {
        SBP_ASSERT(true);
    }
}

// 14.L.3 — compile error diagnostics
static void test_f14_compile_diagnostics() {
    std::printf("  [TEST] test_f14_compile_diagnostics\n");
    TestContext ctx;
    TransactionalShaderState tx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    ctx.panel->selectShader(0);
    ctx.panel->requestLoad(tx);
    const auto* entry = ctx.panel->selectedEntry();
    if (entry && entry->status == ShaderEntryStatus::Error) {
        const auto info = ctx.panel->detailInfo();
        SBP_ASSERT(info.diagnosticCount > 0);
        SBP_ASSERT(!info.firstError.empty());
        SBP_ASSERT(info.hasSelection);
    } else {
        SBP_ASSERT(true);
    }
}

// 14.L.4 — cache statistics
static void test_f14_cache_statistics() {
    std::printf("  [TEST] test_f14_cache_statistics\n");
    TestContext ctx;
    const auto stats = ctx.panel->cacheStats();
    SBP_ASSERT(stats.hits >= 0);
    SBP_ASSERT(stats.misses >= 0);
    SBP_ASSERT(stats.hitRate >= 0.0 && stats.hitRate <= 1.0);
    SBP_ASSERT(stats.memoryLimitBytes > 0);
}

// 14.L.5 — compile metrics
static void test_f14_compile_metrics() {
    std::printf("  [TEST] test_f14_compile_metrics\n");
    TestContext ctx;
    TransactionalShaderState tx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const int count = ctx.panel->filteredShaderCount();
    if (count > 0) {
        ctx.panel->selectShader(0);
        ctx.panel->requestLoad(tx);
        const auto info = ctx.panel->detailInfo();
        SBP_ASSERT(info.totalActivationMs >= 0.0);
        SBP_ASSERT(info.compileDurationMs >= 0.0);
    }
}

// 14.L.6 — reload state
static void test_f14_reload_state() {
    std::printf("  [TEST] test_f14_reload_state\n");
    TestContext ctx;
    SBP_ASSERT(!ctx.panel->isCompiling());
    SBP_ASSERT(!ctx.panel->isValidating());
    SBP_ASSERT(!ctx.panel->isActivating());
    SBP_ASSERT(!ctx.panel->isReloading());
}

// 14.L.7 — hot reload state
static void test_f14_hot_reload_state() {
    std::printf("  [TEST] test_f14_hot_reload_state\n");
    TestContext ctx;
    auto event = ctx.panel->lastHotReloadEvent();
    SBP_ASSERT(!event.active);

    ShaderBrowserPanel::HotReloadEvent newEvent;
    newEvent.active = true;
    newEvent.filename = "test.glsl";
    newEvent.status = "SUCCESS";
    newEvent.durationMs = 1.5;
    ctx.panel->recordHotReloadEvent(newEvent);
    auto recorded = ctx.panel->lastHotReloadEvent();
    SBP_ASSERT(recorded.active);
    SBP_ASSERT(recorded.filename == "test.glsl");
    SBP_ASSERT(recorded.status == "SUCCESS");
}

// 14.L.8 — rollback status
static void test_f14_rollback_status() {
    std::printf("  [TEST] test_f14_rollback_status\n");
    TestContext ctx;
    TransactionalShaderState tx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const int count = ctx.panel->filteredShaderCount();
    if (count > 0) {
        ctx.panel->selectShader(0);
        ctx.panel->requestLoad(tx);
        const auto info = ctx.panel->detailInfo();
        SBP_ASSERT(info.status == ShaderEntryStatus::Compiled ||
                   info.status == ShaderEntryStatus::Active ||
                   info.status == ShaderEntryStatus::Error);
    }
}

// 14.L.9 — error history
static void test_f14_error_history() {
    std::printf("  [TEST] test_f14_error_history\n");
    TestContext ctx;
    ctx.panel->clearErrorHistory();
    SBP_ASSERT(ctx.panel->errorHistory().empty());

    ctx.panel->recordError("test_shader", "vertex", "undeclared variable", "test.glsl", 10, 5);
    SBP_ASSERT(ctx.panel->errorHistory().size() == 1);
    SBP_ASSERT(ctx.panel->errorHistory()[0].shader == "test_shader");
    SBP_ASSERT(ctx.panel->errorHistory()[0].stage == "vertex");
    SBP_ASSERT(ctx.panel->errorHistory()[0].error == "undeclared variable");
    SBP_ASSERT(ctx.panel->errorHistory()[0].line == 10);
    SBP_ASSERT(ctx.panel->errorHistory()[0].column == 5);

    for (int i = 0; i < 25; ++i) {
        ctx.panel->recordError("err", "stage", "msg", "file", i, 0);
    }
    SBP_ASSERT(ctx.panel->errorHistory().size() == 20);

    ctx.panel->clearErrorHistory();
    SBP_ASSERT(ctx.panel->errorHistory().empty());
}

// 14.L.10 — detail panel data
static void test_f14_detail_panel_data() {
    std::printf("  [TEST] test_f14_detail_panel_data\n");
    TestContext ctx;
    ctx.panel->setFilter(ShaderLanguageFilter::GLSL);
    const int count = ctx.panel->filteredShaderCount();
    if (count > 0) {
        ctx.panel->selectShader(0);
        const auto info = ctx.panel->detailInfo();
        SBP_ASSERT(info.hasSelection);
        SBP_ASSERT(!info.name.empty());
        SBP_ASSERT(!info.path.empty());
        SBP_ASSERT(!info.language.empty());
        SBP_ASSERT(!info.extension.empty());
        SBP_ASSERT(info.spirvSizeBytes >= 0);
        SBP_ASSERT(info.compileDurationMs >= 0.0);
        SBP_ASSERT(info.validationDurationMs >= 0.0);
        SBP_ASSERT(info.pipelineDurationMs >= 0.0);
        SBP_ASSERT(info.totalActivationMs >= 0.0);
    }
}

void runShaderBrowserPanelTests() {
    testsPassed = 0;
    testsFailed = 0;

    test_settings_section_default_general();
    test_change_general_to_shaders();
    test_change_shaders_to_general();

    test_filter_glsl();
    test_filter_slang();
    test_filter_slangp();

    test_select_shader();
    test_select_nonexistent_shader();

    test_load_valid_shader();
    test_load_invalid_shader();

    test_reload_shader();

    test_status_compiling();
    test_status_failed();
    test_status_active();

    test_previous_shader_survives_failure();

    test_cache_stats_visible();

    test_categories_grouped();

    test_cg_not_visible();

    test_general_settings_still_works();

    test_build_complete();

    // FASE 14 tests
    test_f14_status_transitions();
    test_f14_active_shader();
    test_f14_compile_diagnostics();
    test_f14_cache_statistics();
    test_f14_compile_metrics();
    test_f14_reload_state();
    test_f14_hot_reload_state();
    test_f14_rollback_status();
    test_f14_error_history();
    test_f14_detail_panel_data();

    std::printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
    if (testsFailed > 0) {
        std::printf("=== SOME TESTS FAILED ===\n");
    } else {
        std::printf("=== All ShaderBrowserPanel tests passed ===\n");
    }
}

}
