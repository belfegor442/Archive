#pragma once

#include "FilesystemTypes.hpp"

#include <string>
#include <vector>

namespace monix::collectors::fs {

class FilesystemHasher {
public:
  static std::string hashFile(const std::filesystem::path& path, FileHashMode mode);
  static std::string hashBytes(const void* data, std::size_t size);
  static std::string hashFileSHA256(const std::filesystem::path& path);
};

}  // namespace monix::collectors::fs
