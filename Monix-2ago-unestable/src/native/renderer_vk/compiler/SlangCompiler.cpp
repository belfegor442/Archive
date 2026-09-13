#include "SlangCompiler.hpp"

#include "../core/FileSystem.hpp"
#include "../core/Hash.hpp"
#include "ShaderCache.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef min
#undef max

namespace monix::renderer_vk {
namespace {

std::wstring quote(const std::filesystem::path& path) {
    return L"\"" + path.wstring() + L"\"";
}

std::vector<unsigned char> readBinary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return {};
    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

}  // namespace

std::string SlangCompiler::stageName(ShaderStage stage) {
    return stage == ShaderStage::Vertex ? "vertex" : "fragment";
}

std::string SlangCompiler::compilerVersion(const std::filesystem::path& slangcPath) {
    std::string seed = slangcPath.string();
    std::error_code ec;
    if (std::filesystem::exists(slangcPath, ec)) {
        seed += std::to_string(std::filesystem::file_size(slangcPath, ec));
        auto writeTime = std::filesystem::last_write_time(slangcPath, ec);
        if (!ec) {
            seed += std::to_string(writeTime.time_since_epoch().count());
        }
    }
    return Hash::hex(Hash::fnv1a(seed));
}

Result<ShaderCompileResult> SlangCompiler::compile(const ShaderCompileRequest& request) const {
    std::error_code ec;

    // Check for pre-compiled SPIR-V on disk first
    auto precompiledDir = request.outputDirectory / "precompiled";
    auto stem = request.sourcePath.stem().string();
    auto stageStr = stageName(request.stage);
    auto precompiledSpv = precompiledDir / (stem + "." + stageStr + ".spv");
    if (std::filesystem::exists(precompiledSpv, ec)) {
        ShaderCompileResult result;
        result.spirv = readBinary(precompiledSpv);
        result.cacheHit = true;
        if (result.spirv.empty()) {
            return Status::failure("Pre-compiled SPIR-V is empty: " + precompiledSpv.string());
        }
        ShaderReflectionParser parser;
        auto reflection = parser.fromSourceLayout(request.source);
        if (!reflection) return reflection.status();
        result.reflection = reflection.value();
        return result;
    }

    // Build cache key from all input factors
    if (cache_) {
        CacheKeyComponents components;
        components.sourceContent = request.source;
        components.stage = stageName(request.stage);
        components.language = (request.language == ShaderLanguage::GLSL) ? "glsl" : "slang";
        components.compilerVersion = compilerVersion(request.slangcPath);
        components.dependencyPaths = request.dependencies;
        components.debugInfo = request.debugInfo;

        for (const auto& dep : request.dependencies) {
            auto content = FileSystem::readText(dep);
            components.dependencyContents.push_back(
                content ? content.value() : "");
        }

        auto key = cache_->makeKey(components);
        auto entry = cache_->entryFor(key);

        if (entry.hit && cache_->verifyIntegrity(entry)) {
            ShaderCompileResult result;
            result.spirv = readBinary(entry.spirvPath);
            result.cacheHit = true;
            if (result.spirv.empty()) {
                cache_->recordMiss();
                return Status::failure("Cached SPIR-V is empty: " + entry.spirvPath.string());
            }
            auto reflectionText = FileSystem::readText(entry.reflectionPath);
            if (reflectionText) {
                result.reflectionJson = reflectionText.value();
            }
            ShaderReflectionParser parser;
            auto reflection = parser.fromSlangJson(result.reflectionJson, request.source);
            if (!reflection) return reflection.status();
            result.reflection = reflection.value();
            cache_->recordHit();
            return result;
        }

        if (entry.hit) {
            cache_->invalidate(key);
        }

        cache_->recordMiss();
    }

    // Fall back to slangc subprocess compilation
    if (!std::filesystem::exists(request.slangcPath, ec)) {
        return Status::failure("slangc not found: " + request.slangcPath.string());
    }

    std::filesystem::create_directories(request.outputDirectory, ec);
    const auto key = Hash::combine({
        request.source,
        stageName(request.stage),
        compilerVersion(request.slangcPath)
    });
    const auto sourceExt = (request.language == ShaderLanguage::GLSL) ? ".glsl" : ".slang";
    const auto sourceFile = request.outputDirectory / (key + "." + stageName(request.stage) + sourceExt);
    const auto spirvFile = request.outputDirectory / (key + "." + stageName(request.stage) + ".spv");
    const auto reflectionFile = request.outputDirectory / (key + "." + stageName(request.stage) + ".reflection.json");

    auto write = FileSystem::writeText(sourceFile, request.source);
    if (!write) return write;

    std::wostringstream cmd;
    cmd << quote(request.slangcPath)
        << L" -target spirv"
        << L" -profile glsl_450"
        << L" -entry main"
        << L" -stage " << (request.stage == ShaderStage::Vertex ? L"vertex" : L"fragment")
        << L" -fspv-reflect"
        << L" -O3"
        << L" -reflection-json " << quote(reflectionFile)
        << L" -o " << quote(spirvFile);
    if (request.language == ShaderLanguage::GLSL) {
        cmd << L" -lang glsl"
            << L" -allow-glsl";
    }
    if (request.debugInfo) {
        cmd << L" -g3";
    }
    for (const auto& includeDir : request.includeDirectories) {
        cmd << L" -I " << quote(includeDir);
    }
    cmd << L" " << quote(sourceFile);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = cmd.str();
    auto stderrFile = request.outputDirectory / (key + ".stderr.txt");
    si.hStdError = CreateFileW(stderrFile.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    si.dwFlags |= STARTF_USESTDHANDLES;
    BOOL ok = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);
    int exitCode = -1;
    std::string slangcOutput;
    if (ok) {
        if (si.hStdError != INVALID_HANDLE_VALUE) CloseHandle(si.hStdError);
        si.hStdError = INVALID_HANDLE_VALUE;
        WaitForSingleObject(pi.hProcess, 120000);
        DWORD exit = 0;
        GetExitCodeProcess(pi.hProcess, &exit);
        exitCode = static_cast<int>(exit);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        auto errText = FileSystem::readText(stderrFile);
        if (errText) slangcOutput = errText.value();
    } else {
        if (si.hStdError != INVALID_HANDLE_VALUE) CloseHandle(si.hStdError);
        DWORD err = GetLastError();
        return Status::failure("CreateProcessW failed for " + request.sourcePath.string() + " (error=" + std::to_string(err) + ")");
    }
    if (exitCode != 0) {
        std::string msg = "slangc failed for " + request.sourcePath.string() + " (exit=" + std::to_string(exitCode) + ")";
        if (!slangcOutput.empty()) {
            msg += "\n" + slangcOutput;
        }
        return Status::failure(msg);
    }

    ShaderCompileResult result;
    result.spirv = readBinary(spirvFile);
    if (result.spirv.empty()) {
        return Status::failure("slangc produced empty SPIR-V: " + spirvFile.string());
    }
    auto reflectionText = FileSystem::readText(reflectionFile);
    if (reflectionText) {
        result.reflectionJson = reflectionText.value();
    }
    ShaderReflectionParser parser;
    auto reflection = parser.fromSlangJson(result.reflectionJson, request.source);
    if (!reflection) return reflection.status();
    result.reflection = reflection.value();

    // Store in cache if available
    if (cache_) {
        CacheKeyComponents components;
        components.sourceContent = request.source;
        components.stage = stageName(request.stage);
        components.language = (request.language == ShaderLanguage::GLSL) ? "glsl" : "slang";
        components.compilerVersion = compilerVersion(request.slangcPath);
        components.dependencyPaths = request.dependencies;
        components.debugInfo = request.debugInfo;
        for (const auto& dep : request.dependencies) {
            auto content = FileSystem::readText(dep);
            components.dependencyContents.push_back(
                content ? content.value() : "");
        }
        auto key = cache_->makeKey(components);
        cache_->store(key, result.spirv, result.reflectionJson);
    }

    return result;
}

}  // namespace monix::renderer_vk
