#include "ShaderCache.hpp"

#include "../core/FileSystem.hpp"
#include "../core/Hash.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace monix::renderer_vk {

std::string CacheManifest::serialize() const {
    std::ostringstream out;
    out << "{\n";
    out << "  \"keyDigest\": \"" << keyDigest << "\",\n";
    out << "  \"sourceHash\": \"" << sourceHash << "\",\n";
    out << "  \"stage\": \"" << stage << "\",\n";
    out << "  \"language\": \"" << language << "\",\n";
    out << "  \"compilerVersion\": \"" << compilerVersion << "\",\n";
    out << "  \"dependencyHashes\": [";
    for (size_t i = 0; i < dependencyHashes.size(); ++i) {
        if (i > 0) out << ", ";
        out << "\"" << dependencyHashes[i] << "\"";
    }
    out << "],\n";
    out << "  \"spirvSize\": " << spirvSize << ",\n";
    out << "  \"spirvHash\": \"" << spirvHash << "\",\n";
    out << "  \"reflectionSize\": " << reflectionSize << ",\n";
    out << "  \"reflectionHash\": \"" << reflectionHash << "\",\n";
    out << "  \"timestamp\": " << timestamp << ",\n";
    out << "  \"debugInfo\": " << (debugInfo ? "true" : "false") << "\n";
    out << "}";
    return out.str();
}

Result<CacheManifest> CacheManifest::deserialize(const std::string& text) {
    CacheManifest m;

    auto extract = [&](const std::string& key, std::string& out) {
        auto pos = text.find("\"" + key + "\"");
        if (pos == std::string::npos) return false;
        pos = text.find(':', pos);
        if (pos == std::string::npos) return false;
        pos = text.find('"', pos + 1);
        if (pos == std::string::npos) return false;
        auto end = text.find('"', pos + 1);
        if (end == std::string::npos) return false;
        out = text.substr(pos + 1, end - pos - 1);
        return true;
    };

    auto extractNum = [&](const std::string& key, uint64_t& out) {
        auto pos = text.find("\"" + key + "\"");
        if (pos == std::string::npos) return false;
        pos = text.find(':', pos);
        if (pos == std::string::npos) return false;
        pos++;
        while (pos < text.size() && text[pos] == ' ') pos++;
        auto end = pos;
        while (end < text.size() && std::isdigit(text[end])) end++;
        if (end == pos) return false;
        out = std::stoull(text.substr(pos, end - pos));
        return true;
    };

    auto extractSize = [&](const std::string& key, size_t& out) {
        uint64_t v = 0;
        if (!extractNum(key, v)) return false;
        out = static_cast<size_t>(v);
        return true;
    };

    if (!extract("keyDigest", m.keyDigest)) return Status::failure("Missing keyDigest");
    if (!extract("sourceHash", m.sourceHash)) return Status::failure("Missing sourceHash");
    if (!extract("stage", m.stage)) return Status::failure("Missing stage");
    if (!extract("language", m.language)) return Status::failure("Missing language");
    if (!extract("compilerVersion", m.compilerVersion)) return Status::failure("Missing compilerVersion");
    if (!extract("spirvHash", m.spirvHash)) return Status::failure("Missing spirvHash");
    if (!extract("reflectionHash", m.reflectionHash)) return Status::failure("Missing reflectionHash");
    if (!extractSize("spirvSize", m.spirvSize)) return Status::failure("Missing spirvSize");
    if (!extractSize("reflectionSize", m.reflectionSize)) return Status::failure("Missing reflectionSize");
    extractNum("timestamp", m.timestamp);

    auto depPos = text.find("\"dependencyHashes\"");
    if (depPos != std::string::npos) {
        auto arrStart = text.find('[', depPos);
        auto arrEnd = text.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            auto arr = text.substr(arrStart + 1, arrEnd - arrStart - 1);
            size_t p = 0;
            while (p < arr.size()) {
                auto q = arr.find('"', p);
                if (q == std::string::npos) break;
                auto r = arr.find('"', q + 1);
                if (r == std::string::npos) break;
                m.dependencyHashes.push_back(arr.substr(q + 1, r - q - 1));
                p = r + 1;
            }
        }
    }

    auto dbgPos = text.find("\"debugInfo\"");
    if (dbgPos != std::string::npos) {
        m.debugInfo = text.find("true", dbgPos) != std::string::npos;
    }

    return m;
}

ShaderCache::ShaderCache(std::filesystem::path directory, uint64_t max_size_bytes)
    : directory_(std::move(directory))
    , max_size_bytes_(max_size_bytes) {}

std::string ShaderCache::hashContent(const std::string& content) {
    return Hash::hex(Hash::fnv1a(content));
}

std::string ShaderCache::hashFile(const std::filesystem::path& path) {
    auto result = FileSystem::readText(path);
    if (!result) return "";
    return hashContent(result.value());
}

ShaderCacheKey ShaderCache::makeKey(const CacheKeyComponents& components) const {
    std::string depHashes;
    for (size_t i = 0; i < components.dependencyPaths.size(); ++i) {
        depHashes += components.dependencyPaths[i].string();
        depHashes.push_back('\0');
        if (i < components.dependencyContents.size()) {
            depHashes += components.dependencyContents[i];
        }
        depHashes.push_back('\n');
    }

    std::string flagPart = components.debugInfo ? "debug" : "release";

    return ShaderCacheKey{
        Hash::combine({
            components.sourceContent,
            components.stage,
            components.language,
            components.compilerVersion,
            depHashes,
            flagPart
        }),
        components
    };
}

ShaderCacheEntry ShaderCache::entryFor(const ShaderCacheKey& key) const {
    auto base = directory_ / key.digest;
    ShaderCacheEntry entry;
    entry.spirvPath = base;
    entry.spirvPath += ".spv";
    entry.reflectionPath = base;
    entry.reflectionPath += ".reflection.json";
    entry.manifestPath = base;
    entry.manifestPath += ".manifest.json";

    std::error_code ec;
    bool hasFiles = std::filesystem::exists(entry.spirvPath, ec) && !ec &&
                    std::filesystem::exists(entry.reflectionPath, ec) && !ec &&
                    std::filesystem::exists(entry.manifestPath, ec) && !ec;

    if (hasFiles) {
        auto manifestText = FileSystem::readText(entry.manifestPath);
        if (manifestText) {
            auto manifest = CacheManifest::deserialize(manifestText.value());
            if (manifest) {
                entry.manifest = manifest.value();
                entry.hit = true;
            } else {
                corrupted_++;
            }
        } else {
            corrupted_++;
        }
    }

    return entry;
}

bool ShaderCache::contains(const ShaderCacheKey& key) const {
    return entryFor(key).hit;
}

Status ShaderCache::store(const ShaderCacheKey& key,
                          const std::vector<unsigned char>& spirv,
                          const std::string& reflectionJson) const {
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);

    auto base = directory_ / key.digest;
    auto spirvPath = base;
    spirvPath += ".spv";
    auto reflectionPath = base;
    reflectionPath += ".reflection.json";
    auto manifestPath = base;
    manifestPath += ".manifest.json";

    auto spirvTmp = base;
    spirvTmp += ".spv.tmp";
    auto reflectionTmp = base;
    reflectionTmp += ".reflection.json.tmp";
    auto manifestTmp = base;
    manifestTmp += ".manifest.json.tmp";

    auto writeStatus = FileSystem::writeBinary(spirvTmp, spirv);
    if (!writeStatus) return writeStatus;

    writeStatus = FileSystem::writeText(reflectionTmp, reflectionJson);
    if (!writeStatus) {
        std::filesystem::remove(spirvTmp, ec);
        return writeStatus;
    }

    CacheManifest manifest;
    manifest.keyDigest = key.digest;
    manifest.sourceHash = hashContent(key.components.sourceContent);
    manifest.stage = key.components.stage;
    manifest.language = key.components.language;
    manifest.compilerVersion = key.components.compilerVersion;
    for (const auto& dep : key.components.dependencyPaths) {
        manifest.dependencyHashes.push_back(hashFile(dep));
    }
    manifest.spirvSize = spirv.size();
    manifest.spirvHash = Hash::hex(Hash::fnv1a(
        std::string(reinterpret_cast<const char*>(spirv.data()), spirv.size())));
    manifest.reflectionSize = reflectionJson.size();
    manifest.reflectionHash = hashContent(reflectionJson);
    manifest.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    manifest.debugInfo = key.components.debugInfo;

    writeStatus = FileSystem::writeText(manifestTmp, manifest.serialize());
    if (!writeStatus) {
        std::filesystem::remove(spirvTmp, ec);
        std::filesystem::remove(reflectionTmp, ec);
        return writeStatus;
    }

    std::filesystem::rename(spirvTmp, spirvPath, ec);
    if (ec) {
        std::filesystem::remove(spirvTmp, ec);
        return Status::failure("Atomic rename failed for SPIR-V: " + ec.message());
    }
    std::filesystem::rename(reflectionTmp, reflectionPath, ec);
    if (ec) {
        return Status::failure("Atomic rename failed for reflection: " + ec.message());
    }
    std::filesystem::rename(manifestTmp, manifestPath, ec);
    if (ec) {
        return Status::failure("Atomic rename failed for manifest: " + ec.message());
    }

    return Status::success();
}

bool ShaderCache::verifyIntegrity(const ShaderCacheEntry& entry) const {
    if (!entry.hit) return false;

    std::error_code ec;
    auto actualSize = std::filesystem::file_size(entry.spirvPath, ec);
    if (ec || actualSize != entry.manifest.spirvSize) return false;

    auto spirvData = FileSystem::readText(entry.spirvPath);
    if (!spirvData) return false;
    std::string spirvStr(spirvData.value().begin(), spirvData.value().end());
    auto actualHash = Hash::hex(Hash::fnv1a(spirvStr));
    if (actualHash != entry.manifest.spirvHash) return false;

    auto reflSize = std::filesystem::file_size(entry.reflectionPath, ec);
    if (ec || reflSize != entry.manifest.reflectionSize) return false;

    auto reflContent = FileSystem::readText(entry.reflectionPath);
    if (!reflContent) return false;
    if (hashContent(reflContent.value()) != entry.manifest.reflectionHash) return false;

    return true;
}

Status ShaderCache::invalidate(const ShaderCacheKey& key) const {
    auto entry = entryFor(key);
    if (!entry.hit) return Status::success();

    std::error_code ec;
    std::filesystem::remove(entry.spirvPath, ec);
    std::filesystem::remove(entry.reflectionPath, ec);
    std::filesystem::remove(entry.manifestPath, ec);
    return Status::success();
}

Status ShaderCache::invalidateAll() {
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(directory_, ec)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".spv" || ext == ".json" || ext == ".tmp") {
                std::filesystem::remove(entry.path(), ec);
            }
        }
    }
    return Status::success();
}

Status ShaderCache::evict() {
    struct CacheEntryInfo {
        std::filesystem::path path;
        uint64_t timestamp = 0;
        uint64_t size = 0;
    };

    std::vector<CacheEntryInfo> entries;
    std::error_code ec;
    uint64_t totalSize = 0;

    for (auto& entry : std::filesystem::directory_iterator(directory_, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".manifest.json") continue;

        auto manifestText = FileSystem::readText(entry.path());
        if (!manifestText) continue;

        auto manifest = CacheManifest::deserialize(manifestText.value());
        if (!manifest) continue;

        auto baseName = entry.path().stem().stem();
        auto spirvPath = directory_ / (baseName.string() + ".spv");
        auto spirvSize = std::filesystem::file_size(spirvPath, ec);
        if (ec) continue;

        totalSize += spirvSize;
        entries.push_back({baseName, manifest.value().timestamp, spirvSize});
    }

    if (totalSize <= max_size_bytes_) return Status::success();

    std::sort(entries.begin(), entries.end(),
        [](const CacheEntryInfo& a, const CacheEntryInfo& b) {
            return a.timestamp < b.timestamp;
        });

    for (const auto& e : entries) {
        if (totalSize <= max_size_bytes_ * 80 / 100) break;
        auto spv = directory_ / (e.path.string() + ".spv");
        auto ref = directory_ / (e.path.string() + ".reflection.json");
        auto mft = directory_ / (e.path.string() + ".manifest.json");
        std::filesystem::remove(spv, ec);
        std::filesystem::remove(ref, ec);
        std::filesystem::remove(mft, ec);
        totalSize -= e.size;
        evictions_++;
    }

    return Status::success();
}

CacheStats ShaderCache::stats() const {
    CacheStats s;
    s.hits = hits_;
    s.misses = misses_;
    s.evictions = evictions_;
    s.corrupted = corrupted_;

    std::error_code ec;
    uint64_t totalSize = 0;
    uint64_t count = 0;
    for (auto& entry : std::filesystem::directory_iterator(directory_, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".spv") {
            totalSize += std::filesystem::file_size(entry.path(), ec);
            count++;
        }
    }
    s.totalEntries = count;
    s.totalSizeBytes = totalSize;
    return s;
}

void ShaderCache::recordHit() const { hits_++; }
void ShaderCache::recordMiss() const { misses_++; }

}  // namespace monix::renderer_vk
