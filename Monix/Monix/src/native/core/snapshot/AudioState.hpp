#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>

namespace monix {

struct AudioState {
  int outputDeviceCount = 0;
  int inputDeviceCount = 0;
  int mixerCount = 0;
  DWORD masterVolume = 0;
  int masterMuted = 0;
  DWORD masterVolumeLeft = 0;
  DWORD masterVolumeRight = 0;
  int sampleRate = 0;
  int bitsPerSample = 0;
  int channels = 0;
  int waveOutOpen = 0;
  int waveInOpen = 0;
  int serviceRunning = 1;
  int micMuted = 0;
  int systemMuted = 0;
  int headphoneJack = -1;
  int spatialization = 0;
  int playbackActive = 0;
  int recordingActive = 0;
  unsigned long deviceHash = 0;
  unsigned long formatHash = 0;
  int latencyMs = 0;
  int codecEvents = 0;
  int decoderErrors = 0;
};

}
