#include "vk/pipeline/VkPipeline.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"

struct VkCtx {
    VulkanRenderer& r;
    VkDevice device() const { return r.device_; }
    bool shutdownRequested() const {
        return r.shutdownRequested_.load(std::memory_order_acquire);
    }
};

VkPipelineLayout vk_pipe::createPipelineLayout(VkCtx& ctx,
    std::span<const VkDescriptorSetLayout> descLayouts,
    std::span<const VkPushConstantRange> pushRanges) {
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(descLayouts.size());
    layoutInfo.pSetLayouts = descLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
    layoutInfo.pPushConstantRanges = pushRanges.data();

    VkPipelineLayout layout = VK_NULL_HANDLE;
    pfn_vkCreatePipelineLayout(ctx.device(), &layoutInfo, nullptr, &layout);
    return layout;
}

void vk_pipe::destroyPipelineLayout(VkCtx& ctx, VkPipelineLayout layout) {
    if (layout) pfn_vkDestroyPipelineLayout(ctx.device(), layout, nullptr);
}

VkShaderModule vk_pipe::createShaderModule(VkCtx& ctx, std::span<const uint32_t> spirv) {
    if (spirv.empty()) {
        OutputDebugStringA("[VK] createShaderModule: EMPTY spirv\n");
        return VK_NULL_HANDLE;
    }
    if (spirv[0] != 0x07230203) {
        char buf[128];
        sprintf_s(buf, "[VK] createShaderModule: BAD magic 0x%08X (expected 0x07230203), size=%zu\n",
            spirv[0], spirv.size());
        OutputDebugStringA(buf);
        return VK_NULL_HANDLE;
    }
    if (ctx.shutdownRequested()) {
        OutputDebugStringA("[VK] createShaderModule: shutdown in progress, skipping\n");
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    if (ctx.shutdownRequested()) return VK_NULL_HANDLE;
    VkResult createResult = pfn_vkCreateShaderModule(ctx.device(), &createInfo, nullptr, &mod);

    char buf[128];
    sprintf_s(buf, "[VK] createShaderModule: result=%d, mod=%p\n", createResult, (void*)mod);
    OutputDebugStringA(buf);
    return mod;
}

void vk_pipe::destroyShaderModule(VkCtx& ctx, VkShaderModule mod) {
    if (mod) pfn_vkDestroyShaderModule(ctx.device(), mod, nullptr);
}

VkPipeline vk_pipe::createGraphicsPipeline(VkCtx& ctx,
    std::span<const uint32_t> vertSpirv,
    std::span<const uint32_t> fragSpirv,
    VkPipelineLayout layout,
    VkFormat colorFormat,
    VkFormat depthFormat,
    bool hasBlend,
    VkVertexInputBindingDescription vertBinding,
    std::span<const VkVertexInputAttributeDescription> vertAttrs) {
    VkShaderModule vertMod = createShaderModule(ctx, vertSpirv);
    VkShaderModule fragMod = createShaderModule(ctx, fragSpirv);
    if (!vertMod || !fragMod) {
        destroyShaderModule(ctx, vertMod);
        destroyShaderModule(ctx, fragMod);
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertMod;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragMod;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    if (vertBinding.binding != 0 || vertBinding.stride != 0) {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &vertBinding;
    }
    if (!vertAttrs.empty()) {
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertAttrs.size());
        vertexInput.pVertexAttributeDescriptions = vertAttrs.data();
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = hasBlend ? VK_TRUE : VK_FALSE;
    if (hasBlend) {
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = pfn_vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    destroyShaderModule(ctx, vertMod);
    destroyShaderModule(ctx, fragMod);

    if (result != VK_SUCCESS) {
        FILE* pf = nullptr;
        fopen_s(&pf, "build\\vk_pipeline_fail.log", "a");
        if (pf) {
            fprintf(pf, "VkCreateGraphicsPipelines FAILED: VkResult=%d\n", (int)result);
            fclose(pf);
        }
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

void vk_pipe::destroyPipeline(VkCtx& ctx, VkPipeline pipeline) {
    if (pipeline) pfn_vkDestroyPipeline(ctx.device(), pipeline, nullptr);
}
