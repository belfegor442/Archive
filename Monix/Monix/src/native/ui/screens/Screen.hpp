#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gdiplus.h>

#include "../Win98Theme.hpp"

namespace monix::ui {

struct ScreenContext {
  HDC dc;
  Win98Theme::Canvas canvas;
  int mouseX = 0;
  int mouseY = 0;
  bool mouseClicked = false;
  bool keyDown = false;
  int keyCode = 0;
};

struct ButtonDef {
  std::wstring label;
  RECT rect = {};
  bool enabled = true;
  bool hovered = false;
  bool pressed = false;
  std::function<void()> onClick;
};

class Screen {
public:
  virtual ~Screen() = default;
  virtual void Update(const ScreenContext& ctx) {}
  virtual void Render(const ScreenContext& ctx) = 0;
  virtual std::wstring Name() const = 0;
  virtual int Id() const = 0;
  virtual void OnActivate() {}
  virtual void OnDeactivate() {}
  virtual void OnResize(int width, int height) {}

  bool ContainsPoint(const RECT& r, int x, int y) const {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
  }

  void AddButton(ButtonDef btn) {
    buttons_.push_back(std::move(btn));
  }

  void ClearButtons() {
    buttons_.clear();
  }

  void UpdateButtons(const ScreenContext& ctx) {
    for (auto& btn : buttons_) {
      btn.hovered = ContainsPoint(btn.rect, ctx.mouseX, ctx.mouseY);
      if (ctx.mouseClicked && btn.hovered && btn.enabled) {
        btn.pressed = true;
        if (btn.onClick) btn.onClick();
      } else {
        btn.pressed = false;
      }
    }
  }

  void RenderButtons(const ScreenContext& ctx) {
    for (const auto& btn : buttons_) {
      RECT r = btn.rect;
      Win98Theme::Fill(ctx.dc, r, Win98Theme::kWin98ButtonFace);
      if (btn.pressed) {
        Win98Theme::Win98Bevel(ctx.dc, r, true);
        r.left += 2; r.top += 2;
      } else {
        Win98Theme::Win98Bevel(ctx.dc, r, false);
        r.left += 1; r.top += 1;
      }
      Win98Theme::Text(ctx.dc, r, btn.label,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
  }

  const std::vector<ButtonDef>& Buttons() const { return buttons_; }

protected:
  std::vector<ButtonDef> buttons_;
};

}
