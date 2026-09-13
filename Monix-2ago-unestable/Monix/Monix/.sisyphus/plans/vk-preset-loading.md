# Vulkan Preset Loading — Implementation Plan

**Generated:** 2026-08-08
**Goal:** Load and render a .slangp Mega Bezel preset through the working VulkanRenderer

---

## Architecture

Bridge the CPU-side preset parser/compiler from `renderer_vk/` to the existing `VulkanRenderer` GPU API.

```
.slangp file
  → SlangPresetParser (parse, build IR)
  → ShaderPreprocessor (#include, #define)
  → StageSplitter (vertex/fragment split)
  → ParameterExtractor (#pragma parameter)
  → SlangCompiler (slangc.exe → SPIR-V)
  → ShaderReflection (from source layout)
  → AliasResolver (texture/pass mapping)
  → RenderGraphBuilder (execution plan)
  → NEW: VkPipeline/VkDescriptorSet/VkImage creation via VulkanRenderer
  → NEW: Multi-pass rendering via cmdBeginRendering/cmdDraw/cmdEndRendering
```

---

## Phase 1: Preset Compilation Integration

**Goal:** Get preset parsing + SPIR-V compilation working inside the main app.

### Files to modify:
- `src/native/main.cpp`: Add preset loading call in `InitializeOpenGlBootstrap()` after Vulkan init
- `src/native/renderer_vk_unity.cpp`: Verify unity build includes all needed files

### Changes:
1. Create `ShaderRendererConfig` from existing paths in `MonixApp::paths`
2. Call `ShaderRenderer::initialize()` + `ShaderRenderer::loadPreset(slangpPath)`
3. Log results: pass count, parameter count, pipeline count
4. Add error handling for compilation failures

### Verification:
- Build succeeds
- App logs "Loaded preset: X passes, Y parameters"
- Debug dump files appear in `logs/shader-runtime-vk/`

---

## Phase 2: Vulkan Resource Creation from Reflection

**Goal:** Convert CompiledPreset data into Vulkan objects.

### New method: `VulkanRenderer::loadShaderPreset(const CompiledPreset& compiled)`

For each pass:
1. **VkShaderModule** from `CompiledPass::vertex.spirv` and `CompiledPass::fragment.spirv`
2. **VkDescriptorSetLayout** from `ShaderReflection::descriptors` + `ShaderReflection::uniformBlocks`
3. **VkPipelineLayout** from descriptor set layout + push constant ranges
4. **VkPipeline** via `createGraphicsPipeline()` with swapchain format
5. **Per-pass render targets**: VkImage + VkImageView (RGBA8, pass output size)
6. **Per-pass uniform buffer**: VkBuffer with mapped memory for MVP/sourceSize/etc.

### External textures:
- Load bezel PNGs via stb_image (need to add `#define STB_IMAGE_IMPLEMENTATION`)
- Upload to VkImage via staging buffer + cmdCopyBufferToImage
- Create VkImageView + VkSampler per texture

### New struct: `ShaderPreset`
```cpp
struct ShaderPresetPass {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSet descriptorSet;
    VkImageResource outputImage;      // render target
    VkBufferResource uniformBuffer;   // std140 uniforms
    uint32_t uniformSize;
    std::vector<VkDescriptorImageInfo> textureBindings;
};
struct ShaderPreset {
    std::vector<ShaderPresetPass> passes;
    std::vector<VkImageResource> externalTextures;  // bezel PNGs
    std::vector<VkSamplerResource> samplers;
    CompiledPreset compiled;  // keep for parameter updates
};
```

---

## Phase 3: Multi-Pass Rendering

**Goal:** Execute all passes in order, chaining outputs.

### New method: `VulkanRenderer::renderShaderPreset(uint32_t frameCount)`

For each pass in `executionPlan.passOrder`:
1. Update uniform buffer (MVP, SourceSize, OutputSize, FrameCount)
2. Update descriptor set with:
   - Binding 0: uniform buffer
   - Binding 1+: textures (Source from prev pass, external bezel textures)
3. Begin dynamic rendering on pass output image
4. Bind pipeline
5. Bind descriptor set
6. Draw full-screen quad (4 vertices, 6 indices)
7. End dynamic rendering
8. Transition pass output to SHADER_READ_ONLY for next pass

Final pass output → blit to swapchain image → present

### Source texture handling:
- Pass 0 reads from the GDI bitmap (already uploaded via uploadTexture)
- Subsequent passes read from previous pass output
- InfoCachePass reads from Source (passthrough)

---

## Phase 4: Runtime Integration

**Goal:** Wire into WM_PAINT loop + preset cycling.

### Changes to `RenderOpenGlFrame()`:
1. If `shaderPreset_` is loaded:
   - Upload GDI bitmap to Source image
   - Call `renderShaderPreset(frameCount)`
   - Blit final output to swapchain
2. Else: fall back to existing GDI-to-swapchain blit

### Preset cycling (Shift+F4):
- Call `loadShaderPreset()` with next .slangp file
- Destroy previous preset resources
- Reset frame count

---

## Implementation Order

1. **Phase 1** — Parse + compile only, no rendering (verify compilation works)
2. **Phase 2** — Create Vulkan objects from reflection (verify no crashes)
3. **Phase 3** — Render first pass only (verify shader execution)
4. **Phase 3b** — Render all passes (verify multi-pass chain)
5. **Phase 4** — Runtime integration + preset cycling

---

## Dependencies

- `stb_image.h` for PNG loading (add to `src/native/`)
- `vulkan_renderer.cpp` already has all needed GPU resource creation methods
- `renderer_vk/` already has complete preset parsing + compilation pipeline

## Risk Assessment

- **Low risk**: Phase 1 (just calling existing code)
- **Medium risk**: Phase 2 (resource creation, but VulkanRenderer has all primitives)
- **Medium risk**: Phase 3 (multi-pass chaining, must get image transitions right)
- **Low risk**: Phase 4 (just wiring into existing render loop)
