#include "vk/descriptor/VkDescriptor.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"

struct VkCtx { VulkanRenderer& r; };

bool vk_desc::createDescriptorPool(VkCtx& ctx) {
    auto& r = ctx.r;
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxDescriptorSets },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxUniformBuffers },
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = kMaxDescriptorSets;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;

    VkResult result = pfn_vkCreateDescriptorPool(r.device_, &poolInfo, nullptr, &r.descPool_);
    return result == VK_SUCCESS;
}

VkDescriptorSetLayout vk_desc::createDescriptorSetLayout(VkCtx& ctx,
    std::span<const VkDescriptorSetLayoutBinding> bindings) {
    auto& r = ctx.r;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    pfn_vkCreateDescriptorSetLayout(r.device_, &layoutInfo, nullptr, &layout);
    return layout;
}

void vk_desc::destroyDescriptorSetLayout(VkCtx& ctx, VkDescriptorSetLayout layout) {
    auto& r = ctx.r;
    if (layout) pfn_vkDestroyDescriptorSetLayout(r.device_, layout, nullptr);
}

VkDescriptorSet vk_desc::allocateDescriptorSet(VkCtx& ctx, VkDescriptorSetLayout layout) {
    auto& r = ctx.r;
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = r.descPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    pfn_vkAllocateDescriptorSets(r.device_, &allocInfo, &set);
    return set;
}

void vk_desc::updateDescriptorSetTexture(VkCtx& ctx, VkDescriptorSet set, uint32_t binding,
                                           VkImageView view, VkSampler sampler, VkImageLayout layout) {
    auto& r = ctx.r;
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = layout;
    imageInfo.imageView = view;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    pfn_vkUpdateDescriptorSets(r.device_, 1, &write, 0, nullptr);
}

void vk_desc::updateDescriptorSetUniform(VkCtx& ctx, VkDescriptorSet set, uint32_t binding,
                                           VkBuffer buffer, VkDeviceSize range) {
    auto& r = ctx.r;
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    pfn_vkUpdateDescriptorSets(r.device_, 1, &write, 0, nullptr);
}
