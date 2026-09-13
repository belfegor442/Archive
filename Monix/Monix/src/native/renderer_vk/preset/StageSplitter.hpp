#pragma once

#include "../core/Result.hpp"

#include <string>

namespace monix::renderer_vk {

struct StageSplitResult {
    std::string common;
    std::string vertex;
    std::string fragment;
    bool hasVertex = false;
    bool hasFragment = false;
};

struct RetroArchSplitResult {
    std::string before;   // code before #if defined(VERTEX)
    std::string vertex;   // code inside #if defined(VERTEX)
    std::string fragment; // code inside #elif defined(FRAGMENT)
    bool isRetroArchStyle = false;
};

class StageSplitter {
public:
    Result<StageSplitResult> split(const std::string& source) const;

    static RetroArchSplitResult splitRetroArchRaw(const std::string& source);
};

}  // namespace monix::renderer_vk
