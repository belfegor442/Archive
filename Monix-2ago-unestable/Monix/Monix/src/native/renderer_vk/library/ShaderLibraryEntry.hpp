#pragma once

#include "../shader_runtime/core/ShaderLanguage.hpp"
#include "../shader_runtime/core/ShaderDiagnostics.hpp"
#include "../shader_runtime/core/ShaderModule.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace monix::renderer_vk {

enum class ShaderEntryStatus : uint8_t {
    Unknown,
    Pending,
    Compiling,
    Validating,
    Activating,
    Compiled,
    Active,
    Reloading,
    Error
};

inline const char* shaderEntryStatusName(ShaderEntryStatus s) {
    switch (s) {
    case ShaderEntryStatus::Unknown:    return "Unknown";
    case ShaderEntryStatus::Pending:    return "Pending";
    case ShaderEntryStatus::Compiling:  return "Compiling";
    case ShaderEntryStatus::Validating: return "Validating";
    case ShaderEntryStatus::Activating: return "Activating";
    case ShaderEntryStatus::Compiled:   return "Compiled";
    case ShaderEntryStatus::Active:     return "Active";
    case ShaderEntryStatus::Reloading:  return "Reloading";
    case ShaderEntryStatus::Error:      return "Error";
    }
    return "Unknown";
}

enum class ShaderSourceKind : uint8_t {
    User,
    Internal,
    Test
};

inline const char* shaderSourceKindName(ShaderSourceKind k) {
    switch (k) {
    case ShaderSourceKind::User:     return "User";
    case ShaderSourceKind::Internal: return "Internal";
    case ShaderSourceKind::Test:     return "Test";
    }
    return "User";
}

struct ShaderLibraryEntry {
    std::string name;
    std::filesystem::path path;
    std::filesystem::path relativePath;
    ShaderLanguage language = ShaderLanguage::Unknown;
    std::string extension;
    std::string category;
    ShaderEntryStatus status = ShaderEntryStatus::Unknown;
    ShaderSourceKind sourceKind = ShaderSourceKind::User;
    std::string error;
    uint64_t contentHash = 0;
    uint64_t fileSize = 0;
    uint64_t lastWriteTime = 0;
    bool isActive = false;
    bool isFavorite = false;

    ShaderModule compiledModule;
    ShaderDiagnostics diagnostics;

    // Compile metrics (14.E)
    double compileDurationMs = 0.0;
    double validationDurationMs = 0.0;
    double pipelineDurationMs = 0.0;
    double totalActivationMs = 0.0;

    // SPIR-V info (14.F)
    size_t spirvSizeBytes = 0;
    std::string lastCacheStatus;  // "HIT", "MISS", "N/A"
};

}  // namespace monix::renderer_vk
