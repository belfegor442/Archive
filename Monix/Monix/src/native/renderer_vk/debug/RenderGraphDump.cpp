#include "RenderGraphDump.hpp"

#include "../core/FileSystem.hpp"

#include <sstream>

namespace monix::renderer_vk {

std::string RenderGraphDump::toDot(const CompiledGraph& graph) const {
    std::ostringstream out;
    out << "digraph MonixRenderGraph {\n";
    out << "  rankdir=LR;\n";
    for (const auto& image : graph.images) {
        out << "  r" << image.id << " [shape=box,label=\"" << image.name << "\"];\n";
    }
    for (const auto& pass : graph.passes) {
        out << "  p" << pass.id << " [shape=ellipse,label=\"" << pass.name << "\"];\n";
        out << "  p" << pass.id << " -> r" << pass.outputImage << ";\n";
        for (const auto& input : pass.inputs) {
            out << "  r" << input.resourceId << " -> p" << pass.id
                << " [label=\"" << input.samplerName << "\"];\n";
        }
    }
    out << "}\n";
    return out.str();
}

Status RenderGraphDump::writeDot(const std::filesystem::path& path, const CompiledGraph& graph) const {
    return FileSystem::writeText(path, toDot(graph));
}

}  // namespace monix::renderer_vk
