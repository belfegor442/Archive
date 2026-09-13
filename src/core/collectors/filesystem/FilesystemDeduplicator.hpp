#pragma once

#include "FilesystemTypes.hpp"

#include <unordered_map>
#include <mutex>
#include <string>

namespace monix::collectors::fs {

class FilesystemDeduplicator {
public:
  explicit FilesystemDeduplicator(std::uint32_t window_ms = 200);

  bool isDuplicate(const FilesystemEvent& event, std::int64_t now_ms);
  std::size_t size() const;
  void clear();
  void setWindow(std::uint32_t window_ms);

private:
  struct Entry {
    FilesystemEventKind kind;
    std::string path;
    std::int64_t timestamp_ms = 0;
  };

  std::uint32_t window_ms_;
  mutable std::mutex mu_;
  std::unordered_map<std::string, Entry> entries_;
};

}  // namespace monix::collectors::fs
