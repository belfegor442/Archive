#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::camera {

enum class CameraEventKind : std::uint8_t {
  Connected,
  Disconnected,
  Unavailable,
  Available,
  StreamAvailable,
  StreamUnavailable,
  RecordingStateChanged,
  DeviceHealthChanged
};

const char* CameraEventKindName(CameraEventKind k);
std::string CameraEventKindAction(CameraEventKind k);

enum class CameraState : std::uint8_t {
  Unknown,
  Connected,
  Disconnected,
  Unavailable,
  Recording,
  NotRecording
};

const char* CameraStateName(CameraState s);

enum class StreamState : std::uint8_t {
  Unknown,
  Available,
  Unavailable
};

const char* StreamStateName(StreamState s);

enum class RecordingState : std::uint8_t {
  Unknown,
  Recording,
  Paused,
  Stopped
};

const char* RecordingStateName(RecordingState s);

struct CameraEvent {
  std::string camera_id;
  std::string source;
  CameraEventKind kind = CameraEventKind::Connected;
  CameraState camera_state = CameraState::Unknown;
  StreamState stream_state = StreamState::Unknown;
  RecordingState recording_state = RecordingState::Unknown;
  std::string reason;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
  std::string action() const;
};

struct CameraInfo {
  std::string camera_id;
  std::string name;
  std::string source;
  std::string endpoint;
  CameraState state = CameraState::Unknown;
  StreamState stream = StreamState::Unknown;
  RecordingState recording = RecordingState::Unknown;
  std::int64_t last_event_ms = 0;

  bool isValid() const;
  bool isConnected() const;
  bool isRecording() const;
  std::string summary() const;
};

using CameraCallback = std::function<void(const CameraEvent&)>;

class CameraAdapter {
public:
  CameraAdapter();
  ~CameraAdapter();

  CameraAdapter(const CameraAdapter&) = delete;
  CameraAdapter& operator=(const CameraAdapter&) = delete;

  void setCallback(CameraCallback cb);

  void registerCamera(const std::string& camera_id, const std::string& name = "",
    const std::string& source = "", const std::string& endpoint = "");

  void cameraConnected(const std::string& camera_id, const std::string& reason = "");
  void cameraDisconnected(const std::string& camera_id, const std::string& reason = "");
  void cameraUnavailable(const std::string& camera_id, const std::string& reason = "");
  void cameraAvailable(const std::string& camera_id, const std::string& reason = "");

  void streamAvailable(const std::string& camera_id, const std::string& reason = "");
  void streamUnavailable(const std::string& camera_id, const std::string& reason = "");

  void recordingStateChanged(const std::string& camera_id, RecordingState state,
    const std::string& reason = "");
  void deviceHealthChanged(const std::string& camera_id, const std::string& reason = "");

  CameraInfo getCamera(const std::string& camera_id) const;
  std::vector<CameraInfo> allCameras() const;
  std::size_t cameraCount() const;
  std::size_t connectedCount() const;
  std::size_t recordingCount() const;

  std::size_t eventsEmitted() const;

private:
  void emit(const CameraEvent& event);

  CameraCallback callback_;
  mutable std::mutex mu_;
  std::unordered_map<std::string, CameraInfo> cameras_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::camera
