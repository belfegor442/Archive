#include "ShaderReflection.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <unordered_map>

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

uint32_t alignTo(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

uint32_t std140Alignment(std::string_view type) {
    if (type == "float" || type == "int" || type == "uint" || type == "bool") return 4;
    if (type == "vec2" || type == "ivec2" || type == "uvec2") return 8;
    if (type == "vec3" || type == "vec4" || type == "ivec3" || type == "ivec4" ||
        type == "uvec3" || type == "uvec4" || type == "mat4") return 16;
    return 16;
}

uint32_t std140Size(std::string_view type) {
    if (type == "float" || type == "int" || type == "uint" || type == "bool") return 4;
    if (type == "vec2" || type == "ivec2" || type == "uvec2") return 8;
    if (type == "vec3" || type == "vec4" || type == "ivec3" || type == "ivec4" ||
        type == "uvec3" || type == "uvec4") return 16;
    if (type == "mat4") return 64;
    return 16;
}

bool parseLayoutSetBinding(const std::string& line, uint32_t& set, uint32_t& binding) {
    const auto layout = line.find("layout");
    if (layout == std::string::npos) return false;
    const auto open = line.find('(', layout);
    const auto close = line.find(')', open == std::string::npos ? layout : open);
    if (open == std::string::npos || close == std::string::npos) return false;
    const auto body = line.substr(open + 1, close - open - 1);
    auto readNumber = [&](const char* key, uint32_t& out) {
        const auto pos = body.find(key);
        if (pos == std::string::npos) return;
        const auto equal = body.find('=', pos);
        if (equal == std::string::npos) return;
        size_t begin = equal + 1;
        while (begin < body.size() && std::isspace(static_cast<unsigned char>(body[begin]))) ++begin;
        size_t end = begin;
        while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end]))) ++end;
        uint32_t value = out;
        auto result = std::from_chars(body.data() + begin, body.data() + end, value);
        if (result.ec == std::errc{}) out = value;
    };
    readNumber("set", set);
    readNumber("binding", binding);
    return true;
}

std::string declarationName(std::string_view declaration) {
    auto text = trim(declaration);
    while (!text.empty() && (text.back() == ';' || text.back() == '}')) text.pop_back();
    const auto lastSpace = text.find_last_of(" \t");
    if (lastSpace == std::string::npos) return {};
    auto name = trim(std::string_view(text).substr(lastSpace + 1));
    const auto array = name.find('[');
    if (array != std::string::npos) name.resize(array);
    return name;
}

std::string declarationType(std::string_view declaration) {
    auto text = trim(declaration);
    const auto firstSpace = text.find_first_of(" \t");
    if (firstSpace == std::string::npos) return {};
    return trim(text.substr(0, firstSpace));
}

void appendUniformMember(UniformBlockReflection& block, std::string_view line, uint32_t& cursor) {
    auto type = declarationType(line);
    auto name = declarationName(line);
    if (type.empty() || name.empty()) return;
    const auto align = std140Alignment(type);
    cursor = alignTo(cursor, align);
    const auto size = std140Size(type);
    block.members.push_back(UniformMemberReflection{name, type, cursor, size});
    cursor += size;
}

}  // namespace

const UniformMemberReflection* ShaderReflection::findUniform(std::string_view name) const {
    for (const auto& block : uniformBlocks) {
        for (const auto& member : block.members) {
            if (member.name == name) return &member;
        }
    }
    for (const auto& push : pushConstants) {
        for (const auto& member : push.members) {
            if (member.name == name) return &member;
        }
    }
    return nullptr;
}

const SamplerReflection* ShaderReflection::findSampler(std::string_view name) const {
    for (const auto& sampler : samplers) {
        if (sampler.name == name) return &sampler;
    }
    return nullptr;
}

Result<ShaderReflection> ShaderReflectionParser::fromSourceLayout(const std::string& source) const {
    ShaderReflection reflection;
    reflection.fallbackSourceReflection = true;

    std::istringstream input(source);
    std::string line;
    bool inBlock = false;
    UniformBlockReflection currentBlock;
    uint32_t blockCursor = 0;
    bool inPush = false;
    PushConstantReflection currentPush;
    uint32_t pushCursor = 0;
    std::string pendingUniformLine;

    // Track struct definitions: name → (members, size)
    struct StructDef { std::vector<UniformMemberReflection> members; uint32_t size = 0; };
    std::unordered_map<std::string, StructDef> structDefs;
    bool inStruct = false;
    std::string currentStructName;
    StructDef currentStructDef;
    uint32_t structCursor = 0;

    while (std::getline(input, line)) {
        const auto clean = trim(line);
        if (clean.empty()) continue;

        // Track "struct Name {" definitions — but NOT function bodies like "float foo(vec2 co) {"
        if (!inBlock && !inPush && !inStruct && clean.find("struct ") != std::string::npos) {
            // Must NOT have '(' before '{' — that would be a function
            bool hasParen = clean.find('(') != std::string::npos;
            if (!hasParen) {
                // Extract name between "struct " and "{" (or end of line)
                auto structPos = clean.find("struct ") + 7;
                auto rest = clean.substr(structPos);
                // Remove braces and semicolons from end
                while (!rest.empty() && (rest.back() == '{' || rest.back() == ';' || rest.back() == '}' || rest.back() == ' '))
                    rest.pop_back();
                auto spacePos = rest.find_last_of(" \t");
                currentStructName = (spacePos != std::string::npos)
                    ? std::string(rest.substr(spacePos + 1))
                    : std::string(rest);
                currentStructDef = {};
                structCursor = 0;
                if (clean.find('{') != std::string::npos) {
                    inStruct = true;
                } else {
                    inStruct = true;
                }
                continue;
            }
        }
        if (inStruct) {
            if (clean.find('}') != std::string::npos) {
                currentStructDef.size = alignTo(structCursor, 16);
                structDefs[currentStructName] = currentStructDef;
                inStruct = false;
                currentStructName.clear();
                continue;
            }
            UniformBlockReflection temp;
            appendUniformMember(temp, clean, structCursor);
            if (!temp.members.empty()) {
                currentStructDef.members.push_back(temp.members.back());
            }
            continue;
        }

        if (!inBlock && !inPush && clean.find("uniform sampler") != std::string::npos) {
            uint32_t set = 0;
            uint32_t binding = 0;
            parseLayoutSetBinding(clean, set, binding);
            const auto name = declarationName(clean);
            if (!name.empty()) {
                reflection.samplers.push_back(SamplerReflection{name, set, binding});
                reflection.descriptors.push_back(DescriptorBindingReflection{
                    name, set, binding, ReflectedResourceKind::CombinedImageSampler});
            }
            continue;
        }

        if (!inBlock && !inPush && clean.find("layout(push_constant)") != std::string::npos) {
            inPush = true;
            currentPush = {};
            currentPush.name = "Push";
            pushCursor = 0;
            continue;
        }

        if (!inBlock && !inPush && clean.find("uniform") != std::string::npos && clean.find('{') != std::string::npos) {
            uint32_t set = 0;
            uint32_t binding = 0;
            parseLayoutSetBinding(clean, set, binding);
            inBlock = true;
            currentBlock = {};
            currentBlock.name = declarationName(clean.substr(0, clean.find('{')));
            currentBlock.set = set;
            currentBlock.binding = binding;
            blockCursor = 0;
            continue;
        }

        // Handle "uniform StructType instanceName;" — look up struct definition
        if (!inBlock && !inPush && clean.find("uniform") != std::string::npos
            && clean.find('{') == std::string::npos
            && clean.find("sampler") == std::string::npos
            && clean.find("layout") == std::string::npos) {
            // Parse "uniform TypeName instanceName;"
            auto uniformPos = clean.find("uniform");
            std::string rest = clean.substr(uniformPos + 8);
            // Remove semicolons
            while (!rest.empty() && (rest.back() == ';' || rest.back() == ' '))
                rest.pop_back();
            std::istringstream ustream(rest);
            std::string typeName, instanceName;
            ustream >> typeName >> instanceName;
            auto it = structDefs.find(typeName);
            if (it != structDefs.end() && !instanceName.empty()) {
                UniformBlockReflection block;
                block.name = typeName;
                block.instanceName = instanceName;
                block.size = it->second.size;
                block.members = it->second.members;
                block.set = 0;
                block.binding = static_cast<uint32_t>(reflection.uniformBlocks.size() + 1);
                reflection.uniformBlocks.push_back(block);
                reflection.descriptors.push_back(DescriptorBindingReflection{
                    instanceName, 0, block.binding, ReflectedResourceKind::UniformBuffer});
            }
            continue;
        }

        // Handle "uniform BlockName" on one line, "{" on next line
        if (!inBlock && !inPush && clean.find("uniform") != std::string::npos && clean.find('{') == std::string::npos) {
            pendingUniformLine = clean;
            continue;
        }
        if (!inBlock && !inPush && !pendingUniformLine.empty()) {
            if (clean.find('{') != std::string::npos) {
                uint32_t set = 0;
                uint32_t binding = 0;
                parseLayoutSetBinding(pendingUniformLine, set, binding);
                inBlock = true;
                currentBlock = {};
                currentBlock.name = declarationName(pendingUniformLine);
                currentBlock.set = set;
                currentBlock.binding = binding;
                blockCursor = 0;
                pendingUniformLine.clear();
                continue;
            } else {
                pendingUniformLine.clear();
            }
        }

        if (inBlock) {
            if (clean.find('}') != std::string::npos) {
                currentBlock.instanceName = declarationName(clean);
                currentBlock.size = alignTo(blockCursor, 16);
                reflection.descriptors.push_back(DescriptorBindingReflection{
                    currentBlock.instanceName.empty() ? currentBlock.name : currentBlock.instanceName,
                    currentBlock.set,
                    currentBlock.binding,
                    ReflectedResourceKind::UniformBuffer});
                reflection.uniformBlocks.push_back(currentBlock);
                inBlock = false;
                continue;
            }
            appendUniformMember(currentBlock, clean, blockCursor);
            continue;
        }

        if (inPush) {
            if (clean.find('}') != std::string::npos) {
                currentPush.size = alignTo(pushCursor, 16);
                reflection.pushConstants.push_back(currentPush);
                inPush = false;
                continue;
            }
            UniformBlockReflection temp;
            appendUniformMember(temp, clean, pushCursor);
            if (!temp.members.empty()) {
                currentPush.members.push_back(temp.members.back());
            }
        }
    }

    return reflection;
}

Result<ShaderReflection> ShaderReflectionParser::fromSlangJson(const std::string& json,
                                                               const std::string& sourceFallback) const {
    auto fallback = fromSourceLayout(sourceFallback);
    if (!fallback) return fallback.status();
    ShaderReflection reflection = fallback.value();
    reflection.fallbackSourceReflection = json.empty();
    return reflection;
}

}  // namespace monix::renderer_vk
