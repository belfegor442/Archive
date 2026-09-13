#include "StageSplitter.hpp"

#include <sstream>

namespace monix::renderer_vk {
namespace {

enum class Stage {
    Common,
    Vertex,
    Fragment
};

bool containsStage(const std::string& line, const char* stage) {
    return line.find("#pragma") != std::string::npos &&
           line.find("stage") != std::string::npos &&
           line.find(stage) != std::string::npos;
}

bool isVertexDefine(const std::string& line) {
    return line.find("#if") != std::string::npos &&
           line.find("defined") != std::string::npos &&
           line.find("VERTEX") != std::string::npos;
}

bool isFragmentDefine(const std::string& line) {
    return (line.find("#elif") != std::string::npos || line.find("#if") != std::string::npos) &&
           line.find("defined") != std::string::npos &&
           line.find("FRAGMENT") != std::string::npos;
}

}  // namespace

RetroArchSplitResult StageSplitter::splitRetroArchRaw(const std::string& source) {
    RetroArchSplitResult result;

    std::istringstream input(source);
    std::string line;
    std::ostringstream before;
    std::ostringstream vertex;
    std::ostringstream fragment;

    enum class RawStage { Before, Vertex, Fragment, Done };
    RawStage stage = RawStage::Before;
    int depth = 0;

    while (std::getline(input, line)) {
        if (stage == RawStage::Before) {
            if (isVertexDefine(line)) {
                stage = RawStage::Vertex;
                result.isRetroArchStyle = true;
                depth = 1;
                continue;
            }
            before << line << "\n";
        } else if (stage == RawStage::Vertex) {
            if (isFragmentDefine(line)) {
                stage = RawStage::Fragment;
                depth = 1;
                continue;
            }
            if (line.find("#if") != std::string::npos && line.find("#elif") == std::string::npos) {
                ++depth;
            } else if (line.find("#endif") != std::string::npos) {
                --depth;
                if (depth == 0) {
                    stage = RawStage::Done;
                    continue;
                }
            }
            vertex << line << "\n";
        } else if (stage == RawStage::Fragment) {
            if (line.find("#if") != std::string::npos && line.find("#elif") == std::string::npos) {
                ++depth;
            } else if (line.find("#endif") != std::string::npos) {
                --depth;
                if (depth == 0) {
                    stage = RawStage::Done;
                    continue;
                }
            }
            fragment << line << "\n";
        }
    }

    result.before = before.str();
    result.vertex = vertex.str();
    result.fragment = fragment.str();
    return result;
}

Result<StageSplitResult> StageSplitter::split(const std::string& source) const {
    StageSplitResult result;
    Stage stage = Stage::Common;
    std::ostringstream common;
    std::ostringstream vertex;
    std::ostringstream fragment;

    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
        if (containsStage(line, "vertex") || isVertexDefine(line)) {
            stage = Stage::Vertex;
            result.hasVertex = true;
            continue;
        }
        if (containsStage(line, "fragment") || isFragmentDefine(line)) {
            stage = Stage::Fragment;
            result.hasFragment = true;
            continue;
        }

        switch (stage) {
        case Stage::Common:
            common << line << "\n";
            break;
        case Stage::Vertex:
            vertex << line << "\n";
            break;
        case Stage::Fragment:
            fragment << line << "\n";
            break;
        }
    }

    result.common = common.str();
    result.vertex = result.common + vertex.str();
    result.fragment = result.common + fragment.str();

    if (!result.hasVertex && !result.hasFragment) {
        result.fragment = source;
        result.hasFragment = true;
    }
    if (result.hasVertex && result.vertex.empty()) {
        return Status::failure("Vertex stage was declared but produced no source");
    }
    if (result.hasFragment && result.fragment.empty()) {
        return Status::failure("Fragment stage was declared but produced no source");
    }
    return result;
}

}  // namespace monix::renderer_vk
