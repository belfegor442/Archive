#pragma once

#include "../core/Result.hpp"
#include "LifetimeAnalysis.hpp"

#include <unordered_map>

namespace monix::renderer_vk {

class RenderGraphBuilder {
public:
    Result<CompiledGraph> build(const PresetIr& preset,
                                const AliasDatabase& aliases,
                                const std::vector<ShaderReflection>& reflections) const;
    ExecutionPlan compileExecutionPlan(const CompiledGraph& graph) const;

private:
    GraphNodeId nextId(GraphNodeId& counter) const;
};

}  // namespace monix::renderer_vk
