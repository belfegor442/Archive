#pragma once

#include "../compiler/ShaderReflection.hpp"
#include "../preset/AliasResolver.hpp"
#include "../preset/SlangPreset.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace monix::renderer_vk {

using GraphNodeId = uint32_t;

enum class GraphResourceKind {
    Image,
    Buffer,
    Sampler
};

struct ImageNode {
    GraphNodeId id = 0;
    std::string name;
    int producerPass = -1;
    bool external = false;
    bool feedback = false;
};

struct BufferNode {
    GraphNodeId id = 0;
    std::string name;
    int producerPass = -1;
};

struct SamplerNode {
    GraphNodeId id = 0;
    std::string name;
    int ownerPass = -1;
};

struct PassDependency {
    std::string samplerName;
    AliasResolution alias;
    GraphNodeId resourceId = 0;
};

struct PassNode {
    GraphNodeId id = 0;
    int passIndex = 0;
    std::string name;
    GraphNodeId outputImage = 0;
    std::vector<PassDependency> inputs;
};

struct ResourceLifetime {
    GraphNodeId resourceId = 0;
    int firstPass = 0;
    int lastPass = 0;
};

struct CompiledGraph {
    std::vector<ImageNode> images;
    std::vector<BufferNode> buffers;
    std::vector<SamplerNode> samplers;
    std::vector<PassNode> passes;
    std::vector<ResourceLifetime> lifetimes;
};

struct ExecutionPlan {
    std::vector<int> passOrder;
    std::vector<ResourceLifetime> lifetimes;
};

}  // namespace monix::renderer_vk
