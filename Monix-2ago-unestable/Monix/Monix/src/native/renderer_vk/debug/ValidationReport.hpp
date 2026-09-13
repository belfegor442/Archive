#pragma once

#include "../compiler/CompiledPreset.hpp"
#include "../core/Result.hpp"
#include "RuntimeStatistics.hpp"

#include <filesystem>
#include <string>

namespace monix::renderer_vk {

class ValidationReport {
public:
    std::string buildText(const CompiledPreset& preset, const RuntimeStatistics& stats) const;
    Status write(const std::filesystem::path& path,
                 const CompiledPreset& preset,
                 const RuntimeStatistics& stats) const;
};

}  // namespace monix::renderer_vk
