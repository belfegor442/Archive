#include "FilesystemCoalescer.hpp"

namespace monix::collectors::fs {

FilesystemCoalescer::FilesystemCoalescer(std::uint32_t window_ms)
  : window_ms_(window_ms) {}

std::string FilesystemCoalescer::makeKey(const FilesystemEvent& event) const {
  return event.path.string() + ":" + std::to_string(static_cast<int>(event.kind));
}

bool FilesystemCoalescer::addEvent(FilesystemEvent event, std::int64_t now_ms) {
  std::lock_guard<std::mutex> lock(mu_);
  std::string key = makeKey(event);

  auto it = pending_.find(key);
  if (it != pending_.end()) {
    auto& existing = it->second;
    existing.last_seen_ms = now_ms;
    existing.modification_count++;
    existing.size = event.size;
    existing.modification_time = event.modification_time;
    if (!event.hash.empty()) {
      existing.hash = event.hash;
      existing.hash_algorithm = event.hash_algorithm;
    }
    return false;
  }

  event.first_seen_ms = now_ms;
  event.last_seen_ms = now_ms;
  event.modification_count = 1;
  pending_[std::move(key)] = std::move(event);
  return true;
}

std::vector<FilesystemEvent> FilesystemCoalescer::flush(std::int64_t now_ms) {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<FilesystemEvent> result;
  auto it = pending_.begin();
  while (it != pending_.end()) {
    if (now_ms - it->second.last_seen_ms >= static_cast<std::int64_t>(window_ms_)) {
      result.push_back(std::move(it->second));
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
  return result;
}

std::size_t FilesystemCoalescer::pendingCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return pending_.size();
}

void FilesystemCoalescer::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  pending_.clear();
}

void FilesystemCoalescer::setWindow(std::uint32_t window_ms) {
  std::lock_guard<std::mutex> lock(mu_);
  window_ms_ = window_ms;
}

}  // namespace monix::collectors::fs
