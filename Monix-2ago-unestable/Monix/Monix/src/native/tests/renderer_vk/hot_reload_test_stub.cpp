#include "../shader_runtime/reload/ShaderHotReload.hpp"

namespace monix::renderer_vk {

ShaderHotReload::ShaderHotReload(HotReloadConfig config) : config_(std::move(config)) {}
ShaderHotReload::~ShaderHotReload() = default;
void ShaderHotReload::start() {}
void ShaderHotReload::stop() {}
void ShaderHotReload::setCallback(ReloadCallback) {}
void ShaderHotReload::setDependencyGraph(ShaderDependencyGraph*) {}
void ShaderHotReload::addWatchPath(const std::filesystem::path&) {}
void ShaderHotReload::removeWatchPath(const std::filesystem::path&) {}
bool ShaderHotReload::checkForChanges() { return false; }
bool ShaderHotReload::processPendingReloads() { return false; }
bool ShaderHotReload::processReloadQueue() { return false; }
bool ShaderHotReload::hasPendingRequests() const { return false; }
size_t ShaderHotReload::pendingRequestCount() const { return 0; }
bool ShaderHotReload::isRelevantExtension(const std::filesystem::path&) const { return false; }
uint64_t ShaderHotReload::computeContentHash(const std::filesystem::path&) const { return 0; }
uint64_t ShaderHotReload::getFileWriteTime(const std::filesystem::path&) const { return 0; }
void ShaderHotReload::processEvent(const std::filesystem::path&) {}
void ShaderHotReload::enqueueRequest(ShaderReloadRequest) {}
HotReloadConfig ShaderHotReload::defaultConfig(const std::filesystem::path&) { return {}; }

}
