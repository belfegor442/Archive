#include "ReflectionDump.hpp"

#include "../core/FileSystem.hpp"

#include <sstream>

namespace monix::renderer_vk {

std::string ReflectionDump::toText(const ShaderReflection& reflection) const {
    std::ostringstream out;
    out << "Descriptors\n";
    for (const auto& descriptor : reflection.descriptors) {
        out << "  set=" << descriptor.set << " binding=" << descriptor.binding
            << " name=" << descriptor.name << " kind=" << static_cast<int>(descriptor.kind) << "\n";
    }
    out << "Uniform blocks\n";
    for (const auto& block : reflection.uniformBlocks) {
        out << "  " << block.name << " instance=" << block.instanceName
            << " size=" << block.size << "\n";
        for (const auto& member : block.members) {
            out << "    " << member.name << " " << member.type
                << " offset=" << member.offset << " size=" << member.size << "\n";
        }
    }
    out << "Push constants\n";
    for (const auto& push : reflection.pushConstants) {
        out << "  " << push.name << " size=" << push.size << "\n";
    }
    return out.str();
}

Status ReflectionDump::write(const std::filesystem::path& path, const ShaderReflection& reflection) const {
    return FileSystem::writeText(path, toText(reflection));
}

}  // namespace monix::renderer_vk
