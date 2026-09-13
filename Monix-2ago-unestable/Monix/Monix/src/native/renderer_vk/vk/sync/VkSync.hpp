#pragma once

#include <cstdint>

struct VkCtx;

namespace vk_sync {
bool createSyncObjects(VkCtx& ctx);
}
