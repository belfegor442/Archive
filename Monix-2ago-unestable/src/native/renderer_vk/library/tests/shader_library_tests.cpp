#include "shader_library_tests.hpp"

#include "renderer_vk/library/ShaderLibrary.hpp"
#include "renderer_vk/library/ShaderLibraryEntry.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace monix::renderer_vk;

static const fs::path kTestRoot = "D:/Monix-2ago-unestable/build/test-shader-library";

static bool ensureTestRoot() {
    std::error_code ec;
    fs::create_directories(kTestRoot, ec);
    return !ec;
}

static void cleanTestRoot() {
    std::error_code ec;
    fs::remove_all(kTestRoot, ec);
}

static void writeFile(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    f << content;
    f.close();
}

static int s_passed = 0;
static int s_failed = 0;

#define TEST_SECTION(name) printf("\n--- %s ---\n", name); fflush(stdout);

#define RUN_TEST(fn) do { fn(); } while(0)

static void test_empty_directory() {
    printf("  [TEST] test_empty_directory\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 0) { printf("  FAIL: expected 0 entries, got %zu\n", lib.entryCount()); s_failed++; return; }
    if (!lib.empty()) { printf("  FAIL: expected empty\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_glsl_discovery() {
    printf("  [TEST] test_glsl_discovery\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "#version 450\nvoid main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1, got %zu\n", lib.entryCount()); s_failed++; return; }
    auto* e = lib.entry(0);
    if (!e) { printf("  FAIL: null entry\n"); s_failed++; return; }
    if (e->language != ShaderLanguage::GLSL) { printf("  FAIL: not GLSL\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_slang_discovery() {
    printf("  [TEST] test_slang_discovery\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.slang", "float4 main() : SV_Target { return 1; }\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1, got %zu\n", lib.entryCount()); s_failed++; return; }
    auto* e = lib.entry(0);
    if (e->language != ShaderLanguage::Slang) { printf("  FAIL: not Slang\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_slangp_discovery() {
    printf("  [TEST] test_slangp_discovery\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.slangp", "shaders=1\nshader0=test.glsl\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1, got %zu\n", lib.entryCount()); s_failed++; return; }
    auto* e = lib.entry(0);
    if (e->language != ShaderLanguage::Slang) { printf("  FAIL: not Slang (slandp)\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_mixed_language_discovery() {
    printf("  [TEST] test_mixed_language_discovery\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "b.slang", "float4 main() : SV_Target {}\n");
    writeFile(kTestRoot / "c.slangp", "shaders=1\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 3) { printf("  FAIL: expected 3, got %zu\n", lib.entryCount()); s_failed++; return; }
    auto glsl = lib.byLanguage(ShaderLanguage::GLSL);
    auto slang = lib.byLanguage(ShaderLanguage::Slang);
    if (glsl.size() != 1) { printf("  FAIL: expected 1 GLSL, got %zu\n", glsl.size()); s_failed++; return; }
    if (slang.size() != 2) { printf("  FAIL: expected 2 Slang, got %zu\n", slang.size()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_recursive_discovery() {
    printf("  [TEST] test_recursive_discovery\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/scanlines.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/Effects/bloom.glsl", "void main() {}\n");
    writeFile(kTestRoot / "SLANG/CRT/crt.slang", "float4 main() : SV_Target {}\n");
    writeFile(kTestRoot / "SLANGP/Presets/basic.slangp", "shaders=1\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 4) { printf("  FAIL: expected 4, got %zu\n", lib.entryCount()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_glsl_classification() {
    printf("  [TEST] test_glsl_classification\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "#version 450\nvoid main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    if (e->language != ShaderLanguage::GLSL) { printf("  FAIL: not GLSL\n"); s_failed++; return; }
    if (e->extension != ".glsl") { printf("  FAIL: wrong extension\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_slang_classification() {
    printf("  [TEST] test_slang_classification\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.slang", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    if (e->language != ShaderLanguage::Slang) { printf("  FAIL: not Slang\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_slangp_classification() {
    printf("  [TEST] test_slangp_classification\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.slangp", "shaders=1\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    if (e->language != ShaderLanguage::Slang) { printf("  FAIL: not Slang (slandp)\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_unknown_extension_ignored() {
    printf("  [TEST] test_unknown_extension_ignored\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.inc", "#define FOO\n");
    writeFile(kTestRoot / "test.h", "#pragma once\n");
    writeFile(kTestRoot / "test.txt", "not a shader\n");
    writeFile(kTestRoot / "test.json", "{}\n");
    writeFile(kTestRoot / "real.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1, got %zu\n", lib.entryCount()); s_failed++; return; }
    if (lib.entry(0)->name != "real") { printf("  FAIL: wrong entry\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_extension_wins_over_directory() {
    printf("  [TEST] test_extension_wins_over_directory\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/shader_in_glsldir.slang", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    if (e->language != ShaderLanguage::Slang) { printf("  FAIL: extension should win, got %s\n", shaderLanguageName(e->language)); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_name_extraction() {
    printf("  [TEST] test_name_extraction\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/scanlines.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    if (e->name != "scanlines") { printf("  FAIL: expected 'scanlines', got '%s'\n", e->name.c_str()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_category_extraction() {
    printf("  [TEST] test_category_extraction\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/scanlines.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/Effects/bloom.glsl", "void main() {}\n");
    writeFile(kTestRoot / "SLANG/Custom/my.slang", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 3) { printf("  FAIL: expected 3\n"); s_failed++; return; }
    bool foundCRT = false, foundEffects = false, foundCustom = false;
    for (size_t i = 0; i < lib.entryCount(); ++i) {
        auto* e = lib.entry(i);
        if (e->category == "CRT") foundCRT = true;
        if (e->category == "Effects") foundEffects = true;
        if (e->category == "Custom") foundCustom = true;
    }
    if (!foundCRT) { printf("  FAIL: CRT not found\n"); s_failed++; return; }
    if (!foundEffects) { printf("  FAIL: Effects not found\n"); s_failed++; return; }
    if (!foundCustom) { printf("  FAIL: Custom not found\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_relative_path() {
    printf("  [TEST] test_relative_path\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/scanlines.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    auto rel = e->relativePath.string();
    if (rel.find("GLSL") == std::string::npos) { printf("  FAIL: missing GLSL: %s\n", rel.c_str()); s_failed++; return; }
    if (rel.find("CRT") == std::string::npos) { printf("  FAIL: missing CRT: %s\n", rel.c_str()); s_failed++; return; }
    if (rel.find("scanlines.glsl") == std::string::npos) { printf("  FAIL: missing filename: %s\n", rel.c_str()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_file_size() {
    printf("  [TEST] test_file_size\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    std::string content = "Hello World! This is a test shader file.";
    writeFile(kTestRoot / "test.glsl", content);
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    if (e->fileSize != content.size()) { printf("  FAIL: expected %zu, got %llu\n", content.size(), (unsigned long long)e->fileSize); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_content_hash() {
    printf("  [TEST] test_content_hash\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "b.glsl", "void main() {}\n");
    writeFile(kTestRoot / "c.glsl", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 3) { printf("  FAIL: expected 3\n"); s_failed++; return; }
    const ShaderLibraryEntry* ea = nullptr;
    const ShaderLibraryEntry* eb = nullptr;
    const ShaderLibraryEntry* ec = nullptr;
    for (size_t i = 0; i < lib.entryCount(); ++i) {
        auto* e = lib.entry(i);
        if (e->name == "a") ea = e;
        if (e->name == "b") eb = e;
        if (e->name == "c") ec = e;
    }
    if (!ea || !eb || !ec) { printf("  FAIL: entries not found\n"); s_failed++; return; }
    if (ea->contentHash == 0) { printf("  FAIL: hash of a is 0\n"); s_failed++; return; }
    if (ea->contentHash != eb->contentHash) { printf("  FAIL: same content, different hash\n"); s_failed++; return; }
    if (ea->contentHash == ec->contentHash) { printf("  FAIL: different content, same hash\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_by_language_glsl() {
    printf("  [TEST] test_by_language_glsl\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "b.glsl", "void main() {}\n");
    writeFile(kTestRoot / "c.slang", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto glsl = lib.byLanguage(ShaderLanguage::GLSL);
    if (glsl.size() != 2) { printf("  FAIL: expected 2 GLSL, got %zu\n", glsl.size()); s_failed++; return; }
    for (auto* e : glsl) {
        if (e->language != ShaderLanguage::GLSL) { printf("  FAIL: non-GLSL in results\n"); s_failed++; return; }
    }
    printf("  PASS\n"); s_passed++;
}

static void test_by_language_slang() {
    printf("  [TEST] test_by_language_slang\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.slang", "float4 main() : SV_Target {}\n");
    writeFile(kTestRoot / "b.slangp", "shaders=1\n");
    writeFile(kTestRoot / "c.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto slang = lib.byLanguage(ShaderLanguage::Slang);
    if (slang.size() != 2) { printf("  FAIL: expected 2 Slang, got %zu\n", slang.size()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_by_language_slangp() {
    printf("  [TEST] test_by_language_slangp\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.slangp", "shaders=1\nshader0=test.glsl\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto slang = lib.byLanguage(ShaderLanguage::Slang);
    if (slang.size() != 1) { printf("  FAIL: expected 1\n"); s_failed++; return; }
    if (slang[0]->extension != ".slangp") { printf("  FAIL: wrong extension\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_by_category() {
    printf("  [TEST] test_by_category\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/CRT/b.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/Effects/c.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto crt = lib.byCategory("CRT");
    if (crt.size() != 2) { printf("  FAIL: expected 2 CRT, got %zu\n", crt.size()); s_failed++; return; }
    auto fx = lib.byCategory("Effects");
    if (fx.size() != 1) { printf("  FAIL: expected 1 Effects, got %zu\n", fx.size()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_categories() {
    printf("  [TEST] test_categories\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/Effects/b.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/Custom/c.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto cats = lib.categories();
    if (cats.size() != 3) { printf("  FAIL: expected 3, got %zu\n", cats.size()); s_failed++; return; }
    if (cats[0] != "CRT") { printf("  FAIL: wrong: %s\n", cats[0].c_str()); s_failed++; return; }
    if (cats[1] != "Custom") { printf("  FAIL: wrong: %s\n", cats[1].c_str()); s_failed++; return; }
    if (cats[2] != "Effects") { printf("  FAIL: wrong: %s\n", cats[2].c_str()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_find() {
    printf("  [TEST] test_find\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/scanlines.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.find(kTestRoot / "GLSL/CRT/scanlines.glsl");
    if (!e) { printf("  FAIL: find returned null\n"); s_failed++; return; }
    if (e->name != "scanlines") { printf("  FAIL: wrong: %s\n", e->name.c_str()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_contains() {
    printf("  [TEST] test_contains\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (!lib.contains(kTestRoot / "test.glsl")) { printf("  FAIL: should contain\n"); s_failed++; return; }
    if (lib.contains(kTestRoot / "nonexistent.glsl")) { printf("  FAIL: should not contain\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_entry_count() {
    printf("  [TEST] test_entry_count\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "b.slang", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 2) { printf("  FAIL: expected 2, got %zu\n", lib.entryCount()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_categories_for_language() {
    printf("  [TEST] test_categories_for_language\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/Effects/b.glsl", "void main() {}\n");
    writeFile(kTestRoot / "SLANG/CRT/c.slang", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto glslCats = lib.categoriesForLanguage(ShaderLanguage::GLSL);
    if (glslCats.size() != 2) { printf("  FAIL: expected 2 GLSL cats\n"); s_failed++; return; }
    auto slangCats = lib.categoriesForLanguage(ShaderLanguage::Slang);
    if (slangCats.size() != 1) { printf("  FAIL: expected 1 Slang cat\n"); s_failed++; return; }
    if (slangCats[0] != "CRT") { printf("  FAIL: expected CRT\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_rescan_unchanged() {
    printf("  [TEST] test_rescan_unchanged\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto diff = lib.rescan();
    if (diff.added.size() != 0) { printf("  FAIL: expected 0 added\n"); s_failed++; return; }
    if (diff.removed.size() != 0) { printf("  FAIL: expected 0 removed\n"); s_failed++; return; }
    if (diff.modified.size() != 0) { printf("  FAIL: expected 0 modified\n"); s_failed++; return; }
    if (diff.unchanged.size() != 1) { printf("  FAIL: expected 1 unchanged\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_rescan_modified() {
    printf("  [TEST] test_rescan_modified\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    writeFile(kTestRoot / "test.glsl", "void main() { float x = 1.0; }\n");
    auto diff = lib.rescan();
    if (diff.modified.size() != 1) { printf("  FAIL: expected 1 modified\n"); s_failed++; return; }
    if (diff.unchanged.size() != 0) { printf("  FAIL: expected 0 unchanged\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_rescan_added() {
    printf("  [TEST] test_rescan_added\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    writeFile(kTestRoot / "b.glsl", "void main() {}\n");
    auto diff = lib.rescan();
    if (diff.added.size() != 1) { printf("  FAIL: expected 1 added\n"); s_failed++; return; }
    if (diff.unchanged.size() != 1) { printf("  FAIL: expected 1 unchanged\n"); s_failed++; return; }
    if (lib.entryCount() != 2) { printf("  FAIL: expected 2 after rescan\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_rescan_removed() {
    printf("  [TEST] test_rescan_removed\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "b.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    std::error_code ec;
    fs::remove(kTestRoot / "b.glsl", ec);
    auto diff = lib.rescan();
    if (diff.removed.size() != 1) { printf("  FAIL: expected 1 removed\n"); s_failed++; return; }
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1 after rescan\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_rescan_content_hash_detects_modification() {
    printf("  [TEST] test_rescan_content_hash_detects_modification\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "original content\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    uint64_t originalHash = lib.entry(0)->contentHash;
    writeFile(kTestRoot / "test.glsl", "modified content\n");
    auto diff = lib.rescan();
    if (diff.modified.size() != 1) { printf("  FAIL: expected 1 modified\n"); s_failed++; return; }
    if (lib.entry(0)->contentHash == originalHash) { printf("  FAIL: hash unchanged\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_nonexistent_root() {
    printf("  [TEST] test_nonexistent_root\n"); fflush(stdout);
    ShaderLibrary lib("D:/Monix-2ago-unestable/build/nonexistent_shader_root_xyz");
    lib.scan();
    if (lib.entryCount() != 0) { printf("  FAIL: expected 0\n"); s_failed++; return; }
    if (!lib.empty()) { printf("  FAIL: expected empty\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_nested_directories() {
    printf("  [TEST] test_nested_directories\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "A/B/C/D/shader.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() != 1) { printf("  FAIL: expected 1\n"); s_failed++; return; }
    auto* e = lib.entry(0);
    if (e->category != "D") { printf("  FAIL: category '%s'\n", e->category.c_str()); s_failed++; return; }
    if (e->name != "shader") { printf("  FAIL: name '%s'\n", e->name.c_str()); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_deterministic_ordering() {
    printf("  [TEST] test_deterministic_ordering\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "GLSL/CRT/z.glsl", "void main() {}\n");
    writeFile(kTestRoot / "GLSL/Effects/a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "SLANG/Custom/m.glsl", "float4 main() : SV_Target {}\n");
    writeFile(kTestRoot / "GLSL/CRT/b.glsl", "void main() {}\n");

    std::vector<std::string> firstRun;
    {
        ShaderLibrary lib(kTestRoot);
        lib.scan();
        for (size_t i = 0; i < lib.entryCount(); ++i) {
            firstRun.push_back(lib.entry(i)->relativePath.string());
        }
    }
    std::vector<std::string> secondRun;
    {
        ShaderLibrary lib(kTestRoot);
        lib.scan();
        for (size_t i = 0; i < lib.entryCount(); ++i) {
            secondRun.push_back(lib.entry(i)->relativePath.string());
        }
    }
    if (firstRun.size() != secondRun.size()) { printf("  FAIL: different sizes\n"); s_failed++; return; }
    for (size_t i = 0; i < firstRun.size(); ++i) {
        if (firstRun[i] != secondRun[i]) {
            printf("  FAIL: [%zu] '%s' != '%s'\n", i, firstRun[i].c_str(), secondRun[i].c_str());
            s_failed++; return;
        }
    }
    printf("  PASS\n"); s_passed++;
}

static void test_rescan_preserves_state() {
    printf("  [TEST] test_rescan_preserves_state\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "b.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    lib.setStatus(0, ShaderEntryStatus::Compiled);
    lib.setError(1, "test error");
    auto diff = lib.rescan();
    if (diff.unchanged.size() != 2) { printf("  FAIL: expected 2 unchanged\n"); s_failed++; return; }
    bool foundCompiled = false, foundError = false;
    for (size_t i = 0; i < lib.entryCount(); ++i) {
        auto* e = lib.entry(i);
        if (e->status == ShaderEntryStatus::Compiled) foundCompiled = true;
        if (e->status == ShaderEntryStatus::Error && e->error == "test error") foundError = true;
    }
    if (!foundCompiled) { printf("  FAIL: Compiled not preserved\n"); s_failed++; return; }
    if (!foundError) { printf("  FAIL: Error not preserved\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_set_status() {
    printf("  [TEST] test_set_status\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entry(0)->status != ShaderEntryStatus::Unknown) { printf("  FAIL: initial status\n"); s_failed++; return; }
    lib.setStatus(0, ShaderEntryStatus::Compiled);
    if (lib.entry(0)->status != ShaderEntryStatus::Compiled) { printf("  FAIL: status\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_set_error() {
    printf("  [TEST] test_set_error\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "test.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    lib.setError(0, "compile failed: line 42");
    auto* e = lib.entry(0);
    if (e->status != ShaderEntryStatus::Error) { printf("  FAIL: status\n"); s_failed++; return; }
    if (e->error != "compile failed: line 42") { printf("  FAIL: error msg\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_set_active() {
    printf("  [TEST] test_set_active\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    writeFile(kTestRoot / "b.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    lib.setActive(0, true);
    if (!lib.entry(0)->isActive) { printf("  FAIL: not active\n"); s_failed++; return; }
    if (lib.entry(0)->status != ShaderEntryStatus::Active) { printf("  FAIL: status\n"); s_failed++; return; }
    if (lib.entry(1)->isActive) { printf("  FAIL: second active\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_clear_active() {
    printf("  [TEST] test_clear_active\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "a.glsl", "void main() {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    lib.setActive(0, true);
    lib.clearActive();
    if (lib.entry(0)->isActive) { printf("  FAIL: still active\n"); s_failed++; return; }
    if (lib.entry(0)->status == ShaderEntryStatus::Active) { printf("  FAIL: status still Active\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_entry_out_of_range() {
    printf("  [TEST] test_entry_out_of_range\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    auto* e = lib.entry(0);
    if (e != nullptr) { printf("  FAIL: should be null\n"); s_failed++; return; }
    e = lib.entry(999);
    if (e != nullptr) { printf("  FAIL: should be null\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_state_out_of_range() {
    printf("  [TEST] test_state_out_of_range\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    lib.setStatus(999, ShaderEntryStatus::Compiled);
    lib.setError(999, "test");
    lib.setActive(999, true);
    printf("  PASS (no crash)\n"); s_passed++;
}

static void test_shader_entry_status_name() {
    printf("  [TEST] test_shader_entry_status_name\n"); fflush(stdout);
    if (std::string(shaderEntryStatusName(ShaderEntryStatus::Unknown)) != "Unknown") { printf("  FAIL: Unknown\n"); s_failed++; return; }
    if (std::string(shaderEntryStatusName(ShaderEntryStatus::Pending)) != "Pending") { printf("  FAIL: Pending\n"); s_failed++; return; }
    if (std::string(shaderEntryStatusName(ShaderEntryStatus::Compiling)) != "Compiling") { printf("  FAIL: Compiling\n"); s_failed++; return; }
    if (std::string(shaderEntryStatusName(ShaderEntryStatus::Compiled)) != "Compiled") { printf("  FAIL: Compiled\n"); s_failed++; return; }
    if (std::string(shaderEntryStatusName(ShaderEntryStatus::Active)) != "Active") { printf("  FAIL: Active\n"); s_failed++; return; }
    if (std::string(shaderEntryStatusName(ShaderEntryStatus::Error)) != "Error") { printf("  FAIL: Error\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

static void test_individual_file_failure_doesnt_abort() {
    printf("  [TEST] test_individual_file_failure_doesnt_abort\n"); fflush(stdout);
    cleanTestRoot();
    ensureTestRoot();
    writeFile(kTestRoot / "good.glsl", "void main() {}\n");
    writeFile(kTestRoot / "alsogood.slang", "float4 main() : SV_Target {}\n");
    ShaderLibrary lib(kTestRoot);
    lib.scan();
    if (lib.entryCount() < 2) { printf("  FAIL: expected >=2\n"); s_failed++; return; }
    printf("  PASS\n"); s_passed++;
}

void monix::renderer_vk::runAllShaderLibraryTests() {
    printf("\n=== ShaderLibrary Tests ===\n");
    fflush(stdout);

    TEST_SECTION("Discovery");
    RUN_TEST(test_empty_directory);
    RUN_TEST(test_glsl_discovery);
    RUN_TEST(test_slang_discovery);
    RUN_TEST(test_slangp_discovery);
    RUN_TEST(test_mixed_language_discovery);
    RUN_TEST(test_recursive_discovery);

    TEST_SECTION("Classification");
    RUN_TEST(test_glsl_classification);
    RUN_TEST(test_slang_classification);
    RUN_TEST(test_slangp_classification);
    RUN_TEST(test_unknown_extension_ignored);
    RUN_TEST(test_extension_wins_over_directory);

    TEST_SECTION("Metadata");
    RUN_TEST(test_name_extraction);
    RUN_TEST(test_category_extraction);
    RUN_TEST(test_relative_path);
    RUN_TEST(test_file_size);
    RUN_TEST(test_content_hash);

    TEST_SECTION("Query");
    RUN_TEST(test_by_language_glsl);
    RUN_TEST(test_by_language_slang);
    RUN_TEST(test_by_language_slangp);
    RUN_TEST(test_by_category);
    RUN_TEST(test_categories);
    RUN_TEST(test_find);
    RUN_TEST(test_contains);
    RUN_TEST(test_entry_count);
    RUN_TEST(test_categories_for_language);

    TEST_SECTION("Rescan");
    RUN_TEST(test_rescan_unchanged);
    RUN_TEST(test_rescan_modified);
    RUN_TEST(test_rescan_added);
    RUN_TEST(test_rescan_removed);
    RUN_TEST(test_rescan_content_hash_detects_modification);
    RUN_TEST(test_rescan_preserves_state);

    TEST_SECTION("Robustness");
    RUN_TEST(test_nonexistent_root);
    RUN_TEST(test_nested_directories);
    RUN_TEST(test_deterministic_ordering);
    RUN_TEST(test_individual_file_failure_doesnt_abort);

    TEST_SECTION("State API");
    RUN_TEST(test_set_status);
    RUN_TEST(test_set_error);
    RUN_TEST(test_set_active);
    RUN_TEST(test_clear_active);
    RUN_TEST(test_entry_out_of_range);
    RUN_TEST(test_state_out_of_range);
    RUN_TEST(test_shader_entry_status_name);

    cleanTestRoot();
    printf("\n=== Results: %d passed, %d failed ===\n", s_passed, s_failed);
    printf("=== All ShaderLibrary tests complete ===\n");
}
