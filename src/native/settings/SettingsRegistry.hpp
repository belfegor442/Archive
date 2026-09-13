#pragma once

#include "MonixConfigTypes.hpp"

#include <array>
#include <cstddef>
#include <ostream>
#include <string>

namespace monix {

enum class SettingType {
  Bool,
  Int,
  UInt,
  Double,
  Enum,
  Hotkey,
  Special
};

struct SettingDef {
  SettingId id;
  const wchar_t* label;
  const char* configKey;
  SettingType type;
  double rangeMin;
  double rangeMax;
  double rangeStep;
  int displayPrecision;
  const wchar_t* unit;
  int enumCount;
  const wchar_t* const* enumNames;
};

constexpr size_t kSettingCount = 78;
static_assert(kSettingCount == 78, "kSettingCount must match SettingId enum count");
const std::array<SettingDef, kSettingCount>& GetSettingsRegistry();
const SettingDef* FindSettingDef(SettingId id);

std::wstring RegistrySettingLabel(SettingId id);
std::wstring RegistrySettingValueText(SettingId id, const Config& config);
bool RegistrySaveSetting(SettingId id, const Config& config, std::ostream& output);
bool RegistryLoadSetting(const std::string& key, const std::string& value, Config& config);

}
