#pragma once

#include <cstdint>
#include <vector>
#include <string>

struct VkCtx;

struct PresetSamplerBinding {
    uint32_t binding = 0;
    std::string samplerName;
};

namespace vk_preset {
bool loadPreset(VkCtx& ctx, uint32_t width, uint32_t height,
                const std::vector<std::vector<uint32_t>>& vertSpvs,
                const std::vector<std::vector<uint32_t>>& fragSpvs,
                const std::vector<uint32_t>& samplerCounts,
                const std::vector<uint32_t>& pushSizes,
                const std::vector<uint32_t>& rtFormats,
                const std::vector<std::vector<float>>& pushDefaultsList,
                const std::vector<uint32_t>& uboSizes);

bool loadPresetReflection(VkCtx& ctx, uint32_t width, uint32_t height,
                const std::vector<std::vector<uint32_t>>& vertSpvs,
                const std::vector<std::vector<uint32_t>>& fragSpvs,
                const std::vector<uint32_t>& pushSizes,
                const std::vector<uint32_t>& rtFormats,
                const std::vector<std::vector<float>>& pushDefaultsList,
                const std::vector<uint32_t>& uboSizes,
                const std::vector<std::vector<PresetSamplerBinding>>& perPassSamplerBindings);

void destroyPreset(VkCtx& ctx);
bool renderPreset(VkCtx& ctx, uint32_t width, uint32_t height);
bool blitAppContentOverPreset(VkCtx& ctx, uint32_t width, uint32_t height);
}
