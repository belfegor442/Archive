#pragma once

#include <cstdint>
#include <string>

struct VkCtx;

namespace vk_inst {
bool createInstance(VkCtx& ctx);
bool pickPhysicalDevice(VkCtx& ctx);
void loadInstanceFuncs(VkCtx& ctx);
std::string getVendor(VkCtx& ctx);
std::string getRenderer(VkCtx& ctx);
std::string getVersion(VkCtx& ctx);
}
