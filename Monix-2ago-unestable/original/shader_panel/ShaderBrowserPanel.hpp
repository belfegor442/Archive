#pragma once

#include "../library/ShaderLibrary.hpp"
#include "../library/ShaderLibraryCompiler.hpp"
#include "../shader_runtime/ShaderRuntime.hpp"
#include "../shader_runtime/TransactionalShaderState.hpp"
#include "../core/Result.hpp"

#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <cstdint>
#include <functional>

namespace monix::renderer_vk {

enum class SettingsSubTab : uint8_t {
    General,
    Display,
    Logging,
    Shaders,
    System,
    Performance
};

enum class ShaderLanguageFilter : uint8_t {
    All,
    GLSL,
    Slang,
    SlangPreset
};

struct ShaderBrowserPanelState {
    SettingsSubTab subTab = SettingsSubTab::General;
    ShaderLanguageFilter filter = ShaderLanguageFilter::All;
    int selectedShaderIndex = -1;
    int scrollOffset = 0;
    bool loadRequested = false;
    bool reloadRequested = false;
};

struct ShaderCategoryGroup {
    std::string name;
    std::vector<size_t> entryIndices;
};

struct ShaderRowInfo {
    size_t libraryIndex = 0;
    std::string name;
    std::string category;
    ShaderEntryStatus status = ShaderEntryStatus::Unknown;
    bool isActive = false;
};

struct DetailPanelInfo {
    bool hasSelection = false;
    std::string name;
    std::string path;
    std::string extension;
    std::string language;
    std::string category;
    ShaderEntryStatus status = ShaderEntryStatus::Unknown;
    uint64_t contentHash = 0;
    uint64_t fileSize = 0;
    uint64_t lastWriteTime = 0;
    bool isActive = false;
    bool hasCompiledModule = false;
    int diagnosticCount = 0;
    std::string firstError;

    // 14.F — extended detail
    size_t spirvSizeBytes = 0;
    std::string lastCacheStatus;
    double compileDurationMs = 0.0;
    double validationDurationMs = 0.0;
    double pipelineDurationMs = 0.0;
    double totalActivationMs = 0.0;
};

struct CacheStatsInfo {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    uint64_t corrupted = 0;
    uint64_t totalEntries = 0;
    uint64_t totalSizeBytes = 0;
    double hitRate = 0.0;
    uint64_t memoryLimitBytes = 0;
};

// 14.I — Error history entry
struct ErrorHistoryEntry {
    uint64_t timestamp = 0;
    std::string shader;
    std::string stage;
    std::string error;
    std::string file;
    int line = 0;
    int column = 0;
};

// 14.J — Renderer diagnostics
struct RendererDiagnosticsInfo {
    std::string gpuVendor;
    std::string gpuRenderer;
    std::string apiVersion;
    bool validationAvailable = false;
    bool validationEnabled = false;
};

class ShaderBrowserPanel {
public:
    ShaderBrowserPanel(
        ShaderLibrary& library,
        ShaderLibraryCompiler& compiler,
        ShaderRuntime& runtime,
        ShaderBrowserPanelState& state);

    void setSubTab(SettingsSubTab tab);
    SettingsSubTab subTab() const;

    void setFilter(ShaderLanguageFilter filter);
    ShaderLanguageFilter filter() const;

    void selectShader(int index);
    int selectedShaderIndex() const;

    std::vector<ShaderCategoryGroup> categoryGroups() const;
    std::vector<ShaderRowInfo> visibleShaders() const;
    DetailPanelInfo detailInfo() const;
    CacheStatsInfo cacheStats() const;

    void requestLoad(TransactionalShaderState& txState);
    void requestReload(TransactionalShaderState& txState);

    bool isLanguageVisible(ShaderLanguage lang) const;
    bool isExtensionVisible(const std::string& ext) const;
    static const char* statusIndicator(ShaderEntryStatus status);
    static const char* statusColor(ShaderEntryStatus status);

    bool hasSelection() const;
    const ShaderLibraryEntry* selectedEntry() const;

    int totalShaderCount() const;
    int filteredShaderCount() const;
    std::string statusSummary() const;

    void handleFileChanged(const std::filesystem::path& path);

    // 14.A — active shader info
    const ShaderLibraryEntry* activeShader() const;

    // 14.I — error history
    void recordError(const std::string& shader, const std::string& stage,
                     const std::string& error, const std::string& file = "",
                     int line = 0, int column = 0);
    const std::deque<ErrorHistoryEntry>& errorHistory() const;
    void clearErrorHistory();

    // 14.J — renderer diagnostics
    void setRendererDiagnostics(RendererDiagnosticsInfo info);
    RendererDiagnosticsInfo rendererDiagnostics() const;

    // 14.G — load/reload state queries
    bool isCompiling() const;
    bool isValidating() const;
    bool isActivating() const;
    bool isReloading() const;

    // 14.H — hot reload state
    struct HotReloadEvent {
        bool active = false;
        std::string filename;
        std::string status;
        double durationMs = 0.0;
    };
    void recordHotReloadEvent(const HotReloadEvent& event);
    HotReloadEvent lastHotReloadEvent() const;

    // FASE 16.8 — Search
    void setSearchQuery(const std::string& query);
    std::string searchQuery() const;

    // FASE 16.6 — Favorites
    void setFavorite(size_t libraryIndex, bool favorite);
    bool isFavorite(size_t libraryIndex) const;
    std::vector<ShaderRowInfo> favoriteShaders() const;

    // FASE 16.11 — auto-populate diagnostics
    void autoPopulateDiagnostics(const std::string& vendor, const std::string& renderer,
                                 const std::string& apiVersion, bool validationAvail, bool validationEn);

private:
    ShaderLanguage languageFromFilter() const;
    std::vector<size_t> filteredIndices() const;
    bool matchesSearch(const ShaderLibraryEntry& entry) const;

    ShaderLibrary& library_;
    ShaderLibraryCompiler& compiler_;
    ShaderRuntime& runtime_;
    ShaderBrowserPanelState& state_;

    // FASE 16.8
    std::string searchQuery_;

    // 14.I
    std::deque<ErrorHistoryEntry> errorHistory_;
    static constexpr size_t kMaxErrorHistory = 20;

    // 14.J
    RendererDiagnosticsInfo rendererDiag_;

    // 14.H
    HotReloadEvent lastHotReload_;
};

}  // namespace monix::renderer_vk
