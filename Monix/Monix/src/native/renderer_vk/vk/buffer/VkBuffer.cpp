#include "vk/buffer/VkBuffer.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"
#include "vk/memory/VkMemory.hpp"
#include <cstring>

struct VkCtx { VulkanRenderer& r; };

VkBufferResource vk_buf::createBuffer(VkCtx& ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                                       VkMemoryPropertyFlags props) {
    auto& r = ctx.r;
    VkBufferResource result{};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (pfn_vkCreateBuffer(r.device_, &bufferInfo, nullptr, &result.buffer) != VK_SUCCESS) {
        return result;
    }

    VkMemoryRequirements memReqs{};
    pfn_vkGetBufferMemoryRequirements(r.device_, result.buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = vk_mem::findMemoryType(ctx, memReqs.memoryTypeBits, props);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        pfn_vkDestroyBuffer(r.device_, result.buffer, nullptr);
        result.buffer = VK_NULL_HANDLE;
        return result;
    }

    if (pfn_vkAllocateMemory(r.device_, &allocInfo, nullptr, &result.memory) != VK_SUCCESS) {
        pfn_vkDestroyBuffer(r.device_, result.buffer, nullptr);
        result.buffer = VK_NULL_HANDLE;
        return result;
    }

    if (pfn_vkBindBufferMemory(r.device_, result.buffer, result.memory, 0) != VK_SUCCESS) {
        pfn_vkFreeMemory(r.device_, result.memory, nullptr);
        pfn_vkDestroyBuffer(r.device_, result.buffer, nullptr);
        result.buffer = VK_NULL_HANDLE;
        result.memory = VK_NULL_HANDLE;
        return result;
    }
    result.size = size;

    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        if (pfn_vkMapMemory(r.device_, result.memory, 0, size, 0, &result.mapped) != VK_SUCCESS) {
            result.mapped = nullptr;
        }
    }

    return result;
}

void vk_buf::destroyBuffer(VkCtx& ctx, VkBufferResource& buf) {
    auto& r = ctx.r;
    if (buf.mapped) {
        pfn_vkUnmapMemory(r.device_, buf.memory);
        buf.mapped = nullptr;
    }
    if (buf.buffer) pfn_vkDestroyBuffer(r.device_, buf.buffer, nullptr);
    if (buf.memory) pfn_vkFreeMemory(r.device_, buf.memory, nullptr);
    buf = {};
}

void vk_buf::uploadBuffer(VkCtx& ctx, VkBufferResource& dst, const void* data, VkDeviceSize size) {
    if (!dst.mapped) return;
    memcpy(dst.mapped, data, static_cast<size_t>(size));
}

bool vk_buf::createQuadBuffer(VkCtx& ctx) {
    auto& r = ctx.r;
    QuadVertex quadVerts[] = {
        {{ -1.0f, -1.0f }, { 0.0f, 1.0f }},
        {{  1.0f, -1.0f }, { 1.0f, 1.0f }},
        {{ -1.0f,  1.0f }, { 0.0f, 0.0f }},
        {{  1.0f,  1.0f }, { 1.0f, 0.0f }},
    };

    VkDeviceSize size = sizeof(quadVerts);
    r.quadVbo_ = createBuffer(ctx, size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (r.quadVbo_.buffer == VK_NULL_HANDLE) return false;

    uploadBuffer(ctx, r.quadVbo_, quadVerts, size);
    r.quadCreated_ = true;

    char dbg[128];
    sprintf_s(dbg, "[VK] createQuadBuffer: BL tex=(%.1f,%.1f) TL tex=(%.1f,%.1f)\n",
        quadVerts[0].texCoord[0], quadVerts[0].texCoord[1],
        quadVerts[2].texCoord[0], quadVerts[2].texCoord[1]);
    OutputDebugStringA(dbg);

    return true;
}
