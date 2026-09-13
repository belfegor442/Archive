#pragma once

#include "../core/Result.hpp"
#include "../graph/RenderGraph.hpp"

#include <filesystem>
#include <string>

namespace monix::renderer_vk {

class RenderGraphDump {
public:
    std::string toDot(const CompiledGraph& graph) const;
    Status writeDot(const std::filesystem::path& path, const CompiledGraph& graph) const;
};

}  // namespace monix::renderer_vk
