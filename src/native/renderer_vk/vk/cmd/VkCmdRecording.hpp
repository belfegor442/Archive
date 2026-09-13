#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;
struct VkImageResource;
struct VkBufferResource;

namespace vk_rec {
void cmdBeginRendering(VkCtx& ctx, VkImageView colorTarget, uint32_t w, uint32_t h, VkClearValue clear);
void cmdEndRendering(VkCtx& ctx);
void cmdBindPipeline(VkCtx& ctx, VkPipeline pipeline, VkPipelineLayout layout);
void cmdBindDescriptorSet(VkCtx& ctx, VkPipelineLayout layout, VkDescriptorSet set, uint32_t setIdx);
void cmdBindVertexBuffer(VkCtx& ctx, VkBufferResource& buf);
void cmdSetViewport(VkCtx& ctx, uint32_t w, uint32_t h);
void cmdSetScissor(VkCtx& ctx, uint32_t w, uint32_t h);
void cmdPushConstants(VkCtx& ctx, VkPipelineLayout layout, VkShaderStageFlags stage,
                       uint32_t offset, uint32_t size, const void* data);
void cmdDraw(VkCtx& ctx, uint32_t vertexCount, uint32_t firstVertex);
void cmdBlitImage(VkCtx& ctx, VkImageResource& src, VkImageResource& dst,
                   uint32_t srcW, uint32_t srcH, uint32_t dstW, uint32_t dstH);
void cmdCopyImageToBuffer(VkCtx& ctx, VkImageResource& src, VkBufferResource& dst, uint32_t w, uint32_t h);
void cmdCopyBufferToImage(VkCtx& ctx, VkBufferResource& src, VkImageResource& dst, uint32_t w, uint32_t h);
void cmdTransitionLayout(VkCtx& ctx, VkImageResource& img, VkImageLayout newLayout);
void cmdTransitionImage(VkCtx& ctx, VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
void drawFullScreenQuad(VkCtx& ctx);
}
