#pragma once

#include "RenderGraph.hpp"

namespace monix::renderer_vk {

class LifetimeAnalysis {
public:
    std::vector<ResourceLifetime> analyze(const CompiledGraph& graph) const;
};

}  // namespace monix::renderer_vk
