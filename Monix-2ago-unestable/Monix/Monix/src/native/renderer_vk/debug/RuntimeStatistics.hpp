#pragma once

#include <cstdint>

namespace monix::renderer_vk {

struct RuntimeStatistics {
    uint32_t passCount = 0;
    uint32_t imageCount = 0;
    uint32_t samplerCount = 0;
    uint32_t parameterCount = 0;
    uint32_t pipelineLayoutCount = 0;
    uint32_t pipelineCount = 0;
    bool usedReflectionFallback = false;
};

}  // namespace monix::renderer_vk
