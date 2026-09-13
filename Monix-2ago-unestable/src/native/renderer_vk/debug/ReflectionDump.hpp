#pragma once

#include "../compiler/ShaderReflection.hpp"
#include "../core/Result.hpp"

#include <filesystem>
#include <string>

namespace monix::renderer_vk {

class ReflectionDump {
public:
    std::string toText(const ShaderReflection& reflection) const;
    Status write(const std::filesystem::path& path, const ShaderReflection& reflection) const;
};

}  // namespace monix::renderer_vk
