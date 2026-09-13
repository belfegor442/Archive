#include "MonixApp.hpp"

#include "../../app/bootstrap/AppConstants.hpp"

#include <filesystem>

using namespace monix;

bool MonixApp::CreateMainWindow(HINSTANCE instance, int showCommand) {
  const wchar_t* className = L"MonixNativeWindow";
  WNDCLASSEXW wc {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = StaticWndProc;
  wc.style = CS_OWNDC;
  wc.hInstance = instance;
  wc.lpszClassName = className;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

  largeIcon_ = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(kMainIconResourceId), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR));
  smallIcon_ = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(kMainIconResourceId), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR));
  if (std::filesystem::exists(paths_.iconFile)) {
    if (!largeIcon_) {
      largeIcon_ = reinterpret_cast<HICON>(LoadImageW(nullptr, paths_.iconFile.c_str(), IMAGE_ICON, 256, 256, LR_LOADFROMFILE));
    }
    if (!smallIcon_) {
      smallIcon_ = reinterpret_cast<HICON>(LoadImageW(nullptr, paths_.iconFile.c_str(), IMAGE_ICON, 48, 48, LR_LOADFROMFILE));
    }
  }
  wc.hIcon = largeIcon_;
  wc.hIconSm = smallIcon_;

  if (!RegisterClassExW(&wc)) {
    return false;
  }

  hwnd_ = CreateWindowExW(
    0,
    className,
    L"Monix",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    1680,
    980,
    nullptr,
    nullptr,
    instance,
    this
  );

  if (!hwnd_) {
    return false;
  }

  CreateUiFonts();
  ShowWindow(hwnd_, showCommand == SW_SHOWMINIMIZED ? SW_SHOWMINIMIZED : SW_MAXIMIZE);
  UpdateWindow(hwnd_);
  return true;
}
