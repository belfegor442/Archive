#pragma once

#include "../core/Diagnostics.hpp"
#include "../core/Result.hpp"
#include "IncludeResolver.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace monix::renderer_vk {

struct PreprocessResult {
    std::filesystem::path rootFile;
    std::string source;
    std::vector<std::filesystem::path> dependencies;
    std::unordered_map<std::string, std::string> macros;
    Diagnostics diagnostics;
};

class ShaderPreprocessor {
public:
    explicit ShaderPreprocessor(IncludeResolver resolver);

    Result<PreprocessResult> preprocessFile(const std::filesystem::path& path,
                                            const std::unordered_map<std::string, std::string>& initialDefines = {}) const;

private:
    IncludeResolver resolver_;

    Result<std::string> preprocessText(const std::string& source,
                                       const std::filesystem::path& path,
                                       PreprocessResult& result,
                                       std::unordered_set<std::string>& includeStack,
                                       std::unordered_map<std::string, std::string>& macros,
                                       bool eraseGuards = true) const;
};

}  // namespace monix::renderer_vk
