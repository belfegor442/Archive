#include "AudioMultimediaRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <cstdlib>

namespace monix {

void AudioMultimediaRule::Evaluate(const Snapshot& current,
                                   const Snapshot* previous,
                                   std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const auto& prev = *previous;

  // 1. Audio device connect
  if (current.audioOutputDeviceCount > prev.audioOutputDeviceCount && prev.audioOutputDeviceCount > 0) {
    findings.push_back({
      L"Audio output device connected.",
      L"New audio output device detected.",
      L"Outputs: " + std::to_wstring(prev.audioOutputDeviceCount) + L" -> " + std::to_wstring(current.audioOutputDeviceCount) + L".",
      4
    });
  }

  // 2. Audio device disconnect
  if (current.audioOutputDeviceCount < prev.audioOutputDeviceCount && prev.audioOutputDeviceCount > 0) {
    findings.push_back({
      L"Audio output device disconnected.",
      L"An audio output device has been removed.",
      L"Outputs: " + std::to_wstring(prev.audioOutputDeviceCount) + L" -> " + std::to_wstring(current.audioOutputDeviceCount) + L".",
      6
    });
  }

  // 3. Default output change
  if (current.audioDeviceHash != prev.audioDeviceHash && prev.audioDeviceHash != 0) {
    findings.push_back({
      L"Default audio output device changed.",
      L"The audio device topology has changed.",
      L"Device hash changed.",
      10
    });
  }

  // 4. Default input change
  if (current.audioInputDeviceCount != prev.audioInputDeviceCount && prev.audioInputDeviceCount > 0) {
    findings.push_back({
      L"Audio input device count changed.",
      L"The number of audio input devices has changed.",
      L"Inputs: " + std::to_wstring(prev.audioInputDeviceCount) + L" -> " + std::to_wstring(current.audioInputDeviceCount) + L".",
      8
    });
  }

  // 5. Sample rate change
  if (current.audioSampleRate != prev.audioSampleRate && prev.audioSampleRate > 0) {
    findings.push_back({
      L"Audio sample rate changed.",
      L"The audio sample rate configuration has been modified.",
      L"Sample rate bits: " + std::to_wstring(prev.audioSampleRate) + L" -> " + std::to_wstring(current.audioSampleRate) + L".",
      10
    });
  }

  // 6. Bit depth change
  if (current.audioBitsPerSample != prev.audioBitsPerSample && prev.audioBitsPerSample > 0) {
    findings.push_back({
      L"Audio bit depth changed.",
      L"The audio bit depth configuration has been modified.",
      L"Bit depth: " + std::to_wstring(prev.audioBitsPerSample) + L" -> " + std::to_wstring(current.audioBitsPerSample) + L".",
      10
    });
  }

  // 7. Channel count change
  if (current.audioChannels != prev.audioChannels && prev.audioChannels > 0) {
    findings.push_back({
      L"Audio channel count changed.",
      L"The audio channel configuration has been modified.",
      L"Channels: " + std::to_wstring(prev.audioChannels) + L" -> " + std::to_wstring(current.audioChannels) + L".",
      10
    });
  }

  // 8. Buffer underrun
  if (current.audioLatencyMs > prev.audioLatencyMs + 50 && prev.audioLatencyMs > 0) {
    findings.push_back({
      L"Audio buffer underrun detected.",
      L"Audio latency spiked, indicating buffer starvation.",
      L"Latency: " + std::to_wstring(prev.audioLatencyMs) + L"ms -> " + std::to_wstring(current.audioLatencyMs) + L"ms.",
      8
    });
  }

  // 9. Buffer overrun
  if (current.audioLatencyMs < prev.audioLatencyMs - 50 && prev.audioLatencyMs > 100) {
    findings.push_back({
      L"Audio buffer behavior changed.",
      L"Audio latency dropped significantly.",
      L"Latency: " + std::to_wstring(prev.audioLatencyMs) + L"ms -> " + std::to_wstring(current.audioLatencyMs) + L"ms.",
      4
    });
  }

  // 10. Audio service restart
  if (current.audioServiceRunning != prev.audioServiceRunning && prev.audioServiceRunning >= 0) {
    findings.push_back({
      L"Audio service state changed.",
      L"Windows Audio service (Audiosrv) state has changed.",
      L"Audiosrv: " + std::to_wstring(prev.audioServiceRunning) + L" -> " + std::to_wstring(current.audioServiceRunning) + L".",
      12
    });
  }

  // 11. Microphone mute
  if (current.audioMicMuted == 1 && prev.audioMicMuted == 0) {
    findings.push_back({
      L"Microphone muted.",
      L"The microphone has been muted.",
      L"Mic muted.",
      2
    });
  }

  // 12. Microphone unmute
  if (current.audioMicMuted == 0 && prev.audioMicMuted == 1) {
    findings.push_back({
      L"Microphone unmuted.",
      L"The microphone has been unmuted.",
      L"Mic unmuted.",
      2
    });
  }

  // 13. System mute
  if (current.audioSystemMuted == 1 && prev.audioSystemMuted == 0) {
    findings.push_back({
      L"System audio muted.",
      L"The system audio has been muted.",
      L"System muted.",
      2
    });
  }

  // 14. System unmute
  if (current.audioSystemMuted == 0 && prev.audioSystemMuted == 1) {
    findings.push_back({
      L"System audio unmuted.",
      L"The system audio has been unmuted.",
      L"System unmuted.",
      2
    });
  }

  // 15. Volume change
  if (current.audioMasterVolume != prev.audioMasterVolume && prev.audioMasterVolume > 0) {
    findings.push_back({
      L"Master volume changed.",
      L"The master volume level has been modified.",
      L"Volume: " + std::to_wstring(prev.audioMasterVolume) + L" -> " + std::to_wstring(current.audioMasterVolume) + L".",
      4
    });
  }

  // 16. Headphone jack insert
  if (current.audioHeadphoneJack == 1 && prev.audioHeadphoneJack == 0) {
    findings.push_back({
      L"Headphone jack inserted.",
      L"A headphone jack has been detected.",
      L"Headphone inserted.",
      2
    });
  }

  // 17. Headphone jack remove
  if (current.audioHeadphoneJack == 0 && prev.audioHeadphoneJack == 1) {
    findings.push_back({
      L"Headphone jack removed.",
      L"The headphone jack has been removed.",
      L"Headphone removed.",
      2
    });
  }

  // 18. Speaker anomaly
  if (current.audioMasterVolumeLeft != prev.audioMasterVolumeLeft && prev.audioMasterVolumeLeft > 0 &&
      current.audioMasterVolumeRight != prev.audioMasterVolumeRight && prev.audioMasterVolumeRight > 0 &&
      std::abs(static_cast<int>(current.audioMasterVolumeLeft) - static_cast<int>(current.audioMasterVolumeRight)) > 10000) {
    findings.push_back({
      L"Speaker volume imbalance detected.",
      L"Left and right speaker volumes are significantly different.",
      L"L: " + std::to_wstring(current.audioMasterVolumeLeft) + L" R: " + std::to_wstring(current.audioMasterVolumeRight) + L".",
      6
    });
  }

  // 19. Codec driver event
  if (current.audioCodecEvents != prev.audioCodecEvents && prev.audioCodecEvents >= 0) {
    findings.push_back({
      L"Audio codec driver event.",
      L"An audio codec driver event has been detected.",
      L"Codec events: " + std::to_wstring(prev.audioCodecEvents) + L" -> " + std::to_wstring(current.audioCodecEvents) + L".",
      10
    });
  }

  // 20. Media playback start
  if (current.audioWaveOutOpen > prev.audioWaveOutOpen && prev.audioWaveOutOpen >= 0) {
    findings.push_back({
      L"Media playback started.",
      L"New audio output stream detected.",
      L"WaveOut: " + std::to_wstring(prev.audioWaveOutOpen) + L" -> " + std::to_wstring(current.audioWaveOutOpen) + L".",
      2
    });
  }

  // 21. Media playback stop
  if (current.audioWaveOutOpen < prev.audioWaveOutOpen && prev.audioWaveOutOpen > 0) {
    findings.push_back({
      L"Media playback stopped.",
      L"An audio output stream has been closed.",
      L"WaveOut: " + std::to_wstring(prev.audioWaveOutOpen) + L" -> " + std::to_wstring(current.audioWaveOutOpen) + L".",
      2
    });
  }

  // 22. Media decoder failure
  if (current.audioDecoderErrors > prev.audioDecoderErrors && prev.audioDecoderErrors >= 0) {
    findings.push_back({
      L"Audio decoder failure.",
      L"Audio decoder errors have been detected.",
      L"Decoder errors: " + std::to_wstring(prev.audioDecoderErrors) + L" -> " + std::to_wstring(current.audioDecoderErrors) + L".",
      12
    });
  }

  // 23. Audio latency spike
  if (current.audioLatencyMs > prev.audioLatencyMs + 100 && prev.audioLatencyMs > 0) {
    findings.push_back({
      L"Audio latency spike detected.",
      L"Audio latency has increased dramatically.",
      L"Latency: " + std::to_wstring(prev.audioLatencyMs) + L"ms -> " + std::to_wstring(current.audioLatencyMs) + L"ms.",
      10
    });
  }

  // 24. Recording permission change
  if (current.audioWaveInOpen != prev.audioWaveInOpen && prev.audioWaveInOpen >= 0) {
    findings.push_back({
      L"Audio recording state changed.",
      L"The number of active audio recording streams has changed.",
      L"WaveIn: " + std::to_wstring(prev.audioWaveInOpen) + L" -> " + std::to_wstring(current.audioWaveInOpen) + L".",
      8
    });
  }

  // 25. Audio spatialization change
  if (current.audioSpatialization != prev.audioSpatialization && prev.audioSpatialization >= 0) {
    findings.push_back({
      L"Audio spatialization changed.",
      L"Audio spatial sound configuration has been modified.",
      L"Spatial: " + std::to_wstring(prev.audioSpatialization) + L" -> " + std::to_wstring(current.audioSpatialization) + L".",
      8
    });
  }
}

} // namespace monix
