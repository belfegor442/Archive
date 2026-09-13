#pragma once

#include <string>
#include <filesystem>

namespace monix {

std::string StripUnsupportedShaderDirectives(const std::string& source);
std::string BuildLottesShaderSource(const std::filesystem::path& path, bool vertexShader);
std::string LoadCommonModules(const std::filesystem::path& rootDir);
std::string PrependCommonModules(const std::string& shaderSource, const std::filesystem::path& rootDir);

} // namespace monix
