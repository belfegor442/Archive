#include "CameraAdapter.hpp"

#include <chrono>

namespace monix::collectors::camera {

const char* CameraEventKindName(CameraEventKind k) {
  switch (k) {
    case CameraEventKind::Connected:           return "Connected";
    case CameraEventKind::Disconnected:        return "Disconnected";
    case CameraEventKind::Unavailable:         return "Unavailable";
    case CameraEventKind::Available:           return "Available";
    case CameraEventKind::StreamAvailable:     return "StreamAvailable";
    case CameraEventKind::StreamUnavailable:   return "StreamUnavailable";
    case CameraEventKind::RecordingStateChanged: return "RecordingStateChanged";
    case CameraEventKind::DeviceHealthChanged: return "DeviceHealthChanged";
  }
  return "Unknown";
}

std::string CameraEventKindAction(CameraEventKind k) {
  switch (k) {
    case CameraEventKind::Connected:           return "external.camera.connected";
    case CameraEventKind::Disconnected:        return "external.camera.disconnected";
    case CameraEventKind::Unavailable:         return "external.camera.unavailable";
    case CameraEventKind::Available:           return "external.camera.available";
    case CameraEventKind::StreamAvailable:     return "external.camera.stream_available";
    case CameraEventKind::StreamUnavailable:   return "external.camera.stream_unavailable";
    case CameraEventKind::RecordingStateChanged: return "external.camera.recording_state_changed";
    case CameraEventKind::DeviceHealthChanged: return "external.camera.health_changed";
  }
  return "unknown";
}

const char* CameraStateName(CameraState s) {
  switch (s) {
    case CameraState::Unknown:     return "Unknown";
    case CameraState::Connected:   return "Connected";
    case CameraState::Disconnected: return "Disconnected";
    case CameraState::Unavailable: return "Unavailable";
    case CameraState::Recording:   return "Recording";
    case CameraState::NotRecording: return "NotRecording";
  }
  return "Unknown";
}

const char* StreamStateName(StreamState s) {
  switch (s) {
    case StreamState::Unknown:     return "Unknown";
    case StreamState::Available:   return "Available";
    case StreamState::Unavailable: return "Unavailable";
  }
  return "Unknown";
}

const char* RecordingStateName(RecordingState s) {
  switch (s) {
    case RecordingState::Unknown: return "Unknown";
    case RecordingState::Recording: return "Recording";
    case RecordingState::Paused:    return "Paused";
    case RecordingState::Stopped:   return "Stopped";
  }
  return "Unknown";
}

bool CameraEvent::isValid() const {
  return !camera_id.empty();
}

std::string CameraEvent::summary() const {
  std::string result = camera_id + " " + std::string(CameraEventKindName(kind));
  result.reserve(256);
  if (!reason.empty()) result += " (" + reason + ")";
  return result;
}

std::string CameraEvent::action() const {
  return CameraEventKindAction(kind);
}

bool CameraInfo::isValid() const {
  return !camera_id.empty();
}

bool CameraInfo::isConnected() const {
  return state == CameraState::Connected || state == CameraState::Recording;
}

bool CameraInfo::isRecording() const {
  return recording == RecordingState::Recording;
}

std::string CameraInfo::summary() const {
  std::string result = camera_id;
  result.reserve(256);
  if (!name.empty()) result += " (" + name + ")";
  result += " state=" + std::string(CameraStateName(state));
  result += " stream=" + std::string(StreamStateName(stream));
  result += " rec=" + std::string(RecordingStateName(recording));
  return result;
}

CameraAdapter::CameraAdapter() {}
CameraAdapter::~CameraAdapter() {}

void CameraAdapter::setCallback(CameraCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void CameraAdapter::registerCamera(const std::string& camera_id, const std::string& name,
    const std::string& source, const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(mu_);
  CameraInfo info;
  info.camera_id = camera_id;
  info.name = name;
  info.source = source;
  info.endpoint = endpoint;
  info.state = CameraState::Unknown;
  cameras_[camera_id] = info;
}

void CameraAdapter::cameraConnected(const std::string& camera_id, const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.state = CameraState::Connected;
    cam.stream = StreamState::Available;
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::Connected;
    event.camera_state = cam.state;
    event.stream_state = cam.stream;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

void CameraAdapter::cameraDisconnected(const std::string& camera_id, const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.state = CameraState::Disconnected;
    cam.stream = StreamState::Unavailable;
    cam.recording = RecordingState::Stopped;
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::Disconnected;
    event.camera_state = cam.state;
    event.stream_state = cam.stream;
    event.recording_state = cam.recording;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

void CameraAdapter::cameraUnavailable(const std::string& camera_id, const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.state = CameraState::Unavailable;
    cam.stream = StreamState::Unavailable;
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::Unavailable;
    event.camera_state = cam.state;
    event.stream_state = cam.stream;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

void CameraAdapter::cameraAvailable(const std::string& camera_id, const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.state = CameraState::Connected;
    cam.stream = StreamState::Available;
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::Available;
    event.camera_state = cam.state;
    event.stream_state = cam.stream;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

void CameraAdapter::streamAvailable(const std::string& camera_id, const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.stream = StreamState::Available;
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::StreamAvailable;
    event.stream_state = cam.stream;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

void CameraAdapter::streamUnavailable(const std::string& camera_id, const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.stream = StreamState::Unavailable;
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::StreamUnavailable;
    event.stream_state = cam.stream;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

void CameraAdapter::recordingStateChanged(const std::string& camera_id, RecordingState state,
    const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.recording = state;
    if (state == RecordingState::Recording) cam.state = CameraState::Recording;
    else if (cam.state == CameraState::Recording) cam.state = CameraState::Connected;
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::RecordingStateChanged;
    event.camera_state = cam.state;
    event.recording_state = cam.recording;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

void CameraAdapter::deviceHealthChanged(const std::string& camera_id, const std::string& reason) {
  CameraEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cam = cameras_[camera_id];
    cam.last_event_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    event.camera_id = camera_id;
    event.source = cam.source;
    event.kind = CameraEventKind::DeviceHealthChanged;
    event.camera_state = cam.state;
    event.reason = reason;
    event.timestamp_ms = cam.last_event_ms;
  }
  emit(event);
}

CameraInfo CameraAdapter::getCamera(const std::string& camera_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = cameras_.find(camera_id);
  if (it == cameras_.end()) return CameraInfo{};
  return it->second;
}

std::vector<CameraInfo> CameraAdapter::allCameras() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CameraInfo> result;
  for (const auto& [id, cam] : cameras_) {
    result.push_back(cam);
  }
  return result;
}

std::size_t CameraAdapter::cameraCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return cameras_.size();
}

std::size_t CameraAdapter::connectedCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t count = 0;
  for (const auto& [id, cam] : cameras_) {
    if (cam.isConnected()) count++;
  }
  return count;
}

std::size_t CameraAdapter::recordingCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t count = 0;
  for (const auto& [id, cam] : cameras_) {
    if (cam.isRecording()) count++;
  }
  return count;
}

std::size_t CameraAdapter::eventsEmitted() const {
  return events_emitted_.load();
}

void CameraAdapter::emit(const CameraEvent& event) {
  events_emitted_++;
  if (callback_) {
    callback_(event);
  }
}

}  // namespace monix::collectors::camera
