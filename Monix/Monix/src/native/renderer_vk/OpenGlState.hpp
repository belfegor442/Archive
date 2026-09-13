#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>

#include "../vulkan_renderer.h"
#include "api/ShaderRenderer.hpp"
#include "opengl/GlBackend.hpp"

struct OpenGlState {
  bool available = false;
  bool functionsLoaded = false;
  HWND hwnd = nullptr;
  HDC dc = nullptr;
  HGLRC rc = nullptr;
  VulkanRenderer vk;
  bool vkAvailable = false;
  std::unique_ptr<monix::renderer_vk::ShaderRenderer> shaderRenderer;
  bool presetLoaded = false;
  monix::renderer_vk::GlBackend glBackend;
  bool glPresetActive = false;
  HDC uiDc = nullptr;
  HBITMAP uiBitmap = nullptr;
  HGDIOBJ oldUiBitmap = nullptr;
  void* uiPixels = nullptr;
  int uiWidth = 0;
  int uiHeight = 0;
  std::uint64_t frameCount = 0;
  float elapsedTime = 0.0f;
  std::wstring vendor = L"Unavailable";
  std::wstring renderer = L"Unavailable";
  std::wstring version = L"Unavailable";
  std::wstring status = L"Pending";
  int debugViewMode = 0;
  int debugPassIndex = 0;
  bool screenshotRequested = false;
  int screenshotWidth = 0;
  int screenshotHeight = 0;
};
