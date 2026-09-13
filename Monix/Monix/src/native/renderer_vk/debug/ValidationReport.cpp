#include "ValidationReport.hpp"

#include "../core/FileSystem.hpp"

#include <sstream>

namespace monix::renderer_vk {

std::string ValidationReport::buildText(const CompiledPreset& compiled, const RuntimeStatistics& stats) const {
    std::ostringstream out;
    out << "MONIX VULKAN SLANG RUNTIME VALIDATION REPORT\n";
    out << "============================================\n\n";
    out << "Preset: " << compiled.preset.name << "\n";
    out << "Passes: " << stats.passCount << "\n";
    out << "Images: " << stats.imageCount << "\n";
    out << "Samplers: " << stats.samplerCount << "\n";
    out << "Parameters: " << stats.parameterCount << "\n";
    out << "Pipeline layouts cached: " << stats.pipelineLayoutCount << "\n";
    out << "Pipelines cached: " << stats.pipelineCount << "\n";
    out << "Reflection fallback: " << (stats.usedReflectionFallback ? "yes" : "no") << "\n\n";

    out << "Execution order:\n";
    for (int passIndex : compiled.executionPlan.passOrder) {
        if (passIndex < 0 || static_cast<size_t>(passIndex) >= compiled.preset.passes.size()) continue;
        const auto& pass = compiled.preset.passes[static_cast<size_t>(passIndex)];
        out << "  Pass " << pass.index << ": "
            << (pass.alias.empty() ? pass.shaderPath.filename().string() : pass.alias)
            << "\n";
    }

    out << "\nResource lifetimes:\n";
    for (const auto& lifetime : compiled.executionPlan.lifetimes) {
        out << "  Resource " << lifetime.resourceId << ": pass "
            << lifetime.firstPass << " -> " << lifetime.lastPass << "\n";
    }
    return out.str();
}

Status ValidationReport::write(const std::filesystem::path& path,
                               const CompiledPreset& preset,
                               const RuntimeStatistics& stats) const {
    return FileSystem::writeText(path, buildText(preset, stats));
}

}  // namespace monix::renderer_vk
