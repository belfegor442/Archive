#include "ShaderRenderer.hpp"

#include "../compiler/SlangCompiler.hpp"
#include "../debug/ReflectionDump.hpp"
#include "../debug/RenderGraphDump.hpp"
#include "../debug/ValidationReport.hpp"
#include "../graph/RenderGraphBuilder.hpp"
#include "../preset/AliasResolver.hpp"
#include "../preset/IncludeResolver.hpp"
#include "../preset/ParameterExtractor.hpp"
#include "../preset/PresetValidator.hpp"
#include "../preset/ShaderPreprocessor.hpp"
#include "../preset/SlangPresetParser.hpp"
#include "../preset/StageSplitter.hpp"
#include "../core/FileSystem.hpp"
#include "../validation/GpuShaderValidator.hpp"

#include <filesystem>
#include <sstream>
#include <unordered_map>

namespace monix::renderer_vk {

ShaderRenderer::ShaderRenderer(RendererConfig config)
    : config_(std::move(config)) {
    if (this->config_.rootDirectory.empty()) {
        this->config_.rootDirectory = std::filesystem::current_path();
    }
    if (this->config_.shaderCacheDirectory.empty()) {
        this->config_.shaderCacheDirectory = this->config_.rootDirectory / "build" / "shader-cache-vk";
    }
    shaderCache_ = std::make_unique<ShaderCache>(this->config_.shaderCacheDirectory);
    hotReload_ = std::make_unique<ShaderHotReload>(
        ShaderHotReload::defaultConfig(this->config_.rootDirectory / "Shaders"));
}

ShaderRenderer::~ShaderRenderer() {
    if (hotReload_ && hotReload_->isRunning()) {
        hotReload_->stop();
    }
}

Status ShaderRenderer::initialize() {
    auto status = backend_.initialize();
    if (!status) {
        return status;
    }
    return Status::success();
}

void ShaderRenderer::setRenderer(VulkanRenderer* renderer) {
    backend_.setRenderer(renderer);
}

Status ShaderRenderer::enableHotReload() {
    if (!hotReload_) return Status::failure("HotReload not initialized");

    hotReload_->setDependencyGraph(&depGraph_);

    auto shaderRoot = config_.rootDirectory / "Shaders";
    hotReload_->addWatchPath(shaderRoot);

    auto self = this;
    hotReload_->setCallback([self](const ShaderReloadRequest& request) {
        uint64_t currentRequestId = self->hotReload_->requestIdCounter();
        if (request.requestId != currentRequestId) return;

        if (!self->loadedPresetPath_.empty()) {
            self->loadPreset(self->loadedPresetPath_);
        }
    });

    hotReload_->start();
    return Status::success();
}

Status ShaderRenderer::disableHotReload() {
    if (hotReload_ && hotReload_->isRunning()) {
        hotReload_->stop();
    }
    return Status::success();
}

Status ShaderRenderer::pollHotReload() {
    if (!hotReload_ || !hotReload_->isRunning()) return Status::success();
    hotReload_->processPendingReloads();
    hotReload_->processReloadQueue();
    return Status::success();
}

Status ShaderRenderer::loadPreset(const std::filesystem::path& slangpPath) {
    std::lock_guard lock(loadMutex_);
    transactional_.beginCompile();

    auto result = compilePreset(slangpPath);
    if (!result) {
        transactional_.discard();
        return result.status();
    }

    auto validation = transactional_.setCandidate(std::move(result.value()));
    if (!validation) {
        const auto* prev = transactional_.previousActive();
        transactional_.discard();
        if (prev) {
            return Status::failure(transactional_.formatRejection(*prev, validation));
        }
        return Status::failure("VALIDATION_ERROR: " + validation.message);
    }

    if (outputExtent_.width > 0 && outputExtent_.height > 0) {
        const auto* cand = transactional_.candidate();
        if (!cand) {
            transactional_.discard();
            return Status::failure("No candidate after setCandidate");
        }

        auto gpuStatus = backend_.loadCompiledPreset(*cand, outputExtent_.width, outputExtent_.height);
        if (!gpuStatus) {
            transactional_.rollback();
            restorePreviousState();
            return gpuStatus;
        }

        if (!gpuValidator_.profile().skipGpuTest) {
            auto gpuTest = runGpuValidationTest(*cand);
            if (gpuTest.result != GpuTestResult::Pass && gpuTest.result != GpuTestResult::Skip) {
                backend_.destroyPreset();
                transactional_.rollback();
                restorePreviousState();
                return Status::failure(gpuTest.format());
            }
        }
    }

    auto swapResult = transactional_.commit();
    if (!swapResult.committed) {
        transactional_.discard();
        return Status::failure(swapResult.rejectionReason);
    }

    compiled_ = transactional_.active() ?
        std::optional<CompiledPreset>(*transactional_.active()) :
        std::nullopt;

    backend_.setCompiledPreset(compiled_ ? &*compiled_ : nullptr);

    if (compiled_) {
        if (outputExtent_.width == 0 || outputExtent_.height == 0) {
            resources_.importGraph(compiled_->graph);
            parameters_ = {};
            for (const auto& pass : compiled_->passes) {
                parameters_.registerParameters(pass.parameters);
                pipelineLayouts_.getOrCreate(pass.fragment.reflection);
                pipelines_.getOrCreate(pass.preset.shaderPath.string());
            }
            rebuildStatistics();
        }

        if (config_.enableDebugDumps) {
            const auto debugDir = config_.rootDirectory / "logs" / "shader-runtime-vk";
            RenderGraphDump{}.writeDot(debugDir / "render-graph.dot", compiled_->graph);
            ValidationReport{}.write(debugDir / "validation-report.txt", *compiled_, statistics_);
            for (const auto& pass : compiled_->passes) {
                const auto path = debugDir / ("pass-" + std::to_string(pass.preset.index) + "-reflection.txt");
                ReflectionDump{}.write(path, pass.fragment.reflection);
            }
        }
    }

    loadedPresetPath_ = slangpPath;
    if (compiled_) {
        registerDependencies(*compiled_);
    }

    return Status::success();
}

Status ShaderRenderer::loadIndividualShader(const ShaderModule& module, const std::filesystem::path& shaderPath) {
    std::lock_guard lock(loadMutex_);
    transactional_.beginCompile();

    if (!module.compiled) {
        transactional_.discard();
        return Status::failure("Shader module is not compiled");
    }

    const auto* vertStage = module.getStage(ShaderStage::Vertex);
    const auto* fragStage = module.getStage(ShaderStage::Fragment);
    if (!fragStage) {
        transactional_.discard();
        return Status::failure("No fragment stage in shader module");
    }

    PresetIr synthPreset;
    synthPreset.path = shaderPath;
    synthPreset.baseDirectory = shaderPath.parent_path();
    synthPreset.name = shaderPath.stem().string();

    PresetPassIr synthPass;
    synthPass.index = 0;
    synthPass.shaderPath = shaderPath;
    synthPass.filterLinear = false;
    synthPass.wrapMode = WrapMode::ClampToBorder;
    synthPreset.passes.push_back(synthPass);

    RenderGraphBuilder graphBuilder;
    std::vector<ShaderReflection> reflections;
    reflections.push_back(fragStage->reflection);

    auto graph = graphBuilder.build(synthPreset, AliasDatabase{}, reflections);
    if (!graph) {
        transactional_.discard();
        return graph.status();
    }

    CompiledPreset compiled;
    compiled.preset = synthPreset;
    compiled.graph = graph.value();
    compiled.executionPlan = graphBuilder.compileExecutionPlan(compiled.graph);

    CompiledPass compiledPass;
    compiledPass.preset = synthPass;
    if (vertStage) {
        compiledPass.vertex.source = vertStage->source;
        compiledPass.vertex.spirv = vertStage->spirv;
        compiledPass.vertex.reflection = vertStage->reflection;
    }
    compiledPass.fragment.source = fragStage->source;
    compiledPass.fragment.spirv = fragStage->spirv;
    compiledPass.fragment.reflection = fragStage->reflection;
    compiled.passes.push_back(std::move(compiledPass));

    auto validation = transactional_.setCandidate(std::move(compiled));
    if (!validation) {
        const auto* prev = transactional_.previousActive();
        transactional_.discard();
        if (prev) {
            return Status::failure(transactional_.formatRejection(*prev, validation));
        }
        return Status::failure("VALIDATION_ERROR: " + validation.message);
    }

    if (outputExtent_.width > 0 && outputExtent_.height > 0) {
        const auto* cand = transactional_.candidate();
        if (!cand) {
            transactional_.discard();
            return Status::failure("No candidate after setCandidate");
        }

        auto gpuStatus = backend_.loadCompiledPreset(*cand, outputExtent_.width, outputExtent_.height);
        if (!gpuStatus) {
            transactional_.rollback();
            restorePreviousState();
            return gpuStatus;
        }
    }

    auto swapResult = transactional_.commit();
    if (!swapResult.committed) {
        transactional_.discard();
        return Status::failure(swapResult.rejectionReason);
    }

    compiled_ = transactional_.active() ?
        std::optional<CompiledPreset>(*transactional_.active()) :
        std::nullopt;

    backend_.setCompiledPreset(compiled_ ? &*compiled_ : nullptr);

    if (compiled_) {
        if (outputExtent_.width == 0 || outputExtent_.height == 0) {
            resources_.importGraph(compiled_->graph);
            parameters_ = {};
            for (const auto& pass : compiled_->passes) {
                parameters_.registerParameters(pass.parameters);
                pipelineLayouts_.getOrCreate(pass.fragment.reflection);
                pipelines_.getOrCreate(pass.preset.shaderPath.string());
            }
            rebuildStatistics();
        }
    }

    loadedPresetPath_ = shaderPath;
    return Status::success();
}

Status ShaderRenderer::resize(uint32_t outputWidth, uint32_t outputHeight) {
    outputExtent_ = Extent2D{outputWidth, outputHeight};
    if (compiled_) {
        return backend_.resize(outputWidth, outputHeight);
    }
    return Status::success();
}

Status ShaderRenderer::uploadSourceFrame(const FrameImage& frame) {
    if (frame.extent.width == 0 || frame.extent.height == 0 || frame.pixels == nullptr || frame.bytes == 0) {
        return Status::failure("Invalid source frame");
    }
    lastSourceFrame_ = frame;
    return backend_.uploadSourceFrame(frame);
}

Status ShaderRenderer::setParameter(std::string_view name, float value) {
    parameters_.set(std::string(name), value);
    return Status::success();
}

Status ShaderRenderer::render(const FrameContext& frame, const FrameImage* sourceFrame) {
    (void)frame;
    if (!compiled_) {
        return Status::failure("No preset loaded");
    }
    auto status = backend_.execute(compiled_->executionPlan, outputExtent_.width, outputExtent_.height, sourceFrame);
    if (status) {
        resources_.feedback().advance();
    }
    return status;
}

const CompiledPreset* ShaderRenderer::compiledPreset() const {
    return compiled_ ? &*compiled_ : nullptr;
}

void ShaderRenderer::runCompilationTests() {
    extern void runCompilationPipelineTests();
    runCompilationPipelineTests();
}

Result<CompiledPreset> ShaderRenderer::compilePreset(const std::filesystem::path& slangpPath) {
    SlangPresetParser parser;
    auto ast = parser.parseFile(slangpPath);
    if (!ast) return ast.status();
    auto ir = parser.buildIr(ast.value());
    if (!ir) return ir.status();

    IncludeResolver resolver;
    resolver.addRoot(ir.value().baseDirectory);
    resolver.addRoot(config_.rootDirectory / "Shaders");
    resolver.addRoot(config_.rootDirectory / "Shaders" / "bezel");
    resolver.addRoot(config_.rootDirectory / "Shaders" / "bezel" / "base");
    resolver.addRoot(config_.rootDirectory / "Shaders" / "bezel" / "common");
    resolver.addRoot(config_.rootDirectory / "Shaders" / "CRT");
    resolver.addRoot(config_.rootDirectory / "Shaders" / "CRT" / "shaders" / "crt-lottes-multipass");
    resolver.addRoot(config_.rootDirectory / "Shaders" / "presets");

    // Resolve shader paths through the include resolver (fixes incorrect relative paths)
    for (auto& pass : ir.value().passes) {
        if (!std::filesystem::exists(pass.shaderPath)) {
            auto resolved = resolver.resolve({}, pass.shaderPath.filename().string());
            if (resolved) {
                pass.shaderPath = resolved.value();
            }
        }
    }

    auto validation = PresetValidator{}.validate(ir.value());
    if (validation.hasErrors()) {
        return Status::failure(validation.summary());
    }

    ShaderPreprocessor preprocessor(resolver);
    StageSplitter splitter;
    ParameterExtractor parameterExtractor;
    ShaderReflectionParser reflectionParser;
    AliasResolver aliasResolver;

    CompiledPreset compiled;
    compiled.preset = ir.value();
    compiled.aliases = aliasResolver.build(compiled.preset);

    bool isGlslPreset = (slangpPath.extension() == ".glslp");

    // Build #define map from parameterOverrides so shaders can use #ifdef / #if
    std::unordered_map<std::string, std::string> initialDefines;
    for (const auto& override_ : compiled.preset.parameterOverrides) {
        initialDefines[override_.name] = override_.rawValue;
    }

    std::vector<ShaderReflection> passReflections;
    for (size_t pi = 0; pi < compiled.preset.passes.size(); pi++) {
        const auto& pass = compiled.preset.passes[pi];

        CompiledPass compiledPass;
        compiledPass.preset = pass;

        auto rawText = FileSystem::readText(pass.shaderPath);
        bool isRetroArchStyle = false;
        if (rawText && isGlslPreset) {
            auto retroSplit = StageSplitter::splitRetroArchRaw(rawText.value());
            isRetroArchStyle = retroSplit.isRetroArchStyle && !retroSplit.vertex.empty() && !retroSplit.fragment.empty();
        }

        if (isRetroArchStyle && rawText) {
            auto retroSplit = StageSplitter::splitRetroArchRaw(rawText.value());

            auto stripPragma = [](const std::string& src) -> std::string {
                std::istringstream in(src);
                std::ostringstream out;
                std::string line;
                while (std::getline(in, line)) {
                    auto t = line;
                    auto p = t.find_first_not_of(" \t\r");
                    if (p != std::string::npos) t = t.substr(p);
                    if (t.rfind("#pragma parameter", 0) == 0) continue;
                    if (t.rfind("#pragma", 0) == 0 && t.find("stage") == std::string::npos) continue;
                    out << line << "\n";
                }
                return out.str();
            };

            auto stripParamUniform = [](const std::string& src) -> std::string {
                std::istringstream in(src);
                std::ostringstream out;
                std::string line;
                int depth = 0;
                bool skipping = false;
                while (std::getline(in, line)) {
                    auto t = line;
                    auto p = t.find_first_not_of(" \t\r");
                    if (p != std::string::npos) t = t.substr(p);
                    if (!skipping && (t.find("#ifdef PARAMETER_UNIFORM") != std::string::npos ||
                                      t.find("#if defined(PARAMETER_UNIFORM)") != std::string::npos)) {
                        skipping = true;
                        depth = 1;
                        continue;
                    }
                    if (skipping) {
                        if (t.find("#if") == 0) ++depth;
                        if (t.find("#endif") != std::string::npos) {
                            --depth;
                            if (depth == 0) skipping = false;
                        }
                        continue;
                    }
                    out << line << "\n";
                }
                return out.str();
            };

            auto cleaned = stripPragma(rawText.value());
            std::string fragDefines = "#define FRAGMENT 1\n#define __VERSION__ 450\n#define GL_FRAGMENT_PRECISION_HIGH 1\n#define PARAMETER_UNIFORM 1\n";

            static const char* synthVert =
                "#version 450\n"
                "layout(location=0) in vec2 aPos;\n"
                "layout(location=1) in vec2 aUV;\n"
                "layout(location=0) out vec2 vTexCoord;\n"
                "layout(set=0,binding=0) uniform UBO { mat4 mvp; } ubo;\n"
                "void main() {\n"
                "    gl_Position = ubo.mvp * vec4(aPos, 0.0, 1.0);\n"
                "    vTexCoord = aUV;\n"
                "}\n";

            compiledPass.parameters = parameterExtractor.extract(cleaned);
            compiledPass.dependencies = {};
            compiledPass.vertex.source = std::string(synthVert);
            compiledPass.fragment.source = fragDefines + cleaned;

            if (config_.compileShadersToSpirv) {
                SlangCompiler compiler;
                compiler.setCache(shaderCache_.get());

                ShaderCompileRequest vertexRequest;
                vertexRequest.stage = ShaderStage::Vertex;
                vertexRequest.language = ShaderLanguage::GLSL;
                vertexRequest.source = compiledPass.vertex.source;
                vertexRequest.sourcePath = pass.shaderPath;
                vertexRequest.outputDirectory = cacheDirectory();
                vertexRequest.slangcPath = slangcPath();
                vertexRequest.includeDirectories = {compiled.preset.baseDirectory, config_.rootDirectory / "Shaders"};
                vertexRequest.dependencies = {};
                auto vertexResult = compiler.compile(vertexRequest);
                if (!vertexResult) return vertexResult.status();
                compiledPass.vertex.spirv = std::move(vertexResult.value().spirv);
                compiledPass.vertex.reflection = std::move(vertexResult.value().reflection);

                ShaderCompileRequest fragmentRequest;
                fragmentRequest.stage = ShaderStage::Fragment;
                fragmentRequest.language = ShaderLanguage::GLSL;
                fragmentRequest.source = compiledPass.fragment.source;
                fragmentRequest.sourcePath = pass.shaderPath;
                fragmentRequest.outputDirectory = cacheDirectory();
                fragmentRequest.slangcPath = slangcPath();
                fragmentRequest.includeDirectories = {compiled.preset.baseDirectory, config_.rootDirectory / "Shaders"};
                fragmentRequest.dependencies = {};
                auto fragmentResult = compiler.compile(fragmentRequest);
                if (!fragmentResult) return fragmentResult.status();
                compiledPass.fragment.spirv = std::move(fragmentResult.value().spirv);
                compiledPass.fragment.reflection = std::move(fragmentResult.value().reflection);
            } else {
                auto vertexReflection = reflectionParser.fromSourceLayout(compiledPass.vertex.source);
                if (!vertexReflection) return vertexReflection.status();
                auto fragmentReflection = reflectionParser.fromSourceLayout(compiledPass.fragment.source);
                if (!fragmentReflection) return fragmentReflection.status();
                compiledPass.vertex.reflection = std::move(vertexReflection.value());
                compiledPass.fragment.reflection = std::move(fragmentReflection.value());
            }
        } else {
            auto preprocessed = preprocessor.preprocessFile(pass.shaderPath, initialDefines);
            if (!preprocessed) return preprocessed.status();
            auto stages = splitter.split(preprocessed.value().source);
            if (!stages) return stages.status();

            compiledPass.parameters = parameterExtractor.extract(preprocessed.value().source);
            compiledPass.dependencies = preprocessed.value().dependencies;
            compiledPass.vertex.source = stages.value().vertex;
            compiledPass.fragment.source = stages.value().fragment;

            if (config_.compileShadersToSpirv) {
                SlangCompiler compiler;
                compiler.setCache(shaderCache_.get());
                ShaderCompileRequest vertexRequest;
                vertexRequest.stage = ShaderStage::Vertex;
                vertexRequest.language = isGlslPreset ? ShaderLanguage::GLSL : ShaderLanguage::Slang;
                vertexRequest.source = compiledPass.vertex.source;
                vertexRequest.sourcePath = pass.shaderPath;
                vertexRequest.outputDirectory = cacheDirectory();
                vertexRequest.slangcPath = slangcPath();
                vertexRequest.includeDirectories = {compiled.preset.baseDirectory, config_.rootDirectory / "Shaders"};
                vertexRequest.dependencies = preprocessed.value().dependencies;
                auto vertexResult = compiler.compile(vertexRequest);
                if (!vertexResult) return vertexResult.status();
                compiledPass.vertex.spirv = std::move(vertexResult.value().spirv);
                compiledPass.vertex.reflection = std::move(vertexResult.value().reflection);

                ShaderCompileRequest fragmentRequest = vertexRequest;
                fragmentRequest.stage = ShaderStage::Fragment;
                fragmentRequest.source = compiledPass.fragment.source;
                auto fragmentResult = compiler.compile(fragmentRequest);
                if (!fragmentResult) return fragmentResult.status();
                compiledPass.fragment.spirv = std::move(fragmentResult.value().spirv);
                compiledPass.fragment.reflection = std::move(fragmentResult.value().reflection);
            } else {
                auto vertexReflection = reflectionParser.fromSourceLayout(compiledPass.vertex.source);
                if (!vertexReflection) return vertexReflection.status();
                auto fragmentReflection = reflectionParser.fromSourceLayout(compiledPass.fragment.source);
                if (!fragmentReflection) return fragmentReflection.status();
                compiledPass.vertex.reflection = std::move(vertexReflection.value());
                compiledPass.fragment.reflection = std::move(fragmentReflection.value());
            }
        }

        passReflections.push_back(compiledPass.fragment.reflection);
        compiled.passes.push_back(std::move(compiledPass));
    }

    RenderGraphBuilder graphBuilder;
    auto graph = graphBuilder.build(compiled.preset, compiled.aliases, passReflections);
    if (!graph) return graph.status();
    compiled.graph = graph.value();
    compiled.executionPlan = graphBuilder.compileExecutionPlan(compiled.graph);
    return compiled;
}

void ShaderRenderer::rebuildStatistics() {
    statistics_ = {};
    if (!compiled_) return;
    statistics_.passCount = static_cast<uint32_t>(compiled_->passes.size());
    statistics_.imageCount = static_cast<uint32_t>(compiled_->graph.images.size());
    statistics_.samplerCount = static_cast<uint32_t>(compiled_->graph.samplers.size());
    for (const auto& pass : compiled_->passes) {
        statistics_.parameterCount += static_cast<uint32_t>(pass.parameters.size());
        statistics_.usedReflectionFallback = statistics_.usedReflectionFallback ||
            pass.fragment.reflection.fallbackSourceReflection;
    }
    statistics_.pipelineLayoutCount = static_cast<uint32_t>(pipelineLayouts_.size());
    statistics_.pipelineCount = static_cast<uint32_t>(pipelines_.size());
}

void ShaderRenderer::restorePreviousState() {
    const auto* restored = transactional_.active();
    if (restored) {
        compiled_ = *restored;
        resources_.importGraph(compiled_->graph);
        parameters_ = {};
        for (const auto& pass : compiled_->passes) {
            parameters_.registerParameters(pass.parameters);
            pipelineLayouts_.getOrCreate(pass.fragment.reflection);
            pipelines_.getOrCreate(pass.preset.shaderPath.string());
        }
        rebuildStatistics();

        if (outputExtent_.width > 0 && outputExtent_.height > 0) {
            backend_.loadCompiledPreset(*compiled_, outputExtent_.width, outputExtent_.height);
        }
    } else {
        compiled_ = std::nullopt;
    }
}

void ShaderRenderer::registerDependencies(const CompiledPreset& preset) {
    auto presetKey = preset.preset.path.string();
    depGraph_.registerPreset(presetKey);

    for (const auto& pass : preset.passes) {
        auto shaderKey = pass.preset.shaderPath.string();
        depGraph_.registerShader(shaderKey);
        depGraph_.addDependency(presetKey, shaderKey);

        for (const auto& dep : pass.dependencies) {
            auto depKey = dep.string();
            if (depGraph_.nodeType(depKey) == DepNodeType::Unknown) {
                depGraph_.registerInclude(depKey);
            }
            depGraph_.addDependency(shaderKey, depKey);
        }
    }
}

GpuTestDiagnostic ShaderRenderer::runGpuValidationTest(const CompiledPreset& preset) {
    GpuTestDiagnostic diag;
    diag.presetName = preset.preset.path.filename().string();
    diag.passIndex = static_cast<int>(preset.passes.size()) - 1;

    if (!backend_.renderer()) {
        diag.result = GpuTestResult::FailNoRenderer;
        diag.detail = "No VulkanRenderer set";
        return diag;
    }

    auto* renderer = backend_.renderer();
    uint32_t tw = gpuValidator_.profile().testWidth;
    uint32_t th = gpuValidator_.profile().testHeight;

    std::vector<uint8_t> testInput(static_cast<size_t>(tw) * th * 4);
    for (uint32_t y = 0; y < th; ++y) {
        for (uint32_t x = 0; x < tw; ++x) {
            size_t idx = (static_cast<size_t>(y) * tw + x) * 4;
            testInput[idx + 0] = static_cast<uint8_t>((x * 255) / tw);
            testInput[idx + 1] = static_cast<uint8_t>((y * 255) / th);
            testInput[idx + 2] = 128;
            testInput[idx + 3] = 255;
        }
    }

    renderer->requestScreenshot(tw, th);

    bool frameOk = renderer->beginFrame(tw, th);
    if (!frameOk) {
        diag.result = GpuTestResult::FailRenderFailed;
        diag.detail = "beginFrame failed";
        return diag;
    }

    renderer->uploadTextureToImage(testInput.data(), tw, th, false);
    renderer->endFrame();
    renderer->present();
    renderer->waitForIdle();
    renderer->finalizeScreenshot();

    if (!renderer->hasScreenshot()) {
        diag.result = GpuTestResult::FailReadbackFailed;
        diag.detail = "Screenshot not ready after render";
        return diag;
    }

    auto readback = renderer->takeScreenshot();
    gpuValidator_.setGpuTestData(readback, tw, th, diag.presetName, diag.passIndex);
    diag = gpuValidator_.runGpuTest();
    diag.presetName = preset.preset.path.filename().string();
    diag.passIndex = static_cast<int>(preset.passes.size()) - 1;
    return diag;
}

std::filesystem::path ShaderRenderer::cacheDirectory() const {
    return config_.shaderCacheDirectory.empty()
        ? (config_.rootDirectory / "build" / "shader-cache-vk")
        : config_.shaderCacheDirectory;
}

std::filesystem::path ShaderRenderer::slangcPath() const {
    return config_.slangcPath.empty()
        ? (config_.rootDirectory / "tools" / "slangc.exe")
        : config_.slangcPath;
}

}  // namespace monix::renderer_vk
