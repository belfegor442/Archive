#include "GlslPreprocessor.hpp"

#include "../../core/FileUtils.hpp"

#include <mutex>
#include <sstream>

namespace monix {

std::string StripUnsupportedShaderDirectives(const std::string& source) {
  std::istringstream input(source);
  std::ostringstream output;
  std::string line;
  bool firstLine = true;
  while (std::getline(input, line)) {
    if (firstLine && line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
      line.erase(0, 3);
    }
    firstLine = false;
    if (line.rfind("#pragma parameter", 0) == 0) {
      continue;
    }
    output << line << "\n";
  }
  return output.str();
}

std::string BuildLottesShaderSource(const std::filesystem::path& path, bool vertexShader) {
  const std::string filtered = StripUnsupportedShaderDirectives(ReadTextFile(path));
  if (filtered.empty()) {
    return "";
  }

  std::ostringstream output;
  output << "#define PARAMETER_UNIFORM 1\n";
  output << (vertexShader ? "#define VERTEX 1\n" : "#define FRAGMENT 1\n");
  output << filtered;
  return output.str();
}

std::string LoadCommonModules(const std::filesystem::path& rootDir) {
  static std::string cached;
  static std::once_flag flag;
  std::call_once(flag, [&]() {
    auto commonDir = rootDir / "Shaders" / "CRT" / "Common";
    std::ostringstream out;

    const wchar_t* files[] = {
      L"geometry.glsl",
      L"common.glsl",
      L"normals.glsl"
    };
    for (const auto& f : files) {
      auto path = commonDir / f;
      std::string src = ReadTextFile(path);
      if (!src.empty()) {
        char fname[64];
        size_t n = 0;
        for (const wchar_t* p = f; *p && n < 63; ++p) fname[n++] = static_cast<char>(*p);
        fname[n] = '\0';
        out << "// === " << fname << " ===\n";
        out << src << "\n";
      }
    }
    cached = out.str();
  });
  return cached;
}

std::string PrependCommonModules(const std::string& shaderSource,
                                 const std::filesystem::path& rootDir) {
  std::string modules = LoadCommonModules(rootDir);
  if (modules.empty()) return shaderSource;

  std::string versionPrefix;
  std::string rest = shaderSource;
  auto pos = shaderSource.find("#version");
  if (pos != std::string::npos) {
    auto eol = shaderSource.find('\n', pos);
    if (eol != std::string::npos) {
      versionPrefix = shaderSource.substr(0, eol + 1);
      rest = shaderSource.substr(eol + 1);
    }
  }
  return versionPrefix + "// --- Monix Common Modules ---\n" + modules + "\n" + rest;
}

} // namespace monix
