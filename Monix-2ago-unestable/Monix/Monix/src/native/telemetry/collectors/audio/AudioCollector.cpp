#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <string>

#include "AudioCollector.hpp"

#pragma comment(lib, "winmm.lib")

namespace monix {

void CollectAudioData(Snapshot& snapshot) {
  snapshot.audioOutputDeviceCount = waveOutGetNumDevs();
  snapshot.audioInputDeviceCount = waveInGetNumDevs();
  snapshot.audioMixerCount = mixerGetNumDevs();

  unsigned long masterVol = 0;
  if (waveOutGetVolume(0, &masterVol) == MMSYSERR_NOERROR) {
    snapshot.audioMasterVolume = masterVol;
    snapshot.audioMasterVolumeLeft = LOWORD(masterVol);
    snapshot.audioMasterVolumeRight = HIWORD(masterVol);
    snapshot.audioSystemMuted = (masterVol == 0) ? 1 : 0;
  }

  if (snapshot.audioInputDeviceCount > 0) {
    WAVEINCAPSW wic;
    if (waveInGetDevCapsW(0, &wic, sizeof(wic)) == MMSYSERR_NOERROR) {
      snapshot.audioSampleRate = 0;
    }
  }

  snapshot.audioWaveOutOpen = 0;
  snapshot.audioWaveInOpen = 0;

  DWORD mixerHash = 2166136261u;
  for (UINT i = 0; i < snapshot.audioMixerCount; ++i) {
    MIXERCAPSW mc;
    if (mixerGetDevCapsW(i, &mc, sizeof(mc)) == MMSYSERR_NOERROR) {
      for (int j = 0; mc.szPname[j]; ++j) {
        mixerHash ^= static_cast<unsigned char>(mc.szPname[j]);
        mixerHash *= 16777619u;
      }
    }
  }
  snapshot.audioDeviceHash = mixerHash;

  DWORD formatHash = 2166136261u;
  for (UINT i = 0; i < snapshot.audioOutputDeviceCount; ++i) {
    WAVEOUTCAPSW woc;
    if (waveOutGetDevCapsW(i, &woc, sizeof(woc)) == MMSYSERR_NOERROR) {
      for (int j = 0; woc.szPname[j]; ++j) {
        formatHash ^= static_cast<unsigned char>(woc.szPname[j]);
        formatHash *= 16777619u;
      }
      formatHash ^= woc.dwFormats;
      formatHash *= 16777619u;
      formatHash ^= woc.wChannels;
      formatHash *= 16777619u;
    }
  }
  snapshot.audioFormatHash = formatHash;

  DWORD svcStatus = 0;
  SERVICE_STATUS_PROCESS ssp;
  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_QUERY_LOCK_STATUS);
  if (scm) {
    SC_HANDLE svc = OpenServiceW(scm, L"Audiosrv", SERVICE_QUERY_STATUS);
    if (svc) {
      if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &svcStatus)) {
        snapshot.audioServiceRunning = (ssp.dwCurrentState == SERVICE_RUNNING) ? 1 : 0;
      }
      CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
  }

  snapshot.audioCodecEvents = 0;
  snapshot.audioDecoderErrors = 0;
}

}
