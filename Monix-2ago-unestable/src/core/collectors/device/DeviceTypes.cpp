#include "DeviceTypes.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace monix::collectors::device {

const char* DeviceClassName(DeviceClass c) {
  switch (c) {
    case DeviceClass::USB:       return "USB";
    case DeviceClass::PCIe:      return "PCIe";
    case DeviceClass::Bluetooth: return "Bluetooth";
    case DeviceClass::Storage:   return "Storage";
    case DeviceClass::Display:   return "Display";
    case DeviceClass::Audio:     return "Audio";
    case DeviceClass::Network:   return "Network";
    case DeviceClass::Input:     return "Input";
    case DeviceClass::Camera:    return "Camera";
    case DeviceClass::Printer:   return "Printer";
    case DeviceClass::Other:     return "Other";
    case DeviceClass::Unknown:   return "Unknown";
  }
  return "Unknown";
}

DeviceClass DeviceClassFromName(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (lower == "usb") return DeviceClass::USB;
  if (lower == "pcie") return DeviceClass::PCIe;
  if (lower == "bluetooth" || lower == "bt") return DeviceClass::Bluetooth;
  if (lower == "storage" || lower == "disk" || lower == "drive") return DeviceClass::Storage;
  if (lower == "display" || lower == "video" || lower == "monitor") return DeviceClass::Display;
  if (lower == "audio" || lower == "sound") return DeviceClass::Audio;
  if (lower == "network" || lower == "net" || lower == "wifi" || lower == "ethernet") return DeviceClass::Network;
  if (lower == "input" || lower == "hid" || lower == "keyboard" || lower == "mouse") return DeviceClass::Input;
  if (lower == "camera" || lower == "webcam") return DeviceClass::Camera;
  if (lower == "printer" || lower == "print") return DeviceClass::Printer;
  return DeviceClass::Unknown;
}

DeviceClass DeviceClassFromInstanceId(const std::string& instance_id) {
  std::string lower = instance_id;
  std::transform(lower.begin(), lower.end(), lower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (lower.find("usb") != std::string::npos) {
    if (lower.find("class_07") != std::string::npos ||
        lower.find("class_08") != std::string::npos)
      return DeviceClass::Storage;
    if (lower.find("class_03") != std::string::npos)
      return DeviceClass::Display;
    if (lower.find("class_04") != std::string::npos)
      return DeviceClass::Audio;
    if (lower.find("class_01") != std::string::npos)
      return DeviceClass::Input;
    if (lower.find("class_0e") != std::string::npos)
      return DeviceClass::Camera;
    return DeviceClass::USB;
  }

  if (lower.find("bluetooth") != std::string::npos ||
      lower.find("bth") != std::string::npos)
    return DeviceClass::Bluetooth;
  if (lower.find("pcie") != std::string::npos ||
      lower.find("pci\\") != std::string::npos ||
      lower.find("pci_") != std::string::npos)
    return DeviceClass::PCIe;
  if (lower.find("display") != std::string::npos ||
      lower.find("monitor") != std::string::npos)
    return DeviceClass::Display;
  if (lower.find("net") != std::string::npos ||
      lower.find("wifi") != std::string::npos ||
      lower.find("ethernet") != std::string::npos)
    return DeviceClass::Network;
  if (lower.find("hid") != std::string::npos ||
      lower.find("keyboard") != std::string::npos ||
      lower.find("mouse") != std::string::npos)
    return DeviceClass::Input;
  if (lower.find("print") != std::string::npos)
    return DeviceClass::Printer;
  if (lower.find("disk") != std::string::npos ||
      lower.find("storage") != std::string::npos)
    return DeviceClass::Storage;
  if (lower.find("audio") != std::string::npos ||
      lower.find("sound") != std::string::npos)
    return DeviceClass::Audio;

  return DeviceClass::Unknown;
}

DeviceClass DeviceClassFromGuid(const std::string& guid) {
  std::string lower = guid;
  std::transform(lower.begin(), lower.end(), lower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (lower == "4d36e967-e325-11ce-bfc1-08002be10318") return DeviceClass::Storage;
  if (lower == "4d36e968-e325-11ce-bfc1-08002be10318") return DeviceClass::Display;
  if (lower == "4d36e96c-e325-11ce-bfc1-08002be10318") return DeviceClass::Audio;
  if (lower == "4d36e96e-e325-11ce-bfc1-08002be10318") return DeviceClass::Network;
  if (lower == "4d36e96b-e325-11ce-bfc1-08002be10318") return DeviceClass::Input;
  if (lower == "6bdd1fc6-810f-11d0-bec7-08002be2092f") return DeviceClass::Camera;
  if (lower == "4d36e979-e325-11ce-bfc1-08002be10318") return DeviceClass::Printer;

  return DeviceClass::Unknown;
}

const char* DeviceOriginName(DeviceOrigin o) {
  switch (o) {
    case DeviceOrigin::Internal: return "Internal";
    case DeviceOrigin::External: return "External";
    case DeviceOrigin::Virtual:  return "Virtual";
    case DeviceOrigin::Unknown:  return "Unknown";
  }
  return "Unknown";
}

DeviceOrigin DetectDeviceOrigin(const std::string& instance_id, const std::string& bus) {
  std::string idLower = instance_id;
  std::transform(idLower.begin(), idLower.end(), idLower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  std::string busLower = bus;
  std::transform(busLower.begin(), busLower.end(), busLower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (busLower.find("usb") != std::string::npos)
    return DeviceOrigin::External;
  if (busLower.find("pcie") != std::string::npos ||
      busLower.find("pci") != std::string::npos)
    return DeviceOrigin::Internal;
  if (busLower.find("virtual") != std::string::npos ||
      busLower.find("vmbus") != std::string::npos ||
      busLower.find("hyper") != std::string::npos)
    return DeviceOrigin::Virtual;
  if (idLower.find("usb") != std::string::npos)
    return DeviceOrigin::External;
  if (idLower.find("pcie") != std::string::npos ||
      idLower.find("pci") != std::string::npos)
    return DeviceOrigin::Internal;
  if (idLower.find("virtual") != std::string::npos ||
      idLower.find("root") != std::string::npos)
    return DeviceOrigin::Virtual;

  return DeviceOrigin::Unknown;
}

const char* DeviceEventKindName(DeviceEventKind k) {
  switch (k) {
    case DeviceEventKind::Connected:    return "Connected";
    case DeviceEventKind::Disconnected: return "Disconnected";
    case DeviceEventKind::Changed:      return "Changed";
    case DeviceEventKind::Available:    return "Available";
    case DeviceEventKind::Unavailable:  return "Unavailable";
  }
  return "Unknown";
}

std::string DeviceEventKindAction(DeviceEventKind k) {
  switch (k) {
    case DeviceEventKind::Connected:    return "connected";
    case DeviceEventKind::Disconnected: return "disconnected";
    case DeviceEventKind::Changed:      return "changed";
    case DeviceEventKind::Available:    return "available";
    case DeviceEventKind::Unavailable:  return "unavailable";
  }
  return "unknown";
}

bool DeviceIdentity::hasIds() const {
  return !vendor_id.empty() && !product_id.empty();
}

bool DeviceIdentity::hasInstanceId() const {
  return !instance_id.empty();
}

std::string DeviceIdentity::summary() const {
  std::string result;
  if (!friendly_name.empty()) {
    result = friendly_name;
  } else if (!description.empty()) {
    result = description;
  } else if (!vendor_id.empty() && !product_id.empty()) {
    result = vendor_id + ":" + product_id;
  } else if (!instance_id.empty()) {
    result = instance_id;
  } else {
    result = "Unknown Device";
  }
  return result;
}

bool DeviceInfo::isValid() const {
  return id != 0 || identity.hasInstanceId();
}

bool DeviceInfo::isSameDevice(const DeviceInfo& other) const {
  if (id != 0 && other.id != 0) return id == other.id;
  if (identity.hasInstanceId() && other.identity.hasInstanceId())
    return identity.instance_id == other.identity.instance_id;
  if (identity.hasIds() && other.identity.hasIds())
    return identity.vendor_id == other.identity.vendor_id &&
           identity.product_id == other.identity.product_id;
  return false;
}

const char* DeviceDetectionOriginName(DeviceDetectionOrigin o) {
  switch (o) {
    case DeviceDetectionOrigin::WMI:      return "WMI";
    case DeviceDetectionOrigin::SetupAPI: return "SetupAPI";
    case DeviceDetectionOrigin::Manual:   return "Manual";
    case DeviceDetectionOrigin::Polling:  return "Polling";
  }
  return "Unknown";
}

}  // namespace monix::collectors::device
