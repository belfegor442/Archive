#include "GlslpPresetParser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace monix {

renderer_vk::GlPresetConfig parseGlslpPreset(
    const std::filesystem::path& glslpPath,
    const std::filesystem::path& rootDir) {
  renderer_vk::GlPresetConfig config;
  std::ifstream file(glslpPath);
  if (!file.is_open()) return config;

  auto basePath = glslpPath.parent_path();
  int shaderCount = 0;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    auto key = line.substr(0, eq);
    auto val = line.substr(eq + 1);
    auto trim = [](std::string& s) {
      while (!s.empty() && s.front() == ' ') s.erase(s.begin());
      while (!s.empty() && s.back() == ' ') s.pop_back();
      if (!s.empty() && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
      }
    };
    trim(key);
    trim(val);

    if (key == "shaders") {
      try {
        shaderCount = std::stoi(val);
      } catch (...) { continue; }
      config.passes.resize(shaderCount);
    } else if (key == "textures") {
      std::istringstream ss(val);
      std::string tok;
      while (std::getline(ss, tok, ';')) {
        trim(tok);
        if (!tok.empty()) {
          config.textures[tok] = {};
        }
      }
    } else if (key.find("shader") == 0 && key.size() > 6) {
      int idx = 0;
      try {
        idx = std::stoi(key.substr(6));
      } catch (...) { continue; }
      if (idx >= 0 && idx < (int)config.passes.size()) {
        auto shaderPath = basePath / val;
        if (!std::filesystem::exists(shaderPath)) {
          shaderPath = rootDir / "Shaders" / val;
        }
        if (!std::filesystem::exists(shaderPath)) {
          auto parent2 = basePath.parent_path();
          shaderPath = parent2 / val;
        }
        config.passes[idx].shaderPath = shaderPath;
      }
    } else if (key.find("filter_linear") == 0 && key.size() > 13) {
      int idx = 0;
      try {
        idx = std::stoi(key.substr(13));
      } catch (...) { continue; }
      if (idx >= 0 && idx < (int)config.passes.size()) {
        config.passes[idx].linearFilter = (val == "true");
      }
    } else if (key.find("float_framebuffer") == 0 && key.size() > 17) {
      int idx = 0;
      try {
        idx = std::stoi(key.substr(17));
      } catch (...) { continue; }
      if (idx >= 0 && idx < (int)config.passes.size()) {
        config.passes[idx].floatFramebuffer = (val == "true");
      }
    }
  }

  for (auto& [name, tex] : config.textures) {
    auto texPath = basePath / tex.path;
    if (!std::filesystem::exists(texPath)) {
      texPath = rootDir / "Shaders" / tex.path;
    }
    tex.path = texPath;
  }

  return config;
}

} // namespace monix
