#include "StagingManager.h"
#include "FileUtils.h"
#include "../core/utils/Uuid.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace archive::filesystem {

StagingManager::StagingManager(const std::string& base_dir)
    : staging_base_(base_dir + "/.staging")
{}

std::string StagingManager::create_staging_dir(const std::string& operation_type) {
    std::string op_id = generate_operation_id();
    std::string path = get_staging_path(op_id);

    FileUtils::create_directories(path);
    FileUtils::create_directories(path + "/files");
    FileUtils::create_directories(path + "/.meta");

    mark_operation_file(op_id, operation_type, false);

    return op_id;
}

std::string StagingManager::get_staging_path(const std::string& operation_id) const {
    return staging_base_ + "/" + operation_id;
}

std::string StagingManager::get_staging_files_path(const std::string& operation_id) const {
    return get_staging_path(operation_id) + "/files";
}

std::string StagingManager::stage_file(const std::string& operation_id, const std::string& source_path) {
    std::string files_dir = get_staging_files_path(operation_id);
    return FileUtils::copy_file_safe(source_path, files_dir);
}

std::string StagingManager::stage_file_in_dir(const std::string& operation_id,
                                               const std::string& source_path,
                                               const std::string& relative_path) {
    std::string files_dir = get_staging_files_path(operation_id);
    std::string dest_path = files_dir + "/" + relative_path;
    std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
    FileUtils::copy_file(source_path, dest_path);
    return dest_path;
}

std::string StagingManager::stage_folder(const std::string& operation_id, const std::string& source_dir) {
    std::string files_dir = get_staging_files_path(operation_id);
    std::error_code ec;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_dir, ec)) {
        if (entry.is_regular_file()) {
            std::string relative = std::filesystem::relative(entry.path(), source_dir).string();
            std::string dest_path = files_dir + "/" + relative;
            std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
            FileUtils::copy_file(entry.path().string(), dest_path);
        }
    }

    if (ec) {
        throw std::runtime_error("Error staging folder: " + ec.message());
    }

    return files_dir;
}

bool StagingManager::staging_dir_exists(const std::string& operation_id) const {
    return std::filesystem::exists(get_staging_path(operation_id));
}

void StagingManager::finalize_staging(const std::string& operation_id, const std::string& dest_dir) {
    std::string staging_files = get_staging_files_path(operation_id);

    if (!std::filesystem::exists(staging_files)) {
        return;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(staging_files, ec)) {
        if (entry.is_regular_file()) {
            std::string relative = std::filesystem::relative(entry.path(), staging_files).string();
            std::string dest_path = dest_dir + "/" + relative;
            std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
            std::filesystem::copy_file(entry.path(), dest_path,
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                throw std::runtime_error("Failed to finalize file: " + relative + " (" + ec.message() + ")");
            }
        }
    }

    mark_operation_file(operation_id, "", true);
}

void StagingManager::cleanup_staging(const std::string& operation_id) {
    std::string path = get_staging_path(operation_id);
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}

std::vector<StagingOperation> StagingManager::detect_abandoned_staging() const {
    std::vector<StagingOperation> abandoned;

    if (!std::filesystem::exists(staging_base_)) {
        return abandoned;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(staging_base_, ec)) {
        if (entry.is_directory()) {
            std::string op_id = entry.path().filename().string();
            std::string meta_file = entry.path().string() + "/.meta/operation.txt";

            StagingOperation op;
            op.operation_id = op_id;
            op.staging_path = entry.path().string();

            if (std::filesystem::exists(meta_file)) {
                std::ifstream f(meta_file);
                if (f.is_open()) {
                    std::getline(f, op.operation_type);
                    std::string completed_str;
                    std::getline(f, completed_str);
                    op.completed = (completed_str == "1");
                }
            }

            abandoned.push_back(std::move(op));
        }
    }

    return abandoned;
}

void StagingManager::cleanup_abandoned() {
    auto operations = detect_abandoned_staging();
    for (const auto& op : operations) {
        if (op.completed) {
            cleanup_staging(op.operation_id);
        }
    }
}

std::string StagingManager::generate_operation_id() {
    return "op_" + core::utils::generate_id();
}

void StagingManager::mark_operation_file(const std::string& operation_id, const std::string& type,
                                          bool completed) {
    std::string meta_dir = get_staging_path(operation_id) + "/.meta";
    FileUtils::create_directories(meta_dir);

    std::string meta_file = meta_dir + "/operation.txt";
    std::ofstream f(meta_file);
    if (f.is_open()) {
        f << type << "\n";
        f << (completed ? "1" : "0") << "\n";
        f << core::utils::now_iso() << "\n";
    }
}

} // namespace archive::filesystem
