#pragma once

#include "../core/Diagnostics.hpp"
#include "SlangPreset.hpp"

namespace monix::renderer_vk {

class PresetValidator {
public:
    Diagnostics validate(const PresetIr& preset) const;
};

}  // namespace monix::renderer_vk
