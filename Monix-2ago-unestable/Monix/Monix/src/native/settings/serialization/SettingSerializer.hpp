#pragma once

#include "../SettingsRegistry.hpp"

#include <ostream>
#include <string>

namespace monix {

std::wstring RegistrySettingLabel(SettingId id);
std::wstring RegistrySettingValueText(SettingId id, const Config& config);
bool RegistrySaveSetting(SettingId id, const Config& config, std::ostream& output);
bool RegistryLoadSetting(const std::string& key, const std::string& value, Config& config);

}
