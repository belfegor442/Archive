#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <utility>

namespace winraii {

struct GdiFont {
    HFONT h = nullptr;
    GdiFont() = default;
    explicit GdiFont(HFONT font) : h(font) {}
    ~GdiFont() { if (h) { DeleteObject(h); h = nullptr; } }
    GdiFont(const GdiFont&) = delete;
    GdiFont& operator=(const GdiFont&) = delete;
    GdiFont(GdiFont&& o) noexcept : h(o.h) { o.h = nullptr; }
    GdiFont& operator=(GdiFont&& o) noexcept {
        if (this != &o) { if (h) DeleteObject(h); h = o.h; o.h = nullptr; }
        return *this;
    }
    HFONT get() const { return h; }
    explicit operator bool() const { return h != nullptr; }
    HFONT release() { HFONT tmp = h; h = nullptr; return tmp; }
    void reset(HFONT font = nullptr) { if (h) DeleteObject(h); h = font; }
};

struct GdiIcon {
    HICON h = nullptr;
    GdiIcon() = default;
    explicit GdiIcon(HICON icon) : h(icon) {}
    ~GdiIcon() { if (h) { DestroyIcon(h); h = nullptr; } }
    GdiIcon(const GdiIcon&) = delete;
    GdiIcon& operator=(const GdiIcon&) = delete;
    GdiIcon(GdiIcon&& o) noexcept : h(o.h) { o.h = nullptr; }
    GdiIcon& operator=(GdiIcon&& o) noexcept {
        if (this != &o) { if (h) DestroyIcon(h); h = o.h; o.h = nullptr; }
        return *this;
    }
    HICON get() const { return h; }
    explicit operator bool() const { return h != nullptr; }
    HICON release() { HICON tmp = h; h = nullptr; return tmp; }
    void reset(HICON icon = nullptr) { if (h) DestroyIcon(h); h = icon; }
};

struct Window {
    HWND h = nullptr;
    Window() = default;
    explicit Window(HWND wnd) : h(wnd) {}
    ~Window() { if (h) { DestroyWindow(h); h = nullptr; } }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& o) noexcept : h(o.h) { o.h = nullptr; }
    Window& operator=(Window&& o) noexcept {
        if (this != &o) { if (h) DestroyWindow(h); h = o.h; o.h = nullptr; }
        return *this;
    }
    HWND get() const { return h; }
    explicit operator bool() const { return h != nullptr; }
    HWND release() { HWND tmp = h; h = nullptr; return tmp; }
    void reset(HWND wnd = nullptr) { if (h) DestroyWindow(h); h = wnd; }
};

struct Dc {
    HDC h = nullptr;
    HWND owner = nullptr;
    Dc() = default;
    Dc(HWND wnd, HDC dc) : h(dc), owner(wnd) {}
    ~Dc() { if (h && owner) { ReleaseDC(owner, h); h = nullptr; owner = nullptr; } }
    Dc(const Dc&) = delete;
    Dc& operator=(const Dc&) = delete;
    Dc(Dc&& o) noexcept : h(o.h), owner(o.owner) { o.h = nullptr; o.owner = nullptr; }
    Dc& operator=(Dc&& o) noexcept {
        if (this != &o) {
            if (h && owner) ReleaseDC(owner, h);
            h = o.h; owner = o.owner; o.h = nullptr; o.owner = nullptr;
        }
        return *this;
    }
    HDC get() const { return h; }
    explicit operator bool() const { return h != nullptr; }
};

struct ThreadHandle {
    HANDLE h = nullptr;
    ThreadHandle() = default;
    explicit ThreadHandle(HANDLE thread) : h(thread) {}
    ~ThreadHandle() {
        if (h) {
            WaitForSingleObject(h, 3000);
            CloseHandle(h);
            h = nullptr;
        }
    }
    ThreadHandle(const ThreadHandle&) = delete;
    ThreadHandle& operator=(const ThreadHandle&) = delete;
    ThreadHandle(ThreadHandle&& o) noexcept : h(o.h) { o.h = nullptr; }
    ThreadHandle& operator=(ThreadHandle&& o) noexcept {
        if (this != &o) {
            if (h) { WaitForSingleObject(h, 3000); CloseHandle(h); }
            h = o.h; o.h = nullptr;
        }
        return *this;
    }
    HANDLE get() const { return h; }
    explicit operator bool() const { return h != nullptr; }
    HANDLE release() { HANDLE tmp = h; h = nullptr; return tmp; }
    void reset(HANDLE thread = nullptr) {
        if (h) { WaitForSingleObject(h, 3000); CloseHandle(h); }
        h = thread;
    }
};

} // namespace winraii
