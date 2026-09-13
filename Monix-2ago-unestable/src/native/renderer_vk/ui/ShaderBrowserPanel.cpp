#include "ShaderBrowserPanel.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <unordered_map>

namespace monix::renderer_vk {

ShaderBrowserPanel::ShaderBrowserPanel(
    ShaderLibrary& library,
    ShaderLibraryCompiler& compiler,
    ShaderRuntime& runtime,
    ShaderBrowserPanelState& state)
    : library_(library)
    , compiler_(compiler)
    , runtime_(runtime)
    , state_(state)
{
}

void ShaderBrowserPanel::setSubTab(SettingsSubTab tab) {
    state_.subTab = tab;
}

SettingsSubTab ShaderBrowserPanel::subTab() const {
    return state_.subTab;
}

void ShaderBrowserPanel::setFilter(ShaderLanguageFilter filter) {
    state_.filter = filter;
    state_.selectedShaderIndex = -1;
    state_.scrollOffset = 0;
}

ShaderLanguageFilter ShaderBrowserPanel::filter() const {
    return state_.filter;
}

void ShaderBrowserPanel::selectShader(int index) {
    const auto indices = filteredIndices();
    if (index >= 0 && index < static_cast<int>(indices.size())) {
        state_.selectedShaderIndex = index;
    }
}

int ShaderBrowserPanel::selectedShaderIndex() const {
    return state_.selectedShaderIndex;
}

ShaderLanguage ShaderBrowserPanel::languageFromFilter() const {
    switch (state_.filter) {
    case ShaderLanguageFilter::GLSL:       return ShaderLanguage::GLSL;
    case ShaderLanguageFilter::Slang:      return ShaderLanguage::Slang;
    case ShaderLanguageFilter::Preset:     return ShaderLanguage::Slang;
    case ShaderLanguageFilter::All:        return ShaderLanguage::Unknown;
    }
    return ShaderLanguage::Unknown;
}

bool ShaderBrowserPanel::isLanguageVisible(ShaderLanguage lang) const {
    if (state_.filter == ShaderLanguageFilter::All) {
        return lang != ShaderLanguage::CG;
    }
    if (state_.filter == ShaderLanguageFilter::Preset) {
        return false;
    }
    return lang == languageFromFilter();
}

bool ShaderBrowserPanel::isExtensionVisible(const std::string& ext) const {
    if (ext == ".cg") return false;
    if (state_.filter == ShaderLanguageFilter::All) return true;
    if (state_.filter == ShaderLanguageFilter::GLSL) {
        return ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".geom" || ext == ".comp";
    }
    if (state_.filter == ShaderLanguageFilter::Slang) {
        return ext == ".slang";
    }
    if (state_.filter == ShaderLanguageFilter::Preset) {
        return ext == ".slangp" || ext == ".glslp";
    }
    return true;
}

const char* ShaderBrowserPanel::statusIndicator(ShaderEntryStatus status) {
    switch (status) {
    case ShaderEntryStatus::Unknown:    return "empty";
    case ShaderEntryStatus::Pending:    return "pending";
    case ShaderEntryStatus::Compiling:  return "compiling";
    case ShaderEntryStatus::Validating: return "validating";
    case ShaderEntryStatus::Activating: return "activating";
    case ShaderEntryStatus::Compiled:   return "compiled";
    case ShaderEntryStatus::Active:     return "active";
    case ShaderEntryStatus::Reloading:  return "reloading";
    case ShaderEntryStatus::Error:      return "error";
    }
    return "unknown";
}

const char* ShaderBrowserPanel::statusColor(ShaderEntryStatus status) {
    switch (status) {
    case ShaderEntryStatus::Unknown:    return "white";
    case ShaderEntryStatus::Pending:    return "gray";
    case ShaderEntryStatus::Compiling:  return "yellow";
    case ShaderEntryStatus::Validating: return "cyan";
    case ShaderEntryStatus::Activating: return "cyan";
    case ShaderEntryStatus::Compiled:   return "green";
    case ShaderEntryStatus::Active:     return "blue";
    case ShaderEntryStatus::Reloading:  return "magenta";
    case ShaderEntryStatus::Error:      return "red";
    }
    return "white";
}

std::vector<ShaderCategoryGroup> ShaderBrowserPanel::categoryGroups() const {
    std::vector<ShaderCategoryGroup> groups;
    const auto indices = filteredIndices();

    std::unordered_map<std::string, size_t> categoryMap;
    for (const size_t idx : indices) {
        const auto* e = library_.entry(idx);
        if (!e) continue;
        auto it = categoryMap.find(e->category);
        if (it == categoryMap.end()) {
            categoryMap[e->category] = groups.size();
            ShaderCategoryGroup g;
            g.name = e->category;
            groups.push_back(std::move(g));
        }
        groups[categoryMap[e->category]].entryIndices.push_back(idx);
    }

    std::sort(groups.begin(), groups.end(),
        [](const ShaderCategoryGroup& a, const ShaderCategoryGroup& b) {
            return a.name < b.name;
        });

    return groups;
}

std::vector<ShaderRowInfo> ShaderBrowserPanel::visibleShaders() const {
    std::vector<ShaderRowInfo> result;
    const auto indices = filteredIndices();
    result.reserve(indices.size());

    for (const size_t idx : indices) {
        const auto* e = library_.entry(idx);
        if (!e) continue;
        ShaderRowInfo row;
        row.libraryIndex = idx;
        row.name = e->name;
        row.category = e->category;
        row.status = e->status;
        row.isActive = e->isActive;
        result.push_back(std::move(row));
    }

    return result;
}

DetailPanelInfo ShaderBrowserPanel::detailInfo() const {
    DetailPanelInfo info;
    const auto indices = filteredIndices();

    if (state_.selectedShaderIndex < 0 || state_.selectedShaderIndex >= static_cast<int>(indices.size())) {
        return info;
    }

    const size_t libIdx = indices[state_.selectedShaderIndex];
    const auto* e = library_.entry(libIdx);
    if (!e) return info;

    info.hasSelection = true;
    info.name = e->name;
    info.path = e->path.string();
    info.extension = e->extension;
    info.language = shaderLanguageName(e->language);
    info.category = e->category;
    info.status = e->status;
    info.contentHash = e->contentHash;
    info.fileSize = e->fileSize;
    info.lastWriteTime = e->lastWriteTime;
    info.isActive = e->isActive;
    info.hasCompiledModule = e->compiledModule.compiled;

    // 14.F — extended detail
    info.spirvSizeBytes = e->spirvSizeBytes;
    info.lastCacheStatus = e->lastCacheStatus;
    info.compileDurationMs = e->compileDurationMs;
    info.validationDurationMs = e->validationDurationMs;
    info.pipelineDurationMs = e->pipelineDurationMs;
    info.totalActivationMs = e->totalActivationMs;

    const auto* diag = compiler_.diagnostics(libIdx);
    if (diag) {
        info.diagnosticCount = static_cast<int>(diag->entries().size());
        for (const auto& d : diag->entries()) {
            if (d.severity == DiagnosticSeverity::Error) {
                info.firstError = d.message;
                break;
            }
        }
    }

    return info;
}

CacheStatsInfo ShaderBrowserPanel::cacheStats() const {
    CacheStatsInfo info;
    info.memoryLimitBytes = 512 * 1024 * 1024;
    const auto* cache = runtime_.cache();
    if (cache) {
        auto s = cache->stats();
        info.hits = s.hits;
        info.misses = s.misses;
        info.evictions = s.evictions;
        info.corrupted = s.corrupted;
        info.totalEntries = s.totalEntries;
        info.totalSizeBytes = s.totalSizeBytes;
        uint64_t total = s.hits + s.misses;
        info.hitRate = total > 0 ? static_cast<double>(s.hits) / total : 0.0;
    }
    return info;
}

void ShaderBrowserPanel::requestLoad(TransactionalShaderState& txState) {
    const auto indices = filteredIndices();
    if (state_.selectedShaderIndex < 0 || state_.selectedShaderIndex >= static_cast<int>(indices.size())) {
        return;
    }

    state_.loadRequested = true;
    const size_t libIdx = indices[state_.selectedShaderIndex];
    compiler_.compileAndActivate(libIdx, txState);

    const auto* entry = library_.entry(libIdx);
    if (entry && entry->status == ShaderEntryStatus::Error) {
        recordError(entry->name, "", entry->error, entry->path.string());
    }
    state_.loadRequested = false;
}

void ShaderBrowserPanel::requestReload(TransactionalShaderState& txState) {
    const auto indices = filteredIndices();
    if (state_.selectedShaderIndex < 0 || state_.selectedShaderIndex >= static_cast<int>(indices.size())) {
        return;
    }

    state_.reloadRequested = true;
    const size_t libIdx = indices[state_.selectedShaderIndex];
    compiler_.compileAndActivate(libIdx, txState);

    const auto* entry = library_.entry(libIdx);
    if (entry && entry->status == ShaderEntryStatus::Error) {
        recordError(entry->name, "", entry->error, entry->path.string());
    }
    state_.reloadRequested = false;
}

bool ShaderBrowserPanel::hasSelection() const {
    return state_.selectedShaderIndex >= 0;
}

const ShaderLibraryEntry* ShaderBrowserPanel::selectedEntry() const {
    const auto indices = filteredIndices();
    if (state_.selectedShaderIndex < 0 || state_.selectedShaderIndex >= static_cast<int>(indices.size())) {
        return nullptr;
    }
    return library_.entry(indices[state_.selectedShaderIndex]);
}

int ShaderBrowserPanel::totalShaderCount() const {
    return static_cast<int>(library_.entryCount());
}

int ShaderBrowserPanel::filteredShaderCount() const {
    return static_cast<int>(filteredIndices().size());
}

std::string ShaderBrowserPanel::statusSummary() const {
    int unknown = 0, pending = 0, compiling = 0, validating = 0, activating = 0;
    int compiled = 0, active = 0, reloading = 0, errored = 0;
    const auto indices = filteredIndices();
    for (const size_t idx : indices) {
        const auto* e = library_.entry(idx);
        if (!e) continue;
        switch (e->status) {
        case ShaderEntryStatus::Unknown:    ++unknown; break;
        case ShaderEntryStatus::Pending:    ++pending; break;
        case ShaderEntryStatus::Compiling:  ++compiling; break;
        case ShaderEntryStatus::Validating: ++validating; break;
        case ShaderEntryStatus::Activating: ++activating; break;
        case ShaderEntryStatus::Compiled:   ++compiled; break;
        case ShaderEntryStatus::Active:     ++active; break;
        case ShaderEntryStatus::Reloading:  ++reloading; break;
        case ShaderEntryStatus::Error:      ++errored; break;
        }
    }
    std::ostringstream oss;
    oss << "Total: " << library_.entryCount()
        << " | Filtered: " << indices.size()
        << " | Active: " << active
        << " | Compiled: " << compiled
        << " | Error: " << errored;
    return oss.str();
}

void ShaderBrowserPanel::handleFileChanged(const std::filesystem::path& path) {
    compiler_.handleFileChanged(path);
}

// ============================================================================
// 14.A — active shader info
// ============================================================================
const ShaderLibraryEntry* ShaderBrowserPanel::activeShader() const {
    const size_t count = library_.entryCount();
    for (size_t i = 0; i < count; ++i) {
        const auto* e = library_.entry(i);
        if (e && e->isActive) return e;
    }
    return nullptr;
}

// ============================================================================
// 14.I — error history
// ============================================================================
void ShaderBrowserPanel::recordError(const std::string& shader, const std::string& stage,
                                     const std::string& error, const std::string& file,
                                     int line, int column) {
    ErrorHistoryEntry entry;
    entry.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    entry.shader = shader;
    entry.stage = stage;
    entry.error = error;
    entry.file = file;
    entry.line = line;
    entry.column = column;
    errorHistory_.push_back(std::move(entry));
    while (errorHistory_.size() > kMaxErrorHistory) {
        errorHistory_.pop_front();
    }
}

const std::deque<ErrorHistoryEntry>& ShaderBrowserPanel::errorHistory() const {
    return errorHistory_;
}

void ShaderBrowserPanel::clearErrorHistory() {
    errorHistory_.clear();
}

// ============================================================================
// 14.J — renderer diagnostics
// ============================================================================
void ShaderBrowserPanel::setRendererDiagnostics(RendererDiagnosticsInfo info) {
    rendererDiag_ = std::move(info);
}

RendererDiagnosticsInfo ShaderBrowserPanel::rendererDiagnostics() const {
    return rendererDiag_;
}

// ============================================================================
// 14.G — load/reload state queries
// ============================================================================
bool ShaderBrowserPanel::isCompiling() const {
    const auto* e = selectedEntry();
    return e && e->status == ShaderEntryStatus::Compiling;
}

bool ShaderBrowserPanel::isValidating() const {
    const auto* e = selectedEntry();
    return e && e->status == ShaderEntryStatus::Validating;
}

bool ShaderBrowserPanel::isActivating() const {
    const auto* e = selectedEntry();
    return e && e->status == ShaderEntryStatus::Activating;
}

bool ShaderBrowserPanel::isReloading() const {
    const auto* e = selectedEntry();
    return e && e->status == ShaderEntryStatus::Reloading;
}

// ============================================================================
// 14.H — hot reload state
// ============================================================================
void ShaderBrowserPanel::recordHotReloadEvent(const HotReloadEvent& event) {
    lastHotReload_ = event;
}

ShaderBrowserPanel::HotReloadEvent ShaderBrowserPanel::lastHotReloadEvent() const {
    return lastHotReload_;
}

// ============================================================================
// FASE 16.8 — Search
// ============================================================================
void ShaderBrowserPanel::setSearchQuery(const std::string& query) {
    searchQuery_ = query;
    state_.selectedShaderIndex = -1;
}

std::string ShaderBrowserPanel::searchQuery() const {
    return searchQuery_;
}

bool ShaderBrowserPanel::matchesSearch(const ShaderLibraryEntry& entry) const {
    if (searchQuery_.empty()) return true;

    auto toLower = [](const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    };

    std::string query = toLower(searchQuery_);
    std::string name = toLower(entry.name);
    std::string path = toLower(entry.relativePath.string());
    std::string category = toLower(entry.category);
    std::string lang = toLower(shaderLanguageName(entry.language));

    return name.find(query) != std::string::npos ||
           path.find(query) != std::string::npos ||
           category.find(query) != std::string::npos ||
           lang.find(query) != std::string::npos;
}

// ============================================================================
// FASE 16.6 — Favorites
// ============================================================================
void ShaderBrowserPanel::setFavorite(size_t libraryIndex, bool favorite) {
    auto* e = library_.entryMutable(libraryIndex);
    if (e) {
        e->isFavorite = favorite;
    }
}

bool ShaderBrowserPanel::isFavorite(size_t libraryIndex) const {
    const auto* e = library_.entry(libraryIndex);
    return e && e->isFavorite;
}

std::vector<ShaderRowInfo> ShaderBrowserPanel::favoriteShaders() const {
    std::vector<ShaderRowInfo> result;
    const size_t count = library_.entryCount();
    for (size_t i = 0; i < count; ++i) {
        const auto* e = library_.entry(i);
        if (!e || !e->isFavorite) continue;
        if (!matchesSearch(*e)) continue;
        ShaderRowInfo row;
        row.libraryIndex = i;
        row.name = e->name;
        row.category = e->category;
        row.status = e->status;
        row.isActive = e->isActive;
        result.push_back(std::move(row));
    }
    return result;
}

// ============================================================================
// FASE 16.11 — auto-populate diagnostics
// ============================================================================
void ShaderBrowserPanel::autoPopulateDiagnostics(const std::string& vendor, const std::string& renderer,
                                                  const std::string& apiVersion, bool validationAv, bool validationEn) {
    rendererDiag_.gpuVendor = vendor;
    rendererDiag_.gpuRenderer = renderer;
    rendererDiag_.apiVersion = apiVersion;
    rendererDiag_.validationAvailable = validationAv;
    rendererDiag_.validationEnabled = validationEn;
}

// ============================================================================
// FASE 16.3 — filteredIndices with search + source kind filtering
// ============================================================================
std::vector<size_t> ShaderBrowserPanel::filteredIndices() const {
    std::vector<size_t> result;
    const size_t count = library_.entryCount();
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const auto* e = library_.entry(i);
        if (!e) continue;

        // FASE 16.2 — Test shaders never shown in production browser
        if (e->sourceKind == ShaderSourceKind::Test) continue;

        if (state_.filter == ShaderLanguageFilter::All) {
            if (e->language == ShaderLanguage::CG) continue;
            if (!matchesSearch(*e)) continue;
            result.push_back(i);
            continue;
        }

        if (state_.filter == ShaderLanguageFilter::Preset) {
            if ((e->extension == ".slangp" || e->extension == ".glslp") && matchesSearch(*e)) {
                result.push_back(i);
            }
            continue;
        }

        ShaderLanguage target = languageFromFilter();
        if (e->language == target && e->extension != ".slangp" && e->extension != ".glslp" && matchesSearch(*e)) {
            result.push_back(i);
        }
    }

    return result;
}

}  // namespace monix::renderer_vk
