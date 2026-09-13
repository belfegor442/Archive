#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace monix {

inline std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return "";
  }
  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

} // namespace monix
