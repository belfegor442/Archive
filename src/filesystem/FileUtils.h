#pragma once

#include <string>
#include <filesystem>

namespace archive::filesystem {

class FileUtils {
public:
    static bool file_exists(const std::string& path);
    static bool directory_exists(const std::string& path);
    static uint64_t file_size(const std::string& path);
    static std::string file_name(const std::string& path);
    static std::string extension(const std::string& path);
    static std::string parent_dir(const std::string& path);
    static std::string stem(const std::string& path);

    static void create_directories(const std::string& path);
    static void copy_file(const std::string& source, const std::string& dest);
    static std::string copy_file_safe(const std::string& source, const std::string& dest_dir);
    static void move_file(const std::string& source, const std::string& dest);
    static void remove_file(const std::string& path);
    static void remove_directory(const std::string& path);

    static int count_files(const std::string& path);
    static uint64_t total_size(const std::string& path);

    static std::string unique_path(const std::string& dir, const std::string& name,
                                   const std::string& ext);
};

} // namespace archive::filesystem
