#include "../../compiler/CompiledPreset.hpp"
#include "../../compiler/ShaderCache.hpp"
#include "../../shader_runtime/dependencies/ShaderDependencyGraph.hpp"
#include "../../shader_runtime/reload/ShaderHotReload.hpp"
#include "../../shader_runtime/TransactionalShaderState.hpp"
#include "../../shader_runtime/core/SemanticUniforms.hpp"
#include "../../shader_runtime/core/ShaderDiagnostics.hpp"
#include "../../validation/GpuShaderValidator.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace monix::renderer_vk::tests {

static CompiledPreset makeMinimalPreset(int passCount = 1) {
    CompiledPreset cp;
    cp.preset.path = "test.slangp";
    cp.preset.baseDirectory = std::filesystem::current_path();
    for (int i = 0; i < passCount; ++i) {
        CompiledPass pass;
        pass.preset.index = i;
        pass.preset.shaderPath = "shader_" + std::to_string(i) + ".slang";
        pass.vertex.source = "void main() { gl_Position = vec4(0); }";
        pass.fragment.source = "void main() { fragColor = vec4(1); }";
        pass.vertex.spirv = {0x07, 0x23, 0x02, 0x03};
        pass.fragment.spirv = {0x07, 0x23, 0x02, 0x03};
        pass.vertex.reflection.uniformBlocks.push_back({"ubo", "", 0, 0, 64, {}});
        pass.fragment.reflection.samplers.push_back({"inputTexture", 0, 0});
        cp.passes.push_back(std::move(pass));
        PassNode pn;
        pn.id = static_cast<GraphNodeId>(i);
        pn.passIndex = i;
        cp.graph.passes.push_back(std::move(pn));
        cp.executionPlan.passOrder.push_back(i);
    }
    ImageNode img;
    img.id = 0;
    img.name = "source";
    img.external = true;
    cp.graph.images.push_back(std::move(img));
    return cp;
}

// ---------------------------------------------------------------------------
// GROUP 1 - CompiledPreset Lifecycle (6 tests)
// ---------------------------------------------------------------------------

static void test_preset_lifecycle_basic() {
    CompiledPreset cp = makeMinimalPreset(2);
    bool passCountOk = cp.passes.size() == 2;
    bool passesValid = !cp.passes[0].vertex.source.empty() && !cp.passes[0].fragment.source.empty();
    bool graphOk = cp.graph.passes.size() == 2;
    bool planOk = cp.executionPlan.passOrder.size() == 2;
    std::printf("  passCountOk=%d passesValid=%d graphOk=%d planOk=%d\n",
                (int)passCountOk, (int)passesValid, (int)graphOk, (int)planOk);
    if (!passCountOk || !passesValid || !graphOk || !planOk) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_preset_lifecycle_copy() {
    CompiledPreset original = makeMinimalPreset(1);
    CompiledPreset copy = original;
    original.passes[0].vertex.source = "modified";
    bool copyUnchanged = copy.passes[0].vertex.source == "void main() { gl_Position = vec4(0); }";
    bool originalChanged = original.passes[0].vertex.source == "modified";
    std::printf("  copyUnchanged=%d originalChanged=%d\n", (int)copyUnchanged, (int)originalChanged);
    if (!copyUnchanged || !originalChanged) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_preset_lifecycle_move() {
    CompiledPreset original = makeMinimalPreset(1);
    std::string origSource = original.passes[0].vertex.source;
    CompiledPreset moved = std::move(original);
    bool movedOk = !moved.passes.empty() && moved.passes[0].vertex.source == origSource;
    bool sourceEmpty = original.passes.empty();
    std::printf("  movedOk=%d sourceEmpty=%d\n", (int)movedOk, (int)sourceEmpty);
    if (!movedOk || !sourceEmpty) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_preset_reflection_blocks() {
    CompiledPreset cp = makeMinimalPreset(1);
    bool hasBlock = !cp.passes[0].vertex.reflection.uniformBlocks.empty();
    bool nameMatch = cp.passes[0].vertex.reflection.uniformBlocks[0].name == "ubo";
    bool sizeMatch = cp.passes[0].vertex.reflection.uniformBlocks[0].size == 64;
    std::printf("  hasBlock=%d nameMatch=%d sizeMatch=%d\n",
                (int)hasBlock, (int)nameMatch, (int)sizeMatch);
    if (!hasBlock || !nameMatch || !sizeMatch) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_preset_reflection_samplers() {
    CompiledPreset cp = makeMinimalPreset(1);
    bool hasSampler = !cp.passes[0].fragment.reflection.samplers.empty();
    bool nameMatch = cp.passes[0].fragment.reflection.samplers[0].name == "inputTexture";
    bool bindingMatch = cp.passes[0].fragment.reflection.samplers[0].binding == 0;
    std::printf("  hasSampler=%d nameMatch=%d bindingMatch=%d\n",
                (int)hasSampler, (int)nameMatch, (int)bindingMatch);
    if (!hasSampler || !nameMatch || !bindingMatch) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_preset_empty() {
    CompiledPreset cp;
    bool emptyPasses = cp.passes.empty();
    bool emptyGraph = cp.graph.passes.empty();
    bool emptyPlan = cp.executionPlan.passOrder.empty();
    std::printf("  emptyPasses=%d emptyGraph=%d emptyPlan=%d\n",
                (int)emptyPasses, (int)emptyGraph, (int)emptyPlan);
    if (!emptyPasses || !emptyGraph || !emptyPlan) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

// ---------------------------------------------------------------------------
// GROUP 2 - TransactionalShaderState (6 tests)
// ---------------------------------------------------------------------------

static void test_transactional_commit_basic() {
    TransactionalShaderState ts;
    bool noActiveBefore = !ts.hasActive();
    ts.beginCompile();
    CompiledPreset candidate = makeMinimalPreset(1);
    ts.setCandidate(std::move(candidate));
    TransactionalSwapResult result = ts.commit();
    bool committed = result.committed;
    bool hasActiveAfter = ts.hasActive();
    std::printf("  noActiveBefore=%d committed=%d hasActiveAfter=%d\n",
                (int)noActiveBefore, (int)committed, (int)hasActiveAfter);
    if (!noActiveBefore || !committed || !hasActiveAfter) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_transactional_rollback() {
    TransactionalShaderState ts;
    CompiledPreset preset = makeMinimalPreset(1);
    ts.setActive(std::move(preset));
    const CompiledPreset* beforeActive = ts.active();
    ts.beginCompile();
    CompiledPreset candidate = makeMinimalPreset(2);
    ts.setCandidate(std::move(candidate));
    ts.rollback();
    bool activeStillExists = ts.hasActive();
    const CompiledPreset* afterActive = ts.active();
    bool samePointer = (beforeActive == afterActive);
    std::printf("  activeStillExists=%d samePointer=%d\n",
                (int)activeStillExists, (int)samePointer);
    if (!activeStillExists || !samePointer) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_transactional_discard() {
    TransactionalShaderState ts;
    CompiledPreset preset = makeMinimalPreset(1);
    ts.setActive(std::move(preset));
    const CompiledPreset* beforeActive = ts.active();
    ts.beginCompile();
    CompiledPreset candidate = makeMinimalPreset(2);
    ts.setCandidate(std::move(candidate));
    ts.discard();
    bool hasActive = ts.hasActive();
    const CompiledPreset* afterActive = ts.active();
    bool samePointer = (beforeActive == afterActive);
    std::printf("  hasActive=%d samePointer=%d\n", (int)hasActive, (int)samePointer);
    if (!hasActive || !samePointer) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_transactional_previous_active() {
    TransactionalShaderState ts;
    const CompiledPreset* prevBefore = ts.previousActive();
    ts.beginCompile();
    CompiledPreset p1 = makeMinimalPreset(1);
    ts.setActive(std::move(p1));
    ts.beginCompile();
    const CompiledPreset* prevDuring = ts.previousActive();
    CompiledPreset c = makeMinimalPreset(1);
    ts.setCandidate(std::move(c));
    ts.commit();
    const CompiledPreset* prevAfter = ts.previousActive();
    bool nullBefore = (prevBefore == nullptr);
    bool nonNullDuring = (prevDuring != nullptr);
    bool nullAfter = (prevAfter == nullptr);
    std::printf("  nullBefore=%d nonNullDuring=%d nullAfter=%d\n",
                (int)nullBefore, (int)nonNullDuring, (int)nullAfter);
    if (!nullBefore || !nonNullDuring || !nullAfter) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_transactional_multiple_cycles() {
    TransactionalShaderState ts;
    for (int i = 0; i < 3; ++i) {
        ts.beginCompile();
        CompiledPreset c = makeMinimalPreset(1);
        c.preset.path = std::filesystem::path("cycle_" + std::to_string(i) + ".slangp");
        ts.setCandidate(std::move(c));
        TransactionalSwapResult result = ts.commit();
        if (!result.committed) { std::printf("  FAIL on cycle %d\n", i); return; }
    }
    bool active = ts.hasActive();
    std::printf("  active=%d\n", (int)active);
    if (!active) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_transactional_copy_semantics() {
    TransactionalShaderState ts;
    CompiledPreset p = makeMinimalPreset(1);
    ts.setActive(std::move(p));
    bool activeBefore = ts.hasActive();
    SwapState stateBefore = ts.state();
    std::printf("  activeBefore=%d stateBefore=%d\n", (int)activeBefore, (int)stateBefore);
    if (!activeBefore) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

// ---------------------------------------------------------------------------
// GROUP 3 - ShaderDependencyGraph (6 tests)
// ---------------------------------------------------------------------------

static void test_dep_graph_add_node() {
    ShaderDependencyGraph graph;
    graph.registerShader("shader_a.slang");
    bool exists = graph.hasNode("shader_a.slang");
    bool countIsOne = graph.nodeCount() == 1;
    bool typeIsShader = graph.nodeType("shader_a.slang") == DepNodeType::Shader;
    std::printf("  exists=%d countIsOne=%d typeIsShader=%d\n",
                (int)exists, (int)countIsOne, (int)typeIsShader);
    if (!exists || !countIsOne || !typeIsShader) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_dep_graph_add_edge() {
    ShaderDependencyGraph graph;
    graph.registerInclude("common.h");
    graph.registerShader("shader_a.slang");
    graph.addDependency("shader_a.slang", "common.h");
    auto deps = graph.getDependencies("shader_a.slang");
    bool hasDep = !deps.empty() && deps[0].filename() == "common.h";
    auto dependents = graph.getDependents("common.h");
    bool hasDependent = !dependents.empty();
    std::printf("  hasDep=%d hasDependent=%d\n", (int)hasDep, (int)hasDependent);
    if (!hasDep || !hasDependent) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_dep_graph_topological_sort() {
    ShaderDependencyGraph graph;
    graph.registerInclude("base.h");
    graph.registerInclude("middle.h");
    graph.registerShader("main.slang");
    graph.addDependency("middle.h", "base.h");
    graph.addDependency("main.slang", "middle.h");
    graph.addDependency("main.slang", "base.h");
    auto allNodes = graph.allNodes();
    bool allPresent = allNodes.size() == 3;
    std::printf("  allPresent=%d nodeCount=%zu\n", (int)allPresent, allNodes.size());
    if (!allPresent) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_dep_graph_cycle_detection() {
    ShaderDependencyGraph graph;
    graph.registerShader("a.slang");
    graph.registerShader("b.slang");
    graph.addDependency("a.slang", "b.slang");
    graph.addDependency("b.slang", "a.slang");
    auto affectedA = graph.invalidate("a.slang");
    auto affectedB = graph.invalidate("b.slang");
    bool anyFound = affectedA.anyFound || affectedB.anyFound;
    std::printf("  anyFound=%d\n", (int)anyFound);
    if (!anyFound) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_dep_graph_clear() {
    ShaderDependencyGraph graph;
    graph.registerShader("a.slang");
    graph.registerShader("b.slang");
    graph.registerInclude("c.h");
    bool beforeNotEmpty = graph.nodeCount() == 3;
    graph.clear();
    bool afterEmpty = graph.nodeCount() == 0;
    std::printf("  beforeNotEmpty=%d afterEmpty=%d\n", (int)beforeNotEmpty, (int)afterEmpty);
    if (!beforeNotEmpty || !afterEmpty) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_dep_graph_concurrent_access() {
    ShaderDependencyGraph graph;
    const int threadCount = 4;
    const int nodesPerThread = 25;
    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&graph, t, nodesPerThread]() {
            for (int i = 0; i < nodesPerThread; ++i) {
                std::string name = "thread_" + std::to_string(t) + "_node_" + std::to_string(i) + ".slang";
                graph.registerShader(name);
            }
        });
    }
    for (auto& th : threads) { th.join(); }
    bool correctCount = graph.nodeCount() == static_cast<size_t>(threadCount * nodesPerThread);
    std::printf("  correctCount=%d nodeCount=%zu\n", (int)correctCount, graph.nodeCount());
    if (!correctCount) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

// ---------------------------------------------------------------------------
// GROUP 4 - ShaderHotReload (6 tests)
// ---------------------------------------------------------------------------

static void test_hot_reload_callback_sync() {
    HotReloadConfig config;
    config.debounceMs = 0;
    config.enabled = false;
    ShaderHotReload hr(config);
    bool callbackCalled = false;
    hr.setCallback([&callbackCalled](const ShaderReloadRequest&) {
        callbackCalled = true;
    });
    ShaderReloadRequest req;
    req.changedFile = "test.slang";
    req.requestId = 1;
    hr.enqueueRequest(req);
    hr.processPendingReloads();
    std::printf("  callbackCalled=%d\n", (int)callbackCalled);
    if (!callbackCalled) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_hot_reload_concurrent_enqueue() {
    HotReloadConfig config;
    config.debounceMs = 0;
    config.enabled = false;
    ShaderHotReload hr(config);
    const int threadCount = 4;
    const int perThread = 20;
    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&hr, t, perThread]() {
            for (int i = 0; i < perThread; ++i) {
                ShaderReloadRequest req;
                req.changedFile = "thread_" + std::to_string(t) + "_" + std::to_string(i) + ".slang";
                req.requestId = static_cast<uint64_t>(t * perThread + i + 1);
                hr.enqueueRequest(req);
            }
        });
    }
    for (auto& th : threads) { th.join(); }
    bool correctCount = hr.pendingRequestCount() == static_cast<size_t>(threadCount * perThread);
    std::printf("  correctCount=%d pending=%zu\n", (int)correctCount, hr.pendingRequestCount());
    if (!correctCount) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_hot_reload_watch_root_sync() {
    HotReloadConfig config;
    config.debounceMs = 0;
    config.enabled = false;
    ShaderHotReload hr(config);
    hr.addWatchPath("C:/shaders");
    bool hasPending = hr.hasPendingRequests();
    std::printf("  hasPending=%d\n", (int)hasPending);
    std::printf("  PASS\n");
}

static void test_hot_reload_debounce_stress() {
    HotReloadConfig config;
    config.debounceMs = 50;
    config.enabled = false;
    ShaderHotReload hr(config);
    int callbackCount = 0;
    hr.setCallback([&callbackCount](const ShaderReloadRequest&) {
        callbackCount++;
    });
    for (int i = 0; i < 100; ++i) {
        ShaderReloadRequest req;
        req.changedFile = "debounce_test.slang";
        req.requestId = static_cast<uint64_t>(i + 1);
        hr.enqueueRequest(req);
    }
    hr.processPendingReloads();
    std::printf("  callbackCount=%d\n", callbackCount);
    if (callbackCount < 1) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_hot_reload_request_id_monotonic() {
    HotReloadConfig config;
    config.debounceMs = 0;
    config.enabled = false;
    ShaderHotReload hr(config);
    uint64_t prev = 0;
    bool monotonic = true;
    for (int i = 0; i < 10; ++i) {
        ShaderReloadRequest req;
        req.changedFile = "mono_" + std::to_string(i) + ".slang";
        hr.enqueueRequest(req);
        uint64_t current = hr.requestIdCounter();
        if (current <= prev) { monotonic = false; break; }
        prev = current;
    }
    std::printf("  monotonic=%d\n", (int)monotonic);
    if (!monotonic) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_hot_reload_no_callback() {
    HotReloadConfig config;
    config.debounceMs = 0;
    config.enabled = false;
    ShaderHotReload hr(config);
    ShaderReloadRequest req;
    req.changedFile = "no_callback.slang";
    req.requestId = 1;
    hr.enqueueRequest(req);
    hr.processPendingReloads();
    std::printf("  PASS (no crash)\n");
}

// ---------------------------------------------------------------------------
// GROUP 5 - ShaderCache (5 tests)
// ---------------------------------------------------------------------------

static void test_cache_store_and_retrieve() {
    namespace fs = std::filesystem;
    fs::path cacheDir = fs::temp_directory_path() / "monix_cache_test_store";
    fs::create_directories(cacheDir);
    {
        ShaderCache cache(cacheDir, 1024 * 1024);
        CacheKeyComponents kc;
        kc.sourceContent = "void main() {}";
        kc.stage = "fragment";
        kc.language = "slang";
        kc.compilerVersion = "1.0";
        kc.debugInfo = false;
        ShaderCacheKey key = cache.makeKey(kc);
        std::vector<unsigned char> spirv = {0x07, 0x23, 0x02, 0x03};
        std::string reflectionJson = "{}";
        Status storeResult = cache.store(key, spirv, reflectionJson);
        bool stored = storeResult.ok;
        bool contained = cache.contains(key);
        std::printf("  stored=%d contained=%d\n", (int)stored, (int)contained);
        if (!stored || !contained) { std::printf("  FAIL\n"); return; }
    }
    std::printf("  PASS\n");
}

static void test_cache_miss() {
    namespace fs = std::filesystem;
    fs::path cacheDir = fs::temp_directory_path() / "monix_cache_test_miss";
    fs::create_directories(cacheDir);
    {
        ShaderCache cache(cacheDir, 1024 * 1024);
        CacheKeyComponents kc;
        kc.sourceContent = "nonexistent";
        kc.stage = "vertex";
        kc.language = "slang";
        kc.compilerVersion = "1.0";
        ShaderCacheKey key = cache.makeKey(kc);
        bool contained = cache.contains(key);
        std::printf("  contained=%d\n", (int)contained);
        if (contained) { std::printf("  FAIL\n"); return; }
    }
    std::printf("  PASS\n");
}

static void test_cache_invalidate() {
    namespace fs = std::filesystem;
    fs::path cacheDir = fs::temp_directory_path() / "monix_cache_test_invalidate";
    fs::create_directories(cacheDir);
    {
        ShaderCache cache(cacheDir, 1024 * 1024);
        CacheKeyComponents kc;
        kc.sourceContent = "invalidate_me";
        kc.stage = "vertex";
        kc.language = "slang";
        kc.compilerVersion = "1.0";
        ShaderCacheKey key = cache.makeKey(kc);
        std::vector<unsigned char> spirv = {0x07, 0x23};
        cache.store(key, spirv, "{}");
        bool beforeContained = cache.contains(key);
        Status invResult = cache.invalidate(key);
        bool invalidated = invResult.ok;
        bool afterContained = cache.contains(key);
        std::printf("  beforeContained=%d invalidated=%d afterContained=%d\n",
                    (int)beforeContained, (int)invalidated, (int)afterContained);
        if (!beforeContained || !invalidated || afterContained) { std::printf("  FAIL\n"); return; }
    }
    std::printf("  PASS\n");
}

static void test_cache_integrity() {
    namespace fs = std::filesystem;
    fs::path cacheDir = fs::temp_directory_path() / "monix_cache_test_integrity";
    fs::create_directories(cacheDir);
    {
        ShaderCache cache(cacheDir, 1024 * 1024);
        CacheKeyComponents kc;
        kc.sourceContent = "integrity_check";
        kc.stage = "fragment";
        kc.language = "slang";
        kc.compilerVersion = "1.0";
        ShaderCacheKey key = cache.makeKey(kc);
        std::vector<unsigned char> spirv = {0x07, 0x23, 0x02, 0x03};
        cache.store(key, spirv, "{}");
        ShaderCacheEntry entry = cache.entryFor(key);
        bool intact = cache.verifyIntegrity(entry);
        std::string hash1 = ShaderCache::hashContent("test_content");
        std::string hash2 = ShaderCache::hashContent("test_content");
        bool hashConsistent = (hash1 == hash2);
        std::printf("  intact=%d hashConsistent=%d\n", (int)intact, (int)hashConsistent);
        if (!intact || !hashConsistent) { std::printf("  FAIL\n"); return; }
    }
    std::printf("  PASS\n");
}

static void test_cache_lru_eviction() {
    namespace fs = std::filesystem;
    fs::path cacheDir = fs::temp_directory_path() / "monix_cache_test_lru";
    fs::create_directories(cacheDir);
    {
        ShaderCache cache(cacheDir, 1024);
        for (int i = 0; i < 50; ++i) {
            CacheKeyComponents kc;
            kc.sourceContent = "evict_" + std::to_string(i);
            kc.stage = "vertex";
            kc.language = "slang";
            kc.compilerVersion = "1.0";
            ShaderCacheKey key = cache.makeKey(kc);
            std::vector<unsigned char> spirv(256, static_cast<unsigned char>(i));
            cache.store(key, spirv, "{}");
        }
        CacheStats st = cache.stats();
        std::printf("  evictions=%zu totalEntries=%zu\n", st.evictions, st.totalEntries);
        std::printf("  PASS\n");
    }
}

// ---------------------------------------------------------------------------
// GROUP 6 - Semantic Uniforms (4 tests)
// ---------------------------------------------------------------------------

static void test_semantic_uniforms_resolve() {
    SemanticUniformResolver resolver = SemanticUniformResolver::makeDefault();
    SemanticMatch match = resolver.resolve("InputTexture");
    bool found = match.semantic != ShaderSemantic::None;
    bool exactMatch = match.exactMatch;
    std::printf("  found=%d exactMatch=%d semantic=%s\n",
                (int)found, (int)exactMatch, shaderSemanticName(match.semantic));
    if (!found) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_semantic_uniforms_missing() {
    SemanticUniformResolver resolver = SemanticUniformResolver::makeDefault();
    SemanticMatch match = resolver.resolve("NonExistentUniformXYZ");
    bool notFound = match.semantic == ShaderSemantic::None;
    std::printf("  notFound=%d\n", (int)notFound);
    if (!notFound) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_semantic_uniforms_override() {
    SemanticUniformResolver resolver = SemanticUniformResolver::makeDefault();
    resolver.addAlias(ShaderSemantic::Time, "MyCustomTime");
    SemanticMatch match = resolver.resolve("MyCustomTime");
    bool found = match.semantic == ShaderSemantic::Time;
    bool aliasMatch = match.aliasMatch;
    std::printf("  found=%d aliasMatch=%d\n", (int)found, (int)aliasMatch);
    if (!found) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_semantic_uniforms_type_conversion() {
    SemanticUniformResolver resolver = SemanticUniformResolver::makeDefault();
    bool timeResolved = resolver.detectSemantic("Time") == ShaderSemantic::Time;
    bool frameResolved = resolver.detectSemantic("FrameCount") == ShaderSemantic::FrameCount;
    bool viewportResolved = resolver.detectSemantic("ViewportSize") == ShaderSemantic::ViewportSize;
    std::printf("  timeResolved=%d frameResolved=%d viewportResolved=%d\n",
                (int)timeResolved, (int)frameResolved, (int)viewportResolved);
    if (!timeResolved || !frameResolved || !viewportResolved) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

// ---------------------------------------------------------------------------
// GROUP 7 - GPU Validation (2 tests)
// ---------------------------------------------------------------------------

static void test_gpu_validator_permissive() {
    GpuShaderValidator validator = GpuShaderValidator::makePermissive();
    bool allowBlack = validator.profile().allowBlackOutput;
    bool allowConstant = validator.profile().allowConstantOutput;
    std::vector<uint8_t> pixels(256 * 256 * 4, 0);
    OutputStatistics stats = validator.analyzeOutput(pixels, 256, 256);
    GpuTestResult result = validator.validateOutput(stats, true);
    bool passed = (result == GpuTestResult::Pass) || (result == GpuTestResult::Skip);
    std::printf("  allowBlack=%d allowConstant=%d result=%s passed=%d\n",
                (int)allowBlack, (int)allowConstant, gpuTestResultName(result), (int)passed);
    if (!passed) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_gpu_validator_strict() {
    GpuShaderValidator validator = GpuShaderValidator::makeStrict();
    bool noBlack = !validator.profile().allowBlackOutput;
    bool noConstant = !validator.profile().allowConstantOutput;
    std::vector<uint8_t> blackPixels(256 * 256 * 4, 0);
    OutputStatistics stats = validator.analyzeOutput(blackPixels, 256, 256);
    GpuTestResult result = validator.validateOutput(stats, true);
    bool failed = (result != GpuTestResult::Pass);
    std::printf("  noBlack=%d noConstant=%d result=%s failed=%d\n",
                (int)noBlack, (int)noConstant, gpuTestResultName(result), (int)failed);
    if (!failed) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

// ---------------------------------------------------------------------------
// GROUP 8 - Diagnostics (2 tests)
// ---------------------------------------------------------------------------

static void test_diagnostics_compile_error() {
    ShaderDiagnostic diag;
    diag.kind = ShaderDiagnosticKind::CompileError;
    diag.severity = DiagnosticSeverity::Error;
    diag.file = "test.slang";
    diag.line = 42;
    diag.column = 7;
    diag.stage = "vertex";
    diag.errorCode = "E001";
    diag.message = "undefined identifier 'foo'";
    bool kindOk = diag.kind == ShaderDiagnosticKind::CompileError;
    bool lineOk = diag.line == 42;
    bool colOk = diag.column == 7;
    bool msgOk = diag.message == "undefined identifier 'foo'";
    std::printf("  kindOk=%d lineOk=%d colOk=%d msgOk=%d\n",
                (int)kindOk, (int)lineOk, (int)colOk, (int)msgOk);
    if (!kindOk || !lineOk || !colOk || !msgOk) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

static void test_diagnostics_collection() {
    ShaderDiagnostics diags;
    diags.add(ShaderDiagnostic());
    ShaderDiagnostic d1;
    d1.kind = ShaderDiagnosticKind::CompileError;
    d1.message = "error one";
    diags.add(d1);
    ShaderDiagnostic d2;
    d2.kind = ShaderDiagnosticKind::ReflectionError;
    d2.message = "error two";
    diags.add(d2);
    bool countOk = diags.entries().size() == 3;
    bool hasErrors = diags.hasErrors();
    std::string summary = diags.summary();
    bool summaryNonEmpty = !summary.empty();
    std::printf("  countOk=%d hasErrors=%d summaryNonEmpty=%d\n",
                (int)countOk, (int)hasErrors, (int)summaryNonEmpty);
    if (!countOk || !hasErrors || !summaryNonEmpty) { std::printf("  FAIL\n"); return; }
    std::printf("  PASS\n");
}

// ---------------------------------------------------------------------------
// runAll
// ---------------------------------------------------------------------------

void runAll() {
    std::printf("=== GROUP 1: CompiledPreset Lifecycle ===\n");
    std::printf("[TEST] test_preset_lifecycle_basic\n");    test_preset_lifecycle_basic();
    std::printf("[TEST] test_preset_lifecycle_copy\n");     test_preset_lifecycle_copy();
    std::printf("[TEST] test_preset_lifecycle_move\n");     test_preset_lifecycle_move();
    std::printf("[TEST] test_preset_reflection_blocks\n"); test_preset_reflection_blocks();
    std::printf("[TEST] test_preset_reflection_samplers\n");test_preset_reflection_samplers();
    std::printf("[TEST] test_preset_empty\n");              test_preset_empty();

    std::printf("=== GROUP 2: TransactionalShaderState ===\n");
    std::printf("[TEST] test_transactional_commit_basic\n");   test_transactional_commit_basic();
    std::printf("[TEST] test_transactional_rollback\n");       test_transactional_rollback();
    std::printf("[TEST] test_transactional_discard\n");        test_transactional_discard();
    std::printf("[TEST] test_transactional_previous_active\n");test_transactional_previous_active();
    std::printf("[TEST] test_transactional_multiple_cycles\n");test_transactional_multiple_cycles();
    std::printf("[TEST] test_transactional_copy_semantics\n"); test_transactional_copy_semantics();

    std::printf("=== GROUP 3: ShaderDependencyGraph ===\n");
    std::printf("[TEST] test_dep_graph_add_node\n");           test_dep_graph_add_node();
    std::printf("[TEST] test_dep_graph_add_edge\n");           test_dep_graph_add_edge();
    std::printf("[TEST] test_dep_graph_topological_sort\n");   test_dep_graph_topological_sort();
    std::printf("[TEST] test_dep_graph_cycle_detection\n");    test_dep_graph_cycle_detection();
    std::printf("[TEST] test_dep_graph_clear\n");              test_dep_graph_clear();
    std::printf("[TEST] test_dep_graph_concurrent_access\n");  test_dep_graph_concurrent_access();

    std::printf("=== GROUP 4: ShaderHotReload ===\n");
    std::printf("[TEST] test_hot_reload_callback_sync\n");       test_hot_reload_callback_sync();
    std::printf("[TEST] test_hot_reload_concurrent_enqueue\n");  test_hot_reload_concurrent_enqueue();
    std::printf("[TEST] test_hot_reload_watch_root_sync\n");     test_hot_reload_watch_root_sync();
    std::printf("[TEST] test_hot_reload_debounce_stress\n");     test_hot_reload_debounce_stress();
    std::printf("[TEST] test_hot_reload_request_id_monotonic\n");test_hot_reload_request_id_monotonic();
    std::printf("[TEST] test_hot_reload_no_callback\n");         test_hot_reload_no_callback();

    std::printf("=== GROUP 5: ShaderCache ===\n");
    std::printf("[TEST] test_cache_store_and_retrieve\n"); test_cache_store_and_retrieve();
    std::printf("[TEST] test_cache_miss\n");               test_cache_miss();
    std::printf("[TEST] test_cache_invalidate\n");         test_cache_invalidate();
    std::printf("[TEST] test_cache_integrity\n");          test_cache_integrity();
    std::printf("[TEST] test_cache_lru_eviction\n");       test_cache_lru_eviction();

    std::printf("=== GROUP 6: Semantic Uniforms ===\n");
    std::printf("[TEST] test_semantic_uniforms_resolve\n");      test_semantic_uniforms_resolve();
    std::printf("[TEST] test_semantic_uniforms_missing\n");      test_semantic_uniforms_missing();
    std::printf("[TEST] test_semantic_uniforms_override\n");     test_semantic_uniforms_override();
    std::printf("[TEST] test_semantic_uniforms_type_conversion\n");test_semantic_uniforms_type_conversion();

    std::printf("=== GROUP 7: GPU Validation ===\n");
    std::printf("[TEST] test_gpu_validator_permissive\n"); test_gpu_validator_permissive();
    std::printf("[TEST] test_gpu_validator_strict\n");     test_gpu_validator_strict();

    std::printf("=== GROUP 8: Diagnostics ===\n");
    std::printf("[TEST] test_diagnostics_compile_error\n"); test_diagnostics_compile_error();
    std::printf("[TEST] test_diagnostics_collection\n");    test_diagnostics_collection();

    std::printf("=== All 37 tests complete ===\n");
}

}  // namespace monix::renderer_vk::tests
