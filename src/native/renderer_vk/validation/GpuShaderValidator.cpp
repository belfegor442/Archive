#include "GpuShaderValidator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace monix::renderer_vk {

OutputStatistics GpuShaderValidator::analyzeOutput(
    const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) const {

    OutputStatistics stats;
    stats.totalPixels = static_cast<size_t>(width) * height;

    if (pixels.empty() || stats.totalPixels == 0) return stats;

    double sumR = 0, sumG = 0, sumB = 0, sumA = 0;
    size_t nonZero = 0;
    std::unordered_set<uint32_t> uniqueColors;

    stats.minR = stats.minG = stats.minB = stats.minA = 255;
    stats.maxR = stats.maxG = stats.maxB = stats.maxA = 0;

    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        uint8_t b = pixels[i + 0];
        uint8_t g = pixels[i + 1];
        uint8_t r = pixels[i + 2];
        uint8_t a = pixels[i + 3];

        stats.minR = std::min(stats.minR, r);
        stats.minG = std::min(stats.minG, g);
        stats.minB = std::min(stats.minB, b);
        stats.minA = std::min(stats.minA, a);
        stats.maxR = std::max(stats.maxR, r);
        stats.maxG = std::max(stats.maxG, g);
        stats.maxB = std::max(stats.maxB, b);
        stats.maxA = std::max(stats.maxA, a);

        sumR += r;
        sumG += g;
        sumB += b;
        sumA += a;

        if (r > 0 || g > 0 || b > 0 || a > 0) ++nonZero;

        uint32_t packed = (static_cast<uint32_t>(r) << 24) |
                          (static_cast<uint32_t>(g) << 16) |
                          (static_cast<uint32_t>(b) << 8) |
                          static_cast<uint32_t>(a);
        uniqueColors.insert(packed);

        float fR = r / 255.0f;
        float fG = g / 255.0f;
        float fB = b / 255.0f;
        if (std::isnan(fR) || std::isnan(fG) || std::isnan(fB)) stats.hasNaN = true;
        if (std::isinf(fR) || std::isinf(fG) || std::isinf(fB)) stats.hasInf = true;
    }

    stats.meanR = sumR / stats.totalPixels / 255.0;
    stats.meanG = sumG / stats.totalPixels / 255.0;
    stats.meanB = sumB / stats.totalPixels / 255.0;
    stats.meanA = sumA / stats.totalPixels / 255.0;
    stats.meanLuminance = 0.2126 * stats.meanR + 0.7152 * stats.meanG + 0.0722 * stats.meanB;
    stats.nonZeroPixels = nonZero;
    stats.uniqueColors = uniqueColors.size();

    return stats;
}

GpuTestResult GpuShaderValidator::validateOutput(const OutputStatistics& stats, bool isFinalPass) const {
    if (stats.totalPixels == 0) return GpuTestResult::FailReadbackFailed;

    if (stats.hasNaN) return GpuTestResult::FailNaN;
    if (stats.hasInf) return GpuTestResult::FailInf;

    double nonZeroRatio = static_cast<double>(stats.nonZeroPixels) / stats.totalPixels;

    if (!profile_.allowBlackOutput) {
        if (nonZeroRatio < profile_.minimumNonZeroPixelRatio &&
            stats.meanLuminance < profile_.minimumLuminance) {
            return GpuTestResult::FailBlackOutput;
        }
    }

    if (!profile_.allowConstantOutput && isFinalPass) {
        if (stats.uniqueColors <= 1 && nonZeroRatio > 0.99) {
            return GpuTestResult::FailConstantOutput;
        }
    }

    return GpuTestResult::Pass;
}

GpuTestDiagnostic GpuShaderValidator::runGpuTest() {
    GpuTestDiagnostic diag;
    diag.presetName = lastPresetName_;
    diag.passIndex = lastPassIndex_;

    if (lastReadback_.empty() || lastWidth_ == 0 || lastHeight_ == 0) {
        diag.result = GpuTestResult::FailReadbackFailed;
        diag.detail = "No readback data available";
        return diag;
    }

    diag.stats = analyzeOutput(lastReadback_, lastWidth_, lastHeight_);
    bool isFinal = (lastPassIndex_ >= 0);
    diag.result = validateOutput(diag.stats, isFinal);

    if (diag.result != GpuTestResult::Pass) {
        std::ostringstream oss;
        oss << "NonZeroPixels: " << diag.stats.nonZeroPixels << "/" << diag.stats.totalPixels
            << " (" << (100.0 * diag.stats.nonZeroPixels / diag.stats.totalPixels) << "%)\n";
        oss << "MeanLuminance: " << diag.stats.meanLuminance << "\n";
        oss << "UniqueColors: " << diag.stats.uniqueColors << "\n";
        oss << "HasNaN: " << (diag.stats.hasNaN ? "yes" : "no") << "\n";
        oss << "HasInf: " << (diag.stats.hasInf ? "yes" : "no");
        diag.detail = oss.str();
    }

    return diag;
}

void GpuShaderValidator::setGpuTestData(
    const std::vector<uint8_t>& readback, uint32_t w, uint32_t h,
    const std::string& preset, int pass) {
    lastReadback_ = readback;
    lastWidth_ = w;
    lastHeight_ = h;
    lastPresetName_ = preset;
    lastPassIndex_ = pass;
}

std::string GpuTestDiagnostic::format() const {
    std::ostringstream oss;
    oss << "[ShaderValidation]\n";
    oss << "Preset: " << presetName << "\n";
    if (passIndex >= 0) oss << "Pass: " << passIndex << "\n";
    oss << "GPU TEST: " << gpuTestResultName(result) << "\n";
    if (!detail.empty()) {
        oss << "\nStatistics:\n" << detail << "\n";
    }
    oss << "\nACTION:\n";
    if (result == GpuTestResult::Pass || result == GpuTestResult::Skip) {
        oss << "Candidate accepted\n";
    } else {
        oss << "Candidate rejected\n";
    }
    return oss.str();
}

}  // namespace monix::renderer_vk
