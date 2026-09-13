#include "RuntimeManagers.hpp"

#include "../core/Hash.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace monix::renderer_vk {
namespace {

Vec4 sizeVec(Extent2D extent) {
    return Vec4{
        static_cast<float>(extent.width),
        static_cast<float>(extent.height),
        extent.width ? 1.0f / static_cast<float>(extent.width) : 0.0f,
        extent.height ? 1.0f / static_cast<float>(extent.height) : 0.0f
    };
}

template <typename T>
void writeValue(std::vector<unsigned char>& buffer, uint32_t offset, const T& value) {
    if (offset + sizeof(T) > buffer.size()) return;
    std::memcpy(buffer.data() + offset, &value, sizeof(T));
}

std::string reflectionKey(const ShaderReflection& reflection) {
    std::ostringstream out;
    for (const auto& descriptor : reflection.descriptors) {
        out << descriptor.name << ":" << descriptor.set << ":" << descriptor.binding << ":"
            << static_cast<int>(descriptor.kind) << ";";
    }
    for (const auto& push : reflection.pushConstants) {
        out << "pc:" << push.size << ";";
    }
    return Hash::hex(Hash::fnv1a(out.str()));
}

}  // namespace

void ParameterManager::registerParameters(const std::vector<ShaderParameter>& parameters) {
    for (const auto& parameter : parameters) {
        definitions_[parameter.name] = parameter;
        if (values_.find(parameter.name) == values_.end()) {
            values_[parameter.name] = parameter.defaultValue;
        }
    }
}

void ParameterManager::set(std::string name, float value) {
    if (auto definition = definitions_.find(name); definition != definitions_.end()) {
        value = std::clamp(value, definition->second.minimum, definition->second.maximum);
    }
    values_[std::move(name)] = value;
}

float ParameterManager::get(std::string_view name) const {
    if (auto it = values_.find(std::string(name)); it != values_.end()) {
        return it->second;
    }
    return 0.0f;
}

std::vector<unsigned char> UniformManager::buildUniformBuffer(const UniformBlockReflection& block,
                                                             const StandardUniformValues& standard,
                                                             const ParameterManager& parameters) const {
    std::vector<unsigned char> buffer(block.size, 0);
    for (const auto& member : block.members) {
        if (writeKnownUniform(buffer, member, standard)) {
            continue;
        }
        const float value = parameters.get(member.name);
        writeValue(buffer, member.offset, value);
    }
    return buffer;
}

bool UniformManager::writeKnownUniform(std::vector<unsigned char>& buffer,
                                       const UniformMemberReflection& member,
                                       const StandardUniformValues& standard) const {
    if (member.name == "MVP" || member.name == "MVPMatrix") {
        if (member.offset + sizeof(standard.mvp) <= buffer.size()) {
            std::memcpy(buffer.data() + member.offset, standard.mvp, sizeof(standard.mvp));
        }
        return true;
    }
    if (member.name == "SourceSize" || member.name == "InputSize") {
        writeValue(buffer, member.offset, sizeVec(standard.source));
        return true;
    }
    if (member.name == "OriginalSize" || member.name == "OriginalFeedbackSize") {
        writeValue(buffer, member.offset, sizeVec(standard.original));
        return true;
    }
    if (member.name == "OutputSize") {
        writeValue(buffer, member.offset, sizeVec(standard.output));
        return true;
    }
    if (member.name == "FinalViewportSize" || member.name == "FinalViewport") {
        writeValue(buffer, member.offset, sizeVec(standard.finalViewport));
        return true;
    }
    if (member.name == "FrameCount") {
        writeValue(buffer, member.offset, standard.frameCount);
        return true;
    }
    if (member.name == "FrameDirection") {
        writeValue(buffer, member.offset, standard.frameDirection);
        return true;
    }
    return false;
}

void TextureManager::registerTexture(TextureResource texture) {
    textures_[texture.name] = std::move(texture);
}

const TextureResource* TextureManager::find(std::string_view name) const {
    if (auto it = textures_.find(std::string(name)); it != textures_.end()) {
        return &it->second;
    }
    return nullptr;
}

void FeedbackManager::registerFeedback(std::string name) {
    feedback_.try_emplace(name, FeedbackState{name, false, false});
}

void FeedbackManager::advance() {
    for (auto& item : feedback_) {
        item.second.ping = !item.second.ping;
        item.second.initialized = true;
    }
}

const FeedbackState* FeedbackManager::find(std::string_view name) const {
    if (auto it = feedback_.find(std::string(name)); it != feedback_.end()) {
        return &it->second;
    }
    return nullptr;
}

uint64_t DescriptorPoolCache::acquirePool(uint32_t descriptorCount) {
    for (auto& pool : pools_) {
        if (pool.second >= descriptorCount) {
            pool.second -= descriptorCount;
            return pool.first;
        }
    }
    const uint64_t id = nextPoolId_++;
    const uint32_t capacity = std::max<uint32_t>(descriptorCount, 128);
    pools_.push_back({id, capacity - descriptorCount});
    return id;
}

DescriptorAllocator::DescriptorAllocator(DescriptorPoolCache& pools) : pools_(pools) {}

DescriptorAllocation DescriptorAllocator::allocate(uint32_t setCount, uint32_t descriptorCount) {
    return DescriptorAllocation{pools_.acquirePool(descriptorCount), setCount};
}

uint64_t PipelineLayoutCache::getOrCreate(const ShaderReflection& reflection) {
    const auto key = reflectionKey(reflection);
    if (auto it = layouts_.find(key); it != layouts_.end()) {
        return it->second;
    }
    const uint64_t id = nextLayoutId_++;
    layouts_[key] = id;
    return id;
}

uint64_t PipelineCache::getOrCreate(std::string key) {
    if (auto it = pipelines_.find(key); it != pipelines_.end()) {
        return it->second;
    }
    const uint64_t id = nextPipelineId_++;
    pipelines_[std::move(key)] = id;
    return id;
}

void ResourceManager::importGraph(const CompiledGraph& graph) {
    for (const auto& image : graph.images) {
        textures_.registerTexture(TextureResource{image.name, {}, image.external, false});
        if (image.feedback) {
            feedback_.registerFeedback(image.name);
        }
    }
}

}  // namespace monix::renderer_vk
