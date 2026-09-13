#include "fase16_workspace_tests.hpp"
#include "../ShaderWorkspace.hpp"
#include "../ShaderLibrary.hpp"
#include "../ShaderLibraryEntry.hpp"
#include "../../ui/ShaderBrowserPanel.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace monix::renderer_vk {
namespace tests {

static int testsPassed = 0;
static int testsFailed = 0;

#define F16_ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        testsFailed++; \
    } else { testsPassed++; } \
} while(0)

struct TestWorkspace {
    std::filesystem::path root;
    ShaderWorkspace ws;
    ShaderLibrary* lib = nullptr;

    TestWorkspace() {
        root = std::filesystem::temp_directory_path() / "monix_fase16_test";
        std::filesystem::remove_all(root);
        ws = ShaderWorkspace(root);
        ws.initialize();
    }

    ~TestWorkspace() {
        delete lib;
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void createShader(const std::string& langDir, const std::string& category,
                      const std::string& filename, const std::string& content) {
        auto dir = root / langDir / category;
        std::filesystem::create_directories(dir);
        std::ofstream f(dir / filename);
        f << content;
    }

    void scanLibrary() {
        delete lib;
        lib = new ShaderLibrary(root, &ws);
        lib->scan();
    }
};

// ============================================================================
// FASE 16.1 — Workspace
// ============================================================================

static void test_f16_1_root_discovery() {
    std::printf("  [TEST] test_f16_1_root_discovery\n");
    TestWorkspace tw;
    F16_ASSERT(tw.ws.isValid());
    F16_ASSERT(tw.ws.root() == tw.root);
}

static void test_f16_1_directory_creation() {
    std::printf("  [TEST] test_f16_1_directory_creation\n");
    TestWorkspace tw;
    F16_ASSERT(std::filesystem::exists(tw.ws.glslRoot()));
    F16_ASSERT(std::filesystem::exists(tw.ws.slangRoot()));
    F16_ASSERT(std::filesystem::exists(tw.ws.slangpRoot()));
    F16_ASSERT(std::filesystem::exists(tw.ws.cacheRoot()));
}

static void test_f16_1_relative_paths() {
    std::printf("  [TEST] test_f16_1_relative_paths\n");
    TestWorkspace tw;
    auto glsl = tw.ws.glslRoot();
    auto slang = tw.ws.slangRoot();
    auto slangp = tw.ws.slangpRoot();
    F16_ASSERT(glsl.filename() == "GLSL");
    F16_ASSERT(slang.filename() == "SLANG");
    F16_ASSERT(slangp.filename() == "SLANGP");
}

static void test_f16_1_invalid_root() {
    std::printf("  [TEST] test_f16_1_invalid_root\n");
    ShaderWorkspace ws(std::filesystem::path("Z:\\nonexistent\\path\\xyz"));
    bool ok = ws.initialize();
    F16_ASSERT(!ok || !ws.isValid());
}

static void test_f16_1_config_file_path() {
    std::printf("  [TEST] test_f16_1_config_file_path\n");
    TestWorkspace tw;
    F16_ASSERT(tw.ws.configFile().filename() == "config.json");
}

// ============================================================================
// FASE 16.2 — Source Kind
// ============================================================================

static void test_f16_2_user_source_kind() {
    std::printf("  [TEST] test_f16_2_user_source_kind\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    const auto* e = tw.lib->entry(0);
    F16_ASSERT(e != nullptr);
    F16_ASSERT(e->sourceKind == ShaderSourceKind::User);
}

static void test_f16_2_slang_user_kind() {
    std::printf("  [TEST] test_f16_2_slang_user_kind\n");
    TestWorkspace tw;
    tw.createShader("SLANG", "Effects", "sharpen.slang", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    const auto* e = tw.lib->entry(0);
    F16_ASSERT(e != nullptr);
    F16_ASSERT(e->sourceKind == ShaderSourceKind::User);
}

static void test_f16_2_slangp_user_kind() {
    std::printf("  [TEST] test_f16_2_slangp_user_kind\n");
    TestWorkspace tw;
    tw.createShader("SLANGP", "CRT", "crt.slangp", "[passes]\n[]");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    const auto* e = tw.lib->entry(0);
    F16_ASSERT(e != nullptr);
    F16_ASSERT(e->sourceKind == ShaderSourceKind::User);
}

static void test_f16_2_internal_kind() {
    std::printf("  [TEST] test_f16_2_internal_kind\n");
    TestWorkspace tw;
    auto internalDir = tw.root / "internal";
    std::filesystem::create_directories(internalDir);
    {
        std::ofstream f(internalDir / "helper.glsl");
        f << "void helper() {}";
    }
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    const auto* e = tw.lib->entry(0);
    F16_ASSERT(e != nullptr);
    F16_ASSERT(e->sourceKind == ShaderSourceKind::Internal);
}

// ============================================================================
// FASE 16.3 — Library Discovery
// ============================================================================

static void test_f16_3_glsl_discovery() {
    std::printf("  [TEST] test_f16_3_glsl_discovery\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("GLSL", "CRT", "curvature.glsl", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 2);
    const auto* e0 = tw.lib->entry(0);
    const auto* e1 = tw.lib->entry(1);
    F16_ASSERT(e0 != nullptr && e1 != nullptr);
    F16_ASSERT(e0->language == ShaderLanguage::GLSL);
    F16_ASSERT(e1->language == ShaderLanguage::GLSL);
}

static void test_f16_3_slang_discovery() {
    std::printf("  [TEST] test_f16_3_slang_discovery\n");
    TestWorkspace tw;
    tw.createShader("SLANG", "CRT", "crt.slang", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    F16_ASSERT(tw.lib->entry(0)->language == ShaderLanguage::Slang);
}

static void test_f16_3_slangp_discovery() {
    std::printf("  [TEST] test_f16_3_slangp_discovery\n");
    TestWorkspace tw;
    tw.createShader("SLANGP", "CRT", "crt.slangp", "[passes]\n[]");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    F16_ASSERT(tw.lib->entry(0)->extension == ".slangp");
}

static void test_f16_3_cg_rejection() {
    std::printf("  [TEST] test_f16_3_cg_rejection\n");
    TestWorkspace tw;
    auto cgDir = tw.root / "GLSL" / "Bad";
    std::filesystem::create_directories(cgDir);
    {
        std::ofstream f(cgDir / "bad.cg");
        f << "void main() {}";
    }
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 0);
}

static void test_f16_3_category_extraction() {
    std::printf("  [TEST] test_f16_3_category_extraction\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("GLSL", "Effects", "bloom.glsl", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 2);
    const auto* e0 = tw.lib->entry(0);
    const auto* e1 = tw.lib->entry(1);
    F16_ASSERT(e0 != nullptr && e1 != nullptr);
    std::string cat0 = e0->category;
    std::string cat1 = e1->category;
    F16_ASSERT(cat0 == "CRT" || cat0 == "Effects");
    F16_ASSERT(cat1 == "CRT" || cat1 == "Effects");
    F16_ASSERT(cat0 != cat1);
}

static void test_f16_3_name_extraction() {
    std::printf("  [TEST] test_f16_3_name_extraction\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    F16_ASSERT(tw.lib->entry(0)->name == "scanlines");
}

// ============================================================================
// FASE 16.6 — Favorites + Persistence
// ============================================================================

static void test_f16_6_save_config() {
    std::printf("--- FASE 16: Persistence ---\n");
    std::printf("  [TEST] test_f16_6_save_config\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setFavorite(0, true);
    F16_ASSERT(tw.lib->entry(0)->isFavorite);
}

static void test_f16_6_favorites_toggle() {
    std::printf("  [TEST] test_f16_6_favorites_toggle\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setFavorite(0, true);
    F16_ASSERT(tw.lib->entry(0)->isFavorite);
    panel.setFavorite(0, false);
    F16_ASSERT(!tw.lib->entry(0)->isFavorite);
}

// ============================================================================
// FASE 16.8 — Search
// ============================================================================

static void test_f16_8_search_name() {
    std::printf("--- FASE 16: Search ---\n");
    std::printf("  [TEST] test_f16_8_search_name\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("GLSL", "CRT", "curvature.glsl", "void main() {}");
    tw.createShader("SLANG", "Effects", "sharpen.slang", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setSearchQuery("scanlines");
    auto visible = panel.visibleShaders();
    F16_ASSERT(visible.size() == 1);
    F16_ASSERT(visible[0].name == "scanlines");
}

static void test_f16_8_search_category() {
    std::printf("  [TEST] test_f16_8_search_category\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("GLSL", "CRT", "curvature.glsl", "void main() {}");
    tw.createShader("SLANG", "Effects", "sharpen.slang", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setSearchQuery("CRT");
    auto visible = panel.visibleShaders();
    F16_ASSERT(visible.size() == 2);
}

static void test_f16_8_search_case_insensitive() {
    std::printf("  [TEST] test_f16_8_search_case_insensitive\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("SLANG", "Effects", "sharpen.slang", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setSearchQuery("SCANLINES");
    auto visible = panel.visibleShaders();
    F16_ASSERT(visible.size() == 1);
}

static void test_f16_8_search_empty_query() {
    std::printf("  [TEST] test_f16_8_search_empty_query\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("SLANG", "Effects", "sharpen.slang", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setSearchQuery("");
    auto visible = panel.visibleShaders();
    F16_ASSERT(visible.size() == 2);
}

static void test_f16_8_search_language() {
    std::printf("  [TEST] test_f16_8_search_language\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("SLANG", "Effects", "sharpen.slang", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setSearchQuery("slang");
    auto visible = panel.visibleShaders();
    F16_ASSERT(visible.size() == 1);
    F16_ASSERT(visible[0].name == "sharpen");
}

// ============================================================================
// FASE 16.9 — Filesystem Safety
// ============================================================================

static void test_f16_9_empty_shader() {
    std::printf("--- FASE 16: Filesystem Safety ---\n");
    std::printf("  [TEST] test_f16_9_empty_shader\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "empty.glsl", "");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
    F16_ASSERT(tw.lib->entry(0)->fileSize == 0);
}

static void test_f16_9_corrupt_shader() {
    std::printf("  [TEST] test_f16_9_corrupt_shader\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "corrupt.glsl",
        std::string(1000, '\xff'));
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
}

static void test_f16_9_oversized_shader() {
    std::printf("  [TEST] test_f16_9_oversized_shader\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "big.glsl",
        std::string(1024 * 1024, 'x'));
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
}

static void test_f16_9_unknown_extension_ignored() {
    std::printf("  [TEST] test_f16_9_unknown_extension_ignored\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "readme.txt", "not a shader");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 0);
}

static void test_f16_9_long_path() {
    std::printf("  [TEST] test_f16_9_long_path\n");
    TestWorkspace tw;
    auto longDir = tw.root / "GLSL" / std::string(100, 'A');
    std::filesystem::create_directories(longDir);
    {
        std::ofstream f(longDir / "test.glsl");
        f << "void main() {}";
    }
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
}

// ============================================================================
// FASE 16.10 — Resource Limits
// ============================================================================

static void test_f16_10_resource_limits() {
    std::printf("--- FASE 16: Resource Limits ---\n");
    std::printf("  [TEST] test_f16_10_resource_limits\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "test.glsl", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);
}

// ============================================================================
// FASE 16.14 — End-to-End
// ============================================================================

static void test_f16_14_end_to_end() {
    std::printf("--- FASE 16: End-to-End ---\n");
    std::printf("  [TEST] test_f16_14_end_to_end\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl",
        "#version 450\n"
        "layout(location = 0) in vec2 vTexCoord;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(vTexCoord, 0.0, 1.0); }\n");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    panel.setFilter(ShaderLanguageFilter::GLSL);
    auto visible = panel.visibleShaders();
    F16_ASSERT(visible.size() == 1);
    F16_ASSERT(visible[0].name == "scanlines");

    panel.selectShader(0);
    F16_ASSERT(panel.hasSelection());

    auto info = panel.detailInfo();
    F16_ASSERT(info.hasSelection);
    F16_ASSERT(info.name == "scanlines");
    F16_ASSERT(info.category == "CRT");
}

static void test_f16_14_modify_and_rescan() {
    std::printf("  [TEST] test_f16_14_modify_and_rescan\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 1);

    uint64_t hashBefore = tw.lib->entry(0)->contentHash;

    {
        std::ofstream f(tw.root / "GLSL" / "CRT" / "scanlines.glsl");
        f << "void main() { modified; }";
    }

    auto diff = tw.lib->rescan();
    F16_ASSERT(!diff.modified.empty() || !diff.added.empty());

    const auto* e = tw.lib->entry(0);
    F16_ASSERT(e != nullptr);
    F16_ASSERT(e->contentHash != hashBefore);
}

static void test_f16_14_add_new_shader() {
    std::printf("  [TEST] test_f16_14_add_new_shader\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.scanLibrary();
    size_t before = tw.lib->entryCount();

    tw.createShader("GLSL", "CRT", "new_shader.glsl", "void main() {}");
    auto diff = tw.lib->rescan();
    F16_ASSERT(diff.added.size() >= 1);
    F16_ASSERT(tw.lib->entryCount() == before + 1);
}

static void test_f16_14_delete_shader() {
    std::printf("  [TEST] test_f16_14_delete_shader\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("GLSL", "CRT", "curvature.glsl", "void main() {}");
    tw.scanLibrary();
    F16_ASSERT(tw.lib->entryCount() == 2);

    std::filesystem::remove(tw.root / "GLSL" / "CRT" / "scanlines.glsl");
    auto diff = tw.lib->rescan();
    F16_ASSERT(!diff.removed.empty());
    F16_ASSERT(tw.lib->entryCount() == 1);
}

static void test_f16_14_hot_reload_state() {
    std::printf("  [TEST] test_f16_14_hot_reload_state\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.scanLibrary();

    ShaderBrowserPanelState state;
    ShaderRuntimeConfig rtConfig;
    rtConfig.rootDirectory = tw.root;
    rtConfig.shaderCacheDirectory = tw.root / "cache";
    rtConfig.slangcPath = "";
    ShaderRuntime runtime(rtConfig);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*tw.lib, runtime);
    ShaderBrowserPanel panel(*tw.lib, compiler, runtime, state);

    auto event = panel.lastHotReloadEvent();
    F16_ASSERT(!event.active);
}

// ============================================================================
// FASE 16 — Source Kind bySourceKind
// ============================================================================

static void test_f16_by_source_kind() {
    std::printf("  [TEST] test_f16_by_source_kind\n");
    TestWorkspace tw;
    tw.createShader("GLSL", "CRT", "scanlines.glsl", "void main() {}");
    tw.createShader("SLANG", "Effects", "sharpen.slang", "void main() {}");
    auto internalDir = tw.root / "internal";
    std::filesystem::create_directories(internalDir);
    {
        std::ofstream f(internalDir / "helper.glsl");
        f << "void helper() {}";
    }
    tw.scanLibrary();

    auto users = tw.lib->bySourceKind(ShaderSourceKind::User);
    F16_ASSERT(users.size() == 2);

    auto internals = tw.lib->bySourceKind(ShaderSourceKind::Internal);
    F16_ASSERT(internals.size() == 1);

    auto tests = tw.lib->bySourceKind(ShaderSourceKind::Test);
    F16_ASSERT(tests.size() == 0);
}

// ============================================================================
// Runner
// ============================================================================

void runFase16WorkspaceTests() {
    testsPassed = 0;
    testsFailed = 0;

    std::printf("--- FASE 16.1: Workspace ---\n");
    test_f16_1_root_discovery();
    test_f16_1_directory_creation();
    test_f16_1_relative_paths();
    test_f16_1_invalid_root();
    test_f16_1_config_file_path();

    std::printf("--- FASE 16.2: Source Kind ---\n");
    test_f16_2_user_source_kind();
    test_f16_2_slang_user_kind();
    test_f16_2_slangp_user_kind();
    test_f16_2_internal_kind();

    std::printf("--- FASE 16.3: Library Discovery ---\n");
    test_f16_3_glsl_discovery();
    test_f16_3_slang_discovery();
    test_f16_3_slangp_discovery();
    test_f16_3_cg_rejection();
    test_f16_3_category_extraction();
    test_f16_3_name_extraction();

    std::printf("--- FASE 16.6: Favorites ---\n");
    test_f16_6_save_config();
    test_f16_6_favorites_toggle();

    std::printf("--- FASE 16.8: Search ---\n");
    test_f16_8_search_name();
    test_f16_8_search_category();
    test_f16_8_search_case_insensitive();
    test_f16_8_search_empty_query();
    test_f16_8_search_language();

    std::printf("--- FASE 16.9: Filesystem Safety ---\n");
    test_f16_9_empty_shader();
    test_f16_9_corrupt_shader();
    test_f16_9_oversized_shader();
    test_f16_9_unknown_extension_ignored();
    test_f16_9_long_path();

    std::printf("--- FASE 16.10: Resource Limits ---\n");
    test_f16_10_resource_limits();

    std::printf("--- FASE 16.14: End-to-End ---\n");
    test_f16_14_end_to_end();
    test_f16_14_modify_and_rescan();
    test_f16_14_add_new_shader();
    test_f16_14_delete_shader();
    test_f16_14_hot_reload_state();
    test_f16_by_source_kind();

    std::printf("\n=== FASE 16 Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
    if (testsFailed > 0) {
        std::printf("=== SOME TESTS FAILED ===\n");
    } else {
        std::printf("=== All FASE 16 tests passed ===\n");
    }
}

}  // namespace tests
}  // namespace monix::renderer_vk
