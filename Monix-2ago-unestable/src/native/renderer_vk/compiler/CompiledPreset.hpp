#pragma once

#include "../graph/RenderGraph.hpp"
#include "../preset/ParameterExtractor.hpp"
#include "../preset/SlangPreset.hpp"
#include "ShaderReflection.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct CompiledShaderStage {
    std::string source;
    std::vector<unsigned char> spirv;
    ShaderReflection reflection;
};

struct CompiledPass {
    PresetPassIr preset;
    CompiledShaderStage vertex;
    CompiledShaderStage fragment;
    std::vector<ShaderParameter> parameters;
    std::vector<std::filesystem::path> dependencies;
};

struct CompiledPreset {
    PresetIr preset;
    std::vector<CompiledPass> passes;
    AliasDatabase aliases;
    CompiledGraph graph;
    ExecutionPlan executionPlan;
};

}  // namespace monix::renderer_vk
