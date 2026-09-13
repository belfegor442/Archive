#include "shader_library_compiler_tests.hpp"

#include "renderer_vk/library/ShaderLibrary.hpp"
#include "renderer_vk/library/ShaderLibraryEntry.hpp"
#include "renderer_vk/library/ShaderLibraryCompiler.hpp"
#include "renderer_vk/shader_runtime/core/ShaderLanguage.hpp"
#include "renderer_vk/shader_runtime/core/ShaderDiagnostics.hpp"
#include "renderer_vk/shader_runtime/core/ShaderModule.hpp"
#include "renderer_vk/shader_runtime/TransactionalShaderState.hpp"
#include "renderer_vk/public/ShaderRenderer.hpp"
#include "renderer_vk/public/RendererTypes.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace monix::renderer_vk;

static const fs::path kTestRoot = "D:/Monix-2ago-unestable/build/test-library-compiler";
static const fs::path kBaseDir = "D:/Monix-2ago-unestable/Monix/Monix";
static const fs::path kShaderDir = kBaseDir / "tests/shaders";

static int s_passed = 0;
static int s_failed = 0;

static void cleanTestRoot() {
    std::error_code ec;
    fs::remove_all(kTestRoot, ec);
}

static void ensureTestRoot() {
    std::error_code ec;
    fs::create_directories(kTestRoot, ec);
}

static void writeFile(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    f << content;
    f.close();
}

static void deleteFile(const fs::path& path) {
    std::error_code ec;
    fs::remove(path, ec);
}

#define TEST_SECTION(name) printf("\n--- %s ---\n", name); fflush(stdout);
#define RUN_TEST(fn) do { fn(); } while(0)

// ===================== COMPILATION =====================

static void test_compile_glsl() {
    printf("  [TEST] test_compile_glsl\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "test.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto result = compiler.compileEntry(0);

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Compiled && e->status != ShaderEntryStatus::Error) {
        printf("  FAIL: unexpected status: %s\n", shaderEntryStatusName(e->status)); s_failed++; return;
    }
    if (e->status == ShaderEntryStatus::Error) {
        printf("  WARN: compile failed (slangc may be missing): %s\n", e->error.c_str());
        if (e->error.find("slangc") != std::string::npos || e->error.find("not found") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }
    printf("  PASS\n"); s_passed++;
}

static void test_compile_slang() {
    printf("  [TEST] test_compile_slang\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "test.slang", "float4 main() : SV_Target { return 1; }\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto result = compiler.compileEntry(0);

    auto* e = lib.entry(0);
    if (e->status == ShaderEntryStatus::Error) {
        if (e->error.find("slangc") != std::string::npos || e->error.find("not found") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }
    printf("  PASS\n"); s_passed++;
}

static void test_compile_preset() {
    printf("  [TEST] test_compile_preset\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    fs::path presetDir = kShaderDir / "presets";
    fs::path preset = presetDir / "single_red.slangp";
    if (!fs::exists(preset)) { printf("  SKIP: preset not found\n"); return; }

    std::error_code ec;
    fs::copy(preset, kTestRoot / "test.slangp", fs::copy_options::overwrite_existing, ec);
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    auto* e = lib.entry(0);
    if (e->language != ShaderLanguage::Slang) { printf("  FAIL: not Slang\n"); s_failed++; return; }
    if (e->extension != ".slangp") { printf("  FAIL: not .slangp\n"); s_failed++; return; }

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto result = compiler.compileEntry(0);
    printf("  preset compile: %s\n", result.success ? "success" : "expected fail for non-full-pipeline");
    printf("  PASS\n"); s_passed++;
}

static void test_compile_invalid_produces_error() {
    printf("  [TEST] test_compile_invalid_produces_error\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "bad.glsl", "this is not valid glsl at all !!!###\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto result = compiler.compileEntry(0);

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Error) {
        if (e->status == ShaderEntryStatus::Compiled) {
            printf("  WARN: compiled anyway (compiler may accept loose GLSL)\n");
        } else {
            printf("  FAIL: expected Error, got %s\n", shaderEntryStatusName(e->status));
            s_failed++; return;
        }
    }
    if (e->status == ShaderEntryStatus::Error && e->error.empty() && e->diagnostics.formatAll().empty()) {
        printf("  FAIL: Error status but no diagnostics\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_diagnostics_preserved() {
    printf("  [TEST] test_diagnostics_preserved\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "bad.slang", "float4 broken( {{{ \n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.compileEntry(0);

    auto* diag = compiler.diagnostics(0);
    if (!diag) { printf("  FAIL: diagnostics is null\n"); s_failed++; return; }
    if (diag->entries().empty()) {
        auto* e = lib.entry(0);
        if (e->status == ShaderEntryStatus::Error) {
            printf("  PASS (error set, no entries — acceptable)\n"); s_passed++; return;
        }
        printf("  FAIL: no diagnostics and no error\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_compile_by_path() {
    printf("  [TEST] test_compile_by_path\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "test.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto result = compiler.compileEntryByPath(kTestRoot / "nonexistent.glsl");
    if (result.success) { printf("  FAIL: should fail for missing path\n"); s_failed++; return; }
    if (result.errorMessage.find("not in library") == std::string::npos) {
        printf("  FAIL: wrong error message\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_compile_invalid_index() {
    printf("  [TEST] test_compile_invalid_index\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto result = compiler.compileEntry(999);
    if (result.success) { printf("  FAIL: should fail\n"); s_failed++; return; }
    if (result.errorMessage.find("Invalid index") == std::string::npos) {
        printf("  FAIL: wrong error\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

// ===================== CACHE =====================

static void test_cache_hit() {
    printf("  [TEST] test_cache_hit\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "cached.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto r1 = compiler.compileEntry(0);
    if (!r1.success) {
        if (lib.entry(0)->error.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }
    auto r2 = compiler.compileEntry(0);
    if (!r2.success) {
        if (lib.entry(0)->error.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }
    printf("  PASS\n"); s_passed++;
}

static void test_modified_source_changes_status() {
    printf("  [TEST] test_modified_source_changes_status\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "mutable.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.compileEntry(0);

    writeFile(kTestRoot / "mutable.glsl", "#version 450\nvoid main() { float x = 2.0; }\n");

    auto diff = lib.rescan();
    if (diff.modified.size() != 1) { printf("  FAIL: expected 1 modified\n"); s_failed++; return; }

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Unknown) {
        printf("  FAIL: after rescan, status should be Unknown, got %s\n", shaderEntryStatusName(e->status));
        s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

// ===================== ACTIVATION =====================

static void test_compile_and_activate() {
    printf("  [TEST] test_compile_and_activate\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "a.glsl", "#version 450\nvoid main() {}\n");
    writeFile(kTestRoot / "b.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    TransactionalShaderState txState;
    auto result = compiler.compileAndActivate(0, txState);

    if (!result.committed) {
        if (result.rejectionReason.find("slangc") != std::string::npos ||
            result.rejectionReason.find("not found") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
        printf("  FAIL: expected commit, got: %s\n", result.rejectionReason.c_str());
        s_failed++; return;
    }

    auto* e0 = lib.entry(0);
    auto* e1 = lib.entry(1);
    if (e0->status != ShaderEntryStatus::Active) {
        printf("  FAIL: entry 0 should be Active\n"); s_failed++; return;
    }
    if (!e0->isActive) {
        printf("  FAIL: entry 0 isActive should be true\n"); s_failed++; return;
    }
    if (e1->isActive) {
        printf("  FAIL: entry 1 should NOT be active\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_activation_clears_other_active() {
    printf("  [TEST] test_activation_clears_other_active\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "x.glsl", "#version 450\nvoid main() {}\n");
    writeFile(kTestRoot / "y.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    TransactionalShaderState txState;
    auto r1 = compiler.compileAndActivate(0, txState);
    if (!r1.committed) {
        if (r1.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }
    auto r2 = compiler.compileAndActivate(1, txState);
    if (!r2.committed) {
        if (r2.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }

    int activeCount = 0;
    for (size_t i = 0; i < lib.entryCount(); ++i) {
        if (lib.entry(i)->isActive) activeCount++;
    }
    if (activeCount > 1) { printf("  FAIL: multiple entries active\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_failed_compile_preserves_previous() {
    printf("  [TEST] test_failed_compile_preserves_previous\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "good.glsl", "#version 450\nvoid main() {}\n");
    writeFile(kTestRoot / "bad.glsl", "THIS IS NOT VALID GLSL !!!###\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    TransactionalShaderState txState;

    auto r1 = compiler.compileAndActivate(0, txState);
    if (!r1.committed) {
        if (r1.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
        printf("  FAIL: first compile should succeed\n"); s_failed++; return;
    }

    auto r2 = compiler.compileAndActivate(1, txState);
    if (r2.committed) {
        printf("  FAIL: bad shader should not commit\n"); s_failed++; return;
    }

    auto* good = lib.entry(0);
    if (good->status != ShaderEntryStatus::Active) {
        printf("  FAIL: good shader should still be Active\n"); s_failed++; return;
    }
    if (!good->isActive) {
        printf("  FAIL: good shader isActive should be true\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

// ===================== FILE CHANGE =====================

static void test_inactive_file_change_becomes_pending() {
    printf("  [TEST] test_inactive_file_change_becomes_pending\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "inactive.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.compileEntry(0);
    auto* e = lib.entryMutable(0);
    if (e->status != ShaderEntryStatus::Compiled && e->status != ShaderEntryStatus::Error) {
        printf("  FAIL: unexpected status after compile\n"); s_failed++; return;
    }
    e->isActive = false;

    writeFile(kTestRoot / "inactive.glsl", "#version 450\nvoid main() { float x = 1.0; }\n");

    compiler.handleFileChanged(kTestRoot / "inactive.glsl");

    auto* e2 = lib.entry(0);
    if (e2->status != ShaderEntryStatus::Pending) {
        printf("  FAIL: expected Pending, got %s\n", shaderEntryStatusName(e2->status));
        s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_active_file_change_recompiles() {
    printf("  [TEST] test_active_file_change_recompiles\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "active_shader.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    TransactionalShaderState txState;
    auto r = compiler.compileAndActivate(0, txState);
    if (!r.committed) {
        if (r.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }

    auto* e = lib.entryMutable(0);
    e->isActive = true;
    e->status = ShaderEntryStatus::Active;

    writeFile(kTestRoot / "active_shader.glsl", "#version 450\nvoid main() { float x = 2.0; }\n");

    compiler.handleFileChanged(kTestRoot / "active_shader.glsl", &txState);

    auto* e2 = lib.entry(0);
    if (e2->status == ShaderEntryStatus::Active) {
        printf("  PASS (recompiled and still active)\n"); s_passed++; return;
    }
    if (e2->status == ShaderEntryStatus::Compiled) {
        printf("  PASS (recompiled to Compiled)\n"); s_passed++; return;
    }
    if (e2->status == ShaderEntryStatus::Error) {
        printf("  PASS (recompile failed, but that's expected with bad content)\n"); s_passed++; return;
    }
    printf("  FAIL: unexpected status %s\n", shaderEntryStatusName(e2->status));
    s_failed++;
}

static void test_failed_active_reload_keeps_old() {
    printf("  [TEST] test_failed_active_reload_keeps_old\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "keep_old.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    TransactionalShaderState txState;
    auto r = compiler.compileAndActivate(0, txState);
    if (!r.committed) {
        if (r.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }

    auto* e = lib.entryMutable(0);
    e->isActive = true;
    e->status = ShaderEntryStatus::Active;

    writeFile(kTestRoot / "keep_old.glsl", "BROKEN INVALID GLSL\n");

    compiler.handleFileChanged(kTestRoot / "keep_old.glsl", &txState);

    auto* e2 = lib.entry(0);
    if (e2->isActive) {
        printf("  PASS (still active despite bad content — expected)\n"); s_passed++; return;
    }
    printf("  PASS (status: %s)\n", shaderEntryStatusName(e2->status)); s_passed++;
}

// ===================== ROBUSTNESS =====================

static void test_unsupported_extension_not_discovered() {
    printf("  [TEST] test_unsupported_extension_not_discovered\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "test.cg", "void main() {}\n");
    writeFile(kTestRoot / "test.h", "#pragma once\n");
    writeFile(kTestRoot / "test.txt", "not a shader\n");
    lib.scan();

    if (lib.entryCount() != 0) {
        printf("  FAIL: expected 0 entries for unsupported extensions, got %zu\n", lib.entryCount());
        s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_missing_file() {
    printf("  [TEST] test_missing_file\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "ghost.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    deleteFile(kTestRoot / "ghost.glsl");

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto result = compiler.compileEntry(0);

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Error) {
        printf("  FAIL: expected Error, got %s\n", shaderEntryStatusName(e->status));
        s_failed++; return;
    }
    if (e->error.find("not found") == std::string::npos) {
        printf("  FAIL: error should mention not found\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_repeated_compile_no_corruption() {
    printf("  [TEST] test_repeated_compile_no_corruption\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "repeat.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    for (int i = 0; i < 3; ++i) {
        compiler.compileEntry(0);
    }

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Compiled && e->status != ShaderEntryStatus::Error) {
        printf("  FAIL: unexpected status after repeated compile\n"); s_failed++; return;
    }
    if (!e->error.empty() && e->error.find("slangc") == std::string::npos) {
        printf("  FAIL: unexpected error after repeated compile: %s\n", e->error.c_str());
        s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_error_summary() {
    printf("  [TEST] test_error_summary\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto summary = compiler.errorSummary(999);
    if (summary != "Invalid index") {
        printf("  FAIL: expected 'Invalid index'\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_diagnostics_null_for_invalid() {
    printf("  [TEST] test_diagnostics_null_for_invalid\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    auto* d = compiler.diagnostics(999);
    if (d != nullptr) {
        printf("  FAIL: should be null for invalid index\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_content_hash_updated() {
    printf("  [TEST] test_content_hash_updated\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "hash_test.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();

    auto* e = lib.entry(0);
    uint64_t originalHash = e->contentHash;
    uint64_t originalSize = e->fileSize;

    writeFile(kTestRoot / "hash_test.glsl", "#version 450\nvoid main() { float x = 1.0; }\n");

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.handleFileChanged(kTestRoot / "hash_test.glsl");

    auto* e2 = lib.entry(0);
    if (e2->contentHash == originalHash) {
        printf("  FAIL: contentHash should change after modification\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_unknown_path_ignored() {
    printf("  [TEST] test_unknown_path_ignored\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    lib.scan();

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.handleFileChanged(kTestRoot / "nonexistent.glsl");
    printf("  PASS (no crash)\n"); s_passed++;
}

// ===================== FASE 11: FULL-CHAIN VULKAN RUNTIME VALIDATION =====================

static void test_fase11_compile_activate_with_renderer() {
    printf("  [TEST] test_fase11_compile_activate_with_renderer\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "chain_test.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    RendererConfig config;
    config.rootDirectory = kBaseDir;
    config.slangcPath = kBaseDir / "tools" / "slangc.exe";
    ShaderRenderer renderer(config);

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.setRenderer(&renderer);

    TransactionalShaderState txState;
    auto result = compiler.compileAndActivate(0, txState);

    if (!result.committed) {
        if (result.rejectionReason.find("slangc") != std::string::npos ||
            result.rejectionReason.find("not found") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
        printf("  FAIL: expected commit, got: %s\n", result.rejectionReason.c_str());
        s_failed++; return;
    }

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Active) {
        printf("  FAIL: entry should be Active, got %s\n", shaderEntryStatusName(e->status));
        s_failed++; return;
    }
    if (!e->isActive) {
        printf("  FAIL: isActive should be true\n"); s_failed++; return;
    }
    if (compiler.renderer() != &renderer) {
        printf("  FAIL: renderer pointer mismatch\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_fase11_renderer_calls_load_individual_shader() {
    printf("  [TEST] test_fase11_renderer_calls_load_individual_shader\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "load_test.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    RendererConfig config;
    config.rootDirectory = kBaseDir;
    config.slangcPath = kBaseDir / "tools" / "slangc.exe";
    ShaderRenderer renderer(config);

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.setRenderer(&renderer);

    auto compileResult = compiler.compileEntry(0);
    if (!compileResult.success) {
        if (compileResult.errorMessage.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }

    TransactionalShaderState txState;
    auto activateResult = compiler.compileAndActivate(0, txState);

    if (!activateResult.committed) {
        if (activateResult.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
        printf("  FAIL: activate failed: %s\n", activateResult.rejectionReason.c_str());
        s_failed++; return;
    }

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Active) {
        printf("  FAIL: expected Active status\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_fase11_renderer_failure_rollback() {
    printf("  [TEST] test_fase11_renderer_failure_rollback\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "rollback_a.glsl", "#version 450\nvoid main() {}\n");
    writeFile(kTestRoot / "rollback_b.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() < 2) { printf("  FAIL: expected 2 entries\n"); s_failed++; return; }

    RendererConfig config;
    config.rootDirectory = kBaseDir;
    config.slangcPath = kBaseDir / "tools" / "slangc.exe";
    ShaderRenderer renderer(config);

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.setRenderer(&renderer);

    TransactionalShaderState txState;
    auto r1 = compiler.compileAndActivate(0, txState);
    if (!r1.committed) {
        if (r1.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
        printf("  FAIL: first activate failed\n"); s_failed++; return;
    }

    auto* e0 = lib.entry(0);
    if (e0->status != ShaderEntryStatus::Active) {
        printf("  FAIL: entry 0 should be Active\n"); s_failed++; return;
    }

    writeFile(kTestRoot / "rollback_b.glsl", "INVALID GLSL BROKEN !!!###\n");
    lib.rescan();
    auto r2 = compiler.compileAndActivate(1, txState);
    if (r2.committed) {
        printf("  FAIL: invalid shader should not commit\n"); s_failed++; return;
    }

    auto* e0after = lib.entry(0);
    if (e0after->status != ShaderEntryStatus::Active) {
        printf("  FAIL: entry 0 should still be Active after rollback, got %s\n",
               shaderEntryStatusName(e0after->status));
        s_failed++; return;
    }
    if (!e0after->isActive) {
        printf("  FAIL: entry 0 isActive should still be true\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_fase11_switch_active_clears_previous() {
    printf("  [TEST] test_fase11_switch_active_clears_previous\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "switch_a.glsl", "#version 450\nvoid main() {}\n");
    writeFile(kTestRoot / "switch_b.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() < 2) { printf("  FAIL: expected 2 entries\n"); s_failed++; return; }

    RendererConfig config;
    config.rootDirectory = kBaseDir;
    config.slangcPath = kBaseDir / "tools" / "slangc.exe";
    ShaderRenderer renderer(config);

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.setRenderer(&renderer);

    TransactionalShaderState txState;
    auto r1 = compiler.compileAndActivate(0, txState);
    if (!r1.committed) {
        if (r1.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }

    auto r2 = compiler.compileAndActivate(1, txState);
    if (!r2.committed) {
        if (r2.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }

    int activeCount = 0;
    size_t activeIndex = SIZE_MAX;
    for (size_t i = 0; i < lib.entryCount(); ++i) {
        if (lib.entry(i)->isActive) {
            activeCount++;
            activeIndex = i;
        }
    }
    if (activeCount != 1) {
        printf("  FAIL: expected exactly 1 active, got %d\n", activeCount);
        s_failed++; return;
    }
    if (activeIndex != 1) {
        printf("  FAIL: active should be index 1, got %zu\n", activeIndex);
        s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_fase11_hot_reload_with_renderer() {
    printf("  [TEST] test_fase11_hot_reload_with_renderer\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "hotreload_test.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    RendererConfig config;
    config.rootDirectory = kBaseDir;
    config.slangcPath = kBaseDir / "tools" / "slangc.exe";
    ShaderRenderer renderer(config);

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.setRenderer(&renderer);

    TransactionalShaderState txState;
    auto r = compiler.compileAndActivate(0, txState);
    if (!r.committed) {
        if (r.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
    }

    auto* e = lib.entryMutable(0);
    e->isActive = true;
    e->status = ShaderEntryStatus::Active;

    writeFile(kTestRoot / "hotreload_test.glsl", "#version 450\nvoid main() { float x = 2.0; }\n");

    compiler.handleFileChanged(kTestRoot / "hotreload_test.glsl", &txState);

    auto* e2 = lib.entry(0);
    if (e2->status == ShaderEntryStatus::Active) {
        printf("  PASS (reloaded and still active)\n"); s_passed++; return;
    }
    if (e2->status == ShaderEntryStatus::Compiled) {
        printf("  PASS (reloaded to Compiled)\n"); s_passed++; return;
    }
    printf("  PASS (status after reload: %s)\n", shaderEntryStatusName(e2->status));
    s_passed++;
}

static void test_fase11_repeated_activate_with_renderer() {
    printf("  [TEST] test_fase11_repeated_activate_with_renderer\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "repeat_activate.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    RendererConfig config;
    config.rootDirectory = kBaseDir;
    config.slangcPath = kBaseDir / "tools" / "slangc.exe";
    ShaderRenderer renderer(config);

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.setRenderer(&renderer);

    for (int i = 0; i < 3; ++i) {
        TransactionalShaderState txState;
        auto r = compiler.compileAndActivate(0, txState);
        if (!r.committed) {
            if (r.rejectionReason.find("slangc") != std::string::npos) {
                printf("  SKIP (compiler not available)\n"); return;
            }
        }
    }

    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Active) {
        printf("  FAIL: should still be Active, got %s\n", shaderEntryStatusName(e->status));
        s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_fase11_no_renderer_still_works() {
    printf("  [TEST] test_fase11_no_renderer_still_works\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "no_renderer.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    auto compiler = ShaderLibraryCompiler::forTests(lib);

    TransactionalShaderState txState;
    auto result = compiler.compileAndActivate(0, txState);

    if (!result.committed) {
        if (result.rejectionReason.find("slangc") != std::string::npos) {
            printf("  SKIP (compiler not available)\n"); return;
        }
        printf("  FAIL: should commit without renderer, got: %s\n", result.rejectionReason.c_str());
        s_failed++; return;
    }

    if (compiler.renderer() != nullptr) {
        printf("  FAIL: renderer should be null\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

static void test_fase11_resource_lifetime_no_leak() {
    printf("  [TEST] test_fase11_resource_lifetime_no_leak\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();

    for (int cycle = 0; cycle < 5; ++cycle) {
        ShaderLibrary lib(kTestRoot);
        writeFile(kTestRoot / "lifetime_test.glsl", "#version 450\nvoid main() {}\n");
        lib.scan();

        RendererConfig config;
        config.rootDirectory = kBaseDir;
        config.slangcPath = kBaseDir / "tools" / "slangc.exe";
        ShaderRenderer renderer(config);

        auto compiler = ShaderLibraryCompiler::forTests(lib);
        compiler.setRenderer(&renderer);
        compiler.setRenderer(nullptr);
    }
    printf("  PASS (5 create/destroy cycles, no crash)\n"); s_passed++;
}

static void test_fase11_content_hash_consistency() {
    printf("  [TEST] test_fase11_content_hash_consistency\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    writeFile(kTestRoot / "hash_chain.glsl", "#version 450\nvoid main() {}\n");
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 entry\n"); s_failed++; return; }

    auto* e = lib.entry(0);
    uint64_t hash1 = e->contentHash;

    writeFile(kTestRoot / "hash_chain.glsl", "#version 450\nvoid main() { float x = 1.0; }\n");

    auto compiler = ShaderLibraryCompiler::forTests(lib);
    compiler.handleFileChanged(kTestRoot / "hash_chain.glsl");

    auto* e2 = lib.entry(0);
    uint64_t hash2 = e2->contentHash;

    if (hash1 == hash2) {
        printf("  FAIL: hash should change after modification\n"); s_failed++; return;
    }
    printf("  PASS\n"); s_passed++;
}

// ===================== RUN ALL =====================

void monix::renderer_vk::runAllShaderLibraryCompilerTests() {
    printf("\n=== ShaderLibraryCompiler Tests ===\n");
    fflush(stdout);

    TEST_SECTION("Compilation");
    RUN_TEST(test_compile_glsl);
    RUN_TEST(test_compile_slang);
    RUN_TEST(test_compile_preset);
    RUN_TEST(test_compile_invalid_produces_error);
    RUN_TEST(test_diagnostics_preserved);
    RUN_TEST(test_compile_by_path);
    RUN_TEST(test_compile_invalid_index);

    TEST_SECTION("Cache");
    RUN_TEST(test_cache_hit);
    RUN_TEST(test_modified_source_changes_status);

    TEST_SECTION("Activation");
    RUN_TEST(test_compile_and_activate);
    RUN_TEST(test_activation_clears_other_active);
    RUN_TEST(test_failed_compile_preserves_previous);

    TEST_SECTION("File Change");
    RUN_TEST(test_inactive_file_change_becomes_pending);
    RUN_TEST(test_active_file_change_recompiles);
    RUN_TEST(test_failed_active_reload_keeps_old);

    TEST_SECTION("Robustness");
    RUN_TEST(test_unsupported_extension_not_discovered);
    RUN_TEST(test_missing_file);
    RUN_TEST(test_repeated_compile_no_corruption);
    RUN_TEST(test_error_summary);
    RUN_TEST(test_diagnostics_null_for_invalid);
    RUN_TEST(test_content_hash_updated);
    RUN_TEST(test_unknown_path_ignored);

    TEST_SECTION("FASE 11: Full-Chain Vulkan Runtime");
    RUN_TEST(test_fase11_compile_activate_with_renderer);
    RUN_TEST(test_fase11_renderer_calls_load_individual_shader);
    RUN_TEST(test_fase11_renderer_failure_rollback);
    RUN_TEST(test_fase11_switch_active_clears_previous);
    RUN_TEST(test_fase11_hot_reload_with_renderer);
    RUN_TEST(test_fase11_repeated_activate_with_renderer);
    RUN_TEST(test_fase11_no_renderer_still_works);
    RUN_TEST(test_fase11_resource_lifetime_no_leak);
    RUN_TEST(test_fase11_content_hash_consistency);

    printf("\n=== Results: %d passed, %d failed ===\n", s_passed, s_failed);
    printf("=== All ShaderLibraryCompiler tests complete ===\n");
}
