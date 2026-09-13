#pragma once

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace monix::renderer_vk {

class Hash {
public:
    static uint64_t fnv1a(std::string_view text) {
        uint64_t value = 14695981039346656037ull;
        for (unsigned char c : text) {
            value ^= static_cast<uint64_t>(c);
            value *= 1099511628211ull;
        }
        return value;
    }

    static std::string hex(uint64_t value) {
        std::ostringstream out;
        out << std::hex << std::setw(16) << std::setfill('0') << value;
        return out.str();
    }

    static std::string combine(std::initializer_list<std::string_view> parts) {
        uint64_t value = 14695981039346656037ull;
        for (auto part : parts) {
            value ^= fnv1a(part);
            value *= 1099511628211ull;
        }
        return hex(value);
    }
};

}  // namespace monix::renderer_vk
