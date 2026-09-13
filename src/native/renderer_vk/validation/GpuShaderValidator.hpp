#pragma once

#include "../core/Result.hpp"
#include "../shader_runtime/core/ShaderDiagnostics.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct OutputStatistics {
    uint8_t minR = 0, minG = 0, minB = 0, minA = 0;
    uint8_t maxR = 0, maxG = 0, maxB = 0, maxA = 0;
    double meanR = 0, meanG = 0, meanB = 0, meanA = 0;
    double meanLuminance = 0;
    size_t totalPixels = 0;
    size_t nonZeroPixels = 0;
    size_t uniqueColors = 0;
    bool hasNaN = false;
    bool hasInf = false;
};

struct ShaderValidationProfile {
    bool allowBlackOutput = false;
    bool allowConstantOutput = false;
    bool allowTransparentOutput = true;
    double minimumNonZeroPixelRatio = 0.01;
    double minimumLuminance = 0.001;
    uint32_t testWidth = 256;
    uint32_t testHeight = 256;
    bool skipGpuTest = false;
};

enum class GpuTestResult : std::uint8_t {
    Pass,
    FailBlackOutput,
    FailConstantOutput,
    FailNaN,
    FailInf,
    FailLowContent,
    FailReadbackFailed,
    FailRenderFailed,
    FailNoRenderer,
    FailNotLoaded,
    FailUnsupported,
    Skip
};

inline const char* gpuTestResultName(GpuTestResult r) {
    switch (r) {
    case GpuTestResult::Pass:               return "PASS";
    case GpuTestResult::FailBlackOutput:    return "OUTPUT_BLACK";
    case GpuTestResult::FailConstantOutput: return "OUTPUT_CONSTANT";
    case GpuTestResult::FailNaN:            return "OUTPUT_INVALID_FLOAT";
    case GpuTestResult::FailInf:            return "OUTPUT_INVALID_FLOAT";
    case GpuTestResult::FailLowContent:     return "OUTPUT_LOW_CONTENT";
    case GpuTestResult::FailReadbackFailed: return "OUTPUT_READBACK_FAILED";
    case GpuTestResult::FailRenderFailed:   return "GPU_RENDER_FAILED";
    case GpuTestResult::FailNoRenderer:     return "NO_RENDERER";
    case GpuTestResult::FailNotLoaded:      return "GPU_NOT_LOADED";
    case GpuTestResult::FailUnsupported:    return "UNSUPPORTED";
    case GpuTestResult::Skip:               return "SKIPPED";
    }
    return "UNKNOWN";
}

struct GpuTestDiagnostic {
    GpuTestResult result = GpuTestResult::Pass;
    std::string presetName;
    int passIndex = -1;
    std::string stage;
    OutputStatistics stats;
    std::string detail;

    std::string format() const;
};

class GpuShaderValidator {
public:
    GpuShaderValidator() = default;

    void setProfile(const ShaderValidationProfile& profile) { profile_ = profile; }
    const ShaderValidationProfile& profile() const { return profile_; }

    OutputStatistics analyzeOutput(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) const;
    GpuTestResult validateOutput(const OutputStatistics& stats, bool isFinalPass) const;
    GpuTestDiagnostic runGpuTest(/* VulkanRenderer and preset params */);
    void setGpuTestData(const std::vector<uint8_t>& readback, uint32_t w, uint32_t h, const std::string& preset, int pass);

    static GpuShaderValidator makePermissive() {
        GpuShaderValidator v;
        v.profile_.allowBlackOutput = true;
        v.profile_.allowConstantOutput = true;
        v.profile_.minimumNonZeroPixelRatio = 0.0;
        v.profile_.minimumLuminance = 0.0;
        return v;
    }

    static GpuShaderValidator makeStrict() {
        GpuShaderValidator v;
        v.profile_.allowBlackOutput = false;
        v.profile_.allowConstantOutput = false;
        v.profile_.minimumNonZeroPixelRatio = 0.05;
        v.profile_.minimumLuminance = 0.01;
        return v;
    }

private:
    ShaderValidationProfile profile_;
    std::vector<uint8_t> lastReadback_;
    uint32_t lastWidth_ = 0;
    uint32_t lastHeight_ = 0;
    std::string lastPresetName_;
    int lastPassIndex_ = -1;
};

}  // namespace monix::renderer_vk
