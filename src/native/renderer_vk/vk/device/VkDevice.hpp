#pragma once

#include <cstdint>

struct VkCtx;

namespace vk_dev {
bool createLogicalDevice(VkCtx& ctx);
void loadDeviceFuncs(VkCtx& ctx);
}
