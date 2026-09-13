#include "ParameterExtractor.hpp"

#include <charconv>
#include <cctype>
#include <sstream>

namespace monix::renderer_vk {
namespace {

std::string trim(std::string_view text) {
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return std::string(text.substr(first, last - first));
}

float parseFloat(std::string_view text) {
    float value = 0.0f;
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} ? value : 0.0f;
}

bool nextToken(std::string_view line, size_t& pos, std::string& out) {
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
    if (pos >= line.size()) return false;
    if (line[pos] == '"') {
        ++pos;
        const size_t begin = pos;
        while (pos < line.size() && line[pos] != '"') ++pos;
        out = std::string(line.substr(begin, pos - begin));
        if (pos < line.size()) ++pos;
        return true;
    }
    const size_t begin = pos;
    while (pos < line.size() && !std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
    out = std::string(line.substr(begin, pos - begin));
    return true;
}

ParameterType parseTypeOrDefault(const std::string& token, bool& consumed) {
    consumed = true;
    if (token == "float") return ParameterType::Float;
    if (token == "int") return ParameterType::Int;
    if (token == "bool") return ParameterType::Bool;
    consumed = false;
    return ParameterType::Float;
}

}  // namespace

std::vector<ShaderParameter> ParameterExtractor::extract(const std::string& source) const {
    std::vector<ShaderParameter> parameters;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
        const auto marker = line.find("#pragma parameter");
        if (marker == std::string::npos) continue;

        std::string rest = trim(std::string_view(line).substr(marker + 17));
        size_t pos = 0;
        std::string token;
        if (!nextToken(rest, pos, token)) continue;

        bool typeConsumed = false;
        ParameterType type = parseTypeOrDefault(token, typeConsumed);
        std::string name;
        if (typeConsumed) {
            if (!nextToken(rest, pos, name)) continue;
        } else {
            name = token;
        }

        std::string description;
        std::string def;
        std::string min;
        std::string max;
        std::string step;
        if (!nextToken(rest, pos, description)) continue;
        if (!nextToken(rest, pos, def)) continue;
        if (!nextToken(rest, pos, min)) continue;
        if (!nextToken(rest, pos, max)) continue;
        if (!nextToken(rest, pos, step)) continue;

        parameters.push_back(ShaderParameter{
            name,
            description,
            type,
            parseFloat(def),
            parseFloat(min),
            parseFloat(max),
            parseFloat(step)
        });
    }
    return parameters;
}

}  // namespace monix::renderer_vk
