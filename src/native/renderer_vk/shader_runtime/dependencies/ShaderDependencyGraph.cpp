#include "ShaderDependencyGraph.hpp"

#include <algorithm>

namespace monix::renderer_vk {

std::filesystem::path ShaderDependencyGraph::normalize(const std::filesystem::path& p) const {
    std::error_code ec;
    auto abs = std::filesystem::absolute(p, ec);
    if (ec) return p;
    auto canonical = std::filesystem::weakly_canonical(abs, ec);
    if (ec) return abs;
    return canonical;
}

void ShaderDependencyGraph::registerPreset(const std::filesystem::path& presetPath) {
    std::lock_guard lock(mutex_);
    auto key = normalize(presetPath).string();
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        DepNode node;
        node.path = normalize(presetPath);
        node.type = DepNodeType::Preset;
        nodes_[key] = std::move(node);
    } else {
        it->second.type = DepNodeType::Preset;
    }
}

void ShaderDependencyGraph::registerShader(const std::filesystem::path& shaderPath) {
    std::lock_guard lock(mutex_);
    auto key = normalize(shaderPath).string();
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        DepNode node;
        node.path = normalize(shaderPath);
        node.type = DepNodeType::Shader;
        nodes_[key] = std::move(node);
    } else {
        if (it->second.type == DepNodeType::Unknown)
            it->second.type = DepNodeType::Shader;
    }
}

void ShaderDependencyGraph::registerInclude(const std::filesystem::path& includePath) {
    std::lock_guard lock(mutex_);
    auto key = normalize(includePath).string();
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        DepNode node;
        node.path = normalize(includePath);
        node.type = DepNodeType::Include;
        nodes_[key] = std::move(node);
    } else {
        if (it->second.type == DepNodeType::Unknown)
            it->second.type = DepNodeType::Include;
    }
}

void ShaderDependencyGraph::addDependency(const std::filesystem::path& from,
                                           const std::filesystem::path& to) {
    std::lock_guard lock(mutex_);
    auto fromKey = normalize(from).string();
    auto toKey = normalize(to).string();

    auto it = nodes_.find(fromKey);
    if (it != nodes_.end()) {
        bool alreadyDepends = false;
        for (const auto& d : it->second.dependsOn) {
            if (normalize(d).string() == toKey) {
                alreadyDepends = true;
                break;
            }
        }
        if (!alreadyDepends) {
            it->second.dependsOn.push_back(normalize(to));
        }
    } else {
        DepNode node;
        node.path = normalize(from);
        node.type = DepNodeType::Unknown;
        node.dependsOn.push_back(normalize(to));
        nodes_[fromKey] = std::move(node);
    }

    reverseIndex_[toKey].insert(fromKey);

    if (nodes_.find(toKey) == nodes_.end()) {
        DepNode node;
        node.path = normalize(to);
        node.type = DepNodeType::Unknown;
        nodes_[toKey] = std::move(node);
    }
}

void ShaderDependencyGraph::setNodeHash(const std::filesystem::path& path, uint64_t hash) {
    std::lock_guard lock(mutex_);
    auto key = normalize(path).string();
    auto it = nodes_.find(key);
    if (it != nodes_.end()) {
        it->second.contentHash = hash;
    }
}

void ShaderDependencyGraph::setNodeDependencies(
    const std::filesystem::path& path,
    std::vector<std::filesystem::path> deps) {

    std::lock_guard lock(mutex_);
    auto key = normalize(path).string();
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        DepNode node;
        node.path = normalize(path);
        node.type = DepNodeType::Unknown;
        node.dependsOn = std::move(deps);
        nodes_[key] = std::move(node);
    } else {
        for (const auto& oldDep : it->second.dependsOn) {
            auto oldKey = normalize(oldDep).string();
            auto rit = reverseIndex_.find(oldKey);
            if (rit != reverseIndex_.end()) {
                rit->second.erase(key);
                if (rit->second.empty()) reverseIndex_.erase(rit);
            }
        }
        it->second.dependsOn = std::move(deps);
    }

    auto it2 = nodes_.find(key);
    for (const auto& dep : it2->second.dependsOn) {
        auto depKey = normalize(dep).string();
        reverseIndex_[depKey].insert(key);
        if (nodes_.find(depKey) == nodes_.end()) {
            DepNode node;
            node.path = normalize(dep);
            node.type = DepNodeType::Unknown;
            nodes_[depKey] = std::move(node);
        }
    }
}

AffectedResult ShaderDependencyGraph::invalidate(const std::filesystem::path& changedFile) {
    std::lock_guard lock(mutex_);
    AffectedResult result;
    std::unordered_set<std::string> visited;
    std::vector<std::filesystem::path> allAffected;
    collectTransitive(changedFile, visited, allAffected);

    for (const auto& affected : allAffected) {
        if (affected == normalize(changedFile)) continue;
        auto* n = nodeUnlocked(affected);
        if (!n) continue;
        if (n->type == DepNodeType::Preset) {
            result.affectedPresets.push_back(affected);
            result.anyFound = true;
        } else if (n->type == DepNodeType::Shader) {
            result.affectedShaders.push_back(affected);
            result.anyFound = true;
        }
    }

    std::sort(result.affectedPresets.begin(), result.affectedPresets.end());
    std::sort(result.affectedShaders.begin(), result.affectedShaders.end());
    return result;
}

std::vector<std::filesystem::path> ShaderDependencyGraph::getDependents(
    const std::filesystem::path& file) const {

    std::lock_guard lock(mutex_);
    auto key = normalize(file).string();
    std::vector<std::filesystem::path> result;
    auto it = reverseIndex_.find(key);
    if (it != reverseIndex_.end()) {
        for (const auto& depKey : it->second) {
            auto nodeIt = nodes_.find(depKey);
            if (nodeIt != nodes_.end()) {
                result.push_back(nodeIt->second.path);
            }
        }
    }
    return result;
}

std::vector<std::filesystem::path> ShaderDependencyGraph::getDependencies(
    const std::filesystem::path& file) const {

    std::lock_guard lock(mutex_);
    auto key = normalize(file).string();
    auto it = nodes_.find(key);
    if (it != nodes_.end()) {
        return it->second.dependsOn;
    }
    return {};
}

bool ShaderDependencyGraph::hasNode(const std::filesystem::path& path) const {
    std::lock_guard lock(mutex_);
    return nodes_.count(normalize(path).string()) > 0;
}

const DepNode* ShaderDependencyGraph::nodeUnlocked(const std::filesystem::path& path) const {
    auto it = nodes_.find(normalize(path).string());
    if (it != nodes_.end()) return &it->second;
    return nullptr;
}

const DepNode* ShaderDependencyGraph::node(const std::filesystem::path& path) const {
    std::lock_guard lock(mutex_);
    return nodeUnlocked(path);
}

DepNodeType ShaderDependencyGraph::nodeType(const std::filesystem::path& path) const {
    std::lock_guard lock(mutex_);
    auto* n = nodeUnlocked(path);
    return n ? n->type : DepNodeType::Unknown;
}

uint64_t ShaderDependencyGraph::nodeHash(const std::filesystem::path& path) const {
    std::lock_guard lock(mutex_);
    auto* n = nodeUnlocked(path);
    return n ? n->contentHash : 0;
}

size_t ShaderDependencyGraph::edgeCount() const {
    std::lock_guard lock(mutex_);
    size_t count = 0;
    for (const auto& [key, node] : nodes_) {
        count += node.dependsOn.size();
    }
    return count;
}

void ShaderDependencyGraph::clear() {
    std::lock_guard lock(mutex_);
    nodes_.clear();
    reverseIndex_.clear();
}

std::vector<std::filesystem::path> ShaderDependencyGraph::allNodes() const {
    std::lock_guard lock(mutex_);
    std::vector<std::filesystem::path> result;
    result.reserve(nodes_.size());
    for (const auto& [key, node] : nodes_) {
        result.push_back(node.path);
    }
    return result;
}

std::vector<std::filesystem::path> ShaderDependencyGraph::allPresets() const {
    std::lock_guard lock(mutex_);
    std::vector<std::filesystem::path> result;
    for (const auto& [key, node] : nodes_) {
        if (node.type == DepNodeType::Preset) result.push_back(node.path);
    }
    return result;
}

std::vector<std::filesystem::path> ShaderDependencyGraph::allShaders() const {
    std::lock_guard lock(mutex_);
    std::vector<std::filesystem::path> result;
    for (const auto& [key, node] : nodes_) {
        if (node.type == DepNodeType::Shader) result.push_back(node.path);
    }
    return result;
}

void ShaderDependencyGraph::collectTransitive(
    const std::filesystem::path& start,
    std::unordered_set<std::string>& visited,
    std::vector<std::filesystem::path>& result) const {

    auto startKey = normalize(start).string();
    if (visited.count(startKey)) return;
    visited.insert(startKey);

    auto rit = reverseIndex_.find(startKey);
    if (rit != reverseIndex_.end()) {
        for (const auto& depKey : rit->second) {
            if (visited.count(depKey)) continue;
            auto nodeIt = nodes_.find(depKey);
            if (nodeIt != nodes_.end()) {
                result.push_back(nodeIt->second.path);
                collectTransitive(nodeIt->second.path, visited, result);
            }
        }
    }
}

}  // namespace monix::renderer_vk
