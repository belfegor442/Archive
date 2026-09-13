#include "RenderGraphBuilder.hpp"

#include <unordered_map>

namespace monix::renderer_vk {

GraphNodeId RenderGraphBuilder::nextId(GraphNodeId& counter) const {
    return counter++;
}

Result<CompiledGraph> RenderGraphBuilder::build(const PresetIr& preset,
                                                const AliasDatabase& aliases,
                                                const std::vector<ShaderReflection>& reflections) const {
    if (reflections.size() < preset.passes.size()) {
        return Status::failure("Missing shader reflection for one or more passes");
    }

    CompiledGraph graph;
    GraphNodeId id = 1;
    std::unordered_map<std::string, GraphNodeId> imageByName;

    auto addImage = [&](std::string name, int producer, bool external, bool feedback) {
        GraphNodeId node = nextId(id);
        graph.images.push_back(ImageNode{node, name, producer, external, feedback});
        imageByName[name] = node;
        return node;
    };

    addImage("Source", -1, true, false);
    addImage("Original", -1, true, false);
    addImage("ORIG_LINEARIZED", -1, true, false);

    for (const auto& texture : preset.externalTextures) {
        const std::string texName = texture.alias.empty()
            ? ("texture" + std::to_string(texture.textureIndex))
            : texture.alias;
        addImage(texName, -1, true, false);
    }

    for (const auto& pass : preset.passes) {
        const std::string passName = pass.alias.empty() ? ("Pass" + std::to_string(pass.index)) : pass.alias;
        const auto output = addImage(passName, pass.index, false, false);
        if (pass.framebufferFeedback) {
            addImage(passName + "Feedback", pass.index, false, true);
        }

        PassNode node;
        node.id = nextId(id);
        node.passIndex = pass.index;
        node.name = passName;
        node.outputImage = output;

        for (const auto& sampler : reflections[pass.index].samplers) {
            auto resolution = aliases.resolve(sampler.name, pass.index);
            GraphNodeId resource = 0;
            if (resolution.kind == AliasKind::Builtin || resolution.kind == AliasKind::ExternalTexture ||
                resolution.kind == AliasKind::PassOutput) {
                if (auto it = imageByName.find(sampler.name); it != imageByName.end()) {
                    resource = it->second;
                } else if (resolution.kind == AliasKind::PassOutput && resolution.passIndex >= 0) {
                    const auto& producerPass = preset.passes[resolution.passIndex];
                    const auto producerName = producerPass.alias.empty()
                        ? ("Pass" + std::to_string(producerPass.index))
                        : producerPass.alias;
                    if (auto it = imageByName.find(producerName); it != imageByName.end()) resource = it->second;
                }
            } else if (resolution.kind == AliasKind::Feedback) {
                const auto feedbackName = resolution.name + "Feedback";
                if (auto it = imageByName.find(feedbackName); it != imageByName.end()) {
                    resource = it->second;
                } else {
                    resource = addImage(feedbackName, resolution.passIndex, false, true);
                }
            }
            if (resource != 0) {
                node.inputs.push_back(PassDependency{sampler.name, resolution, resource});
            }
        }

        graph.passes.push_back(std::move(node));
    }

    LifetimeAnalysis lifetime;
    graph.lifetimes = lifetime.analyze(graph);
    return graph;
}

ExecutionPlan RenderGraphBuilder::compileExecutionPlan(const CompiledGraph& graph) const {
    ExecutionPlan plan;
    plan.passOrder.reserve(graph.passes.size());
    for (const auto& pass : graph.passes) {
        plan.passOrder.push_back(pass.passIndex);
    }
    plan.lifetimes = graph.lifetimes;
    return plan;
}

}  // namespace monix::renderer_vk
