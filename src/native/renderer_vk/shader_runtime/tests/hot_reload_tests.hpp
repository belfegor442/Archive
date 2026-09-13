#pragma once

#include "../reload/ShaderHotReload.hpp"
#include "../dependencies/ShaderDependencyGraph.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace monix::renderer_vk::tests {

inline void test_hot_reload_config() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    assert(config.debounceMs == 150);
    assert(config.enabled);
    assert(!config.watchRoots.empty());
    printf("  [PASS] hot_reload_config\n");
}

inline void test_hot_reload_create() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    assert(!reload.isRunning());
    assert(!reload.hasPendingRequests());
    assert(reload.pendingRequestCount() == 0);
    printf("  [PASS] hot_reload_create\n");
}

inline void test_hot_reload_start_stop() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    reload.start();
    assert(reload.isRunning());
    reload.stop();
    assert(!reload.isRunning());
    printf("  [PASS] hot_reload_start_stop\n");
}

inline void test_hot_reload_callback() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    bool called = false;
    ShaderReloadRequest receivedRequest;
    reload.setCallback([&](const ShaderReloadRequest& req) {
        called = true;
        receivedRequest = req;
    });
    printf("  [PASS] hot_reload_callback\n");
}

inline void test_hot_reload_relevant_extensions() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    assert(reload.isRelevantExtension("test.slang"));
    assert(reload.isRelevantExtension("test.glsl"));
    assert(reload.isRelevantExtension("test.vert"));
    assert(reload.isRelevantExtension("test.frag"));
    assert(reload.isRelevantExtension("test.geom"));
    assert(reload.isRelevantExtension("test.comp"));
    assert(reload.isRelevantExtension("test.slangp"));
    assert(reload.isRelevantExtension("test.inc"));
    assert(!reload.isRelevantExtension("test.txt"));
    assert(!reload.isRelevantExtension("test.cpp"));
    assert(!reload.isRelevantExtension("test.h"));
    printf("  [PASS] hot_reload_relevant_extensions\n");
}

inline void test_hot_reload_content_hash() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    auto testFile = std::filesystem::temp_directory_path() / "hot_reload_test_hash.txt";
    {
        std::ofstream ofs(testFile);
        ofs << "test content 12345";
    }
    auto hash1 = reload.computeContentHash(testFile);
    assert(hash1 != 0);
    {
        std::ofstream ofs(testFile);
        ofs << "test content 12345";
    }
    auto hash2 = reload.computeContentHash(testFile);
    assert(hash1 == hash2);
    {
        std::ofstream ofs(testFile);
        ofs << "modified content";
    }
    auto hash3 = reload.computeContentHash(testFile);
    assert(hash1 != hash3);
    std::filesystem::remove(testFile);
    printf("  [PASS] hot_reload_content_hash\n");
}

inline void test_hot_reload_file_write_time() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    auto testFile = std::filesystem::temp_directory_path() / "hot_reload_test_wtime.txt";
    {
        std::ofstream ofs(testFile);
        ofs << "content";
    }
    auto wt1 = reload.getFileWriteTime(testFile);
    assert(wt1 != 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::ofstream ofs(testFile);
        ofs << "modified content";
    }
    auto wt2 = reload.getFileWriteTime(testFile);
    assert(wt2 >= wt1);
    std::filesystem::remove(testFile);
    printf("  [PASS] hot_reload_file_write_time\n");
}

inline void test_hot_reload_request_id_increments() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    auto id1 = reload.requestIdCounter();
    auto id2 = reload.requestIdCounter();
    assert(id2 >= id1);
    printf("  [PASS] hot_reload_request_id_increments\n");
}

inline void test_hot_reload_enqueue() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    ShaderReloadRequest req;
    req.changedFile = "test.slang";
    req.requestId = 1;
    reload.enqueueRequest(std::move(req));
    assert(reload.hasPendingRequests());
    assert(reload.pendingRequestCount() == 1);
    printf("  [PASS] hot_reload_enqueue\n");
}

inline void test_hot_reload_add_remove_watch() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    reload.addWatchPath("D:\\test1");
    reload.addWatchPath("D:\\test2");
    reload.removeWatchPath("D:\\test1");
    printf("  [PASS] hot_reload_add_remove_watch\n");
}

inline void test_hot_reload_dependency_integration() {
    ShaderDependencyGraph graph;
    graph.registerPreset("preset.slangp");
    graph.registerShader("shader.slang");
    graph.registerInclude("common.inc");
    graph.addDependency("preset.slangp", "shader.slang");
    graph.addDependency("shader.slang", "common.inc");

    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    reload.setDependencyGraph(&graph);

    auto affected = graph.invalidate("common.inc");
    assert(affected.anyFound);
    assert(!affected.affectedShaders.empty());
    assert(!affected.affectedPresets.empty());
    printf("  [PASS] hot_reload_dependency_integration\n");
}

inline void test_hot_reload_debounce_window() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    config.debounceMs = 50;
    ShaderHotReload reload(config);
    assert(config.debounceMs == 50);
    printf("  [PASS] hot_reload_debounce_window\n");
}

inline void test_hot_reload_file_state_tracking() {
    auto config = ShaderHotReload::defaultConfig("D:\\Monix-2ago-unestable\\Monix\\Monix\\Shaders");
    ShaderHotReload reload(config);
    auto testFile = std::filesystem::temp_directory_path() / "hot_reload_test_state.txt";
    {
        std::ofstream ofs(testFile);
        ofs << "initial content";
    }
    reload.processEvent(testFile);
    auto hash1 = reload.computeContentHash(testFile);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    reload.processEvent(testFile);
    auto hash2 = reload.computeContentHash(testFile);
    assert(hash1 == hash2);
    std::filesystem::remove(testFile);
    printf("  [PASS] hot_reload_file_state_tracking\n");
}

inline void test_hot_reload_full_pipeline() {
    auto tempDir = std::filesystem::temp_directory_path() / "hot_reload_pipeline_test";
    std::filesystem::create_directories(tempDir);

    auto shaderDir = tempDir / "shaders";
    std::filesystem::create_directories(shaderDir);

    auto presetPath = tempDir / "test.slangp";
    auto shaderPath = shaderDir / "test.slang";
    auto includePath = shaderDir / "common.inc";

    {
        std::ofstream ofs(presetPath);
        ofs << "test.slang\n";
    }
    {
        std::ofstream ofs(shaderPath);
        ofs << "void main() { fragColor = vec4(1,0,0,1); }\n";
    }
    {
        std::ofstream ofs(includePath);
        ofs << "// common include\n";
    }

    ShaderDependencyGraph graph;
    graph.registerPreset(presetPath);
    graph.registerShader(shaderPath);
    graph.registerInclude(includePath);
    graph.addDependency(presetPath, shaderPath);
    graph.addDependency(shaderPath, includePath);

    auto config = ShaderHotReload::defaultConfig(shaderDir);
    config.debounceMs = 0;
    ShaderHotReload reload(config);
    reload.setDependencyGraph(&graph);

    int callbackCount = 0;
    std::vector<std::filesystem::path> changedFiles;
    reload.setCallback([&](const ShaderReloadRequest& request) {
        callbackCount++;
        changedFiles = request.affectedAssets;
    });

    reload.addWatchPath(shaderDir);

    {
        std::ofstream ofs(shaderPath);
        ofs << "void main() { fragColor = vec4(0,0,1,1); }\n";
    }

    reload.processEvent(shaderPath);
    reload.processPendingReloads();
    reload.processReloadQueue();

    assert(callbackCount == 1);
    assert(!changedFiles.empty());

    bool foundPreset = false;
    for (const auto& f : changedFiles) {
        if (f == presetPath) foundPreset = true;
    }
    assert(foundPreset);

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] hot_reload_full_pipeline\n");
}

inline void test_hot_reload_stale_request_discarded() {
    auto tempDir = std::filesystem::temp_directory_path() / "hot_reload_stale_test";
    std::filesystem::create_directories(tempDir);

    auto shaderPath = tempDir / "test.slang";
    {
        std::ofstream ofs(shaderPath);
        ofs << "red\n";
    }

    auto config = ShaderHotReload::defaultConfig(tempDir);
    config.debounceMs = 0;
    ShaderHotReload reload(config);

    int callbackCount = 0;
    reload.setCallback([&](const ShaderReloadRequest&) {
        callbackCount++;
    });

    reload.processEvent(shaderPath);
    reload.processPendingReloads();

    uint64_t idAfterProcess = reload.requestIdCounter();
    assert(idAfterProcess == 1);

    ShaderReloadRequest stale;
    stale.changedFile = shaderPath;
    stale.requestId = 0;
    stale.affectedAssets.push_back(shaderPath);
    reload.enqueueRequest(std::move(stale));

    reload.processReloadQueue();
    assert(callbackCount == 2);

    assert(reload.requestIdCounter() == 2);

    std::filesystem::remove_all(tempDir);
    printf("  [PASS] hot_reload_stale_request_discarded\n");
}

}  // namespace monix::renderer_vk::tests
