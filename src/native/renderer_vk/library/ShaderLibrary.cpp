#include "ShaderLibrary.hpp"
#include "ShaderWorkspace.hpp"

#include "../core/FileSystem.hpp"
#include "../core/Hash.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace monix::renderer_vk {

ShaderLibrary::ShaderLibrary(std::filesystem::path shadersRoot)
    : root_(std::move(shadersRoot)) {}

ShaderLibrary::ShaderLibrary(std::filesystem::path shadersRoot, const ShaderWorkspace* workspace)
    : root_(std::move(shadersRoot)), workspace_(workspace) {}

std::filesystem::path ShaderLibrary::normalizePath(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) return path.lexically_normal();
    return canonical;
}

std::string ShaderLibrary::extractName(const std::filesystem::path& filePath) {
    return filePath.stem().string();
}

std::string ShaderLibrary::extractCategory(const std::filesystem::path& relativePath) {
    auto parent = relativePath.parent_path();
    if (parent.empty() || parent == "." || parent == "/") {
        auto firstComponent = *relativePath.begin();
        return firstComponent.string();
    }
    return parent.filename().string();
}

uint64_t ShaderLibrary::computeContentHash(const std::filesystem::path& path) {
    auto result = FileSystem::readText(path);
    if (!result.ok()) return 0;
    return Hash::fnv1a(result.value());
}

uint64_t ShaderLibrary::getFileSize(const std::filesystem::path& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) return 0;
    return size;
}

uint64_t ShaderLibrary::getLastWriteTime(const std::filesystem::path& path) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return static_cast<uint64_t>(sctp.time_since_epoch().count());
}

bool ShaderLibrary::isRelevantExtension(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".glsl" || ext == ".slang" || ext == ".slangp" || ext == ".glslp";
}

ShaderSourceKind ShaderLibrary::classifySourceKind(const std::filesystem::path& path) const {
    if (!workspace_) return ShaderSourceKind::User;

    std::error_code ec;
    auto rel = std::filesystem::relative(path, root_, ec);
    if (ec) return ShaderSourceKind::User;

    auto firstDir = rel.begin();
    if (firstDir == rel.end()) return ShaderSourceKind::User;

    auto dirName = firstDir->string();
    if (dirName == "GLSL" || dirName == "SLANG" || dirName == "SLANGP") {
        return ShaderSourceKind::User;
    }

    return ShaderSourceKind::Internal;
}

void ShaderLibrary::scan() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    pathIndex_.clear();

    if (!std::filesystem::exists(root_) || !std::filesystem::is_directory(root_)) return;

    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(root_, ec)) {
        if (!entry.is_regular_file()) continue;
        if (!isRelevantExtension(entry.path())) continue;

        ShaderLibraryEntry libEntry;
        libEntry.path = normalizePath(entry.path());

        std::error_code pathEc;
        libEntry.relativePath = std::filesystem::relative(libEntry.path, root_, pathEc);
        if (pathEc) libEntry.relativePath = libEntry.path;

        libEntry.name = extractName(entry.path());
        libEntry.language = detectLanguageFromExtension(entry.path().extension().string());
        libEntry.extension = entry.path().extension().string();
        libEntry.category = extractCategory(libEntry.relativePath);
        libEntry.sourceKind = classifySourceKind(entry.path());
        libEntry.fileSize = getFileSize(entry.path());
        libEntry.lastWriteTime = getLastWriteTime(entry.path());
        libEntry.contentHash = computeContentHash(entry.path());

        std::string key = normalizePath(libEntry.path).string();
        size_t index = entries_.size();
        entries_.push_back(std::move(libEntry));
        pathIndex_[key] = index;
    }

    std::sort(entries_.begin(), entries_.end(), [](const ShaderLibraryEntry& a, const ShaderLibraryEntry& b) {
        if (a.category != b.category) return a.category < b.category;
        if (a.name != b.name) return a.name < b.name;
        return a.relativePath < b.relativePath;
    });

    pathIndex_.clear();
    for (size_t i = 0; i < entries_.size(); ++i) {
        std::string key = normalizePath(entries_[i].path).string();
        pathIndex_[key] = i;
    }
}

ScanDiff ShaderLibrary::rescan() {
    std::lock_guard<std::mutex> lock(mutex_);
    ScanDiff diff;

    std::unordered_map<std::string, size_t> oldIndex;
    for (size_t i = 0; i < entries_.size(); ++i) {
        oldIndex[normalizePath(entries_[i].path).string()] = i;
    }

    std::vector<ShaderLibraryEntry> newEntries;
    std::unordered_map<std::string, size_t> newPathIndex;

    if (std::filesystem::exists(root_) && std::filesystem::is_directory(root_)) {
        std::error_code ec;
        for (auto& entry : std::filesystem::recursive_directory_iterator(root_, ec)) {
            if (!entry.is_regular_file()) continue;
            if (!isRelevantExtension(entry.path())) continue;

            auto normalized = normalizePath(entry.path());
            std::string key = normalized.string();
            auto it = oldIndex.find(key);

            ShaderLibraryEntry libEntry;
            libEntry.path = normalized;

            std::error_code pathEc;
            libEntry.relativePath = std::filesystem::relative(libEntry.path, root_, pathEc);
            if (pathEc) libEntry.relativePath = libEntry.path;

            libEntry.name = extractName(entry.path());
            libEntry.language = detectLanguageFromExtension(entry.path().extension().string());
            libEntry.extension = entry.path().extension().string();
            libEntry.category = extractCategory(libEntry.relativePath);
            libEntry.sourceKind = classifySourceKind(entry.path());
            libEntry.fileSize = getFileSize(entry.path());
            libEntry.lastWriteTime = getLastWriteTime(entry.path());
            libEntry.contentHash = computeContentHash(entry.path());

            if (it != oldIndex.end()) {
                const auto& oldEntry = entries_[it->second];
                if (libEntry.contentHash == oldEntry.contentHash &&
                    libEntry.fileSize == oldEntry.fileSize) {
                    libEntry.status = oldEntry.status;
                    libEntry.error = oldEntry.error;
                    libEntry.isActive = oldEntry.isActive;
                    diff.unchanged.push_back(newEntries.size());
                } else {
                    libEntry.status = ShaderEntryStatus::Unknown;
                    diff.modified.push_back(newEntries.size());
                }
                oldIndex.erase(it);
            } else {
                libEntry.status = ShaderEntryStatus::Unknown;
                diff.added.push_back(newEntries.size());
            }

            size_t newIndex = newEntries.size();
            newEntries.push_back(std::move(libEntry));
            newPathIndex[key] = newIndex;
        }
    }

    for (auto& [key, oldIdx] : oldIndex) {
        diff.removed.push_back(oldIdx);
    }

    entries_ = std::move(newEntries);
    pathIndex_ = std::move(newPathIndex);

    return diff;
}

size_t ShaderLibrary::entryCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

bool ShaderLibrary::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.empty();
}

const ShaderLibraryEntry* ShaderLibrary::entry(size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) return nullptr;
    return &entries_[index];
}

const ShaderLibraryEntry* ShaderLibrary::find(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = normalizePath(path).string();
    auto it = pathIndex_.find(key);
    if (it == pathIndex_.end()) return nullptr;
    if (it->second >= entries_.size()) return nullptr;
    return &entries_[it->second];
}

bool ShaderLibrary::contains(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = normalizePath(path).string();
    return pathIndex_.find(key) != pathIndex_.end();
}

std::vector<const ShaderLibraryEntry*> ShaderLibrary::byLanguage(ShaderLanguage lang) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const ShaderLibraryEntry*> result;
    for (auto& e : entries_) {
        if (e.language == lang) result.push_back(&e);
    }
    return result;
}

std::vector<const ShaderLibraryEntry*> ShaderLibrary::byCategory(std::string_view category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const ShaderLibraryEntry*> result;
    for (auto& e : entries_) {
        if (e.category == category) result.push_back(&e);
    }
    return result;
}

std::vector<std::string> ShaderLibrary::categories() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_set<std::string> seen;
    std::vector<std::string> result;
    for (auto& e : entries_) {
        if (seen.insert(e.category).second) {
            result.push_back(e.category);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> ShaderLibrary::categoriesForLanguage(ShaderLanguage lang) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_set<std::string> seen;
    std::vector<std::string> result;
    for (auto& e : entries_) {
        if (e.language == lang && seen.insert(e.category).second) {
            result.push_back(e.category);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void ShaderLibrary::setStatus(size_t index, ShaderEntryStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) return;
    entries_[index].status = status;
}

void ShaderLibrary::setError(size_t index, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) return;
    entries_[index].status = ShaderEntryStatus::Error;
    entries_[index].error = error;
}

void ShaderLibrary::setActive(size_t index, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) return;
    entries_[index].isActive = active;
    if (active) {
        entries_[index].status = ShaderEntryStatus::Active;
    }
}

void ShaderLibrary::clearActive() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& e : entries_) {
        if (e.isActive) {
            e.isActive = false;
            if (e.status == ShaderEntryStatus::Active) {
                e.status = ShaderEntryStatus::Compiled;
            }
        }
    }
}

void ShaderLibrary::clearActiveExcept(size_t keepIndex) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (i == keepIndex) continue;
        if (entries_[i].isActive) {
            entries_[i].isActive = false;
            if (entries_[i].status == ShaderEntryStatus::Active) {
                entries_[i].status = ShaderEntryStatus::Compiled;
            }
        }
    }
}

ShaderLibraryEntry* ShaderLibrary::entryMutable(size_t index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) return nullptr;
    return &entries_[index];
}

std::vector<const ShaderLibraryEntry*> ShaderLibrary::bySourceKind(ShaderSourceKind kind) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const ShaderLibraryEntry*> result;
    for (auto& e : entries_) {
        if (e.sourceKind == kind) result.push_back(&e);
    }
    return result;
}

}  // namespace monix::renderer_vk
