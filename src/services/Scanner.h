#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include "../core/models/Scan.h"
#include "../core/models/ScanItem.h"
#include "../core/types/AnalysisResult.h"
#include "../storage/DatabaseManager.h"
#include "../storage/ScanRepository.h"
#include "../storage/ScanItemRepository.h"

namespace archive::services {

using ScanProgressFn = std::function<void(int files_scanned, int folders_scanned, int64_t bytes_scanned)>;

class Scanner {
public:
    Scanner(storage::DatabaseManager& db,
            storage::ScanRepository& scans,
            storage::ScanItemRepository& scan_items);

    core::Scan scan_directory(const std::string& root_path, bool compute_hash = true,
                              const ScanProgressFn& progress = nullptr);
    core::AnalysisResult analyze(const std::string& scan_id) const;
    std::vector<core::ScanItem> get_items(const std::string& scan_id) const;

private:
    storage::DatabaseManager& db_;
    storage::ScanRepository& scans_;
    storage::ScanItemRepository& scan_items_;
    std::unordered_map<std::string, std::string> project_cache_;

    core::ScanItem analyze_file(const std::string& filepath, const std::string& scan_id,
                                bool compute_hash);
    std::string detect_mime(const std::string& extension);
    std::string read_content_preview(const std::string& path, int max_bytes = 512);
    std::string detect_project_context(const std::string& filepath);

    static bool is_text_extension(const std::string& ext);
    static bool is_ignored(const std::string& filename);
};

} // namespace archive::services
