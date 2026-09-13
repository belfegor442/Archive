#pragma once

#include "../compiler/ShaderCache.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace monix::renderer_vk::tests {

inline void test_cache_key_generation() {
    ShaderCache cache("shader-cache-test-key");
    CacheKeyComponents comp;
    comp.sourceContent = "void main() { fragColor = vec4(1,0,0,1); }";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "abc123";
    comp.debugInfo = false;
    auto key = cache.makeKey(comp);
    assert(!key.digest.empty());
    assert(key.digest.size() == 16);
    printf("  [PASS] cache_key_generation\n");
}

inline void test_cache_key_different_source() {
    ShaderCache cache("shader-cache-test-diff");
    CacheKeyComponents a;
    a.sourceContent = "void main() { fragColor = vec4(1,0,0,1); }";
    a.stage = "fragment";
    a.language = "glsl";
    a.compilerVersion = "v1";
    CacheKeyComponents b;
    b.sourceContent = "void main() { fragColor = vec4(0,1,0,1); }";
    b.stage = "fragment";
    b.language = "glsl";
    b.compilerVersion = "v1";
    auto keyA = cache.makeKey(a);
    auto keyB = cache.makeKey(b);
    assert(keyA.digest != keyB.digest);
    printf("  [PASS] cache_key_different_source\n");
}

inline void test_cache_key_different_stage() {
    ShaderCache cache("shader-cache-test-stage");
    CacheKeyComponents a;
    a.sourceContent = "same";
    a.stage = "vertex";
    a.language = "slang";
    a.compilerVersion = "v1";
    CacheKeyComponents b = a;
    b.stage = "fragment";
    auto keyA = cache.makeKey(a);
    auto keyB = cache.makeKey(b);
    assert(keyA.digest != keyB.digest);
    printf("  [PASS] cache_key_different_stage\n");
}

inline void test_cache_key_different_deps() {
    ShaderCache cache("shader-cache-test-deps");
    CacheKeyComponents a;
    a.sourceContent = "same";
    a.stage = "fragment";
    a.language = "glsl";
    a.compilerVersion = "v1";
    a.dependencyPaths = {"common.inc"};
    a.dependencyContents = {"include A"};
    CacheKeyComponents b = a;
    b.dependencyContents = {"include B"};
    auto keyA = cache.makeKey(a);
    auto keyB = cache.makeKey(b);
    assert(keyA.digest != keyB.digest);
    printf("  [PASS] cache_key_different_deps\n");
}

inline void test_cache_key_same_inputs() {
    ShaderCache cache("shader-cache-test-same");
    CacheKeyComponents a;
    a.sourceContent = "void main() {}";
    a.stage = "vertex";
    a.language = "slang";
    a.compilerVersion = "v1";
    a.debugInfo = false;
    auto keyA = cache.makeKey(a);
    auto keyB = cache.makeKey(a);
    assert(keyA.digest == keyB.digest);
    printf("  [PASS] cache_key_same_inputs\n");
}

inline void test_cache_store_and_retrieve() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_store";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "test source";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);

    assert(!cache.contains(key));

    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x00, 0x00};
    std::string refl = "{\"test\": true}";

    auto status = cache.store(key, spirv, refl);
    assert(status.ok);

    assert(cache.contains(key));

    auto entry = cache.entryFor(key);
    assert(entry.hit);
    assert(entry.spirvPath.extension() == ".spv");
    assert(entry.reflectionPath.extension() == ".json");

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_store_and_retrieve\n");
}

inline void test_cache_integrity_valid() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_integrity";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "integrity test";
    comp.stage = "vertex";
    comp.language = "slang";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);

    std::vector<unsigned char> spirv = {1, 2, 3, 4, 5};
    std::string refl = "{\"ok\": true}";
    cache.store(key, spirv, refl);

    auto entry = cache.entryFor(key);
    assert(cache.verifyIntegrity(entry));

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_integrity_valid\n");
}

inline void test_cache_integrity_corrupted_spirv() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_corrupt";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "corrupt test";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);

    std::vector<unsigned char> spirv = {1, 2, 3, 4, 5};
    std::string refl = "{\"ok\": true}";
    cache.store(key, spirv, refl);

    auto entry = cache.entryFor(key);
    assert(cache.verifyIntegrity(entry));

    {
        std::ofstream ofs(entry.spirvPath, std::ios::binary);
        unsigned char garbage[] = {0xFF, 0xFE, 0xFD};
        ofs.write(reinterpret_cast<char*>(garbage), 3);
    }

    auto entry2 = cache.entryFor(key);
    assert(!cache.verifyIntegrity(entry2));

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_integrity_corrupted_spirv\n");
}

inline void test_cache_invalidate() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_invalidate";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "invalidate test";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);

    std::vector<unsigned char> spirv = {1, 2, 3};
    cache.store(key, spirv, "{}");
    assert(cache.contains(key));

    cache.invalidate(key);
    assert(!cache.contains(key));

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_invalidate\n");
}

inline void test_cache_invalidate_all() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_invalidate_all";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    for (int i = 0; i < 5; i++) {
        CacheKeyComponents comp;
        comp.sourceContent = "source " + std::to_string(i);
        comp.stage = "fragment";
        comp.language = "glsl";
        comp.compilerVersion = "v1";
        auto key = cache.makeKey(comp);
        cache.store(key, {1, 2, 3}, "{}");
    }

    auto stats = cache.stats();
    assert(stats.totalEntries == 5);

    cache.invalidateAll();

    stats = cache.stats();
    assert(stats.totalEntries == 0);

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_invalidate_all\n");
}

inline void test_cache_stats() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_stats";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "stats test";
    comp.stage = "vertex";
    comp.language = "slang";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);

    cache.store(key, {1, 2, 3, 4}, "{}");

    cache.recordHit();
    cache.recordHit();
    cache.recordMiss();

    auto stats = cache.stats();
    assert(stats.hits == 2);
    assert(stats.misses == 1);
    assert(stats.totalEntries == 1);
    assert(stats.totalSizeBytes > 0);

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_stats\n");
}

inline void test_cache_atomic_write_no_tmp_left() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_atomic";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "atomic test";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "v1";
    auto key = cache.makeKey(comp);

    cache.store(key, {1, 2, 3}, "{}");

    bool foundTmp = false;
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(tempDir, ec)) {
        if (entry.path().extension() == ".tmp") {
            foundTmp = true;
        }
    }
    assert(!foundTmp);

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_atomic_write_no_tmp_left\n");
}

inline void test_cache_manifest_content() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_manifest";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "manifest test";
    comp.stage = "vertex";
    comp.language = "slang";
    comp.compilerVersion = "v1";
    comp.debugInfo = true;
    auto key = cache.makeKey(comp);

    std::vector<unsigned char> spirv = {0x03, 0x02, 0x23, 0x07};
    std::string refl = "{\"uniforms\": []}";
    cache.store(key, spirv, refl);

    auto entry = cache.entryFor(key);
    assert(entry.hit);
    assert(entry.manifest.stage == "vertex");
    assert(entry.manifest.language == "slang");
    assert(entry.manifest.compilerVersion == "v1");
    assert(entry.manifest.spirvSize == 4);
    assert(entry.manifest.debugInfo == true);
    assert(entry.manifest.reflectionSize == 16);
    assert(!entry.manifest.spirvHash.empty());
    assert(!entry.manifest.reflectionHash.empty());

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_manifest_content\n");
}

inline void test_cache_manifest_roundtrip() {
    CacheManifest m;
    m.keyDigest = "abcdef1234567890";
    m.sourceHash = "src123";
    m.stage = "fragment";
    m.language = "glsl";
    m.compilerVersion = "cv456";
    m.dependencyHashes = {"dep1", "dep2"};
    m.spirvSize = 1024;
    m.spirvHash = "spirvhash";
    m.reflectionSize = 256;
    m.reflectionHash = "reflhash";
    m.timestamp = 1234567890;
    m.debugInfo = false;

    auto serialized = m.serialize();
    auto deserialized = CacheManifest::deserialize(serialized);
    assert(deserialized);
    assert(deserialized.value().keyDigest == m.keyDigest);
    assert(deserialized.value().sourceHash == m.sourceHash);
    assert(deserialized.value().stage == m.stage);
    assert(deserialized.value().language == m.language);
    assert(deserialized.value().compilerVersion == m.compilerVersion);
    assert(deserialized.value().spirvSize == m.spirvSize);
    assert(deserialized.value().spirvHash == m.spirvHash);
    assert(deserialized.value().reflectionSize == m.reflectionSize);
    assert(deserialized.value().reflectionHash == m.reflectionHash);
    assert(deserialized.value().timestamp == m.timestamp);
    assert(deserialized.value().debugInfo == m.debugInfo);
    assert(deserialized.value().dependencyHashes.size() == 2);
    assert(deserialized.value().dependencyHashes[0] == "dep1");
    assert(deserialized.value().dependencyHashes[1] == "dep2");
    printf("  [PASS] cache_manifest_roundtrip\n");
}

inline void test_cache_hit_miss_tracking() {
    auto tempDir = std::filesystem::temp_directory_path() / "shader_cache_test_hitmiss";
    std::filesystem::create_directories(tempDir);
    ShaderCache cache(tempDir);

    CacheKeyComponents comp;
    comp.sourceContent = "hit miss test";
    comp.stage = "fragment";
    comp.language = "glsl";
    comp.compilerVersion = "v1";

    auto key = cache.makeKey(comp);
    assert(!cache.contains(key));
    cache.recordMiss();

    cache.store(key, {1, 2, 3}, "{}");
    assert(cache.contains(key));
    cache.recordHit();

    auto stats = cache.stats();
    assert(stats.hits == 1);
    assert(stats.misses == 1);

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] cache_hit_miss_tracking\n");
}

}  // namespace monix::renderer_vk::tests
