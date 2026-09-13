#pragma once

#include "../core/Result.hpp"
#include "ShaderLibraryEntry.hpp"

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace monix::renderer_vk {

class ShaderWorkspace;

enum class ScanChange : uint8_t {
    Added,
    Removed,
    Modified,
    Unchanged
};

struct ScanDiff {
    std::vector<size_t> added;
    std::vector<size_t> removed;
    std::vector<size_t> modified;
    std::vector<size_t> unchanged;
};

class ShaderLibrary {
public:
    explicit ShaderLibrary(std::filesystem::path shadersRoot);
    ShaderLibrary(std::filesystem::path shadersRoot, const ShaderWorkspace* workspace);

    void scan();
    ScanDiff rescan();

    size_t entryCount() const;
    bool empty() const;

    const ShaderLibraryEntry* entry(size_t index) const;
    const ShaderLibraryEntry* find(const std::filesystem::path& path) const;
    bool contains(const std::filesystem::path& path) const;

    std::vector<const ShaderLibraryEntry*> byLanguage(ShaderLanguage lang) const;
    std::vector<const ShaderLibraryEntry*> byCategory(std::string_view category) const;
    std::vector<std::string> categories() const;
    std::vector<std::string> categoriesForLanguage(ShaderLanguage lang) const;

    void setStatus(size_t index, ShaderEntryStatus status);
    void setError(size_t index, const std::string& error);
    void setActive(size_t index, bool active);
    void clearActive();
    void clearActiveExcept(size_t keepIndex);

    const std::filesystem::path& root() const { return root_; }

    ShaderLibraryEntry* entryMutable(size_t index);

    // FASE 16.3 — source kind filtering
    std::vector<const ShaderLibraryEntry*> bySourceKind(ShaderSourceKind kind) const;

private:
    std::filesystem::path root_;
    const ShaderWorkspace* workspace_ = nullptr;
    std::vector<ShaderLibraryEntry> entries_;
    std::unordered_map<std::string, size_t> pathIndex_;
    mutable std::mutex mutex_;

    static std::filesystem::path normalizePath(const std::filesystem::path& path);
    static std::string extractName(const std::filesystem::path& filePath);
    static std::string extractCategory(const std::filesystem::path& relativePath);
    static uint64_t computeContentHash(const std::filesystem::path& path);
    static uint64_t getFileSize(const std::filesystem::path& path);
    static uint64_t getLastWriteTime(const std::filesystem::path& path);
    static bool isRelevantExtension(const std::filesystem::path& path);
    ShaderSourceKind classifySourceKind(const std::filesystem::path& path) const;
};

}  // namespace monix::renderer_vk
