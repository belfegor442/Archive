#pragma once

#include "CpuState.hpp"
#include "MemoryState.hpp"
#include "GpuState.hpp"
#include "StorageState.hpp"
#include "NetworkState.hpp"
#include "ProcessTable.hpp"
#include "PowerState.hpp"
#include "ThermalState.hpp"
#include "SecurityState.hpp"
#include "FilesystemState.hpp"
#include "RegistryState.hpp"
#include "AudioState.hpp"
#include "DisplayState.hpp"
#include "KernelState.hpp"
#include "ReliabilityState.hpp"
#include "HardwareBoardState.hpp"

namespace monix {

struct SystemSnapshot {
  uint64_t id = 0;
  uint64_t timestampNs = 0;
  uint64_t wallTimeMs = 0;
  std::wstring host = L"UNKNOWN";
  uint64_t uptimeSeconds = 0;
  int bootPhase = 0;

  CpuState cpu;
  MemoryState memory;
  GpuState gpu;
  StorageState storage;
  NetworkState network;
  ProcessTable processes;
  PowerState power;
  ThermalState thermal;
  SecurityState security;
  FilesystemState filesystem;
  RegistryState registry;
  AudioState audio;
  DisplayState display;
  KernelState kernel;
  ReliabilityState reliability;
  HardwareBoardState hardware;
};

}
