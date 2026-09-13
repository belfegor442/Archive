#include "ShaderWorkspaceConfig.hpp"

#include "../core/FileSystem.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace monix::renderer_vk {

ShaderWorkspaceConfig ShaderWorkspaceConfig::defaultConfig() {
    ShaderWorkspaceConfig cfg;
    cfg.lastLanguage = "GLSL";
    return cfg;
}

static bool isRelativePathSafe(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] == '/' || path[0] == '\\') return false;
    if (path.find("..") != std::string::npos) return false;
    if (path.find(':') != std::string::npos) return false;
    return true;
}

bool ShaderWorkspaceConfig::validatePaths() const {
    for (const auto& fav : favorites) {
        if (!isRelativePathSafe(fav)) return false;
    }
    if (!lastSelected.empty() && !isRelativePathSafe(lastSelected)) return false;
    return true;
}

bool ShaderWorkspaceConfig::isValid() const {
    if (!validatePaths()) return false;
    if (!lastLanguage.empty() &&
        lastLanguage != "GLSL" &&
        lastLanguage != "SLANG" &&
        lastLanguage != "SLANGP") {
        return false;
    }
    return true;
}

static std::string trimString(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n\"");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n\"");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> parseJsonStringArray(const std::string& content, const std::string& key) {
    std::vector<std::string> result;
    size_t keyPos = content.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return result;

    size_t bracketStart = content.find('[', keyPos);
    if (bracketStart == std::string::npos) return result;
    size_t bracketEnd = content.find(']', bracketStart);
    if (bracketEnd == std::string::npos) return result;

    std::string arrayContent = content.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
    size_t pos = 0;
    while (pos < arrayContent.size()) {
        size_t quoteStart = arrayContent.find('"', pos);
        if (quoteStart == std::string::npos) break;
        size_t quoteEnd = arrayContent.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) break;
        result.push_back(arrayContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
        pos = quoteEnd + 1;
    }
    return result;
}

static std::string parseJsonString(const std::string& content, const std::string& key) {
    size_t keyPos = content.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "";
    size_t colonPos = content.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    size_t quoteStart = content.find('"', colonPos);
    if (quoteStart == std::string::npos) return "";
    size_t quoteEnd = content.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";
    return content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

ShaderWorkspaceConfig ShaderWorkspaceConfig::load(const std::filesystem::path& configPath) {
    std::error_code ec;
    if (!std::filesystem::exists(configPath, ec) || ec) {
        return defaultConfig();
    }

    auto readResult = FileSystem::readText(configPath);
    if (!readResult) {
        return defaultConfig();
    }

    const std::string& content = readResult.value();
    if (content.empty()) {
        return defaultConfig();
    }

    ShaderWorkspaceConfig config;
    config.favorites = parseJsonStringArray(content, "favorites");
    config.lastSelected = parseJsonString(content, "lastSelected");
    config.lastLanguage = parseJsonString(content, "lastLanguage");

    if (config.lastLanguage.empty()) {
        config.lastLanguage = "GLSL";
    }

    if (!config.validatePaths()) {
        std::fprintf(stderr, "[ShaderWorkspaceConfig] Invalid paths in config, using defaults\n");
        return defaultConfig();
    }

    return config;
}

bool ShaderWorkspaceConfig::save(const std::filesystem::path& configPath) const {
    if (!validatePaths()) {
        std::fprintf(stderr, "[ShaderWorkspaceConfig] Refusing to save config with invalid paths\n");
        return false;
    }

    std::ostringstream json;
    json << "{\n";

    json << "  \"favorites\": [";
    for (size_t i = 0; i < favorites.size(); ++i) {
        if (i > 0) json << ", ";
        json << "\n    \"" << favorites[i] << "\"";
    }
    if (!favorites.empty()) json << "\n  ";
    json << "],\n";

    json << "  \"lastSelected\": \"" << lastSelected << "\",\n";
    json << "  \"lastLanguage\": \"" << lastLanguage << "\"\n";
    json << "}\n";

    std::string tmpPath = configPath.string() + ".tmp";
    {
        std::ofstream ofs(tmpPath, std::ios::binary);
        if (!ofs.is_open()) {
            std::fprintf(stderr, "[ShaderWorkspaceConfig] Failed to open temp file for writing: %s\n", tmpPath.c_str());
            return false;
        }
        ofs << json.str();
        ofs.flush();
        if (!ofs.good()) {
            std::fprintf(stderr, "[ShaderWorkspaceConfig] Failed to write temp file\n");
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, configPath, ec);
    if (ec) {
        std::fprintf(stderr, "[ShaderWorkspaceConfig] Atomic rename failed: %s\n", ec.message().c_str());
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    return true;
}

}  // namespace monix::renderer_vk
