#include "MonixApp.hpp"

#include "../../shader/preset_parser/GlslpPresetParser.hpp"
#include "../../core/StringUtils.hpp"

#include <objbase.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace monix;

bool MonixApp::InitializeOpenGlBootstrap() {
  if (openGl_.available || openGl_.vkAvailable) {
    return true;
  }
  if (!hwnd_) {
    openGl_.status = L"Window handle is not ready";
    return false;
  }

  if (gdiplusToken_ == 0) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::Status status = Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr);
    if (status != Gdiplus::Ok) {
      gdiplusToken_ = 0;
      openGl_.status = L"GDI+ initialization failed";
      PushLog(L"OPENGL", L"ERROR", L"GDI+ startup failed.",
        ColorRole::Error, L"opengl", L"gdiplus", L"action=init_fail");
    }
  }

  RECT rc {};
  if (!GetClientRect(hwnd_, &rc)) {
    openGl_.status = L"Failed to get client rect";
    return false;
  }
  uint32_t width = static_cast<uint32_t>(rc.right - rc.left);
  uint32_t height = static_cast<uint32_t>(rc.bottom - rc.top);

  if (!openGl_.vk.initialize(hwnd_, width, height)) {
    openGl_.status = L"Vulkan initialization failed";
    return false;
  }

  openGl_.vkAvailable = true;
  openGl_.available = true;
  openGl_.hwnd = hwnd_;

  if (!openGl_.dc) {
    openGl_.dc = GetDC(hwnd_);
  }

  std::string vendor = openGl_.vk.getVendor();
  std::string rendererName = openGl_.vk.getRenderer();
  std::string versionStr = openGl_.vk.getVersion();
  openGl_.vendor = Utf8ToWide(vendor);
  openGl_.renderer = Utf8ToWide(rendererName);
  openGl_.version = Utf8ToWide(versionStr);
  openGl_.status = L"Vulkan renderer active";

  PushLog(L"VULKAN", L"INFO",
    L"Vulkan renderer: " + openGl_.renderer +
    L" (" + openGl_.vendor + L") " + openGl_.version,
    ColorRole::Primary, L"vulkan", L"init", L"action=init");

  if (!paths_.crtShaderFile.empty() && std::filesystem::exists(paths_.crtShaderFile)) {
    FILE* slog = nullptr;
    fopen_s(&slog, (paths_.rootDir / "build" / "preset_load.log").string().c_str(), "w");
    auto slogf = [&](const char* msg) { if (slog) { fprintf(slog, "%s\n", msg); fflush(slog); } };

    slogf("Starting ShaderRenderer init...");
    char pbuf[512];
    sprintf_s(pbuf, "rootDir = %s", paths_.rootDir.string().c_str());
    slogf(pbuf);
    sprintf_s(pbuf, "slangcPath = %s", (paths_.rootDir / "tools" / "slangc.exe").string().c_str());
    slogf(pbuf);
    sprintf_s(pbuf, "crtShaderFile = %s", paths_.crtShaderFile.string().c_str());
    slogf(pbuf);

    bool isGlslp = (paths_.crtShaderFile.extension() == ".glslp");
    if (isGlslp) {
      slogf("Detected .glslp preset — using OpenGL backend");
      if (!openGl_.glBackend.initialize(hwnd_)) {
        slogf("ERROR: GL backend init failed");
        if (slog) fclose(slog);
        return false;
      }
      auto glConfig = parseGlslpPreset(paths_.crtShaderFile, paths_.rootDir);
      slogf("Parsed .glslp: loading into GL backend...");
      if (openGl_.glBackend.loadPreset(glConfig)) {
        openGl_.glPresetActive = true;
        openGl_.presetLoaded = true;
        slogf("SUCCESS: GL backend loaded .glslp preset");
        openGl_.status = L"OpenGL GLSL preset active";
        PushLog(L"SHADER", L"INFO", L"Loaded .glslp preset via OpenGL",
          ColorRole::Primary, L"shader", L"load", L"action=load_gl");
      } else {
        slogf("ERROR: GL backend loadPreset failed");
        openGl_.glPresetActive = false;
      }
      if (slog) fclose(slog);
      return true;
    }

    monix::renderer_vk::RendererConfig cfg;
    cfg.rootDirectory = paths_.rootDir;
    cfg.slangcPath = paths_.rootDir / "tools" / "slangc.exe";
    cfg.compileShadersToSpirv = true;
    cfg.enableDebugDumps = true;

    openGl_.shaderRenderer = std::make_unique<monix::renderer_vk::ShaderRenderer>(std::move(cfg));
    openGl_.shaderRenderer->setRenderer(&openGl_.vk);
    if (shaderCompiler_) {
      shaderCompiler_->setRenderer(openGl_.shaderRenderer.get());
    }
    if (shaderRuntime_ && openGl_.shaderRenderer->shaderCache()) {
      shaderRuntime_->setCache(openGl_.shaderRenderer->shaderCache());
    }
    slogf("ShaderRenderer created, calling loadPreset...");

    if (openGl_.vk.isInitialized()) {
      openGl_.shaderRenderer->resize(
        openGl_.vk.swapchainWidth(), openGl_.vk.swapchainHeight());
      slogf("Output extent set for ShaderRenderer");
    }

    auto loadStatus = openGl_.shaderRenderer->loadPreset(paths_.crtShaderFile);
    if (loadStatus) {
      const auto* compiled = openGl_.shaderRenderer->compiledPreset();
      if (compiled) {
        openGl_.presetLoaded = true;
        sprintf_s(pbuf, "SUCCESS: %d passes, %d images, %d parameters",
          (int)compiled->passes.size(), (int)compiled->graph.images.size(),
          (int)compiled->graph.samplers.size());
        slogf(pbuf);
        for (size_t i = 0; i < compiled->passes.size(); i++) {
          sprintf_s(pbuf, "  pass[%d]: %s (vertex=%zu bytes, fragment=%zu bytes, spirv_v=%zu, spirv_f=%zu)",
            (int)i, compiled->passes[i].preset.shaderPath.filename().string().c_str(),
            compiled->passes[i].vertex.source.size(),
            compiled->passes[i].fragment.source.size(),
            compiled->passes[i].vertex.spirv.size(),
            compiled->passes[i].fragment.spirv.size());
          slogf(pbuf);
        }

        wchar_t wbuf[256];
        swprintf_s(wbuf, L"Loaded preset: %d passes, %d images",
          (int)compiled->passes.size(), (int)compiled->graph.images.size());
        openGl_.status = wbuf;
        PushLog(L"SHADER", L"INFO", wbuf,
          ColorRole::Primary, L"shader", L"load", L"action=load");
      } else {
        slogf("loadStatus OK but compiledPreset() returned null");
      }
    } else {
      auto& err = loadStatus.message;
      slogf("loadPreset FAILED:");
      slogf(err.c_str());
      std::wstring werr(err.begin(), err.end());
      PushLog(L"SHADER", L"ERROR",
        L"Preset load failed: " + werr,
        ColorRole::Error, L"shader", L"load", L"action=load_fail");
    }
    if (slog) fclose(slog);
  }

  return true;
}

void MonixApp::DestroyOpenGlUiSurface() {
  if (openGl_.oldUiBitmap && openGl_.uiDc) {
    SelectObject(openGl_.uiDc, openGl_.oldUiBitmap);
    openGl_.oldUiBitmap = nullptr;
  }
  if (openGl_.uiBitmap) {
    DeleteObject(openGl_.uiBitmap);
    openGl_.uiBitmap = nullptr;
  }
  if (openGl_.uiDc) {
    DeleteDC(openGl_.uiDc);
    openGl_.uiDc = nullptr;
  }
  openGl_.uiPixels = nullptr;
  openGl_.uiWidth = 0;
  openGl_.uiHeight = 0;
}

bool MonixApp::EnsureOpenGlUiSurface(int width, int height) {
  if (width <= 0 || height <= 0 || !openGl_.dc) {
    return false;
  }
  if (openGl_.uiBitmap && openGl_.uiWidth == width && openGl_.uiHeight == height) {
    return true;
  }

  DestroyOpenGlUiSurface();

  openGl_.uiDc = CreateCompatibleDC(openGl_.dc);
  if (!openGl_.uiDc) {
    openGl_.status = L"Unable to allocate UI backbuffer DC";
    return false;
  }

  BITMAPINFO info {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  openGl_.uiBitmap = CreateDIBSection(openGl_.dc, &info, DIB_RGB_COLORS, &openGl_.uiPixels, nullptr, 0);
  if (!openGl_.uiBitmap || !openGl_.uiPixels) {
    openGl_.status = L"Unable to allocate UI bitmap surface";
    DestroyOpenGlUiSurface();
    return false;
  }

  openGl_.oldUiBitmap = SelectObject(openGl_.uiDc, openGl_.uiBitmap);
  openGl_.uiWidth = width;
  openGl_.uiHeight = height;
  return true;
}

bool MonixApp::RenderOpenGlFrame(const RECT& clientRect) {
  ComputeViewport(clientRect);

  if (gdiplusToken_ == 0) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::Status gdiStatus = Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr);
    if (gdiStatus != Gdiplus::Ok) {
      gdiplusToken_ = 0;
      openGl_.status = L"GDI+ initialization failed";
      PushLog(L"OPENGL", L"ERROR", L"GDI+ startup failed in RenderOpenGlFrame.",
        ColorRole::Error, L"opengl", L"gdiplus", L"action=init_fail");
      return false;
    }
  }

  if (!config_.crtEnabled) {
    return false;
  }
  const int width = std::max(1L, clientRect.right - clientRect.left);
  const int height = std::max(1L, clientRect.bottom - clientRect.top);

  if (!InitializeOpenGlBootstrap()) {
    return false;
  }

  if (!openGl_.vkAvailable) {
    openGl_.status = L"Vulkan renderer not available";
    return false;
  }

  if (!EnsureOpenGlUiSurface(width, height)) {
    return false;
  }
  RECT fullClient { 0, 0, width, height };
  Render(openGl_.uiDc, fullClient);
  GdiFlush();

  if (openGl_.glPresetActive && openGl_.glBackend.isValid() && openGl_.vk.isInitialized()) {
    auto* bgraPixels = static_cast<const uint8_t*>(openGl_.uiPixels);
    uint32_t w = static_cast<uint32_t>(openGl_.uiWidth);
    uint32_t h = static_cast<uint32_t>(openGl_.uiHeight);

    size_t pixelCount = static_cast<size_t>(w) * static_cast<size_t>(h);
    size_t requiredSize = pixelCount * 4;

    // Reuse pre-allocated buffer (avoids per-frame heap allocation)
    if (openGl_.rgbaBufferSize < requiredSize) {
      openGl_.rgbaBuffer.resize(requiredSize);
      openGl_.rgbaBufferSize = requiredSize;
    }
    auto& rgbaPixels = openGl_.rgbaBuffer;

    for (size_t i = 0; i < pixelCount; i++) {
      rgbaPixels[i * 4 + 0] = bgraPixels[i * 4 + 2];
      rgbaPixels[i * 4 + 1] = bgraPixels[i * 4 + 1];
      rgbaPixels[i * 4 + 2] = bgraPixels[i * 4 + 0];
      rgbaPixels[i * 4 + 3] = bgraPixels[i * 4 + 3];
    }

    auto readback = openGl_.glBackend.execute(rgbaPixels.data(), w, h);
    if (!readback.pixels.empty()) {
      // Reuse pre-allocated buffer (avoids per-frame heap allocation)
      if (openGl_.bgraResultSize < readback.pixels.size()) {
        openGl_.bgraResultBuffer.resize(readback.pixels.size());
        openGl_.bgraResultSize = readback.pixels.size();
      }
      auto& bgraResult = openGl_.bgraResultBuffer;

      for (size_t i = 0; i < readback.pixels.size(); i += 4) {
        bgraResult[i + 0] = readback.pixels[i + 2];
        bgraResult[i + 1] = readback.pixels[i + 1];
        bgraResult[i + 2] = readback.pixels[i + 0];
        bgraResult[i + 3] = readback.pixels[i + 3];
      }

      bool frameOk = openGl_.vk.beginFrame(readback.width, readback.height);
      if (frameOk) {
        openGl_.vk.uploadTextureToImage(bgraResult.data(), readback.width, readback.height, true);
        openGl_.vk.endFrame();
        openGl_.vk.present();
      }
    }
  } else if (openGl_.presetLoaded && openGl_.shaderRenderer && openGl_.vk.isInitialized()) {
    monix::renderer_vk::FrameImage srcFrame{};
    srcFrame.pixels = openGl_.uiPixels;
    srcFrame.extent = {static_cast<uint32_t>(openGl_.uiWidth), static_cast<uint32_t>(openGl_.uiHeight)};
    srcFrame.bytes = openGl_.uiWidth * openGl_.uiHeight * 4;
    monix::renderer_vk::FrameContext fc{};
    auto status = openGl_.shaderRenderer->render(fc, srcFrame.pixels ? &srcFrame : nullptr);
    char buf[256];
    sprintf_s(buf, "[MONIX] SHADER: ShaderRenderer::render %s\n", status.ok ? "OK" : "FAILED");
    OutputDebugStringA(buf);
    if (!status.ok) {
      OutputDebugStringA(status.message.c_str());
      OutputDebugStringA("\n");
    }
  } else {
    return false;
  }

  ++openGl_.frameCount;
  openGl_.elapsedTime += 1.0f / 30.0f;
  return true;
}

void MonixApp::DestroyOpenGlResources() {
  DestroyOpenGlUiSurface();
}

void MonixApp::ShutdownOpenGlBootstrap() {
  DestroyOpenGlResources();
  openGl_.glBackend.shutdown();
  openGl_.glPresetActive = false;
  openGl_.shaderRenderer.reset();
  if (openGl_.vkAvailable) {
    openGl_.vk.shutdown();
    openGl_.vkAvailable = false;
  }
  if (openGl_.dc && hwnd_) {
    ReleaseDC(hwnd_, openGl_.dc);
    openGl_.dc = nullptr;
  }
  if (gdiplusToken_ != 0) {
    Gdiplus::GdiplusShutdown(gdiplusToken_);
    gdiplusToken_ = 0;
  }
  openGl_.hwnd = nullptr;
  openGl_.available = false;
  openGl_.functionsLoaded = false;
}
