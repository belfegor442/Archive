#pragma once

#include "../core/Result.hpp"
#include "SlangPreset.hpp"

#include <filesystem>
#include <unordered_set>

namespace monix::renderer_vk {

class SlangPresetParser {
public:
    Result<PresetAst> parseFile(const std::filesystem::path& path) const;
    Result<PresetIr> buildIr(const PresetAst& ast) const;

private:
    Result<PresetAst> parseFileInternal(const std::filesystem::path& path,
                                        std::unordered_set<std::string>& visited) const;
};

}  // namespace monix::renderer_vk
