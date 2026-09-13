#pragma once

#include "FilesystemTypes.hpp"

#include <unordered_map>
#include <mutex>
#include <string>

namespace monix::collectors::fs {

class FilesystemCoalescer {
public:
  explicit FilesystemCoalescer(std::uint32_t window_ms = 500);

  bool addEvent(FilesystemEvent event, std::int64_t now_ms);
  std::vector<FilesystemEvent> flush(std::int64_t now_ms);
  std::size_t pendingCount() const;
  void clear();
  void setWindow(std::uint32_t window_ms);

private:
  std::string makeKey(const FilesystemEvent& event) const;

  std::uint32_t window_ms_;
  mutable std::mutex mu_;
  std::unordered_map<std::string, FilesystemEvent> pending_;
};

}  // namespace monix::collectors::fs
