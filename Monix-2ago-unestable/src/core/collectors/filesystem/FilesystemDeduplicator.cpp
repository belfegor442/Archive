#include "FilesystemDeduplicator.hpp"

namespace monix::collectors::fs {

FilesystemDeduplicator::FilesystemDeduplicator(std::uint32_t window_ms)
  : window_ms_(window_ms) {}

bool FilesystemDeduplicator::isDuplicate(const FilesystemEvent& event, std::int64_t now_ms) {
  std::lock_guard<std::mutex> lock(mu_);

  std::string key = event.path.string() + ":" +
                    std::to_string(static_cast<int>(event.kind));

  auto it = entries_.find(key);
  if (it != entries_.end()) {
    if (now_ms - it->second.timestamp_ms < static_cast<std::int64_t>(window_ms_)) {
      return true;
    }
  }

  Entry entry;
  entry.kind = event.kind;
  entry.path = event.path.string();
  entry.timestamp_ms = now_ms;
  entries_[std::move(key)] = std::move(entry);
  return false;
}

std::size_t FilesystemDeduplicator::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return entries_.size();
}

void FilesystemDeduplicator::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  entries_.clear();
}

void FilesystemDeduplicator::setWindow(std::uint32_t window_ms) {
  std::lock_guard<std::mutex> lock(mu_);
  window_ms_ = window_ms;
}

}  // namespace monix::collectors::fs
