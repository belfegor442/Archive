#include "MonixApp.hpp"

#include "SettingGroups.hpp"
#include "AdjustSetting.hpp"
#include "../ui/CoreMonitorTheme.hpp"

#include <string>

using namespace monix;

std::wstring MonixApp::SettingLabel(SettingId id) const {
  return monix::RegistrySettingLabel(id);
}

std::wstring MonixApp::SettingValueText(SettingId id) const {
  std::wstring registryText = monix::RegistrySettingValueText(id, config_);
  if (!registryText.empty()) return registryText;

  const auto yesNo = [](bool value) {
    return std::wstring(value ? L"YES" : L"NO");
  };

  switch (id) {
    case SettingId::LogLevel: return LogLevelText(config_.logLevel);
    case SettingId::LogViewMode: return LogViewModeText(config_.logViewMode);
    case SettingId::FontFaceIndex: {
      if (!paths_.fontList.empty()) {
        const auto& font = paths_.fontList[state_.currentFontIndex % paths_.fontList.size()];
        return font.displayName;
      }
      return L"DEFAULT";
    }
    case SettingId::FrameTargetFps: {
      if (config_.frameTargetFps == 0) return L"UNLIMITED";
      return std::to_wstring(config_.frameTargetFps) + L" FPS";
    }
    case SettingId::WindowOpacity: {
      wchar_t buf[16]; swprintf(buf, 16, L"%d%%", static_cast<int>(config_.windowOpacity * 100));
      return buf;
    }
    default: return L"-";
  }
}

SettingMutation MonixApp::AdjustSetting(SettingId id, int direction) {
  auto mutation = monix::AdjustSetting(config_, id, direction, paths_.fontList, state_.currentFontIndex);

  if (id == SettingId::FontFaceIndex && mutation.changed) {
    SwitchFont(direction < 0 ? -1 : 1);
    const auto& font = paths_.fontList[state_.currentFontIndex];
    PushLog(L"ENGINE", L"INFO", L"Font switched to " + font.displayName + L" (" + font.faceName + L").", ColorRole::UserInput, L"engine", L"font", L"name=" + font.displayName);
  }

  return mutation;
}

void MonixApp::CommitSettingMutation(const SettingMutation& mutation) {
  if (!mutation.changed) {
    return;
  }

  if (mutation.clearNotifications || config_.notificationMaxStack > 0) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (mutation.clearNotifications) {
      state_.notifState.items.clear();
    }
    if (config_.notificationMaxStack > 0 && static_cast<int>(state_.notifState.items.size()) > config_.notificationMaxStack) {
      state_.notifState.items.resize(config_.notificationMaxStack);
    }
  }
  if (mutation.trimHistory) {
    TrimHistoryBuffers();
  }

  if (mutation.id == SettingId::LogLevel && logManager_) {
    logManager_->SetMinLevel(config_.logLevel);
  }
  if (mutation.id == SettingId::LogDeduplicate && logManager_) {
    logManager_->SetDeduplicate(config_.logDeduplicate);
  }
  if (mutation.id == SettingId::LogMilliseconds && logManager_) {
    logManager_->SetLogMilliseconds(config_.logMilliseconds);
  }

  SaveConfig();
  if (mutation.recreateFonts) {
    CreateUiFonts();
  }
  if (mutation.resetFrameTimer) {
    SetFrameTimer();
  }
  if (mutation.requestRefresh) {
    RequestRefresh();
  }

  PushLog(
    L"CONFIG",
    L"INFO",
    mutation.label + L" updated to " + SettingValueText(mutation.id) + L".",
    ColorRole::UserInput,
    L"config",
    L"settings-panel",
    L"live=true"
  );
  SetToast(mutation.label + L" saved");
}
