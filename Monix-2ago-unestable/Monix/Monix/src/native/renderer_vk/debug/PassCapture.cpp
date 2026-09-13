#include "PassCapture.hpp"

#include <fstream>

namespace monix::renderer_vk {

Status PassCapture::writeRgba8Ppm(const std::filesystem::path& path,
                                  uint32_t width,
                                  uint32_t height,
                                  const std::vector<unsigned char>& rgba) const {
    if (rgba.size() < static_cast<size_t>(width) * height * 4u) {
        return Status::failure("Pass capture buffer is too small");
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return Status::failure("Cannot write pass capture: " + path.string());
    }
    out << "P6\n" << width << " " << height << "\n255\n";
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        out.put(static_cast<char>(rgba[i * 4 + 0]));
        out.put(static_cast<char>(rgba[i * 4 + 1]));
        out.put(static_cast<char>(rgba[i * 4 + 2]));
    }
    return Status::success();
}

}  // namespace monix::renderer_vk
