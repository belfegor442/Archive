# RenderGraph Integration Test Results

Date: 2026-08-15
Build: Monix.exe (1921KB, MSVC C++20 /MT)

## Compilation Pipeline Tests (run via `Monix.exe --test`)

| Test               | Status | Description |
| ------------------ | ------ | ----------- |
| 2-pass             | PASS   | Preset parses 2 passes; RenderGraph creates correct image nodes and PassDependency links |
| Multi-binding      | PASS   | Binding 2=Source, 3=PassOutput resolved correctly through AliasResolver→RenderGraph |
| Multi-binding 3    | PASS   | Same PassA output bound to 2 different sampler slots (binding 3, 4) |
| Alias              | PASS   | `pass_a` resolves to AliasKind::PassOutput with correct passIndex |
| Alias graph        | PASS   | Graph links `pass_a` sampler to the correct ImageNode via PassDependency |
| Feedback parse     | PASS   | `framebuffer_feedback0 = true` parsed correctly from .slangp |
| Feedback alias     | PASS   | `Pass0Feedback` resolves to AliasKind::Feedback |
| Feedback graph     | PASS   | Graph creates feedback ImageNode and links it as pass input |
| External texture   | PASS   | `textures = 1` with `filter_linear0`, `wrap_mode0`, `mipmap0` parsed |
| External graph     | PASS   | Graph creates external ImageNode for texture0 even without explicit alias |
| Parameter override | PASS   | `TEST_EFFECT = 1` parsed into parameterOverrides |
| Parameter preproc  | PASS   | `#ifdef TEST_EFFECT` correctly activated via initialDefines → preprocessor |
| #reference         | PASS   | Derived.slangp merges base entries; derived overrides base alias |
| Cycle detection    | PASS   | A→B→A returns "Circular #reference detected" error, no stack overflow |
| Resize             | PASS   | Graph rebuild at different resolutions produces same structure/passOrder |

## Bug Fixes Applied

| File | Function | Line | Bug | Fix |
| ---- | -------- | ---- | --- | --- |
| SlangPresetParser.cpp | parseFileInternal() | 112 | `stripComment()` removes `#reference` because `#` is comment char | Check for `#reference` BEFORE `stripComment()` |
| RenderGraphBuilder.cpp | build() | 33-37 | External textures without alias get no ImageNode | Use default name `"textureN"` when alias is empty |

## Vulkan Integration Tests (require GPU)

| Test               | Status | Notes |
| ------------------ | ------ | ----- |
| Validation layers  | PEND   | Requires runtime with GPU; VUID checking not yet wired |
| FXAA               | PEND   | Requires Slang compiler (slangc.exe) + .slang→SPIR-V |
| Bloom              | PEND   | Requires Slang compiler + multi-pass SPIR-V |
| Halo               | PEND   | Requires Slang compiler + real shader |
| Reflection blur    | PEND   | Requires Slang compiler + real shader |
| MegaDrive preset   | PEND   | Requires Base.slangp, config.inc, includes/*.include.slang |

## Data Flow Verified

```
.slangp (preset file)
  ↓ SlangPresetParser::parseFile() — handles #reference recursion, cycle detection
PresetAst
  ↓ SlangPresetParser::buildIr() — builds passes[], externalTextures[], parameterOverrides[]
PresetIr
  ↓ AliasResolver::build() — registers builtins + pass aliases + texture aliases
AliasDatabase
  ↓ RenderGraphBuilder::build() — resolves sampler names → ImageNodes via AliasResolver
CompiledGraph { images[], passes[{inputs[]}], lifetimes[] }
  ↓ VulkanBackend::loadCompiledPreset() — converts reflection → SamplerBindingInfo per pass
  ↓ VulkanRenderer::loadPresetReflection() — creates VkDescriptorSetLayout from reflection bindings
  ↓ VulkanBackend::resolvePassBindings() — maps sampler names → VkImageView/VkSampler via graph
  ↓ VulkanBackend::execute() — writes descriptors at correct binding numbers, draws
VkDescriptorSet → VkPipeline → draw → correct image
```

## Missing Dependencies for MegaDrive Preset

| File | Package | Status |
| ---- | ------- | ------ |
| Base.slangp | Mega Bezel base preset | NOT FOUND |
| config.inc | Mega Bezel configuration | NOT FOUND |
| includes/functions.include.slang | Mega Bezel utility functions | NOT FOUND |
| includes/blooms.include.slang | Mega Bezel bloom effects | NOT FOUND |

These files are part of the Mega Bezel preset pack (by HyperspaceMadness) and are not included in the Monix repository. They must be obtained from the original Mega Bezel release.
