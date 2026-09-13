#pragma once

#include "../core/Result.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct CacheKeyComponents {
    std::string sourceContent;
    std::string stage;
    std::string language;
    std::string compilerVersion;
    std::vector<std::filesystem::path> dependencyPaths;
    std::vector<std::string> dependencyContents;
    bool debugInfo = false;
};

struct ShaderCacheKey {
    std::string digest;
    CacheKeyComponents components;
};

struct CacheManifest {
    std::string keyDigest;
    std::string sourceHash;
    std::string stage;
    std::string language;
    std::string compilerVersion;
    std::vector<std::string> dependencyHashes;
    size_t spirvSize = 0;
    std::string spirvHash;
    size_t reflectionSize = 0;
    std::string reflectionHash;
    uint64_t timestamp = 0;
    bool debugInfo = false;

    std::string serialize() const;
    static Result<CacheManifest> deserialize(const std::string& text);
};

struct ShaderCacheEntry {
    std::filesystem::path spirvPath;
    std::filesystem::path reflectionPath;
    std::filesystem::path manifestPath;
    bool hit = false;
    CacheManifest manifest;
};

struct CacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    uint64_t corrupted = 0;
    uint64_t totalEntries = 0;
    uint64_t totalSizeBytes = 0;
};

class ShaderCache {
public:
    explicit ShaderCache(std::filesystem::path directory, uint64_t max_size_bytes = 512 * 1024 * 1024);

    ShaderCacheKey makeKey(const CacheKeyComponents& components) const;
    ShaderCacheEntry entryFor(const ShaderCacheKey& key) const;
    bool contains(const ShaderCacheKey& key) const;

    Status store(const ShaderCacheKey& key,
                 const std::vector<unsigned char>& spirv,
                 const std::string& reflectionJson) const;

    bool verifyIntegrity(const ShaderCacheEntry& entry) const;
    Status invalidate(const ShaderCacheKey& key) const;
    Status invalidateAll();
    Status evict();

    CacheStats stats() const;
    void recordHit() const;
    void recordMiss() const;

    static std::string hashContent(const std::string& content);
    static std::string hashFile(const std::filesystem::path& path);

private:
    std::filesystem::path directory_;
    uint64_t max_size_bytes_;
    mutable uint64_t hits_ = 0;
    mutable uint64_t misses_ = 0;
    mutable uint64_t evictions_ = 0;
    mutable uint64_t corrupted_ = 0;
};

}  // namespace monix::renderer_vk
