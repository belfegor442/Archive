# Mega Bezel Shader Pipeline — Master Plan & Status

**Generated:** 2026-07-31
**Status:** Phase 4 COMPLETE (all 3 bugs fixed + 5 new features implemented)

---

## Phase 4 — Visual Validation (COMPLETE)

### Critical Bugs Fixed

#### P4-1: Texture Alias Parsing
- **Problem:** Mega Bezel presets use `alias8 = TubeDiffuseImage` but the parser looked for `alias_texture8`. The parser failed to recognize that aliases beyond the shader count are texture aliases, not sampler aliases.
- **Fix:** Detect `aliasN` where N >= numShaders as texture aliases, mapped via `(i - numShaders)`.
- **Status:** PASS — Mega Bezel InfoCachePass now correctly binds TubeDiffuseImage, StencilMask, DiffusePass, InfoCachePass textures.

#### P4-2: UBO Members Not Emitted
- **Problem:** 700+ HSM parameters from globals.inc were extracted into the UBO struct but never emitted as individual `uniform` declarations. Mega Bezel shaders reference these as `global.HSM_BEZEL_WIDTH` etc. which became undefined symbols after `global.` stripping.
- **Fix:** Emit UBO members as individual `uniform` declarations, skip already-declared names.
- **Status:** PASS — All HSM uniforms now available in GLSL.

#### P4-3: Self-Referencing Local Variables
- **Problem:** `params-0-screen-scale.inc` creates lines like `float HSM_ScreenScale_X = global.HSM_ScreenScale_X;`. After `global.` stripping during preamble helper extraction, this becomes `float HSM_ScreenScale_X = HSM_ScreenScale_X;` — a self-referencing assignment.
- **Fix:** Detect and remove self-referencing lines after UBO emission.
- **Status:** PASS — No more NaN propagation from self-references.

### New Features

- **F5** — HSM Parameters debug mode (InfoCache view + parameter dump)
- **F10** — Screenshot capture to PNG (screenshots/ directory, timestamped)
- **NaN/Zero Detection** — Runtime detection of NaN parameters and zero-dimension uniforms
- **Comprehensive OpenGL Info Panel** — Pipeline stats, render log, keyboard shortcuts
- **Per-Pass Viewport Logging** — [Viewport] pass=N viewport=(WxH) for every pass

### Updated Debug Shortcuts

| Key | Function |
|-----|----------|
| F1 | Pass output |
| F2 | InfoCache |
| F3 | Feedback |
| F4 | External textures |
| F5 | HSM params |
| F10 | Screenshot |
| Shift+F4 | Cycle presets |
| Shift+F5 | Reload config |

---

## Phase 3 — Runtime Accuracy (COMPLETE)

### Critical Bugs Fixed

#### B1: Sampler Resolution System
- Replaced fallback-to-previous-pass logic with priority-based resolution: external texture → pass alias → built-in (ORIG_LINEARIZED, Original, Source) → feedback → BLACK_PLACEHOLDER (with log, no silent fallback)
- Added structured `[Sampler Resolve]` logging for every sampler binding

#### B2: Original/ORIG_LINEARIZED
- Created `linearizeFbo`/`linearizeTex` with `GL_SRGB8_ALPHA8` (0x8C43) for automatic sRGB→linear conversion on texture reads
- Created `copyProgram` to blit input texture to linearize FBO
- ORIG_LINEARIZED now reads from the SRGB texture (GPU does sRGB→linear conversion automatically)
- Lazy initialization in render() — only created if a pass needs ORIG_LINEARIZED
- Cleanup in ShaderPipeline::destroy()

#### B3: FrameCount (uint) + FrameDirection (int)
- Changed `uniform int FrameCount` → `uniform uint FrameCount` in GLSL output
- Added `uniform int FrameDirection` declaration in GLSL output
- Added `uniformFrameDirection` to CompiledPass struct
- `glGetUniformLocation` lookup for "FrameDirection" in load()
- `gl.Uniform1ui` for FrameCount (uint), `gl.Uniform1i` for FrameDirection
- Updated preamble helpers: `global.FrameCount` → `uint(FrameCount)`
- Added `Uniform1ui` to GlFunctions struct + LoadGlFunctions

#### B4: SourceSize/OriginalSize/OutputSize Per-Pass
- SourceSize = current input dimensions (curW, curH)
- OutputSize = pass FBO output dimensions (cp.width, cp.height)
- OriginalSize = original content dimensions (inputWidth, inputHeight)
- Added `[PassUniforms]` logging with all three sizes per pass

#### B5: Per-Sampler Texture Filtering
- Added `std::unordered_map<std::string, bool> samplerFilterMode` to CompiledPass
- Per-sampler filtering in render loop: checks samplerFilterMode override, falls back to pass default
- Applied `GL_LINEAR`/`GL_NEAREST` per texture unit
- Added `[Sampler Filter]` logging per sampler

#### B6: Viewport Support
- Added `glViewport(0, 0, cp.width, cp.height)` before each pass draw call
- Added `[Viewport]` logging per pass
- `Viewport` function pointer was NOT added to GlFunctions (GL 1.1, called directly)

#### B7: OpenGL Leak Test
- Added `RunLeakTest()` function: loads/unloads a preset N iterations with dummy render
- Snapshot GL_TEXTURE_BINDING_2D before and after
- Reports texture leak delta
- Uses GlFunctions wrapper for all GL 3.0+ calls, raw GL 1.1 for glBindTexture/glTexImage2D

### Compilation Fixes
- Moved `linearizeFbo`/`linearizeTex`/`copyProgram` cleanup from per-pass loop to ShaderPipeline::destroy() level
- Added `Uniform1ui` to GlFunctions + LoadGlFunctions (PFNGLUNIFORM1UIPROC)
- `Viewport` and `TexImage2D` are GL 1.1 — used raw (not in GlFunctions wrapper)
- Fixed `pi.filterLinear` → `preset.passes[i].filterLinear` in render loop (pi is load-time variable)
- Fixed all raw GL calls in leak test to use `gl.` wrapper for GL 3.0+ functions

### Build Status
- Monix.exe builds clean: 1162KB
- test_translator: all 8 passes translate OK
- Phase 1 and Phase 2 fixes verified still working

---

## Phase 1 Fixes (Critical)

### C1: Nested #include Resolution
**STATUS:** PASS

FIX: Replaced flat include resolver with ResolveIncludesRecursive()
  - Recursive depth-first resolution of all #include "file" directives
  - Circular dependency detection via set<string> includedFiles
  - Duplicate include deduplication (same file included from multiple places)
  - Relative path resolution per-including-file (not just top-level shader)
  - Missing files logged to status output (non-fatal)

CHANGES:
  - main.cpp:2162-2200  ResolveIncludesRecursive lambda (replaces flat loop)
  - main.cpp:60         Added #include <functional> for std::function
  - test_translator.cpp:270-310  Same recursive resolver

VALIDATION:
  - test_output/pass4_fragment.glsl: 0 raw #include directives (was 2)
  - test_output/pass5_fragment.glsl: 0 raw #include directives (was multiple)
  - test_output/pass6_fragment.glsl: 0 raw #include directives (was multiple)
  - test_output/pass7_fragment.glsl: 0 raw #include directives (was 3)
  - crt-lottes-with-bezel.slangp: All 8 passes translate OK
  - crt-lottes-multipass.slangp: All 3 passes translate OK

### C2: FinalViewportSize Uniform
**STATUS:** PASS

FIX: Added FinalViewportSize as a standard pipeline uniform
  - Emitted in GLSL output: uniform vec4 FinalViewportSize
  - Added to skipNames to prevent UBO re-declaration
  - Added global.FinalViewportSize -> FinalViewportSize mapping (3 locations)
  - CompiledPass.uniformFinalViewportSize lookup in load()
  - Set in render loop: (outW, outH, 1/outW, 1/outH) before each pass

CHANGES:
  - main.cpp:2372   Added "uniform vec4 FinalViewportSize;\n" to GLSL output
  - main.cpp:2307   Added "FinalViewportSize" to skipNames[]
  - main.cpp:2560   Added global.FinalViewportSize mapping (fragment)
  - main.cpp:2715   Added global.FinalViewportSize mapping (vertex)
  - main.cpp:2374   Added global.FinalViewportSize mapping (preamble helpers)
  - main.cpp:1063   Added GLint uniformFinalViewportSize = -1 to CompiledPass
  - main.cpp:1433   GetUniformLocation lookup for FinalViewportSize
  - main.cpp:1700   Uniform4f call in render loop

VALIDATION:
  - pass4_fragment.glsl contains: float output_aspect = FinalViewportSize.x / FinalViewportSize.y
  - Uniform set per-pass with correct viewport dimensions

### C3: HSM Parameter Defaults (#pragma parameter)
**STATUS:** PASS

FIX: Full #pragma parameter extraction with name, default, min, max, step
  - New ShaderParameter struct: {name, description, defaultValue, minValue, maxValue, step}
  - Regex-like parser extracts all 6 fields from #pragma parameter lines
  - Stores in TranslatedShader.parameters (replaces old parameterNames vector)
  - CompiledPass.parameters stores ShaderParameter per-pass
  - Render loop applies defaultValue (not hardcoded 0.0)

CHANGES:
  - main.cpp:1036-1043  New ShaderParameter struct
  - main.cpp:1044       TranslatedShader uses vector<ShaderParameter>
  - main.cpp:1066       CompiledPass uses vector<ShaderParameter>
  - main.cpp:2787-2816  Full pragma parameter parser (was 7 lines, now 30)
  - main.cpp:1435-1442  Load parameters with logging
  - main.cpp:1707       Apply defaultValue (was paramDefaults[p])
  - test_translator.cpp:63-68  Same ShaderParameter struct
  - test_translator.cpp:771-794 Full pragma parameter parser

VALIDATION:
  - Pass 2 (BloomPass): 13 params extracted with correct defaults
    hardScan=-8, hardPix=-3, warpX=0.031, warpY=0.041, maskDark=0.5,
    maskLight=1.5, shadowMask=3, brightBoost=1, bloomAmount=0.4, shape=2
  - Pass 3 (CRTOut): 13 params extracted (same CRT params)
  - Range values correct: e.g. warpX range=[0, 0.125]

### Additional Fix: Preamble Helpers #include Skip
**STATUS:** PASS

FIX: Skip #include directives in preamble helpers extraction
  - Unresolved includes from nested files would cause GLSL compilation failure
  - Added #include to skip conditions in both main.cpp and test_translator.cpp

CHANGES:
  - main.cpp:2334       Added trimmed.substr(0, 8) == "#include" to skip
  - test_translator.cpp:424  Same skip added

---

## Phase 2 Fixes (Medium/High Priority)

### A1: GL_CLAMP_TO_BORDER Real
**STATUS:** PASS

FIX: Implemented real GL_CLAMP_TO_BORDER with border color
  - ensureFBOs: wrapMode "clamp_to_border" now maps to GL_CLAMP_TO_BORDER
    (was incorrectly mapped to GL_CLAMP_TO_EDGE)
  - glTexParameterfv(GL_TEXTURE_BORDER_COLOR, {0,0,0,0}) when clamp_to_border
  - LoadShaderTexture: same fix for external textures
  - Both FBO textures and external textures now support border clamping

### A2: mipmap_input Generation
**STATUS:** PASS

FIX: Generate mipmaps when mipmap_input=true in preset
  - Added PFNGLGENERATEMIPMAPPROC GenerateMipmap to GlFunctions struct
  - Added LOAD_GL_PROC(GenerateMipmap) in LoadGlFunctions
  - ensureFBOs: when mipmapInput=true, set GL_TEXTURE_MIN_FILTER to
    GL_LINEAR_MIPMAP_LINEAR and GL_TEXTURE_MAG_FILTER to GL_LINEAR
  - Render loop: after draw call, if mipmapInput=true, call
    gl.GenerateMipmap(GL_TEXTURE_2D) on the pass's output texture

### A3: Token-safe Source Renaming
**STATUS:** PASS

FIX: Replaced fragile substring matching with word-boundary detection
  - Old code: 3 separate find-replace patterns (sampler2D Source, texture(Source, , Source)
  - New code: Token-safe loop checking isalnum/_ before and after "Source"
  - Only replaces standalone "Source" identifiers
  - Does NOT replace "SourceSize", "SourceTexture", "SourceCoord", etc.
  - Same fix applied to test_translator.cpp

### A4: GL State Reset Before Each Pass
**STATUS:** PASS

FIX: Reset GL state before every pass draw
  - Added before each pass:
    glDisable(GL_BLEND)
    glDisable(GL_DEPTH_TEST)
    glDisable(GL_CULL_FACE)
    glDisable(GL_SCISSOR_TEST)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)
  - Prevents cross-pass contamination from previous pass's GL state

### M3: Bool/Int/Float Parameter Types
**STATUS:** PASS

FIX: Full type detection for #pragma parameter declarations
  - ShaderParameter struct: added std::string type field ("float"/"bool"/"int")
  - Parser detects type keyword before parameter name:
    "#pragma parameter bool X ..." -> type="bool"
    "#pragma parameter int X ..."  -> type="int"
    "#pragma parameter X ..."      -> type="float" (default)
  - Render loop: uses correct uniform setter:
    bool:  gl.Uniform1i(loc, value != 0 ? 1 : 0)
    int:   gl.Uniform1i(loc, (int)value)
    float: gl.Uniform1f(loc, value)
  - Parameter logging now includes type

### M4: srgb_framebuffer Support
**STATUS:** PASS

FIX: Create SRGB FBOs when srgb_framebuffer=true
  - ensureFBOs: internal format selection:
    floatFramebuffer: GL_RGBA32F
    srgbFramebuffer:  GL_SRGB8_ALPHA8 (0x8C43)
    default:          GL_RGBA8
  - Render loop: glEnable/glDisable GL_FRAMEBUFFER_SRGB (0x8C90) per-pass
  - SRGB FBOs get automatic linear<->sRGB conversion by GPU

### M6: Skip Unused Sampler Uniforms
**STATUS:** PASS

FIX: Skip binding when no texture resolves for a sampler
  - After texture resolution logic, if texToBind == 0:
    - Skip glBindTexture and glUniform1i
    - Log: "[Sampler] skipped unused: <samplerName>"
  - Prevents binding texture ID 0 (which would unbind current texture)

---

## Build Verification

| Build | Size | Status |
|-------|------|--------|
| Monix.exe (Phase 4) | — | Clean |
| Monix.exe (Phase 3) | 1162KB | Clean |
| Monix.exe (Phase 2) | 1155KB | Clean |
| Monix.exe (Phase 1) | 1154KB | Clean |
| test_translator.exe | — | Clean |

Compiler flags: `/MT /std:c++20 /O1`

---

## Active

- **Phase:** 4 COMPLETE
- **All critical visual bugs resolved:** Texture alias parsing, UBO emission, self-referencing locals
- **Debug tools:** F1-F10 shortcuts, NaN detection, OpenGL info panel, screenshots
- **Regression:** All presets (crt-lottes-multipass, crt-lottes-with-bezel) render correctly

---

## Regression Check

### crt-lottes-multipass.slangp (3 passes)
- Pass 0 (bloompass): OK — 13 params, 1 sampler
- Pass 1 (stock): OK — passthrough
- Pass 2 (scanpass): OK — 13 params, 2 samplers

### crt-lottes-with-bezel.slangp (8 passes)
- Pass 0 (InfoCachePass): OK — 0.1MB fragment (geometry cache)
- Pass 1 (Reference): OK — stock passthrough
- Pass 2 (BloomPass): OK — 13 params
- Pass 3 (CRTOut): OK — 13 params, 2 samplers
- Pass 4 (PostCRTPass): OK — bezel prep (uses FinalViewportSize)
- Pass 5 (BR_LayersUnderCRT): OK — bezel layers
- Pass 6 (REMAINING): OK — bezel layers over CRT
- Pass 7 (combine): OK — final output

All 8 passes translate OK, all params extracted, no compilation errors.

---

## Remaining Known Issues (Low Priority)

- No explicit GL state reset between passes (PARTIALLY FIXED in A4)
- Source -> Texture rename incomplete for edge cases (FIXED in A3)

---

## Phase 4 Files

| File | Description |
|------|-------------|
| `PHASE4_VISUAL_REPORT.txt` | Full visual validation report with pass-by-pass analysis |

---

## What Was Done — Summary

### Phase 1 (Critical Fixes)
- C1: Nested #include Resolution — recursive resolver with circular detection
- C2: FinalViewportSize Uniform — per-pass viewport dimensions
- C3: HSM Parameter Defaults — full #pragma parameter extraction

### Phase 2 (Medium/High Priority)
- A1: GL_CLAMP_TO_BORDER — real border clamping with color
- A2: mipmap_input — automatic mipmap generation
- A3: Token-safe Source Renaming — word-boundary detection
- A4: GL State Reset — prevent cross-pass contamination
- M3: Bool/Int/Float Parameter Types — type-aware uniform setters
- M4: srgb_framebuffer — SRGB FBO creation and toggle
- M6: Skip Unused Sampler Uniforms — no-op for unresolvable samplers

### Phase 3 (Runtime Accuracy — Complete)
- B1: Sampler Resolution System — priority-based resolution chain
- B2: Original/ORIG_LINEARIZED — sRGB-to-linear conversion pipeline
- B3: FrameCount (uint) + FrameDirection (int) — correct types
- B4: SourceSize/OriginalSize/OutputSize — per-pass dimension uniforms
- B5: Per-Sampler Texture Filtering — independent GL_LINEAR/GL_NEAREST
- B6: Viewport Support — glViewport per pass
- B7: OpenGL Leak Test — texture leak detection

### Phase 4 (Visual Validation — Complete)
- P4-1: Texture Alias Parsing — detect aliasN >= numShaders as texture aliases
- P4-2: UBO Members Not Emitted — emit 700+ HSM uniforms as individual declarations
- P4-3: Self-Referencing Local Variables — detect and remove self-assignment after stripping
- F5: HSM Parameters debug mode (InfoCache view + parameter dump)
- F10: Screenshot capture to PNG (screenshots/ directory, timestamped)
- NaN/Zero Detection — runtime NaN and zero-dimension uniform detection
- Comprehensive OpenGL Info Panel — pipeline stats, render log, shortcuts
- Per-Pass Viewport Logging — [Viewport] pass=N viewport=(WxH) for every pass

**Status:** Phase 4 COMPLETE (all 3 bugs fixed + 5 new features implemented)
