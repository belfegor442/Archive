#pragma once

#include "../SettingsRegistry.hpp"

#include <array>

namespace monix {

const wchar_t* const* GetAccentColorNames();
const wchar_t* const* GetDefaultTabNames();
const wchar_t* const* GetCrtScanlineModeNames();
const wchar_t* const* GetBorderImagePresetNames();
const wchar_t* const* GetLanguageNames();
const wchar_t* const* GetThemeModeNames();
const wchar_t* const* GetProcessPriorityNames();

std::string ToLowerAscii(std::string s);
std::string TrimAscii(const std::string& s);
bool TryParseBool(const std::string& value, bool& out);
bool TryParseInt(const std::string& value, int& out);
bool TryParseUInt(const std::string& value, unsigned int& out);
bool TryParseDouble(const std::string& value, double& out);

const std::array<SettingDef, kSettingCount>& BuildRegistry();
const std::array<SettingDef, kSettingCount>& GetSettingsRegistry();
const SettingDef* FindSettingDef(SettingId id);

}
