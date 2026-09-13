#include "SlangPresetParser.hpp"

#include "../core/FileSystem.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace monix::renderer_vk {
namespace {

#ifndef MONIX_ANON_TRIM
#define MONIX_ANON_TRIM
std::string trim(std::string_view text) {
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return std::string(text.substr(first, last - first));
}
#endif

std::string stripQuotes(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string stripComment(std::string_view line) {
    bool inQuote = false;
    char quote = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if ((c == '"' || c == '\'') && (i == 0 || line[i - 1] != '\\')) {
            if (!inQuote) {
                inQuote = true;
                quote = c;
            } else if (quote == c) {
                inQuote = false;
            }
        }
        if (!inQuote && c == '#') {
            return std::string(line.substr(0, i));
        }
    }
    return std::string(line);
}

int parseIntValue(const PresetAst& ast, std::string_view key, int defaultValue) {
    auto value = ast.get(key);
    if (!value) return defaultValue;
    int parsed = defaultValue;
    auto result = std::from_chars(value->data(), value->data() + value->size(), parsed);
    return result.ec == std::errc{} ? parsed : defaultValue;
}

std::optional<int> suffixInt(std::string_view key, std::string_view prefix) {
    if (key.rfind(prefix.data(), 0) != 0) return std::nullopt;
    const auto tail = key.substr(prefix.size());
    if (tail.empty()) return std::nullopt;
    int value = 0;
    auto result = std::from_chars(tail.data(), tail.data() + tail.size(), value);
    if (result.ec != std::errc{} || result.ptr != tail.data() + tail.size()) return std::nullopt;
    return value;
}

std::string indexedKey(std::string_view prefix, int index) {
    return std::string(prefix) + std::to_string(index);
}

}  // namespace

Result<PresetAst> SlangPresetParser::parseFile(const std::filesystem::path& path) const {
    std::unordered_set<std::string> visited;
    return parseFileInternal(path, visited);
}

Result<PresetAst> SlangPresetParser::parseFileInternal(
    const std::filesystem::path& path,
    std::unordered_set<std::string>& visited) const {

    auto normalizedPath = FileSystem::normalize(path);
    auto pathStr = normalizedPath.string();

    if (visited.count(pathStr)) {
        return Status::failure("Circular #reference detected: " + pathStr);
    }
    visited.insert(pathStr);

    auto text = FileSystem::readText(path);
    if (!text) {
        return text.status();
    }

    PresetAst ast;
    ast.path = normalizedPath;
    ast.baseDirectory = ast.path.parent_path();

    // Collect referenced entries first (base entries)
    std::vector<PresetAstEntry> referencedEntries;

    std::istringstream input(text.value());
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        auto trimmedLine = trim(std::string_view(line));

        // Check for #reference directive BEFORE stripping comments
        // (since '#' is also the comment character)
        if (trimmedLine.rfind("#reference", 0) == 0) {
            auto rest = trim(trimmedLine.substr(10));
            auto refPath = stripQuotes(rest);
            if (refPath.empty()) {
                ast.diagnostics.warning("Empty #reference at line " + std::to_string(lineNumber));
                continue;
            }

            auto refFullPath = FileSystem::normalize(ast.baseDirectory / refPath);
            auto refAst = parseFileInternal(refFullPath, visited);
            if (!refAst) {
                return refAst.status();
            }
            for (auto& entry : refAst.value().entries) {
                referencedEntries.push_back(std::move(entry));
            }
            continue;
        }

        auto clean = trim(stripComment(line));
        if (clean.empty()) continue;

        const auto equal = clean.find('=');
        if (equal == std::string::npos) {
            ast.diagnostics.warning("Ignoring malformed preset line " + std::to_string(lineNumber));
            continue;
        }
        auto key = trim(std::string_view(clean).substr(0, equal));
        auto value = stripQuotes(std::string_view(clean).substr(equal + 1).data());
        value = stripQuotes(value);
        if (!key.empty()) {
            ast.entries.push_back(PresetAstEntry{std::move(key), std::move(value), lineNumber});
        }
    }

    // Prepend referenced entries so current entries can override them
    std::vector<PresetAstEntry> merged;
    merged.reserve(referencedEntries.size() + ast.entries.size());
    merged.insert(merged.end(), std::make_move_iterator(referencedEntries.begin()),
                  std::make_move_iterator(referencedEntries.end()));
    merged.insert(merged.end(), std::make_move_iterator(ast.entries.begin()),
                  std::make_move_iterator(ast.entries.end()));
    ast.entries = std::move(merged);

    if (!ast.get("shaders")) {
        return Status::failure("Preset does not define shaders: " + path.string());
    }
    return ast;
}

Result<PresetIr> SlangPresetParser::buildIr(const PresetAst& ast) const {
    PresetIr ir;
    ir.path = ast.path;
    ir.baseDirectory = ast.baseDirectory;
    ir.name = ast.path.stem().string();

    const int shaderCount = parseIntValue(ast, "shaders", 0);
    const int textureCount = parseIntValue(ast, "textures", 0);
    if (shaderCount <= 0) {
        return Status::failure("Preset has no shader passes: " + ast.path.string());
    }

    for (int i = 0; i < shaderCount; ++i) {
        const auto shader = ast.get(indexedKey("shader", i));
        if (!shader || shader->empty()) {
            return Status::failure("Missing shader" + std::to_string(i) + " in " + ast.path.string());
        }

        PresetPassIr pass;
        pass.index = i;
        pass.shaderPathRaw = *shader;
        pass.shaderPath = FileSystem::normalize(ast.baseDirectory / *shader);
        if (auto alias = ast.get(indexedKey("alias", i))) pass.alias = *alias;
        if (auto value = ast.get(indexedKey("filter_linear", i))) pass.filterLinear = parseBool(*value, pass.filterLinear);
        if (auto value = ast.get(indexedKey("wrap_mode", i))) pass.wrapMode = parseWrapMode(*value);
        if (auto value = ast.get(indexedKey("mipmap_input", i))) pass.mipmapInput = parseBool(*value, pass.mipmapInput);
        if (auto value = ast.get(indexedKey("float_framebuffer", i))) pass.floatFramebuffer = parseBool(*value, pass.floatFramebuffer);
        if (auto value = ast.get(indexedKey("srgb_framebuffer", i))) pass.srgbFramebuffer = parseBool(*value, pass.srgbFramebuffer);
        if (auto value = ast.get(indexedKey("framebuffer_feedback", i))) pass.framebufferFeedback = parseBool(*value, pass.framebufferFeedback);

        if (auto value = ast.get(indexedKey("scale_type", i))) {
            pass.scale.typeX = parseScaleType(*value);
            pass.scale.typeY = pass.scale.typeX;
        }
        if (auto value = ast.get(indexedKey("scale", i))) {
            pass.scale.scaleX = parseFloat(*value, 1.0f);
            pass.scale.scaleY = pass.scale.scaleX;
        }
        if (auto value = ast.get(indexedKey("scale_type_x", i))) pass.scale.typeX = parseScaleType(*value);
        if (auto value = ast.get(indexedKey("scale_type_y", i))) pass.scale.typeY = parseScaleType(*value);
        if (auto value = ast.get(indexedKey("scale_x", i))) pass.scale.scaleX = parseFloat(*value, pass.scale.scaleX);
        if (auto value = ast.get(indexedKey("scale_y", i))) pass.scale.scaleY = parseFloat(*value, pass.scale.scaleY);

        ir.passes.push_back(std::move(pass));
    }

    std::unordered_map<int, std::string> aliasByFlatIndex;
    for (const auto& entry : ast.entries) {
        if (auto index = suffixInt(entry.key, "alias")) {
            aliasByFlatIndex[*index] = entry.value;
        }
    }

    for (int i = 0; i < textureCount; ++i) {
        const auto texture = ast.get(indexedKey("texture", i));
        if (!texture || texture->empty()) {
            ir.diagnostics.warning("Missing texture" + std::to_string(i) + " entry");
            continue;
        }
        PresetTextureIr external;
        external.textureIndex = i;
        external.aliasIndex = shaderCount + i;
        external.pathRaw = *texture;
        external.path = FileSystem::normalize(ast.baseDirectory / *texture);
        if (auto alias = aliasByFlatIndex.find(external.aliasIndex); alias != aliasByFlatIndex.end()) {
            external.alias = alias->second;
        }
        if (auto value = ast.get(indexedKey("filter_linear", external.aliasIndex))) {
            external.filterLinear = parseBool(*value, external.filterLinear);
        }
        if (auto value = ast.get(indexedKey("wrap_mode", external.aliasIndex))) {
            external.wrapMode = parseWrapMode(*value);
        }
        if (auto value = ast.get(indexedKey("mipmap_input", external.aliasIndex))) {
            external.mipmap = parseBool(*value, external.mipmap);
        }
        ir.externalTextures.push_back(std::move(external));
    }

    if (auto parameters = ast.get("parameters")) {
        std::string list = *parameters;
        std::replace(list.begin(), list.end(), ';', ',');
        std::istringstream names(list);
        std::string name;
        while (std::getline(names, name, ',')) {
            name = stripQuotes(trim(name));
            if (name.empty()) continue;
            if (auto value = ast.get(name)) {
                ir.parameterOverrides.push_back(PresetParameterOverride{name, *value});
            }
        }
    }

    return ir;
}

}  // namespace monix::renderer_vk
