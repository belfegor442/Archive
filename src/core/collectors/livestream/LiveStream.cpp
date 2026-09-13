#include "LiveStream.hpp"

#include <algorithm>

namespace monix::collectors::livestream {

bool StreamEvent::isValid() const {
  return !event_id.empty();
}

bool StreamFilter::matches(const StreamEvent& event) const {
  if (!event_type.empty() && event.event_type != event_type) return false;
  if (!source.empty() && event.source != source) return false;
  if (!severity.empty() && event.severity != severity) return false;
  if (!search_text.empty()) {
    std::string lower_summary = event.summary;
    std::string lower_search = search_text;
    std::transform(lower_summary.begin(), lower_summary.end(), lower_summary.begin(), ::tolower);
    std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), ::tolower);
    if (lower_summary.find(lower_search) == std::string::npos) return false;
  }
  return true;
}

const char* StreamStateName(StreamState s) {
  switch (s) {
    case StreamState::Paused:   return "Paused";
    case StreamState::Running:  return "Running";
    case StreamState::Following: return "Following";
  }
  return "Unknown";
}

LiveStream::LiveStream() {}
LiveStream::~LiveStream() {}

void LiveStream::setCallback(StreamCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void LiveStream::pushEvent(const StreamEvent& event) {
  std::lock_guard<std::mutex> lock(mu_);
  events_.push_back(event);
  if (!has_filter_ || filter_.matches(event)) {
    visible_.push_back(event);
    if (callback_) callback_(event);
  }
}

void LiveStream::pushEvents(const std::vector<StreamEvent>& events) {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& event : events) {
    events_.push_back(event);
    if (!has_filter_ || filter_.matches(event)) {
      visible_.push_back(event);
      if (callback_) callback_(event);
    }
  }
}

void LiveStream::setFilter(const StreamFilter& filter) {
  std::lock_guard<std::mutex> lock(mu_);
  filter_ = filter;
  has_filter_ = true;
  visible_.clear();
  for (const auto& event : events_) {
    if (filter_.matches(event)) {
      visible_.push_back(event);
    }
  }
}

void LiveStream::clearFilter() {
  std::lock_guard<std::mutex> lock(mu_);
  has_filter_ = false;
  visible_ = events_;
}

void LiveStream::pause() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = StreamState::Paused;
}

void LiveStream::resume() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = StreamState::Running;
}

void LiveStream::follow() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = StreamState::Following;
}

void LiveStream::expand(const std::string& event_id) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& event : visible_) {
    if (event.event_id == event_id) {
      event.expanded = true;
      break;
    }
  }
}

void LiveStream::collapse(const std::string& event_id) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& event : visible_) {
    if (event.event_id == event_id) {
      event.expanded = false;
      break;
    }
  }
}

std::vector<StreamEvent> LiveStream::visibleEvents() const {
  std::lock_guard<std::mutex> lock(mu_);
  return visible_;
}

std::vector<StreamEvent> LiveStream::filteredEvents() const {
  std::lock_guard<std::mutex> lock(mu_);
  return visible_;
}

StreamState LiveStream::state() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_;
}

bool LiveStream::isPaused() const {
  return state() == StreamState::Paused;
}

bool LiveStream::isFollowing() const {
  return state() == StreamState::Following;
}

std::size_t LiveStream::totalEvents() const {
  std::lock_guard<std::mutex> lock(mu_);
  return events_.size();
}

std::size_t LiveStream::visibleCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return visible_.size();
}

std::size_t LiveStream::filteredCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return visible_.size();
}

void LiveStream::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  events_.clear();
  visible_.clear();
}

}  // namespace monix::collectors::livestream
