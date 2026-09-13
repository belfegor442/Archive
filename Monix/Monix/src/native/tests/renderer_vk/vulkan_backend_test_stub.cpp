#include "../vulkan/VulkanBackend.hpp"

namespace monix::renderer_vk {

struct VulkanBackend::Impl {};

VulkanBackend::VulkanBackend() : impl_(std::make_unique<Impl>()) {}
VulkanBackend::~VulkanBackend() = default;
Status VulkanBackend::initialize() { return Status::success(); }
void VulkanBackend::setRenderer(VulkanRenderer*) {}
Status VulkanBackend::resize(uint32_t, uint32_t) { return Status::success(); }
Status VulkanBackend::uploadSourceFrame(const FrameImage&) { return Status::success(); }
Status VulkanBackend::execute(const ExecutionPlan&, uint32_t, uint32_t, const FrameImage*) { return Status::success(); }
Status VulkanBackend::loadCompiledPreset(const CompiledPreset&, uint32_t, uint32_t) { return Status::success(); }
void VulkanBackend::setCompiledPreset(const CompiledPreset*) {}
void VulkanBackend::destroyPreset() {}
RuntimeBuildInfo VulkanBackend::buildInfo() const { return {}; }
std::vector<uint32_t> VulkanBackend::spirvToWords(const std::vector<unsigned char>&) { return {}; }
std::vector<float> VulkanBackend::buildPushDefaults(const CompiledPass&) const { return {}; }
uint32_t VulkanBackend::pickUboSize(const CompiledPass&) const { return 0; }

}
