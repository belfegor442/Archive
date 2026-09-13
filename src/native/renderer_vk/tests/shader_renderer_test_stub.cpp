#include "../public/ShaderRenderer.hpp"

namespace monix::renderer_vk {

ShaderRenderer::ShaderRenderer(RendererConfig config) : config_(std::move(config)) {}
ShaderRenderer::~ShaderRenderer() = default;
Status ShaderRenderer::initialize() { return Status::success(); }
void ShaderRenderer::setRenderer(VulkanRenderer*) {}
Status ShaderRenderer::resize(uint32_t, uint32_t) { return Status::success(); }
Status ShaderRenderer::uploadSourceFrame(const FrameImage&) { return Status::success(); }
Status ShaderRenderer::setParameter(std::string_view, float) { return Status::success(); }
Status ShaderRenderer::render(const FrameContext&, const FrameImage*) { return Status::success(); }
Status ShaderRenderer::enableHotReload() { return Status::success(); }
Status ShaderRenderer::disableHotReload() { return Status::success(); }
Status ShaderRenderer::pollHotReload() { return Status::success(); }
void ShaderRenderer::runCompilationTests() {}
const CompiledPreset* ShaderRenderer::compiledPreset() const { return nullptr; }
Status ShaderRenderer::loadPreset(const std::filesystem::path&) { return Status::success(); }

Status ShaderRenderer::loadIndividualShader(const ShaderModule&, const std::filesystem::path&) {
    return Status::success();
}

Result<CompiledPreset> ShaderRenderer::compilePreset(const std::filesystem::path&) {
    return Status::success();
}
void ShaderRenderer::rebuildStatistics() {}
void ShaderRenderer::restorePreviousState() {}
void ShaderRenderer::registerDependencies(const CompiledPreset&) {}
std::filesystem::path ShaderRenderer::cacheDirectory() const { return {}; }
std::filesystem::path ShaderRenderer::slangcPath() const { return {}; }
GpuTestDiagnostic ShaderRenderer::runGpuValidationTest(const CompiledPreset&) { return {}; }

}
