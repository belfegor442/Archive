#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace monix::collectors::livestream {

struct StreamEvent {
  std::string event_id;
  std::int64_t timestamp_ms = 0;
  std::string event_type;
  std::string severity;
  std::string priority;
  std::string source;
  std::string actor;
  std::string entity;
  std::string summary;
  bool expanded = false;

  bool isValid() const;
};

struct StreamFilter {
  std::string event_type;
  std::string source;
  std::string severity;
  std::string search_text;

  bool matches(const StreamEvent& event) const;
};

enum class StreamState : std::uint8_t {
  Paused,
  Running,
  Following
};

const char* StreamStateName(StreamState s);

using StreamCallback = std::function<void(const StreamEvent&)>;

class LiveStream {
public:
  LiveStream();
  ~LiveStream();

  LiveStream(const LiveStream&) = delete;
  LiveStream& operator=(const LiveStream&) = delete;

  void setCallback(StreamCallback cb);

  void pushEvent(const StreamEvent& event);
  void pushEvents(const std::vector<StreamEvent>& events);

  void setFilter(const StreamFilter& filter);
  void clearFilter();

  void pause();
  void resume();
  void follow();

  void expand(const std::string& event_id);
  void collapse(const std::string& event_id);

  std::vector<StreamEvent> visibleEvents() const;
  std::vector<StreamEvent> filteredEvents() const;

  StreamState state() const;
  bool isPaused() const;
  bool isFollowing() const;

  std::size_t totalEvents() const;
  std::size_t visibleCount() const;
  std::size_t filteredCount() const;

  void clear();

private:
  StreamCallback callback_;
  mutable std::mutex mu_;
  std::vector<StreamEvent> events_;
  std::vector<StreamEvent> visible_;
  StreamFilter filter_;
  StreamState state_ = StreamState::Running;
  bool has_filter_ = false;
};

}  // namespace monix::collectors::livestream
