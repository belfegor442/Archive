#include "LifetimeAnalysis.hpp"

#include <algorithm>
#include <unordered_map>

namespace monix::renderer_vk {

std::vector<ResourceLifetime> LifetimeAnalysis::analyze(const CompiledGraph& graph) const {
    std::unordered_map<GraphNodeId, ResourceLifetime> map;
    for (const auto& image : graph.images) {
        map[image.id] = ResourceLifetime{image.id, image.producerPass < 0 ? 0 : image.producerPass, image.producerPass};
    }
    for (const auto& pass : graph.passes) {
        if (auto it = map.find(pass.outputImage); it != map.end()) {
            it->second.firstPass = std::min(it->second.firstPass, pass.passIndex);
            it->second.lastPass = std::max(it->second.lastPass, pass.passIndex);
        }
        for (const auto& input : pass.inputs) {
            if (auto it = map.find(input.resourceId); it != map.end()) {
                it->second.lastPass = std::max(it->second.lastPass, pass.passIndex);
            }
        }
    }
    std::vector<ResourceLifetime> lifetimes;
    lifetimes.reserve(map.size());
    for (auto& item : map) {
        lifetimes.push_back(item.second);
    }
    std::sort(lifetimes.begin(), lifetimes.end(), [](const auto& a, const auto& b) {
        return a.resourceId < b.resourceId;
    });
    return lifetimes;
}

}  // namespace monix::renderer_vk
