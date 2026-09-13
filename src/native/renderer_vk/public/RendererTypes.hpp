#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct Extent2D {
    uint32_t width = 0;
    uint32_t height = 0;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct FrameImage {
    Extent2D extent;
    const void* pixels = nullptr;
    size_t bytes = 0;
    uint32_t stride = 0;
};

struct RendererConfig {
    std::filesystem::path rootDirectory;
    std::filesystem::path shaderCacheDirectory;
    std::filesystem::path slangcPath;
    bool enableDebugDumps = true;
    bool enableHotReload = false;
    bool compileShadersToSpirv = false;
    uint32_t framesInFlight = 2;
};

struct FrameContext {
    uint64_t frameIndex = 0;
    int frameDirection = 1;
    Extent2D sourceExtent;
    Extent2D outputExtent;
};

struct RuntimeBuildInfo {
    bool vulkanHeadersAvailable = false;
    bool vmaHeadersAvailable = false;
    std::string compiler;
};

}  // namespace monix::renderer_vk
