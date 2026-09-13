// Standalone test for TranslateSlangToGlsl + ParseSlangp
// Compile: cl.exe /MT /std:c++20 /EHsc /utf-8 test_translator.cpp /Fe:test_translator.exe
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// ============================================================================
// Minimal types copied from main.cpp
// ============================================================================

enum class ScaleType { Source, Viewport, Absolute };

std::ostream& operator<<(std::ostream& os, const ScaleType& s) {
  switch (s) {
    case ScaleType::Source: return os << "source";
    case ScaleType::Viewport: return os << "viewport";
    case ScaleType::Absolute: return os << "absolute";
  }
  return os << "unknown";
}

struct ExternalTexture {
  std::string samplerName;
  std::string filePath;
  int binding = -1;
  bool linear = true;
  std::string wrapMode = "clamp_to_edge";
};

struct PassInfo {
  int index = 0;
  fs::path shaderPath;
  bool filterLinear = false;
  std::string wrapMode = "clamp_to_border";
  bool mipmapInput = false;
  std::string alias;
  bool floatFramebuffer = false;
  bool srgbFramebuffer = false;
  ScaleType scaleTypeX = ScaleType::Source;
  ScaleType scaleTypeY = ScaleType::Source;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  std::vector<ExternalTexture> externalTextures;
};

struct SlangpPreset {
  std::string name;
  fs::path basePath;
  std::vector<PassInfo> passes;
  bool valid = false;
  std::string error;
};

struct ShaderParameter {
  std::string name;
  std::string description;
  std::string type = "float";
  float defaultValue = 0.0f;
  float minValue = 0.0f;
  float maxValue = 1.0f;
  float step = 0.01f;
};

struct TranslatedShader {
  std::string vertex;
  std::string fragment;
  std::vector<ShaderParameter> parameters;
  std::vector<std::pair<std::string, int>> samplerBindings;
  std::vector<ExternalTexture> externalTextures;
  int uboMemberCount = 0;
  bool extractFromFragment = false;
};

// ============================================================================
// Utility functions
// ============================================================================

std::string ReadTextFile(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return "";
  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

static ScaleType ParseScaleType(const std::string& s) {
  if (s == "viewport") return ScaleType::Viewport;
  if (s == "absolute") return ScaleType::Absolute;
  return ScaleType::Source;
}

static std::string TrimQuoted(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n\"");
  size_t end = s.find_last_not_of(" \t\r\n\"");
  if (start == std::string::npos) return "";
  return s.substr(start, end - start + 1);
}

// ============================================================================
// ParseSlangp (copied from main.cpp)
// ============================================================================

SlangpPreset ParseSlangp(const fs::path& slangpPath) {
  SlangpPreset preset;
  preset.basePath = slangpPath.parent_path();
  preset.name = slangpPath.stem().string();
  std::ifstream file(slangpPath);
  if (!file.is_open()) {
    preset.error = "Cannot open file: " + slangpPath.string();
    return preset;
  }
  int numShaders = 0;
  int numTextures = 0;
  std::unordered_map<int, std::unordered_map<std::string, std::string>> passParams;
  std::unordered_map<int, std::string> texturePaths;
  std::unordered_map<int, std::string> textureAliases;
  std::string line;
  while (std::getline(file, line)) {
    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = TrimQuoted(line.substr(0, eq));
    std::string val = TrimQuoted(line.substr(eq + 1));
    if (key == "shaders") {
      numShaders = 0;
      try { numShaders = std::stoi(val); } catch (...) {}
      continue;
    }
    if (key == "textures") {
      numTextures = 0;
      try { numTextures = std::stoi(val); } catch (...) {}
      continue;
    }
    for (int i = 0; i < 64; ++i) {
      std::string prefix = "texture" + std::to_string(i);
      if (key == prefix) {
        texturePaths[i] = val;
        break;
      }
      prefix = "alias_texture" + std::to_string(i);
      if (key == prefix) {
        textureAliases[i] = val;
        break;
      }
    }
    for (int i = 0; i < 64; ++i) {
      std::string prefix = "shader" + std::to_string(i);
      if (key == prefix) {
        passParams[i] = {};
        passParams[i]["__path"] = val;
        break;
      }
      prefix = "filter_linear" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["filter_linear"] = val; break; }
      prefix = "wrap_mode" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["wrap_mode"] = val; break; }
      prefix = "mipmap_input" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["mipmap_input"] = val; break; }
      prefix = "alias" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["alias"] = val; break; }
      // Texture aliases: aliasN where no shaderN exists (e.g., alias8 = TubeDiffuseImage)
      if (key == prefix && !passParams.count(i) && i < 64) {
        int texIdx = i - numShaders;
        if (texIdx >= 0) textureAliases[texIdx] = val;
        break;
      }
      prefix = "float_framebuffer" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["float_framebuffer"] = val; break; }
      prefix = "srgb_framebuffer" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["srgb_framebuffer"] = val; break; }
      prefix = "scale_type_x" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["scale_type_x"] = val; break; }
      prefix = "scale_x" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["scale_x"] = val; break; }
      prefix = "scale_type_y" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["scale_type_y"] = val; break; }
      prefix = "scale_y" + std::to_string(i);
      if (key == prefix && passParams.count(i)) { passParams[i]["scale_y"] = val; break; }
    }
  }
  if (numShaders <= 0) {
    preset.error = "No shaders defined in preset";
    return preset;
  }
  // Build external texture list from texture directives
  std::vector<ExternalTexture> globalTextures;
  for (auto& [idx, texPath] : texturePaths) {
    ExternalTexture et;
    auto it = textureAliases.find(idx);
    if (it != textureAliases.end()) {
      et.samplerName = it->second;
    } else {
      std::string stem = fs::path(texPath).stem().string();
      if (stem.size() > 8 && stem.substr(0, 8) == "texture_") {
        stem = stem.substr(8);
      }
      et.samplerName = stem;
    }
    et.filePath = texPath;
    et.binding = -1;
    et.linear = true;
    et.wrapMode = "clamp_to_edge";
    globalTextures.push_back(et);
  }
  // Auto-discover textures directory
  if (globalTextures.empty()) {
    auto texDir = preset.basePath / "textures";
    if (fs::is_directory(texDir)) {
      for (auto& entry : fs::directory_iterator(texDir)) {
        if (entry.is_regular_file()) {
          auto ext = entry.path().extension().string();
          for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
          if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
            ExternalTexture et;
            et.samplerName = entry.path().stem().string();
            et.filePath = entry.path().string();
            et.binding = -1;
            et.linear = true;
            et.wrapMode = "clamp_to_edge";
            globalTextures.push_back(et);
          }
        }
      }
    }
  }
  for (int i = 0; i < numShaders; ++i) {
    if (passParams.find(i) == passParams.end() || passParams[i].find("__path") == passParams[i].end()) {
      preset.error = "Missing shader" + std::to_string(i) + " definition";
      return preset;
    }
    PassInfo pi;
    pi.index = i;
    pi.shaderPath = preset.basePath / passParams[i]["__path"];
    if (!fs::exists(pi.shaderPath)) {
      auto flatPath = preset.basePath / fs::path(passParams[i]["__path"]).filename();
      if (fs::exists(flatPath)) {
        pi.shaderPath = flatPath;
      } else {
        preset.error = "Shader file not found: " + pi.shaderPath.string();
        return preset;
      }
    }
    pi.filterLinear = (passParams[i]["filter_linear"] == "true");
    pi.wrapMode = passParams[i].count("wrap_mode") ? passParams[i]["wrap_mode"] : "clamp_to_border";
    pi.mipmapInput = (passParams[i]["mipmap_input"] == "true");
    pi.alias = passParams[i]["alias"];
    pi.floatFramebuffer = (passParams[i]["float_framebuffer"] == "true");
    pi.srgbFramebuffer = (passParams[i]["srgb_framebuffer"] == "true");
    pi.scaleTypeX = ParseScaleType(passParams[i].count("scale_type_x") ? passParams[i]["scale_type_x"] : "source");
    pi.scaleTypeY = ParseScaleType(passParams[i].count("scale_type_y") ? passParams[i]["scale_type_y"] : "source");
    pi.scaleX = 1.0f;
    pi.scaleY = 1.0f;
    try { if (passParams[i].count("scale_x")) pi.scaleX = std::stof(passParams[i]["scale_x"]); } catch (...) {}
    try { if (passParams[i].count("scale_y")) pi.scaleY = std::stof(passParams[i]["scale_y"]); } catch (...) {}
    pi.externalTextures = globalTextures;
    preset.passes.push_back(pi);
  }
  preset.valid = true;
  return preset;
}

// ============================================================================
// TranslateSlangToGlsl (copied from main.cpp - THE FULL FUNCTION)
// ============================================================================

TranslatedShader TranslateSlangToGlsl(const std::string& source, const fs::path& shaderPath) {
  std::istringstream input(source);
  std::string line;

  // --- Pass 1: resolve #include directives (recursive) ---
  std::set<std::string> includedFiles;
  std::function<std::string(const std::string&, const std::filesystem::path&)> resolveIncludes =
    [&](const std::string& src, const std::filesystem::path& currentDir) -> std::string {
    std::string result;
    std::istringstream stream(src);
    std::string ln;
    while (std::getline(stream, ln)) {
      if (ln.rfind("#include", 0) == 0) {
        auto q1 = ln.find('"');
        auto q2 = ln.rfind('"');
        if (q1 != std::string::npos && q2 > q1) {
          std::string incFile = ln.substr(q1 + 1, q2 - q1 - 1);
          auto incPath = (currentDir / incFile).lexically_normal();
          std::string incPathStr = incPath.string();
          std::string incSrc = ReadTextFile(incPath);
          if (incSrc.empty() && !fs::exists(incPath)) {
            auto parentPath = (currentDir.parent_path() / incFile).lexically_normal();
            incSrc = ReadTextFile(parentPath);
            if (!incSrc.empty()) incPath = parentPath;
          }
          incPathStr = incPath.string();
          if (includedFiles.count(incPathStr)) {
            continue;
          }
          includedFiles.insert(incPathStr);
          if (!incSrc.empty()) {
            result += "// --- include: " + incFile + " ---\n";
            result += resolveIncludes(incSrc, incPath.parent_path());
            result += "\n// --- end include ---\n";
          } else {
            result += ln + "\n";
          }
        } else {
          result += ln + "\n";
        }
      } else {
        result += ln + "\n";
      }
    }
    return result;
  };
  std::string resolved = resolveIncludes(source, shaderPath.parent_path());

  // --- Pass 2: split into vertex and fragment sections ---
  std::string vertexSection, fragmentSection, preamble;
  enum Section { PREAMBLE, VERTEX, FRAGMENT };
  Section sec = PREAMBLE;
  std::istringstream s2(resolved);
  while (std::getline(s2, line)) {
    if (line.find("#pragma stage vertex") != std::string::npos) { sec = VERTEX; continue; }
    if (line.find("#pragma stage fragment") != std::string::npos) { sec = FRAGMENT; continue; }
    if (line.rfind("#pragma parameter", 0) == 0) continue;
    if (line.rfind("#pragma", 0) == 0) continue;
    if (sec == PREAMBLE) preamble += line + "\n";
    else if (sec == VERTEX) vertexSection += line + "\n";
    else fragmentSection += line + "\n";
  }
  if (vertexSection.empty() && fragmentSection.empty()) {
    fragmentSection = preamble;
    preamble.clear();
  }

  bool extractFromFragment = preamble.empty() && !fragmentSection.empty();

  // --- Pass 3: extract push_constant block ---
  std::string pushMembers;
  std::string pushStructVarName;
  {
    std::istringstream ps(extractFromFragment ? fragmentSection : preamble);
    std::string pl;
    bool inPush = false;
    std::ostringstream rest;
    while (std::getline(ps, pl)) {
      if (pl.find("layout(push_constant)") != std::string::npos) { inPush = true; continue; }
      if (inPush) {
        std::string trimmedPl = pl;
        while (!trimmedPl.empty() && (trimmedPl[0] == ' ' || trimmedPl[0] == '\t')) trimmedPl.erase(0, 1);
        if (trimmedPl.find("};") == 0 || (trimmedPl.size() >= 2 && trimmedPl[0] == '}' && trimmedPl[1] == ' ')) {
          inPush = false;
          auto semiPos = trimmedPl.find(';');
          auto namePart = trimmedPl.substr(1, (semiPos != std::string::npos ? semiPos : trimmedPl.size()) - 1);
          while (!namePart.empty() && namePart[0] == ' ') namePart.erase(0, 1);
          while (!namePart.empty() && (namePart.back() == ' ' || namePart.back() == ';' || namePart.back() == '\r')) namePart.pop_back();
          if (!namePart.empty() && namePart.find(' ') == std::string::npos && namePart.find('/') == std::string::npos) {
            pushStructVarName = namePart;
          }
          continue;
        }
        static const char* glslTypes[] = {
          "float", "int", "uint", "bool", "vec2", "vec3", "vec4",
          "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4",
          "bvec2", "bvec3", "bvec4", "mat2", "mat3", "mat4",
          "sampler2D", "sampler3D", "samplerCube"
        };
        bool isMember = false;
        for (auto* t : glslTypes) {
          size_t tlen = strlen(t);
          if (trimmedPl.size() > tlen && trimmedPl.compare(0, tlen, t) == 0 && trimmedPl[tlen] == ' ') {
            isMember = true;
            break;
          }
        }
        if (isMember) pushMembers += trimmedPl + "\n";
      } else {
        rest << pl << "\n";
      }
    }
    if (extractFromFragment) fragmentSection = rest.str(); else preamble = rest.str();
  }

  // --- Pass 4: extract UBO block ---
  std::string uboMembers;
  {
    std::string& scanSrc = extractFromFragment ? fragmentSection : preamble;
    bool hasLayoutStd140 = scanSrc.find("layout(std140") != std::string::npos;
    bool hasUniformBlock = scanSrc.find("uniform") != std::string::npos && scanSrc.find('{') != std::string::npos;
    std::cerr << "  [DIAG] preamble.size=" << preamble.size() << " fragmentSection.size=" << fragmentSection.size()
              << " extractFromFrag=" << extractFromFragment
              << " hasLayoutStd140=" << hasLayoutStd140 << " hasUniformBlock=" << hasUniformBlock << "\n";
    std::istringstream us(scanSrc);
    std::string ul;
    bool inUbo = false;
    std::ostringstream rest;
    while (std::getline(us, ul)) {
      if ((ul.find("layout(std140") != std::string::npos && ul.find("uniform") != std::string::npos) ||
          (ul.find("uniform") != std::string::npos && ul.find('{') != std::string::npos && ul.find('(') == std::string::npos)) { inUbo = true; continue; }
      if (inUbo) {
        if (ul.find("};") != std::string::npos || (ul.find('}') != std::string::npos && ul.find("uniform") == std::string::npos && ul.find("//") == std::string::npos)) { inUbo = false; continue; }
        std::string trimmedUl = ul;
        while (!trimmedUl.empty() && (trimmedUl[0] == ' ' || trimmedUl[0] == '\t')) trimmedUl.erase(0, 1);
        static const char* glslTypes2[] = {
          "float", "int", "uint", "bool", "vec2", "vec3", "vec4",
          "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4",
          "bvec2", "bvec3", "bvec4", "mat2", "mat3", "mat4"
        };
        bool isMember = false;
        for (auto* t : glslTypes2) {
          size_t tlen = strlen(t);
          if (trimmedUl.size() > tlen && trimmedUl.compare(0, tlen, t) == 0 && trimmedUl[tlen] == ' ') {
            isMember = true;
            break;
          }
        }
        if (isMember) uboMembers += trimmedUl + "\n";
      } else {
        rest << ul << "\n";
      }
    }
    if (extractFromFragment) fragmentSection = rest.str(); else preamble = rest.str();
  }

  static const char* skipNames[] = {
    "SourceSize", "OriginalSize", "OutputSize", "InputSize",
    "FrameCount", "FrameDirection", "MVP", "Texture"
  };

  // --- Pass 4b: collect preamble helpers ---
  std::string preambleHelpers;
  {
    std::istringstream ph(extractFromFragment ? fragmentSection : preamble);
    std::string pl;
    while (std::getline(ph, pl)) {
      std::string trimmed = pl;
      while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t')) trimmed.erase(0, 1);
      while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) trimmed.pop_back();
      if (trimmed.empty() || trimmed.substr(0, 8) == "#version" ||
          trimmed.substr(0, 17) == "#pragma parameter" ||
          trimmed.substr(0, 14) == "#pragma format" ||
          trimmed.substr(0, 8) == "#include" ||
          trimmed == "//") continue;
      if (trimmed.substr(0, 7) == "#define") {
        bool skipDef = false;
        for (auto* s : skipNames) {
          if (trimmed.find(s) != std::string::npos) { skipDef = true; break; }
        }
        if (!skipDef) {
          size_t pp;
          if (!pushStructVarName.empty()) {
            std::string dotPrefix = pushStructVarName + ".";
            while ((pp = trimmed.find(dotPrefix)) != std::string::npos) trimmed.erase(pp, dotPrefix.size());
          }
          while ((pp = trimmed.find("params.")) != std::string::npos) { trimmed.erase(pp, 7); }
          while ((pp = trimmed.find("param.")) != std::string::npos) { trimmed.erase(pp, 6); }
          preambleHelpers += trimmed + "\n";
        }
        continue;
      }
      preambleHelpers += trimmed + "\n";
    }
  }

  // Rename global.X in preamble helpers
  {
    size_t pos;
    while ((pos = preambleHelpers.find("global.MVP")) != std::string::npos) preambleHelpers.replace(pos, 10, "MVPMatrix");
    while ((pos = preambleHelpers.find("global.OutputSize")) != std::string::npos) preambleHelpers.replace(pos, 17, "OutputSize");
    while ((pos = preambleHelpers.find("global.SourceSize")) != std::string::npos) preambleHelpers.replace(pos, 17, "TextureSize");
    while ((pos = preambleHelpers.find("global.InputSize")) != std::string::npos) preambleHelpers.replace(pos, 16, "InputSize");
    while ((pos = preambleHelpers.find("global.OriginalSize")) != std::string::npos) preambleHelpers.replace(pos, 19, "InputSize");
    while ((pos = preambleHelpers.find("global.FrameCount")) != std::string::npos) preambleHelpers.replace(pos, 17, "float(FrameCount)");
    // Catch-all: rename global.TextureSize -> TextureSize (and any other global.X)
    while ((pos = preambleHelpers.find("global.TextureSize")) != std::string::npos) preambleHelpers.replace(pos, 17, "TextureSize");
    while ((pos = preambleHelpers.find("global.")) != std::string::npos) preambleHelpers.erase(pos, 7);
    if (!pushStructVarName.empty()) {
      std::string dotPrefix = pushStructVarName + ".";
      while ((pos = preambleHelpers.find(dotPrefix)) != std::string::npos) preambleHelpers.erase(pos, dotPrefix.size());
    }
    while ((pos = preambleHelpers.find("params.")) != std::string::npos) { preambleHelpers.erase(pos, 7); }
    while ((pos = preambleHelpers.find("param.")) != std::string::npos) { preambleHelpers.erase(pos, 6); }
    // HLSL -> GLSL type/function conversions in preamble helpers
    {
      auto hlslToGlsl = [&](const std::string& from, const std::string& to) {
        size_t p = 0;
        while ((p = preambleHelpers.find(from, p)) != std::string::npos) {
          preambleHelpers.replace(p, from.size(), to);
          p += to.size();
        }
      };
      hlslToGlsl("float4x4", "mat4");
      hlslToGlsl("float3x3", "mat3");
      hlslToGlsl("float2x2", "mat2");
      hlslToGlsl("float4", "vec4");
      hlslToGlsl("float3", "vec3");
      hlslToGlsl("float2", "vec2");
      hlslToGlsl("int4", "ivec4");
      hlslToGlsl("int3", "ivec3");
      hlslToGlsl("int2", "ivec2");
      hlslToGlsl("uint4", "uvec4");
      hlslToGlsl("uint3", "uvec3");
      hlslToGlsl("uint2", "uvec2");
      hlslToGlsl("bool4", "bvec4");
      hlslToGlsl("bool3", "bvec3");
      hlslToGlsl("bool2", "bvec2");
      hlslToGlsl("lerp(", "mix(");
      hlslToGlsl("frac(", "fract(");
      hlslToGlsl("rsqrt(", "inversesqrt(");
      hlslToGlsl("tex2D(", "texture(");
      hlslToGlsl("tex2Dlod(", "textureLod(");
    }
    // Fix HLSL scalar swizzle: float.x -> float (GLSL doesn't allow swizzle on scalars)
    {
      std::istringstream phLines(preambleHelpers);
      std::string line;
      std::string fixed;
      while (std::getline(phLines, line)) {
        std::string trimmed = line;
        while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t')) trimmed.erase(0, 1);
        auto parenPos = trimmed.find('(');
        if (parenPos != std::string::npos && parenPos >= 3) {
          auto sigEnd = trimmed.find(')', parenPos);
          if (sigEnd != std::string::npos) {
            std::string sig = trimmed.substr(parenPos, sigEnd - parenPos + 1);
            std::vector<std::string> floatParams;
            size_t fp = 0;
            while ((fp = sig.find("float ", fp)) != std::string::npos) {
              fp += 6;
              size_t end = sig.find_first_of(",)", fp);
              if (end != std::string::npos) {
                std::string pname = sig.substr(fp, end - fp);
                while (!pname.empty() && pname[0] == ' ') pname.erase(0, 1);
                while (!pname.empty() && (pname.back() == ' ' || pname.back() == '\t')) pname.pop_back();
                if (!pname.empty() && pname.find(' ') == std::string::npos)
                  floatParams.push_back(pname);
              }
            }
            if (!floatParams.empty()) {
              std::string funcBody = line + "\n";
              int bd = 0;
              for (char c : line) { if (c == '{') bd++; else if (c == '}') bd--; }
              while (bd == 0 && std::getline(phLines, line)) {
                funcBody += line + "\n";
                for (char c : line) { if (c == '{') bd++; else if (c == '}') bd--; }
              }
              while (bd > 0 && std::getline(phLines, line)) {
                funcBody += line + "\n";
                for (char c : line) { if (c == '{') bd++; else if (c == '}') bd--; }
              }
              for (auto& pname : floatParams) {
                auto stripSuffix = [&](const std::string& suffix) {
                  std::string search = pname + suffix;
                  size_t sp = 0;
                  while ((sp = funcBody.find(search, sp)) != std::string::npos) {
                    char after = (sp + search.size() < funcBody.size()) ? funcBody[sp + search.size()] : ' ';
                    if (after == ' ' || after == ';' || after == ')' || after == ',' || after == '*' || after == '/' || after == '+' || after == '-' || after == '<' || after == '>' || after == '&' || after == '|' || after == '\n' || after == '\t' || after == '}' || after == '{' || after == '=') {
                      funcBody.erase(sp + pname.size(), suffix.size());
                    } else {
                      sp += search.size();
                    }
                  }
                };
                stripSuffix(".x");
                stripSuffix(".y");
                stripSuffix(".z");
                stripSuffix(".w");
              }
              fixed += funcBody;
              continue;
            }
          }
        }
        fixed += line + "\n";
      }
      preambleHelpers = fixed;
    }
  }

  // --- Pass 5: build fragment output ---
  std::ostringstream out;
  out << "#version 330 compatibility\n\n";
  out << "uniform sampler2D Texture;\n";
  out << "uniform vec4 InputSize;\n";
  out << "uniform vec4 OutputSize;\n";
  out << "uniform vec4 TextureSize;\n";
  out << "uniform vec4 SourceSize;\n";
  out << "uniform vec4 OriginalSize;\n";
  out << "uniform vec4 FinalViewportSize;\n";
  out << "uniform int FrameDirection;\n";
  out << "uniform uint FrameCount;\n";
  out << "uniform mat4 MVPMatrix;\n";
  out << "in vec2 vTexCoord;\n";
  out << "out vec4 FragColor;\n\n";

  // Emit additional sampler2D declarations
  std::vector<std::string> emittedSamplerNames;
  {
    std::istringstream samplerScan(resolved);
    std::string samplerLine;
    while (std::getline(samplerScan, samplerLine)) {
      if (samplerLine.find("uniform") == std::string::npos) continue;
      auto spos = samplerLine.find("sampler2D ");
      if (spos == std::string::npos) continue;
      std::string safter = samplerLine.substr(spos + 10);
      std::string sname;
      for (char c : safter) {
        if (c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ')') break;
        sname += c;
      }
      if (sname.empty() || sname == "Texture" || sname == "Source") continue;
      emittedSamplerNames.push_back(sname);
      out << "uniform sampler2D " << sname << ";\n";
    }
  }

  // Map push_constant members to individual uniforms
  {
    std::istringstream ps(pushMembers);
    std::string pl;
    while (std::getline(ps, pl)) {
      while (!pl.empty() && (pl.back() == ';' || pl.back() == ' ' || pl.back() == '\r')) pl.pop_back();
      if (pl.empty()) continue;
      if (pl.find("//") != std::string::npos) continue;
      std::istringstream ms(pl);
      std::string type;
      ms >> type;
      if (type == "float" || type == "int" || type == "uint" || type == "vec2" || type == "vec3" || type == "vec4" ||
          type == "mat2" || type == "mat3" || type == "mat4") {
        std::string rest2;
        std::getline(ms, rest2);
        std::istringstream ns(rest2);
        std::string member;
        while (std::getline(ns, member, ',')) {
          while (!member.empty() && (member[0] == ' ' || member[0] == '\t')) member.erase(0, 1);
          while (!member.empty() && (member.back() == ' ' || member.back() == ';' || member.back() == '\r' || member.back() == '\t'))
            member.pop_back();
          if (!member.empty() && member.find("//") == std::string::npos) {
            bool skip = false;
            for (auto* s : skipNames) { if (member == s) { skip = true; break; } }
            if (!skip) out << "uniform " << type << " " << member << ";\n";
          }
        }
      }
    }
  }

  // Emit UBO members as individual uniforms (skip already-declared names)
  {
    std::set<std::string> uboNames;
    std::istringstream uboStream(uboMembers);
    std::string uboLine;
    while (std::getline(uboStream, uboLine)) {
      while (!uboLine.empty() && (uboLine.back() == ';' || uboLine.back() == ' ' || uboLine.back() == '\r'))
        uboLine.pop_back();
      if (uboLine.empty()) continue;
      if (uboLine.find("//") != std::string::npos) continue;
      std::istringstream ms(uboLine);
      std::string type;
      ms >> type;
      if (type.empty()) continue;
      std::string rest2;
      std::getline(ms, rest2);
      std::istringstream ns(rest2);
      std::string member;
      while (std::getline(ns, member, ',')) {
        while (!member.empty() && (member[0] == ' ' || member[0] == '\t')) member.erase(0, 1);
        while (!member.empty() && (member.back() == ' ' || member.back() == ';' || member.back() == '\r' || member.back() == '\t'))
          member.pop_back();
        if (member.empty() || member.find("//") != std::string::npos) continue;
        std::string baseName = member;
        auto bracketPos = baseName.find('[');
        if (bracketPos != std::string::npos) baseName = baseName.substr(0, bracketPos);
        bool skip = false;
        for (auto* s : skipNames) { if (baseName == s) { skip = true; break; } }
        if (!skip) {
          out << "uniform " << type << " " << member << ";\n";
          uboNames.insert(baseName);
        }
      }
    }
    // Fix self-referencing local variable declarations in preamble helpers
    std::istringstream phFix(preambleHelpers);
    std::string fixedHelpers;
    std::string phLine;
    while (std::getline(phFix, phLine)) {
      std::string trimmed = phLine;
      while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t')) trimmed.erase(0, 1);
      bool isSelfRef = false;
      for (auto& uname : uboNames) {
        std::string pattern1 = "float " + uname + " = " + uname + ";";
        std::string pattern2 = "float " + uname + " = " + uname + " ";
        if (trimmed.find(pattern1) == 0 || trimmed.find(pattern2) == 0) {
          isSelfRef = true;
          break;
        }
      }
      if (isSelfRef) continue;
      fixedHelpers += phLine + "\n";
    }
    preambleHelpers = fixedHelpers;
  }

  // Emit preamble helpers AFTER UBO uniforms so declarations come first
  if (!preambleHelpers.empty()) {
    out << "// --- preamble helpers ---\n";
    out << preambleHelpers;
    out << "// --- end preamble helpers ---\n\n";
  }

  out << "\n";

  // --- Pass 6: transform fragment code ---
  std::istringstream fc(fragmentSection);
  std::string fl;
  while (std::getline(fc, fl)) {
    // Strip layout(set=...) qualifiers
    {
      size_t pos;
      while ((pos = fl.find("layout(set = 0, binding = ")) != std::string::npos) {
        auto end = fl.find(')', pos);
        if (end != std::string::npos) fl.erase(pos, end - pos + 2);
        else break;
      }
      while ((pos = fl.find("layout(set=0,binding=")) != std::string::npos) {
        auto end = fl.find(')', pos);
        if (end != std::string::npos) fl.erase(pos, end - pos + 1);
        else break;
      }
      {
        size_t lp = fl.find("layout(location = ");
        if (lp == std::string::npos) lp = fl.find("layout(location=");
        if (lp != std::string::npos) {
          size_t rp = fl.find(')', lp);
          if (rp != std::string::npos) {
            fl.erase(lp, rp - lp + 1);
            while (!fl.empty() && fl.front() == ' ') fl.erase(0, 1);
          }
        }
      }
      {
        size_t lp = fl.find("layout(set = ");
        if (lp == std::string::npos) lp = fl.find("layout(set=");
        if (lp != std::string::npos) {
          size_t rp = fl.find(')', lp);
          if (rp != std::string::npos) {
            fl.erase(lp, rp - lp + 1);
            while (!fl.empty() && fl.front() == ' ') fl.erase(0, 1);
          }
        }
      }
    }
    // Token-safe Source -> Texture renaming
    {
      size_t pos;
      while ((pos = fl.find("SourceSize")) != std::string::npos) fl.replace(pos, 10, "TextureSize");
      while ((pos = fl.find("OriginalSize")) != std::string::npos) fl.replace(pos, 12, "InputSize");
    }
    {
      size_t pos = 0;
      while ((pos = fl.find("Source", pos)) != std::string::npos) {
        bool wordBoundaryBefore = (pos == 0) || !isalnum(static_cast<unsigned char>(fl[pos - 1])) && fl[pos - 1] != '_';
        size_t end = pos + 6;
        bool wordBoundaryAfter = (end >= fl.size()) || !isalnum(static_cast<unsigned char>(fl[end])) && fl[end] != '_';
        if (wordBoundaryBefore && wordBoundaryAfter) {
          fl.replace(pos, 6, "Texture");
          pos += 7;
        } else {
          pos += 6;
        }
      }
    }
    // Replace pushConstantVarName.X with X
    {
      size_t pos;
      if (!pushStructVarName.empty()) {
        std::string dotPrefix = pushStructVarName + ".";
        while ((pos = fl.find(dotPrefix)) != std::string::npos) fl.replace(pos, dotPrefix.size(), "");
      }
      while ((pos = fl.find("params.")) != std::string::npos) { fl.replace(pos, 7, ""); }
      while ((pos = fl.find("param.")) != std::string::npos) { fl.replace(pos, 6, ""); }
    }
    // Replace global.X
    {
      size_t pos;
      while ((pos = fl.find("global.MVP")) != std::string::npos) fl.replace(pos, 10, "MVPMatrix");
      while ((pos = fl.find("global.OutputSize")) != std::string::npos) fl.replace(pos, 17, "OutputSize");
      while ((pos = fl.find("global.SourceSize")) != std::string::npos) fl.replace(pos, 17, "TextureSize");
      while ((pos = fl.find("global.InputSize")) != std::string::npos) fl.replace(pos, 16, "InputSize");
      while ((pos = fl.find("global.FrameCount")) != std::string::npos) fl.replace(pos, 17, "float(FrameCount)");
      // Catch-all: rename global.TextureSize -> TextureSize (and any other global.X)
      while ((pos = fl.find("global.TextureSize")) != std::string::npos) fl.replace(pos, 17, "TextureSize");
      while ((pos = fl.find("global.")) != std::string::npos) fl.erase(pos, 7);
    }
    // Skip duplicate declarations
    {
      std::string trimmed = fl;
      while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t')) trimmed.erase(0, 1);
      while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == ';' || trimmed.back() == ' '))
        trimmed.pop_back();
      std::string collapsed;
      for (size_t i = 0; i < trimmed.size(); i++) {
        if (trimmed[i] == ' ' || trimmed[i] == '\t') {
          if (!collapsed.empty() && collapsed.back() != ' ') collapsed += ' ';
        } else {
          collapsed += trimmed[i];
        }
      }
      if (collapsed.find("out vec4 FragColor") == 0) continue;
      if (collapsed.find("in vec2 vTexCoord") == 0) continue;
      if (collapsed.find("uniform sampler2D Texture") == 0) continue;
      if (collapsed.find("uniform vec4 OutputSize") == 0) continue;
      if (collapsed.find("uniform vec4 InputSize") == 0) continue;
      if (collapsed.find("uniform vec4 TextureSize") == 0) continue;
      if (collapsed.find("uniform int FrameCount") == 0) continue;
      if (collapsed.find("uniform mat4 MVPMatrix") == 0) continue;
      // Skip additional sampler2D declarations we already emitted above
      if (collapsed.find("uniform sampler2D ") == 0) {
        bool isEmitted = false;
        for (auto& sn : emittedSamplerNames) {
          std::string check = "uniform sampler2D " + sn;
          if (collapsed == check) { isEmitted = true; break; }
        }
        if (isEmitted) continue;
      }
    }
    out << fl << "\n";
  }

  TranslatedShader result;
  result.fragment = out.str();

  // Build vertex shader
  if (!vertexSection.empty()) {
    std::ostringstream vs;
    vs << "#version 330 compatibility\n";
    vs << "layout(location = 0) in vec4 VertexCoord;\n";
    vs << "layout(location = 1) in vec4 Color;\n";
    vs << "layout(location = 2) in vec4 TexCoord;\n";
    std::vector<std::pair<int,std::string>> outVars;
    {
      std::istringstream vsParse(vertexSection);
      std::string vsLine;
      while (std::getline(vsParse, vsLine)) {
        std::string tl = vsLine;
        while (!tl.empty() && (tl[0] == ' ' || tl[0] == '\t')) tl.erase(0, 1);
        if (tl.find("layout(location") != std::string::npos && tl.find("out ") != std::string::npos) {
          auto lp = tl.find("location = ");
          auto rp = tl.find(')', lp);
          int loc = -1;
          if (lp != std::string::npos && rp != std::string::npos) {
            try { loc = std::stoi(tl.substr(lp + 11, rp - lp - 11)); } catch(...) {}
          }
          auto sp = tl.find("out ", tl.find("layout"));
          if (sp != std::string::npos) {
            std::string rest = tl.substr(sp + 4);
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(0, 1);
            while (!rest.empty() && (rest.back() == ';' || rest.back() == ' ' || rest.back() == '\r')) rest.pop_back();
            auto spacePos = rest.find(' ');
            if (spacePos != std::string::npos) {
              std::string varName = rest.substr(spacePos + 1);
              outVars.push_back({loc, varName});
              vs << "out " << rest.substr(0, spacePos) << " " << varName << ";\n";
            }
          }
        }
      }
    }
    // Extract push_constant uniforms for vertex shader
    {
      std::istringstream ps(pushMembers);
      std::string pl;
      while (std::getline(ps, pl)) {
        while (!pl.empty() && (pl.back() == ';' || pl.back() == ' ' || pl.back() == '\r')) pl.pop_back();
        if (pl.empty() || pl.find("//") != std::string::npos) continue;
        std::string trimmed = pl;
        while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t')) trimmed.erase(0, 1);
        if (trimmed.find("SourceSize") == 0 || trimmed.find("OriginalSize") == 0 ||
            trimmed.find("OutputSize") == 0 || trimmed.find("FrameCount") == 0) continue;
        vs << "uniform " << trimmed << ";\n";
      }
    }
    vs << "uniform mat4 MVPMatrix;\n\n";
    // Vertex body
    {
      std::istringstream vsBody(vertexSection);
      std::string vl;
      while (std::getline(vsBody, vl)) {
        std::string tl = vl;
        while (!tl.empty() && (tl[0] == ' ' || tl[0] == '\t')) tl.erase(0, 1);
        if (tl.find("layout(location") != std::string::npos) continue;
        // Position -> VertexCoord
        {
          std::string src = vl;
          std::string dst;
          for (size_t i = 0; i < src.size(); ) {
            if (src.compare(i, 8, "Position") == 0) {
              bool prevIsWord = (i > 0 && (std::isalnum(static_cast<unsigned char>(src[i-1])) || src[i-1] == '_'));
              bool nextIsWord = (i + 8 < src.size() && (std::isalnum(static_cast<unsigned char>(src[i+8])) || src[i+8] == '_'));
              if (!prevIsWord && !nextIsWord) { dst += "VertexCoord"; i += 8; continue; }
            }
            dst += src[i]; i++;
          }
          vl = dst;
        }
        // Strip pushConstantVarName.
        {
          size_t pp;
          if (!pushStructVarName.empty()) {
            std::string dotPrefix = pushStructVarName + ".";
            while ((pp = vl.find(dotPrefix)) != std::string::npos) vl.erase(pp, dotPrefix.size());
          }
          while ((pp = vl.find("params.")) != std::string::npos) vl.erase(pp, 7);
          while ((pp = vl.find("param.")) != std::string::npos) vl.erase(pp, 6);
        }
        // global.X -> mapped names
        {
          size_t pp;
          while ((pp = vl.find("global.MVP")) != std::string::npos) vl.replace(pp, 10, "MVPMatrix");
          while ((pp = vl.find("global.OutputSize")) != std::string::npos) vl.replace(pp, 17, "OutputSize");
          while ((pp = vl.find("global.SourceSize")) != std::string::npos) vl.replace(pp, 17, "TextureSize");
          while ((pp = vl.find("global.InputSize")) != std::string::npos) vl.replace(pp, 16, "InputSize");
          while ((pp = vl.find("global.FrameCount")) != std::string::npos) vl.replace(pp, 17, "float(FrameCount)");
          while ((pp = vl.find("global.TextureSize")) != std::string::npos) vl.replace(pp, 17, "TextureSize");
          while ((pp = vl.find("global.")) != std::string::npos) vl.erase(pp, 7);
        }
        vs << vl << "\n";
      }
    }
    // Fix TexCoord: VAO provides vec4 but slang uses vec2
    {
      std::string vsOut = vs.str();
      std::istringstream fixStream(vsOut);
      std::string fixedVs;
      std::string fixLine;
      while (std::getline(fixStream, fixLine)) {
        std::string tl = fixLine;
        while (!tl.empty() && (tl[0] == ' ' || tl[0] == '\t')) tl.erase(0, 1);
        bool isDecl = (tl.find("in ") == 0 || tl.find("out ") == 0 || tl.find("uniform ") == 0 ||
                        tl.find("layout") == 0 || tl.find("#version") == 0);
        if (!isDecl) {
          size_t pos = 0;
          while ((pos = fixLine.find("TexCoord", pos)) != std::string::npos) {
            bool prevOk = (pos == 0 || (!std::isalnum(static_cast<unsigned char>(fixLine[pos-1])) && fixLine[pos-1] != '_'));
            size_t after = pos + 8;
            bool nextOk = (after >= fixLine.size() || (!std::isalnum(static_cast<unsigned char>(fixLine[after])) && fixLine[after] != '_' && fixLine[after] != '.'));
            if (prevOk && nextOk) {
              fixLine.insert(after, ".xy");
              pos += 11;
            } else {
              pos += 8;
            }
          }
        }
        fixedVs += fixLine + "\n";
      }
      result.vertex = fixedVs;
    }
  } else {
    result.vertex = R"(
#version 330 compatibility
layout(location = 0) in vec4 VertexCoord;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec4 TexCoord;
out vec2 vTexCoord;
uniform mat4 MVPMatrix;
void main() {
  gl_Position = MVPMatrix * VertexCoord;
  vTexCoord = TexCoord.xy;
}
)";
  }

  // Extract #pragma parameter names (from resolved source to get includes)
  {
    std::istringstream s2(resolved);
    std::string ln;
    while (std::getline(s2, ln)) {
      if (ln.rfind("#pragma parameter", 0) == 0) {
        std::istringstream ps(ln);
        std::string tok;
        ps >> tok; ps >> tok;
        ShaderParameter sp;
        if (ps >> tok) {
          if (tok == "bool" || tok == "int" || tok == "float") {
            sp.type = tok;
            ps >> sp.name;
          } else {
            sp.name = tok;
          }
        }
        if (!sp.name.empty()) {
          auto q1 = ln.find('"');
          auto q2 = ln.rfind('"');
          if (q1 != std::string::npos && q2 > q1) {
            sp.description = ln.substr(q1 + 1, q2 - q1 - 1);
          }
          std::string afterDesc;
          if (q2 != std::string::npos) afterDesc = ln.substr(q2 + 1);
          else afterDesc = ln.substr(ln.find(sp.name) + sp.name.size());
          std::istringstream ds(afterDesc);
          float val;
          if (ds >> val) sp.defaultValue = val;
          if (ds >> val) sp.minValue = val;
          if (ds >> val) sp.maxValue = val;
          if (ds >> val) sp.step = val;
          result.parameters.push_back(sp);
        }
      }
    }
  }

  // Extract additional sampler2D declarations (from resolved source to get includes)
  {
    std::istringstream s2(resolved);
    std::string ln;
    while (std::getline(s2, ln)) {
      if (ln.find("uniform") == std::string::npos) continue;
      auto pos = ln.find("sampler2D ");
      if (pos == std::string::npos) continue;
      std::string after = ln.substr(pos + 10);
      std::string sname;
      for (char c : after) {
        if (c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ')') break;
        sname += c;
      }
      if (sname.empty() || sname == "Texture" || sname == "Source") continue;
      int binding = -1;
      auto bp = ln.find("binding");
      if (bp != std::string::npos) {
        auto eq = ln.find('=', bp);
        if (eq != std::string::npos) {
          size_t numStart = eq + 1;
          while (numStart < ln.size() && ln[numStart] == ' ') numStart++;
          std::string numStr;
          while (numStart < ln.size() && std::isdigit(static_cast<unsigned char>(ln[numStart]))) {
            numStr += ln[numStart++];
          }
          try { binding = std::stoi(numStr); } catch (...) {}
        }
      }
      result.samplerBindings.push_back({sname, binding});
    }
  }

  { std::istringstream cnt(uboMembers); std::string tmp; while (std::getline(cnt, tmp)) if (!tmp.empty()) result.uboMemberCount++; }
  result.extractFromFragment = extractFromFragment;

  return result;
}

// ============================================================================
// Test harness
// ============================================================================

void WriteFile(const fs::path& p, const std::string& content) {
  std::ofstream ofs(p);
  ofs << content;
}

int main() {
  fs::path root = "D:\\Monix\\Monix";
  fs::path slangpFile = root / "Shaders" / "presets" / "crt-lottes-with-bezel.slangp";
  fs::path outDir = root / "test_output";
  fs::create_directories(outDir);

  std::cout << "========================================\n";
  std::cout << "  .slangp Preset Translation Test\n";
  std::cout << "========================================\n\n";

  // Step 1: Parse .slangp
  std::cout << "[PARSE] " << slangpFile.filename().string() << "\n";
  SlangpPreset preset = ParseSlangp(slangpFile);
  if (!preset.valid) {
    std::cout << "  FAIL: " << preset.error << "\n";
    return 1;
  }
  std::cout << "  OK: " << preset.passes.size() << " passes\n\n";

  // Build alias map
  std::unordered_map<std::string, size_t> aliasMap;
  for (size_t i = 0; i < preset.passes.size(); ++i) {
    if (!preset.passes[i].alias.empty()) {
      aliasMap[preset.passes[i].alias] = i;
      std::cout << "  ALIAS: \"" << preset.passes[i].alias << "\" -> pass " << i << "\n";
    }
  }
  std::cout << "\n";

  // Step 2: Translate each pass
  bool allOk = true;
  for (size_t i = 0; i < preset.passes.size(); ++i) {
    const auto& pi = preset.passes[i];
    std::string passName = pi.alias.empty() ? ("pass" + std::to_string(i)) : pi.alias;

    std::cout << "========================================\n";
    std::cout << "  PASS " << i << ": " << passName << "\n";
    std::cout << "========================================\n";
    std::cout << "  File:     " << pi.shaderPath.string() << "\n";
    std::cout << "  Resolved: " << fs::absolute(pi.shaderPath).string() << "\n";
    std::cout << "  Exists:   " << (fs::exists(pi.shaderPath) ? "YES" : "NO") << "\n";
    std::cout << "  Filter:   " << (pi.filterLinear ? "linear" : "nearest") << "\n";
    std::cout << "  Scale:    X=" << pi.scaleTypeX << " Y=" << pi.scaleTypeY << "\n";

    if (!fs::exists(pi.shaderPath)) {
      std::cout << "  FAIL: File not found\n";
      allOk = false;
      continue;
    }

    std::string raw = ReadTextFile(pi.shaderPath);
    std::cout << "  Size:     " << raw.size() << " bytes\n";

    TranslatedShader ts = TranslateSlangToGlsl(raw, pi.shaderPath);

    std::cout << "  Vertex:   " << (ts.vertex.empty() ? "EMPTY!" : std::to_string(ts.vertex.size()) + " bytes") << "\n";
    std::cout << "  Fragment: " << (ts.fragment.empty() ? "EMPTY!" : std::to_string(ts.fragment.size()) + " bytes") << "\n";
    std::cout << "  Params:   " << ts.parameters.size() << "\n";
    for (auto& sp : ts.parameters) {
      std::cout << "    - " << sp.name << " default=" << sp.defaultValue
                << " range=[" << sp.minValue << ", " << sp.maxValue << "]\n";
    }
    std::cout << "  Samplers: " << ts.samplerBindings.size() << "\n";
    for (auto& [sname, binding] : ts.samplerBindings) {
      std::cout << "    - " << sname << " (binding=" << binding << ")\n";
    }
    // Show external textures from preset
    if (!pi.externalTextures.empty()) {
      std::cout << "  External textures: " << pi.externalTextures.size() << "\n";
      for (auto& et : pi.externalTextures) {
        std::cout << "    - " << et.samplerName << " -> " << et.filePath << "\n";
      }
    }

    if (ts.vertex.empty()) {
      std::cout << "  FAIL: Empty vertex shader\n";
      allOk = false;
    }
    if (ts.fragment.empty()) {
      std::cout << "  FAIL: Empty fragment shader\n";
      allOk = false;
    }

    // Check for common errors in output
    std::string fragLower = ts.fragment;
    std::string vertLower = ts.vertex;

    // Check for unresolved references
    auto checkUnresolved = [&](const std::string& code, const std::string& stage) {
      std::vector<std::string> issues;
      if (code.find("global.") != std::string::npos) issues.push_back("unresolved global.");
      if (code.find("params.") != std::string::npos) issues.push_back("unresolved params.");
      if (code.find("param.") != std::string::npos) issues.push_back("unresolved param.");
      if (code.find("layout(push_constant)") != std::string::npos) issues.push_back("unresolved push_constant");
      if (code.find("layout(std140") != std::string::npos) issues.push_back("unresolved UBO");
      if (code.find("layout(set") != std::string::npos) issues.push_back("unresolved layout(set=...)");
      if (code.find("#pragma stage") != std::string::npos) issues.push_back("unresolved #pragma stage");
      if (code.find("#pragma parameter") != std::string::npos) issues.push_back("unresolved #pragma parameter");
      if (code.find("#pragma format") != std::string::npos) issues.push_back("unresolved #pragma format");
      for (auto& iss : issues) {
        std::cout << "  WARNING [" << stage << "]: " << iss << "\n";
      }
    };
    checkUnresolved(ts.vertex, "vert");
    checkUnresolved(ts.fragment, "frag");

    // Check that sampler uniforms are declared in fragment
    for (auto& [sname, binding] : ts.samplerBindings) {
      std::string decl = "uniform sampler2D " + sname + ";";
      if (ts.fragment.find(decl) == std::string::npos) {
        std::cout << "  WARNING [frag]: missing declaration for sampler " << sname << "\n";
      }
    }

    // Write output files
    WriteFile(outDir / ("pass" + std::to_string(i) + "_vertex.glsl"), ts.vertex);
    WriteFile(outDir / ("pass" + std::to_string(i) + "_fragment.glsl"), ts.fragment);
    std::cout << "  Output:   test_output/pass" << i << "_vertex.glsl\n";
    std::cout << "            test_output/pass" << i << "_fragment.glsl\n";

    std::cout << "  UBO members found: " << ts.uboMemberCount << (ts.extractFromFragment ? " (from fragmentSection)" : " (from preamble)") << "\n";
    std::cout << "  STATUS:   " << (!ts.vertex.empty() && !ts.fragment.empty() ? "OK" : "FAIL") << "\n\n";

    if (ts.vertex.empty() || ts.fragment.empty()) allOk = false;
  }

  // Step 3: Verify pass chaining
  std::cout << "========================================\n";
  std::cout << "  PASS CHAINING VERIFICATION\n";
  std::cout << "========================================\n";
  for (size_t i = 0; i < preset.passes.size(); ++i) {
    const auto& pi = preset.passes[i];
    std::string passName = pi.alias.empty() ? ("pass" + std::to_string(i)) : pi.alias;
    std::string nextName = (i + 1 < preset.passes.size()) ?
      (preset.passes[i+1].alias.empty() ? ("pass" + std::to_string(i+1)) : preset.passes[i+1].alias) :
      "SCREEN";

    // Read the translated fragment to check sampler bindings
    std::ifstream fragFile(outDir / ("pass" + std::to_string(i) + "_fragment.glsl"));
    std::string fragContent((std::istreambuf_iterator<char>(fragFile)), std::istreambuf_iterator<char>());

    std::cout << "  Pass " << i << " (" << passName << ") -> " << nextName << ":\n";

    // Check what samplers this pass reads
    if (fragContent.find("uniform sampler2D Texture") != std::string::npos) {
      std::cout << "    Reads: Texture (input from " << (i > 0 ? "pass" + std::to_string(i-1) : "scene") << ")\n";
    }
    // Check additional samplers
    std::istringstream fragLines(fragContent);
    std::string fl2;
    while (std::getline(fragLines, fl2)) {
      if (fl2.find("uniform sampler2D ") == std::string::npos) continue;
      auto spos = fl2.find("sampler2D ") + 10;
      std::string sname;
      for (size_t c = spos; c < fl2.size(); ++c) {
        if (fl2[c] == ';' || fl2[c] == ' ') break;
        sname += fl2[c];
      }
      if (sname == "Texture" || sname.empty()) continue;

      auto aliasIt = aliasMap.find(sname);
      if (aliasIt != aliasMap.end()) {
        std::cout << "    Reads: " << sname << " (alias -> pass " << aliasIt->second << ": "
                  << (preset.passes[aliasIt->second].alias.empty() ? "pass" + std::to_string(aliasIt->second) : preset.passes[aliasIt->second].alias) << ")\n";
      } else if (sname == "ORIG_LINEARIZED") {
        std::cout << "    Reads: " << sname << " (builtin -> original input)\n";
      } else {
        std::cout << "    Reads: " << sname << " (UNRESOLVED ALIAS!)\n";
        allOk = false;
      }
    }

    // Check if this pass outputs (has FBO)
    if (i < preset.passes.size() - 1) {
      std::cout << "    Writes: FBO texture (to pass " << (i+1) << ")\n";
    } else {
      std::cout << "    Writes: SCREEN (final output)\n";
    }
  }
  std::cout << "\n";

  // Summary
  std::cout << "========================================\n";
  std::cout << "  SUMMARY\n";
  std::cout << "========================================\n";
  std::cout << "  Preset:    " << preset.name << "\n";
  std::cout << "  Passes:    " << preset.passes.size() << "\n";
  std::cout << "  Aliases:   " << aliasMap.size() << "\n";
  for (auto& [name, idx] : aliasMap) {
    std::cout << "    " << name << " -> pass " << idx << "\n";
  }
  std::cout << "  Result:    " << (allOk ? "ALL PASSES OK" : "SOME PASSES FAILED") << "\n";
  std::cout << "  Output:    " << fs::absolute(outDir).string() << "\n";
  std::cout << "========================================\n";

  return allOk ? 0 : 1;
}
