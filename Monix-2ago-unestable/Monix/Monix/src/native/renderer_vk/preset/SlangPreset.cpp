#include "SlangPreset.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>

namespace monix::renderer_vk {
namespace {

std::string lowerCopy(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

}  // namespace

std::optional<std::string> PresetAst::get(std::string_view key) const {
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (it->key == key) {
            return it->value;
        }
    }
    return std::nullopt;
}

std::vector<PresetAstEntry> PresetAst::findPrefix(std::string_view prefix) const {
    std::vector<PresetAstEntry> result;
    for (const auto& entry : entries) {
        if (entry.key.rfind(prefix.data(), 0) == 0) {
            result.push_back(entry);
        }
    }
    return result;
}

const char* toString(ScaleType type) {
    switch (type) {
    case ScaleType::Source: return "source";
    case ScaleType::Viewport: return "viewport";
    case ScaleType::Absolute: return "absolute";
    case ScaleType::Original: return "original";
    }
    return "source";
}

const char* toString(WrapMode mode) {
    switch (mode) {
    case WrapMode::ClampToBorder: return "clamp_to_border";
    case WrapMode::ClampToEdge: return "clamp_to_edge";
    case WrapMode::Repeat: return "repeat";
    case WrapMode::MirroredRepeat: return "mirrored_repeat";
    }
    return "clamp_to_border";
}

ScaleType parseScaleType(std::string_view text) {
    const auto value = lowerCopy(text);
    if (value == "viewport") return ScaleType::Viewport;
    if (value == "absolute") return ScaleType::Absolute;
    if (value == "original") return ScaleType::Original;
    return ScaleType::Source;
}

WrapMode parseWrapMode(std::string_view text) {
    const auto value = lowerCopy(text);
    if (value == "repeat") return WrapMode::Repeat;
    if (value == "mirrored_repeat" || value == "mirror" || value == "mirrored") return WrapMode::MirroredRepeat;
    if (value == "clamp_to_edge" || value == "clamp") return WrapMode::ClampToEdge;
    return WrapMode::ClampToBorder;
}

bool parseBool(std::string_view text, bool defaultValue) {
    const auto value = lowerCopy(text);
    if (value == "true" || value == "1" || value == "yes" || value == "on") return true;
    if (value == "false" || value == "0" || value == "no" || value == "off") return false;
    return defaultValue;
}

float parseFloat(std::string_view text, float defaultValue) {
    float value = defaultValue;
    const char* first = text.data();
    const char* last = first + text.size();
    auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} ? value : defaultValue;
}

}  // namespace monix::renderer_vk
