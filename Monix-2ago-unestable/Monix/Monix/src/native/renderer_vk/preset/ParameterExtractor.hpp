#pragma once

#include <string>
#include <vector>

namespace monix::renderer_vk {

enum class ParameterType {
    Float,
    Int,
    Bool
};

struct ShaderParameter {
    std::string name;
    std::string description;
    ParameterType type = ParameterType::Float;
    float defaultValue = 0.0f;
    float minimum = 0.0f;
    float maximum = 0.0f;
    float step = 0.0f;
};

class ParameterExtractor {
public:
    std::vector<ShaderParameter> extract(const std::string& source) const;
};

}  // namespace monix::renderer_vk
