#pragma once

#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstdint>
#include <limits>

namespace archive::core::utils {

inline std::string generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
    std::ostringstream oss;
    oss << std::hex << dis(gen) << dis(gen);
    std::string id = oss.str();
    id.resize(32);
    return id;
}

inline std::string now_iso() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace archive::core::utils
