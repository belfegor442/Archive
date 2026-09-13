#pragma once

#include "../core/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace monix::renderer_vk {

class PassCapture {
public:
    Status writeRgba8Ppm(const std::filesystem::path& path,
                         uint32_t width,
                         uint32_t height,
                         const std::vector<unsigned char>& rgba) const;
};

}  // namespace monix::renderer_vk
