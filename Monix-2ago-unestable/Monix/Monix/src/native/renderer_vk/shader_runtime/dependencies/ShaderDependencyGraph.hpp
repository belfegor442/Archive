#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace monix::renderer_vk {

enum class DepNodeType : std::uint8_t {
    Preset,
    Shader,
    Include,
    Unknown
};

inline const char* depNodeTypeName(DepNodeType t) {
    switch (t) {
    case DepNodeType::Preset:  return "Preset";
    case DepNodeType::Shader:  return "Shader";
    case DepNodeType::Include: return "Include";
    case DepNodeType::Unknown: return "Unknown";
    }
    return "Unknown";
}

struct DepNode {
    std::filesystem::path path;
    DepNodeType type = DepNodeType::Unknown;
    uint64_t contentHash = 0;
    std::vector<std::filesystem::path> dependsOn;
};

struct AffectedResult {
    std::vector<std::filesystem::path> affectedPresets;
    std::vector<std::filesystem::path> affectedShaders;
    bool anyFound = false;
};

class ShaderDependencyGraph {
public:
    void registerPreset(const std::filesystem::path& presetPath);
    void registerShader(const std::filesystem::path& shaderPath);
    void registerInclude(const std::filesystem::path& includePath);

    void addDependency(const std::filesystem::path& from, const std::filesystem::path& to);

    void setNodeHash(const std::filesystem::path& path, uint64_t hash);
    void setNodeDependencies(const std::filesystem::path& path,
                             std::vector<std::filesystem::path> deps);

    AffectedResult invalidate(const std::filesystem::path& changedFile);
    std::vector<std::filesystem::path> getDependents(const std::filesystem::path& file) const;
    std::vector<std::filesystem::path> getDependencies(const std::filesystem::path& file) const;

    bool hasNode(const std::filesystem::path& path) const;
    const DepNode* node(const std::filesystem::path& path) const;
    DepNodeType nodeType(const std::filesystem::path& path) const;
    uint64_t nodeHash(const std::filesystem::path& path) const;

    size_t nodeCount() const { return nodes_.size(); }
    size_t edgeCount() const;

    void clear();

    std::vector<std::filesystem::path> allNodes() const;
    std::vector<std::filesystem::path> allPresets() const;
    std::vector<std::filesystem::path> allShaders() const;

private:
    std::filesystem::path normalize(const std::filesystem::path& p) const;
    void collectTransitive(const std::filesystem::path& start,
                           std::unordered_set<std::string>& visited,
                           std::vector<std::filesystem::path>& result) const;
    const DepNode* nodeUnlocked(const std::filesystem::path& path) const;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, DepNode> nodes_;
    std::unordered_map<std::string, std::unordered_set<std::string>> reverseIndex_;
};

}  // namespace monix::renderer_vk
