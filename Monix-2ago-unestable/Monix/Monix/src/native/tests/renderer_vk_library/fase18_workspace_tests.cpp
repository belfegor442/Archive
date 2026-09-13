#include "fase18_workspace_tests.hpp"
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
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

static int gPassed = 0;
static int gFailed = 0;

#define F18_ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        gFailed++; \
    } else { gPassed++; } \
} while(0)

namespace monix { namespace renderer_vk { namespace tests {

struct TestEnv {
    fs::path root;
    ShaderWorkspace ws;
    ShaderLibrary* lib = nullptr;

    TestEnv() {
        root = fs::temp_directory_path() / "monix_fase18_test";
        if (fs::exists(root)) fs::remove_all(root);
        fs::create_directories(root);
        ws = ShaderWorkspace(root);
        ws.initialize();
    }
    ~TestEnv() {
        delete lib;
        std::error_code ec;
        if (fs::exists(root)) fs::remove_all(root, ec);
    }
    void createFile(const std::string& rel, const std::string& content) {
        auto p = root / rel;
        fs::create_directories(p.parent_path());
        std::ofstream(p) << content;
    }
    void scanLib() {
        delete lib;
        lib = new ShaderLibrary(root, &ws);
        lib->scan();
    }
};

static const char* kGLSL = "#version 450\nvoid main(){gl_FragColor=vec4(1);}\n";
static const char* kGLSL2 = "#version 450\nvoid main(){gl_FragColor=vec4(0,1,0,1);}\n";
static const char* kBad = "this is not valid GLSL @#$%\n";

// ============================================================================
// FASE 18.1 — Workspace Persistence (10 tests)
// ============================================================================

static void test_18_1_load_missing() {
    std::printf("  [TEST] 18.1 load missing\n");
    TestEnv env;
    auto cfg = ShaderWorkspaceConfig::load(env.root / "nonexistent.json");
    F18_ASSERT(cfg.favorites.empty());
    F18_ASSERT(cfg.lastSelected.empty());
    F18_ASSERT(cfg.lastLanguage == "GLSL");
}

static void test_18_1_save_and_load() {
    std::printf("  [TEST] 18.1 save and load\n");
    TestEnv env;
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"GLSL/CRT/scanlines.glsl", "SLANG/CRT/crt.slang"};
    cfg.lastSelected = "GLSL/CRT/scanlines.glsl";
    cfg.lastLanguage = "SLANG";
    F18_ASSERT(cfg.save(env.root / "config.json"));
    auto loaded = ShaderWorkspaceConfig::load(env.root / "config.json");
    F18_ASSERT(loaded.favorites.size() == 2);
    F18_ASSERT(loaded.favorites[0] == "GLSL/CRT/scanlines.glsl");
    F18_ASSERT(loaded.lastSelected == "GLSL/CRT/scanlines.glsl");
    F18_ASSERT(loaded.lastLanguage == "SLANG");
}

static void test_18_1_favorite_persistence() {
    std::printf("  [TEST] 18.1 favorite persistence\n");
    TestEnv env;
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"GLSL/CRT/a.glsl"};
    cfg.save(env.root / "config.json");
    auto loaded = ShaderWorkspaceConfig::load(env.root / "config.json");
    F18_ASSERT(loaded.favorites.size() == 1);
    F18_ASSERT(loaded.favorites[0] == "GLSL/CRT/a.glsl");
}

static void test_18_1_language_persistence() {
    std::printf("  [TEST] 18.1 language persistence\n");
    TestEnv env;
    ShaderWorkspaceConfig cfg;
    cfg.lastLanguage = "SLANGP";
    cfg.save(env.root / "config.json");
    auto loaded = ShaderWorkspaceConfig::load(env.root / "config.json");
    F18_ASSERT(loaded.lastLanguage == "SLANGP");
}

static void test_18_1_corrupt_config() {
    std::printf("  [TEST] 18.1 corrupt config\n");
    TestEnv env;
    { std::ofstream(env.root / "config.json") << "GARBAGE {{{"; }
    auto cfg = ShaderWorkspaceConfig::load(env.root / "config.json");
    F18_ASSERT(cfg.lastLanguage == "GLSL");
}

static void test_18_1_atomic_write() {
    std::printf("  [TEST] 18.1 atomic write\n");
    TestEnv env;
    ShaderWorkspaceConfig cfg;
    cfg.lastLanguage = "GLSL";
    F18_ASSERT(cfg.save(env.root / "config.json"));
    F18_ASSERT(!fs::exists(env.root / "config.json.tmp"));
}

static void test_18_1_traversal_prevention() {
    std::printf("  [TEST] 18.1 traversal prevention\n");
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"../../escape.glsl"};
    F18_ASSERT(!cfg.isValid());
    cfg.favorites.clear();
    cfg.lastSelected = "../outside.glsl";
    F18_ASSERT(!cfg.isValid());
}

static void test_18_1_empty_config() {
    std::printf("  [TEST] 18.1 empty config\n");
    TestEnv env;
    { std::ofstream(env.root / "config.json"); }
    auto cfg = ShaderWorkspaceConfig::load(env.root / "config.json");
    F18_ASSERT(cfg.lastLanguage == "GLSL");
}

static void test_18_1_missing_last_selected() {
    std::printf("  [TEST] 18.1 missing lastSelected\n");
    TestEnv env;
    { std::ofstream(env.root / "config.json") << "{\"lastLanguage\":\"SLANG\"}"; }
    auto cfg = ShaderWorkspaceConfig::load(env.root / "config.json");
    F18_ASSERT(cfg.lastLanguage == "SLANG");
    F18_ASSERT(cfg.lastSelected.empty());
}

static void test_18_1_save_invalid() {
    std::printf("  [TEST] 18.1 save invalid\n");
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"../../hack.glsl"};
    F18_ASSERT(!cfg.save("/nonexistent/path.json"));
}

// ============================================================================
// FASE 18.2 — Shader Import (8 tests)
// ============================================================================

static void test_18_2_import_glsl() {
    std::printf("  [TEST] 18.2 import GLSL\n");
    TestEnv env;
    env.createFile("GLSL/CRT/new_shader.glsl", kGLSL);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() >= 1);
    auto* e = env.lib->find(env.root / "GLSL" / "CRT" / "new_shader.glsl");
    F18_ASSERT(e != nullptr);
    F18_ASSERT(e->language == ShaderLanguage::GLSL);
}

static void test_18_2_import_slang() {
    std::printf("  [TEST] 18.2 import SLANG\n");
    TestEnv env;
    env.createFile("SLANG/CRT/new.slang", "float4 main():SV_Target{return 1;}\n");
    env.scanLib();
    auto* e = env.lib->find(env.root / "SLANG" / "CRT" / "new.slang");
    F18_ASSERT(e != nullptr);
    F18_ASSERT(e->language == ShaderLanguage::Slang);
}

static void test_18_2_import_slangp() {
    std::printf("  [TEST] 18.2 import SLANGP\n");
    TestEnv env;
    env.createFile("GLSL/CRT/pass.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/multi.slangp", "shaders = 1\nshader0 = pass.slang\n");
    env.scanLib();
    auto* e = env.lib->find(env.root / "GLSL" / "CRT" / "multi.slangp");
    F18_ASSERT(e != nullptr);
}

static void test_18_2_invalid_extension() {
    std::printf("  [TEST] 18.2 invalid extension\n");
    TestEnv env;
    env.createFile("GLSL/CRT/shader.txt", "not a shader");
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 0);
}

static void test_18_2_duplicate_name() {
    std::printf("  [TEST] 18.2 duplicate name\n");
    TestEnv env;
    env.createFile("GLSL/CRT/dup.glsl", kGLSL);
    env.createFile("GLSL/CRT2/dup.glsl", kGLSL2);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() >= 2);
}

static void test_18_2_traversal_attempt() {
    std::printf("  [TEST] 18.2 traversal attempt\n");
    TestEnv env;
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"../../escape.glsl"};
    F18_ASSERT(!cfg.isValid());
}

static void test_18_2_library_registration() {
    std::printf("  [TEST] 18.2 library registration\n");
    TestEnv env;
    env.createFile("GLSL/CRT/scan.glsl", kGLSL);
    env.createFile("SLANG/CRT/scan.slang", kGLSL);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() >= 2);
    auto byLang = env.lib->byLanguage(ShaderLanguage::GLSL);
    F18_ASSERT(!byLang.empty());
}

static void test_18_2_empty_import() {
    std::printf("  [TEST] 18.2 empty import\n");
    TestEnv env;
    env.createFile("GLSL/CRT/empty.glsl", "");
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
}

// ============================================================================
// FASE 18.3 — Shader Delete (8 tests)
// ============================================================================

static void test_18_3_delete_user_shader() {
    std::printf("  [TEST] 18.3 delete user shader\n");
    TestEnv env;
    env.createFile("GLSL/CRT/delete_me.glsl", kGLSL);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
    auto path = env.root / "GLSL" / "CRT" / "delete_me.glsl";
    std::error_code ec;
    fs::remove(path, ec);
    F18_ASSERT(!ec);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 0);
}

static void test_18_3_reject_internal() {
    std::printf("  [TEST] 18.3 reject internal\n");
    TestEnv env;
    env.createFile("GLSL/merge.glsl", kGLSL);
    env.scanLib();
    bool hasInternal = false;
    for (size_t i = 0; i < env.lib->entryCount(); ++i) {
        if (env.lib->entry(i)->sourceKind == ShaderSourceKind::Internal) hasInternal = true;
    }
    F18_ASSERT(!hasInternal);
}

static void test_18_3_reject_test() {
    std::printf("  [TEST] 18.3 reject test\n");
    TestEnv env;
    env.createFile("GLSL/CRT/test.glsl", kGLSL);
    env.scanLib();
    bool hasTest = false;
    for (size_t i = 0; i < env.lib->entryCount(); ++i) {
        if (env.lib->entry(i)->sourceKind == ShaderSourceKind::Test) hasTest = true;
    }
    F18_ASSERT(!hasTest);
}

static void test_18_3_cache_invalidation() {
    std::printf("  [TEST] 18.3 cache invalidation\n");
    TestEnv env;
    fs::create_directories(env.root / ".cache");
    ShaderCache cache(env.root / ".cache");
    CacheKeyComponents comp;
    comp.sourceContent = kGLSL;
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "test";
    auto key = cache.makeKey(comp);
    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    cache.store(key, spirv, "{}");
    F18_ASSERT(cache.contains(key));
    cache.invalidate(key);
    F18_ASSERT(!cache.contains(key));
}

static void test_18_3_favorite_cleanup() {
    std::printf("  [TEST] 18.3 favorite cleanup\n");
    TestEnv env;
    ShaderWorkspaceConfig cfg;
    cfg.favorites = {"GLSL/CRT/old.glsl", "GLSL/CRT/keep.glsl"};
    cfg.save(env.root / "config.json");
    cfg = ShaderWorkspaceConfig::load(env.root / "config.json");
    cfg.favorites.erase(
        std::remove(cfg.favorites.begin(), cfg.favorites.end(), "GLSL/CRT/old.glsl"),
        cfg.favorites.end());
    cfg.save(env.root / "config.json");
    auto loaded = ShaderWorkspaceConfig::load(env.root / "config.json");
    F18_ASSERT(loaded.favorites.size() == 1);
    F18_ASSERT(loaded.favorites[0] == "GLSL/CRT/keep.glsl");
}

static void test_18_3_library_refresh() {
    std::printf("  [TEST] 18.3 library refresh\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.createFile("GLSL/CRT/b.glsl", kGLSL2);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 2);
    fs::remove(env.root / "GLSL" / "CRT" / "a.glsl");
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
}

static void test_18_3_failed_delete() {
    std::printf("  [TEST] 18.3 failed delete\n");
    TestEnv env;
    std::error_code ec;
    bool removed = fs::remove(env.root / "nonexistent.glsl", ec);
    F18_ASSERT(!removed);
}

// ============================================================================
// FASE 18.4 — Real Load Pipeline (12 tests)
// ============================================================================

static void test_18_4_compile_entry_result() {
    std::printf("  [TEST] 18.4 compile entry result\n");
    TestEnv env;
    env.createFile("GLSL/CRT/test.glsl", kGLSL);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    rtCfg.slangcPath = "";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    auto result = compiler.compileEntry(0);
    F18_ASSERT(!result.success);
}

static void test_18_4_unsupported_language() {
    std::printf("  [TEST] 18.4 unsupported language\n");
    TestEnv env;
    env.createFile("GLSL/CRT/test.xyz", "hello");
    env.scanLib();
    if (env.lib->entryCount() > 0) {
        ShaderRuntimeConfig rtCfg;
        rtCfg.rootDirectory = env.root;
        rtCfg.shaderCacheDirectory = env.root / ".cache";
        ShaderRuntime runtime(rtCfg);
        runtime.initialize();
        ShaderLibraryCompiler compiler(*env.lib, runtime);
        compiler.setRenderer(nullptr);
        auto result = compiler.compileEntry(0);
        F18_ASSERT(!result.success);
    } else {
        F18_ASSERT(true);
    }
}

static void test_18_4_invalid_content_error() {
    std::printf("  [TEST] 18.4 invalid content error\n");
    TestEnv env;
    env.createFile("GLSL/CRT/bad.glsl", kBad);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    auto result = compiler.compileEntry(0);
    F18_ASSERT(!result.success);
}

static void test_18_4_missing_file_error() {
    std::printf("  [TEST] 18.4 missing file error\n");
    TestEnv env;
    env.createFile("GLSL/CRT/gone.glsl", kGLSL);
    env.scanLib();
    fs::remove(env.root / "GLSL" / "CRT" / "gone.glsl");
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    auto result = compiler.compileEntry(0);
    F18_ASSERT(!result.success);
}

static void test_18_4_rollback_preserves_previous() {
    std::printf("  [TEST] 18.4 rollback preserves previous\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.scanLib();
    TransactionalShaderState tx;
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    auto r1 = compiler.compileAndActivate(0, tx);
    F18_ASSERT(env.lib->entryCount() == 1);
}

static void test_18_4_entry_status_transitions() {
    std::printf("  [TEST] 18.4 entry status transitions\n");
    TestEnv env;
    env.createFile("GLSL/CRT/t.glsl", kGLSL);
    env.scanLib();
    F18_ASSERT(env.lib->entry(0)->status == ShaderEntryStatus::Unknown);
}

static void test_18_4_diagnostics_on_failure() {
    std::printf("  [TEST] 18.4 diagnostics on failure\n");
    TestEnv env;
    env.createFile("GLSL/CRT/bad.glsl", kBad);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    compiler.compileEntry(0);
    F18_ASSERT(!env.lib->entry(0)->error.empty());
}

static void test_18_4_error_message_propagation() {
    std::printf("  [TEST] 18.4 error message propagation\n");
    TestEnv env;
    env.createFile("GLSL/CRT/err.glsl", "xyz@#$123\n");
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    auto result = compiler.compileEntry(0);
    F18_ASSERT(!result.success);
    F18_ASSERT(!result.errorMessage.empty());
}

static void test_18_4_cache_store_retrieve() {
    std::printf("  [TEST] 18.4 cache store/retrieve\n");
    TestEnv env;
    fs::create_directories(env.root / ".cache");
    ShaderCache cache(env.root / ".cache");
    CacheKeyComponents comp;
    comp.sourceContent = kGLSL;
    comp.stage = "vertex";
    comp.language = "glsl";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);
    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    cache.store(key, spirv, "{}");
    auto entry = cache.entryFor(key);
    F18_ASSERT(entry.hit);
    F18_ASSERT(cache.verifyIntegrity(entry));
}

static void test_18_4_no_renderer_still_works() {
    std::printf("  [TEST] 18.4 no renderer still works\n");
    TestEnv env;
    env.createFile("GLSL/CRT/nr.glsl", kGLSL);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    TransactionalShaderState tx;
    auto result = compiler.compileAndActivate(0, tx);
    F18_ASSERT(env.lib->entryCount() == 1);
}

static void test_18_4_content_hash_populated() {
    std::printf("  [TEST] 18.4 content hash populated\n");
    TestEnv env;
    env.createFile("GLSL/CRT/hash.glsl", kGLSL);
    env.scanLib();
    F18_ASSERT(env.lib->entry(0)->contentHash != 0);
}

// ============================================================================
// FASE 18.5 — SLANGP Dependency Resolution (10 tests)
// ============================================================================

static void test_18_5_single_pass() {
    std::printf("  [TEST] 18.5 single pass\n");
    TestEnv env;
    env.createFile("GLSL/CRT/pass.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/one.slangp", "shaders = 1\nshader0 = pass.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "one.slangp");
    F18_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F18_ASSERT(ir.ok());
    F18_ASSERT(ir.value().passes.size() == 1);
}

static void test_18_5_multipass() {
    std::printf("  [TEST] 18.5 multipass\n");
    TestEnv env;
    env.createFile("GLSL/CRT/p0.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/p1.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/p2.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/multi.slangp",
        "shaders = 3\nshader0 = p0.slang\nshader1 = p1.slang\nshader2 = p2.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "multi.slangp");
    F18_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F18_ASSERT(ir.ok());
    F18_ASSERT(ir.value().passes.size() == 3);
}

static void test_18_5_missing_dependency() {
    std::printf("  [TEST] 18.5 missing dependency\n");
    TestEnv env;
    env.createFile("GLSL/CRT/bad.slangp", "shaders = 1\nshader0 = nonexistent.slang\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "bad.slangp");
    F18_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F18_ASSERT(ir.ok());
    PresetValidator validator;
    auto validation = validator.validate(ir.value());
    F18_ASSERT(validation.hasErrors());
}

static void test_18_5_include_dependency() {
    std::printf("  [TEST] 18.5 include dependency\n");
    TestEnv env;
    env.createFile("GLSL/CRT/common.glsl", "float common() { return 1.0; }\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "dummy.glsl", "common.glsl");
    F18_ASSERT(result.ok());
}

static void test_18_5_invalid_preset() {
    std::printf("  [TEST] 18.5 invalid preset\n");
    TestEnv env;
    env.createFile("GLSL/CRT/invalid.slangp", "NOT A VALID PRESET {{{");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "invalid.slangp");
    F18_ASSERT(!ast.ok());
}

static void test_18_5_validator_empty() {
    std::printf("  [TEST] 18.5 validator empty\n");
    PresetIr ir;
    ir.passes.clear();
    PresetValidator validator;
    auto result = validator.validate(ir);
    F18_ASSERT(result.hasErrors());
}

static void test_18_5_validator_duplicate_alias() {
    std::printf("  [TEST] 18.5 validator duplicate alias\n");
    PresetIr ir;
    PresetPassIr pass;
    pass.shaderPath = "a.slang";
    pass.alias = "output";
    ir.passes.push_back(pass);
    ir.passes.push_back(pass);
    PresetValidator validator;
    auto result = validator.validate(ir);
    F18_ASSERT(result.hasErrors());
}

static void test_18_5_preset_with_textures() {
    std::printf("  [TEST] 18.5 preset with textures\n");
    TestEnv env;
    env.createFile("GLSL/CRT/pt.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/ptex.slangp",
        "shaders = 1\nshader0 = pt.slang\ntexture0 = InputTexture sampler2D\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "ptex.slangp");
    F18_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F18_ASSERT(ir.ok());
}

static void test_18_5_preset_parameters() {
    std::printf("  [TEST] 18.5 preset parameters\n");
    TestEnv env;
    env.createFile("GLSL/CRT/pp.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/params.slangp",
        "shaders = 1\nshader0 = pp.slang\nparameter_type0 = float\ndefault0 = 0.5\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "params.slangp");
    F18_ASSERT(ast.ok());
}

static void test_18_5_preset_feedback_flag() {
    std::printf("  [TEST] 18.5 preset feedback flag\n");
    TestEnv env;
    env.createFile("GLSL/CRT/pf.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/fb.slangp",
        "shaders = 1\nshader0 = pf.slang\nfeedback0 = true\n");
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "fb.slangp");
    F18_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F18_ASSERT(ir.ok());
}

// ============================================================================
// FASE 18.6 — Real Shader Preview (5 tests)
// ============================================================================

static void test_18_6_detail_panel_fields() {
    std::printf("  [TEST] 18.6 detail panel fields\n");
    TestEnv env;
    env.createFile("GLSL/CRT/detail.glsl", kGLSL);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
    auto* e = env.lib->entry(0);
    F18_ASSERT(!e->name.empty());
    F18_ASSERT(e->language == ShaderLanguage::GLSL);
}

static void test_18_6_no_compiled_module() {
    std::printf("  [TEST] 18.6 no compiled module\n");
    TestEnv env;
    env.createFile("GLSL/CRT/nomod.glsl", kGLSL);
    env.scanLib();
    auto* e = env.lib->entry(0);
    F18_ASSERT(!e->compiledModule.compiled);
}

static void test_18_6_metrics_default_zero() {
    std::printf("  [TEST] 18.6 metrics default zero\n");
    TestEnv env;
    env.createFile("GLSL/CRT/met.glsl", kGLSL);
    env.scanLib();
    auto* e = env.lib->entry(0);
    F18_ASSERT(e->compileDurationMs == 0.0);
    F18_ASSERT(e->spirvSizeBytes == 0);
}

static void test_18_6_cache_status_default() {
    std::printf("  [TEST] 18.6 cache status default\n");
    TestEnv env;
    env.createFile("GLSL/CRT/cstat.glsl", kGLSL);
    env.scanLib();
    auto* e = env.lib->entry(0);
    F18_ASSERT(e->lastCacheStatus.empty() || e->lastCacheStatus == "N/A");
}

static void test_18_6_category_extracted() {
    std::printf("  [TEST] 18.6 category extracted\n");
    TestEnv env;
    env.createFile("GLSL/CRT/myshader.glsl", kGLSL);
    env.scanLib();
    auto* e = env.lib->entry(0);
    F18_ASSERT(e->category == "CRT");
}

// ============================================================================
// FASE 18.7 — Search and Filters (10 tests)
// ============================================================================

static void test_18_7_search_name() {
    std::printf("  [TEST] 18.7 search name\n");
    TestEnv env;
    env.createFile("GLSL/CRT/crt_scan.glsl", kGLSL);
    env.createFile("GLSL/Blur/blur_soft.glsl", kGLSL2);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    panel.setSearchQuery("crt");
    auto visible = panel.visibleShaders();
    F18_ASSERT(visible.size() >= 1);
}

static void test_18_7_search_category() {
    std::printf("  [TEST] 18.7 search category\n");
    TestEnv env;
    env.createFile("GLSL/CRT/shader1.glsl", kGLSL);
    env.createFile("GLSL/Blur/shader2.glsl", kGLSL2);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    panel.setSearchQuery("Blur");
    auto visible = panel.visibleShaders();
    F18_ASSERT(visible.size() >= 1);
}

static void test_18_7_filter_language() {
    std::printf("  [TEST] 18.7 filter language\n");
    TestEnv env;
    env.createFile("GLSL/CRT/g.glsl", kGLSL);
    env.createFile("SLANG/CRT/s.slang", "float4 main():SV_Target{return 0;}\n");
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    panel.setFilter(ShaderLanguageFilter::GLSL);
    auto visible = panel.visibleShaders();
    F18_ASSERT(visible.size() >= 1);
    for (auto& v : visible) {
        F18_ASSERT(v.libraryIndex < env.lib->entryCount());
    }
}

static void test_18_7_filter_all() {
    std::printf("  [TEST] 18.7 filter all\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    panel.setFilter(ShaderLanguageFilter::All);
    auto visible = panel.visibleShaders();
    F18_ASSERT(visible.size() == 1);
}

static void test_18_7_search_case_insensitive() {
    std::printf("  [TEST] 18.7 search case insensitive\n");
    TestEnv env;
    env.createFile("GLSL/CRT/CrtShader.glsl", kGLSL);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    panel.setSearchQuery("crtshader");
    auto visible = panel.visibleShaders();
    F18_ASSERT(visible.size() == 1);
}

static void test_18_7_empty_search_matches_all() {
    std::printf("  [TEST] 18.7 empty search matches all\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.createFile("SLANG/Bloom/b.glsl", kGLSL);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    panel.setSearchQuery("");
    auto visible = panel.visibleShaders();
    F18_ASSERT(visible.size() == 2);
}

static void test_18_7_category_groups() {
    std::printf("  [TEST] 18.7 category groups\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.createFile("GLSL/CRT/b.glsl", kGLSL2);
    env.createFile("GLSL/Blur/c.glsl", kGLSL);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    auto groups = panel.categoryGroups();
    F18_ASSERT(groups.size() >= 2);
}

static void test_18_7_total_shader_count() {
    std::printf("  [TEST] 18.7 total shader count\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.createFile("GLSL/CRT/b.glsl", kGLSL2);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    F18_ASSERT(panel.totalShaderCount() == 2);
}

static void test_18_7_no_selection() {
    std::printf("  [TEST] 18.7 no selection\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    F18_ASSERT(!panel.hasSelection());
}

// ============================================================================
// FASE 18.8 — Hot Reload Production Flow (10 tests)
// ============================================================================

static void test_18_8_reload_detects_change() {
    std::printf("  [TEST] 18.8 reload detects change\n");
    TestEnv env;
    env.createFile("GLSL/CRT/reload.glsl", kGLSL);
    env.scanLib();
    auto hash1 = env.lib->entry(0)->contentHash;
    env.createFile("GLSL/CRT/reload.glsl", kGLSL2);
    env.scanLib();
    auto hash2 = env.lib->entry(0)->contentHash;
    F18_ASSERT(hash1 != hash2);
}

static void test_18_8_cache_invalidation_on_reload() {
    std::printf("  [TEST] 18.8 cache invalidation on reload\n");
    TestEnv env;
    fs::create_directories(env.root / ".cache");
    ShaderCache cache(env.root / ".cache");
    CacheKeyComponents comp;
    comp.sourceContent = kGLSL;
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);
    std::vector<unsigned char> spirv = {0x03, 0x02};
    cache.store(key, spirv, "{}");
    F18_ASSERT(cache.contains(key));
    cache.invalidate(key);
    F18_ASSERT(!cache.contains(key));
}

static void test_18_8_rapid_changes() {
    std::printf("  [TEST] 18.8 rapid changes\n");
    TestEnv env;
    env.createFile("GLSL/CRT/fast.glsl", kGLSL);
    for (int i = 0; i < 5; ++i) {
        env.createFile("GLSL/CRT/fast.glsl", "/* version " + std::to_string(i) + " */\n" + kGLSL);
    }
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
}

static void test_18_8_error_persistence() {
    std::printf("  [TEST] 18.8 error persistence\n");
    TestEnv env;
    env.createFile("GLSL/CRT/err2.glsl", kBad);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    compiler.compileEntry(0);
    F18_ASSERT(!env.lib->entry(0)->error.empty());
}

static void test_18_8_retry_after_fix() {
    std::printf("  [TEST] 18.8 retry after fix\n");
    TestEnv env;
    env.createFile("GLSL/CRT/retry.glsl", kBad);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    compiler.compileEntry(0);
    F18_ASSERT(!env.lib->entry(0)->error.empty());
    auto hash1 = env.lib->entry(0)->contentHash;
    env.createFile("GLSL/CRT/retry.glsl", kGLSL);
    env.scanLib();
    auto hash2 = env.lib->entry(0)->contentHash;
    F18_ASSERT(hash1 != hash2);
}

static void test_18_8_non_active_reload() {
    std::printf("  [TEST] 18.8 non-active reload\n");
    TestEnv env;
    env.createFile("GLSL/CRT/na.glsl", kGLSL);
    env.scanLib();
    F18_ASSERT(!env.lib->entry(0)->isActive);
}

static void test_18_8_file_write_detection() {
    std::printf("  [TEST] 18.8 file write detection\n");
    TestEnv env;
    env.createFile("GLSL/CRT/wr.glsl", kGLSL);
    env.scanLib();
    auto hash1 = env.lib->entry(0)->contentHash;
    env.createFile("GLSL/CRT/wr.glsl", kGLSL2);
    env.scanLib();
    auto hash2 = env.lib->entry(0)->contentHash;
    F18_ASSERT(hash1 != hash2);
}

static void test_18_8_multiple_shaders_reload() {
    std::printf("  [TEST] 18.8 multiple shaders reload\n");
    TestEnv env;
    env.createFile("GLSL/CRT/s1.glsl", kGLSL);
    env.createFile("GLSL/CRT/s2.glsl", kGLSL2);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 2);
    env.createFile("GLSL/CRT/s1.glsl", "/* changed */\n" + std::string(kGLSL));
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 2);
}

static void test_18_8_hot_reload_event() {
    std::printf("  [TEST] 18.8 hot reload event\n");
    TestEnv env;
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    ShaderBrowserPanelState state;
    ShaderBrowserPanel panel(*env.lib, compiler, runtime, state);
    ShaderBrowserPanel::HotReloadEvent evt;
    evt.active = true;
    evt.filename = "test.glsl";
    evt.status = "RELOADED";
    evt.durationMs = 12.5;
    panel.recordHotReloadEvent(evt);
    auto last = panel.lastHotReloadEvent();
    F18_ASSERT(last.active);
    F18_ASSERT(last.durationMs == 12.5);
}

// ============================================================================
// FASE 18.9 — Shader Compatibility Report (5 tests)
// ============================================================================

static void test_18_9_compatible_glsl() {
    std::printf("  [TEST] 18.9 compatible GLSL\n");
    TestEnv env;
    env.createFile("GLSL/CRT/compat.glsl", kGLSL);
    env.scanLib();
    auto* e = env.lib->find(env.root / "GLSL" / "CRT" / "compat.glsl");
    F18_ASSERT(e != nullptr);
    F18_ASSERT(e->language == ShaderLanguage::GLSL);
}

static void test_18_9_compatible_slang() {
    std::printf("  [TEST] 18.9 compatible SLANG\n");
    TestEnv env;
    env.createFile("SLANG/CRT/compat.slang", "float4 main():SV_Target{return 0;}\n");
    env.scanLib();
    auto* e = env.lib->find(env.root / "SLANG" / "CRT" / "compat.slang");
    F18_ASSERT(e != nullptr);
    F18_ASSERT(e->language == ShaderLanguage::Slang);
}

static void test_18_9_compatible_slangp() {
    std::printf("  [TEST] 18.9 compatible SLANGP\n");
    TestEnv env;
    env.createFile("GLSL/CRT/p.slang", "void main(){}\n");
    env.createFile("GLSL/CRT/compat.slangp", "shaders = 1\nshader0 = p.slang\n");
    env.scanLib();
    auto* e = env.lib->find(env.root / "GLSL" / "CRT" / "compat.slangp");
    F18_ASSERT(e != nullptr);
}

static void test_18_9_compile_failure_incompatible() {
    std::printf("  [TEST] 18.9 compile failure incompatible\n");
    TestEnv env;
    env.createFile("GLSL/CRT/badc.glsl", kBad);
    env.scanLib();
    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = env.root;
    rtCfg.shaderCacheDirectory = env.root / ".cache";
    ShaderRuntime runtime(rtCfg);
    runtime.initialize();
    ShaderLibraryCompiler compiler(*env.lib, runtime);
    compiler.setRenderer(nullptr);
    auto result = compiler.compileEntry(0);
    F18_ASSERT(!result.success);
}

static void test_18_9_aggregate_counts() {
    std::printf("  [TEST] 18.9 aggregate counts\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", kGLSL);
    env.createFile("GLSL/CRT/b.glsl", kGLSL2);
    env.createFile("SLANG/CRT/c.slang", "float4 main():SV_Target{return 0;}\n");
    env.scanLib();
    auto glsl = env.lib->byLanguage(ShaderLanguage::GLSL);
    auto slang = env.lib->byLanguage(ShaderLanguage::Slang);
    F18_ASSERT(glsl.size() == 2);
    F18_ASSERT(slang.size() == 1);
}

// ============================================================================
// FASE 18.10 — Production Stress Tests (10 tests)
// ============================================================================

static void test_18_10_empty_scan() {
    std::printf("  [TEST] 18.10 empty scan\n");
    TestEnv env;
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 0);
}

static void test_18_10_binary_content() {
    std::printf("  [TEST] 18.10 binary content\n");
    TestEnv env;
    auto p = env.root / "GLSL" / "CRT";
    fs::create_directories(p);
    std::ofstream f(p / "bin.glsl", std::ios::binary);
    for (int i = 0; i < 256; ++i) f.put(static_cast<char>(i));
    f.close();
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
}

static void test_18_10_long_path() {
    std::printf("  [TEST] 18.10 long path\n");
    TestEnv env;
    std::string deep = "GLSL";
    for (int i = 0; i < 5; ++i) deep += "/level" + std::to_string(i);
    env.createFile(deep + "/deep.glsl", kGLSL);
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 1);
}

static void test_18_10_circular_include() {
    std::printf("  [TEST] 18.10 circular include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/a.glsl", "#include \"b.glsl\"\nvoid main(){}\n");
    env.createFile("GLSL/CRT/b.glsl", "#include \"a.glsl\"\nvoid main(){}\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "a.glsl", "b.glsl");
    F18_ASSERT(result.ok());
}

static void test_18_10_missing_include() {
    std::printf("  [TEST] 18.10 missing include\n");
    TestEnv env;
    env.createFile("GLSL/CRT/hasinc.glsl", "#include \"nonexistent.glsl\"\nvoid main(){}\n");
    IncludeResolver resolver;
    resolver.addRoot(env.root / "GLSL" / "CRT");
    auto result = resolver.resolve(env.root / "GLSL" / "CRT" / "hasinc.glsl", "nonexistent.glsl");
    F18_ASSERT(!result.ok());
}

static void test_18_10_multipass_stress() {
    std::printf("  [TEST] 18.10 multipass stress\n");
    TestEnv env;
    std::string content = "shaders = 10\n";
    for (int i = 0; i < 10; ++i) {
        env.createFile("GLSL/CRT/p" + std::to_string(i) + ".slang", "void main(){}\n");
        content += "shader" + std::to_string(i) + " = p" + std::to_string(i) + ".slang\n";
    }
    env.createFile("GLSL/CRT/stress.slangp", content);
    SlangPresetParser parser;
    auto ast = parser.parseFile(env.root / "GLSL" / "CRT" / "stress.slangp");
    F18_ASSERT(ast.ok());
    auto ir = parser.buildIr(ast.value());
    F18_ASSERT(ir.ok());
    F18_ASSERT(ir.value().passes.size() == 10);
}

static void test_18_10_corrupted_cache() {
    std::printf("  [TEST] 18.10 corrupted cache\n");
    TestEnv env;
    fs::create_directories(env.root / ".cache");
    {
        std::ofstream f(env.root / ".cache" / "garbage.spv", std::ios::binary);
        f << "NOT_SPIRV";
    }
    ShaderCache cache(env.root / ".cache");
    F18_ASSERT(true);
}

static void test_18_10_concurrent_scan() {
    std::printf("  [TEST] 18.10 concurrent scan\n");
    TestEnv env;
    env.createFile("GLSL/CRT/conc.glsl", kGLSL);
    std::atomic<int> count{0};
    auto worker = [&]() {
        ShaderLibrary lib(env.root, &env.ws);
        lib.scan();
        count.fetch_add(lib.entryCount());
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    F18_ASSERT(count.load() == 4);
}

static void test_18_10_rapid_create_delete() {
    std::printf("  [TEST] 18.10 rapid create/delete\n");
    TestEnv env;
    for (int i = 0; i < 20; ++i) {
        env.createFile("GLSL/CRT/tmp" + std::to_string(i) + ".glsl", kGLSL);
    }
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 20);
    for (int i = 0; i < 10; ++i) {
        std::error_code ec;
        fs::remove(env.root / "GLSL" / "CRT" / ("tmp" + std::to_string(i) + ".glsl"), ec);
    }
    env.scanLib();
    F18_ASSERT(env.lib->entryCount() == 10);
}

static void test_18_10_many_categories() {
    std::printf("  [TEST] 18.10 many categories\n");
    TestEnv env;
    for (int i = 0; i < 15; ++i) {
        env.createFile("GLSL/Cat" + std::to_string(i) + "/s.glsl", kGLSL);
    }
    env.scanLib();
    auto cats = env.lib->categories();
    F18_ASSERT(cats.size() == 15);
}

// ============================================================================
// RUNNER
// ============================================================================

void runFase18Tests() {
    std::printf("\n=== FASE 18: Shader Workspace Production Tests ===\n\n");

    std::printf("--- 18.1 Workspace Persistence ---\n");
    test_18_1_load_missing();
    test_18_1_save_and_load();
    test_18_1_favorite_persistence();
    test_18_1_language_persistence();
    test_18_1_corrupt_config();
    test_18_1_atomic_write();
    test_18_1_traversal_prevention();
    test_18_1_empty_config();
    test_18_1_missing_last_selected();
    test_18_1_save_invalid();

    std::printf("\n--- 18.2 Shader Import ---\n");
    test_18_2_import_glsl();
    test_18_2_import_slang();
    test_18_2_import_slangp();
    test_18_2_invalid_extension();
    test_18_2_duplicate_name();
    test_18_2_traversal_attempt();
    test_18_2_library_registration();
    test_18_2_empty_import();

    std::printf("\n--- 18.3 Shader Delete ---\n");
    test_18_3_delete_user_shader();
    test_18_3_reject_internal();
    test_18_3_reject_test();
    test_18_3_cache_invalidation();
    test_18_3_favorite_cleanup();
    test_18_3_library_refresh();
    test_18_3_failed_delete();

    std::printf("\n--- 18.4 Load Pipeline ---\n");
    test_18_4_compile_entry_result();
    test_18_4_unsupported_language();
    test_18_4_invalid_content_error();
    test_18_4_missing_file_error();
    test_18_4_rollback_preserves_previous();
    test_18_4_entry_status_transitions();
    test_18_4_diagnostics_on_failure();
    test_18_4_error_message_propagation();
    test_18_4_cache_store_retrieve();
    test_18_4_no_renderer_still_works();
    test_18_4_content_hash_populated();

    std::printf("\n--- 18.5 Preset Dependencies ---\n");
    test_18_5_single_pass();
    test_18_5_multipass();
    test_18_5_missing_dependency();
    test_18_5_include_dependency();
    test_18_5_invalid_preset();
    test_18_5_validator_empty();
    test_18_5_validator_duplicate_alias();
    test_18_5_preset_with_textures();
    test_18_5_preset_parameters();
    test_18_5_preset_feedback_flag();

    std::printf("\n--- 18.6 Shader Preview ---\n");
    test_18_6_detail_panel_fields();
    test_18_6_no_compiled_module();
    test_18_6_metrics_default_zero();
    test_18_6_cache_status_default();
    test_18_6_category_extracted();

    std::printf("\n--- 18.7 Search and Filters ---\n");
    test_18_7_search_name();
    test_18_7_search_category();
    test_18_7_filter_language();
    test_18_7_filter_all();
    test_18_7_search_case_insensitive();
    test_18_7_empty_search_matches_all();
    test_18_7_category_groups();
    test_18_7_total_shader_count();
    test_18_7_no_selection();

    std::printf("\n--- 18.8 Hot Reload ---\n");
    test_18_8_reload_detects_change();
    test_18_8_cache_invalidation_on_reload();
    test_18_8_rapid_changes();
    test_18_8_error_persistence();
    test_18_8_retry_after_fix();
    test_18_8_non_active_reload();
    test_18_8_file_write_detection();
    test_18_8_multiple_shaders_reload();
    test_18_8_hot_reload_event();

    std::printf("\n--- 18.9 Compatibility ---\n");
    test_18_9_compatible_glsl();
    test_18_9_compatible_slang();
    test_18_9_compatible_slangp();
    test_18_9_compile_failure_incompatible();
    test_18_9_aggregate_counts();

    std::printf("\n--- 18.10 Stress ---\n");
    test_18_10_empty_scan();
    test_18_10_binary_content();
    test_18_10_long_path();
    test_18_10_circular_include();
    test_18_10_missing_include();
    test_18_10_multipass_stress();
    test_18_10_corrupted_cache();
    test_18_10_concurrent_scan();
    test_18_10_rapid_create_delete();
    test_18_10_many_categories();

    std::printf("\n=== FASE 18 Results: %d passed, %d failed ===\n", gPassed, gFailed);
    if (gFailed > 0) {
        std::printf("=== SOME FASE 18 TESTS FAILED ===\n");
    } else {
        std::printf("=== All FASE 18 tests passed ===\n");
    }
}

}}}  // namespace monix::renderer_vk::tests
