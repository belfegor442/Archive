# Win98 Theme Audit Report

**Date:** 2025-09-14
**Commits:** `66e0173`, `f2889f8`

---

## Bugs Found and Fixed

### BUG-1: DrawUpdateDialog Double-Scaling (CRITICAL)
- **File:** `Win98Theme.hpp:2036-2115`
- **Cause:** `R(c, 0, cy, dlgW, 20)` used screen-coordinate `cy` as a design coordinate in `R()`, which then multiplied it by `c.scale` again. Text and notes box were drawn at wrong positions at any scale other than 1:1.
- **Fix:** Replaced all `R(c, 0, cy, dlgW, 20)` calls with direct RECT construction using `dlg.left`/`dlg.right` for horizontal bounds and `cy`/`cy + S(c, 20)` for vertical. Introduced `dlgLine()` lambda for consistent text positioning.
- **Verification:** At 1536x1024 (1:1), 1920x1080, 1280x720 — text is now centered in dialog.

### BUG-2: HitTestUpdateDialog Inconsistent Scaling
- **File:** `Win98Theme.hpp:2117-2149`
- **Cause:** Used raw `static_cast<int>(N * c.scale)` instead of `S()`, making it inconsistent with the rest of the codebase and potentially mismatched with DrawUpdateDialog.
- **Fix:** Rewritten to use `S(c, N)` consistently, matching the fixed DrawUpdateDialog.

### BUG-3: Font/Asset Leak on Shutdown
- **File:** `main.cpp:1160`
- **Cause:** `Win98ThemeFonts` (5 HFONTs) and `Win98Assets` (25 Gdiplus::Bitmap*) were not cleaned up in `~MonixApp()`.
- **Fix:** Added `win98Fonts_.Destroy()` and `win98Assets_.Clear()` in destructor.

### BUG-4: Win98Theme Not Integrated into Primary Tree
- **Cause:** `Win98Theme.hpp` (2206 lines) only existed in `Monix/Monix/src/native/ui/` (extended/legacy tree). Primary tree had no Win98 support.
- **Fix:** Copied `Win98Theme.hpp` and `Win98Types.hpp` to `src/native/ui/`. Added `kWin98ThemeMode = 4`, `IsWin98ThemeActive()`, `RenderWin98Theme()`, hit test handling in `WM_LBUTTONDOWN`, theme switching support for modes 0-4.

---

## Architecture

### Theme Dispatch Flow
```
Render() → IsCoreMonitorThemeActive() → RenderCoreMonitorTheme()
         → IsWin98ThemeActive()        → RenderWin98Theme()
         → else                         → DrawTabs + DrawXxxView
```

### Hit Test Flow
```
WM_LBUTTONDOWN → IsCoreMonitorThemeActive() → CoreMonitorTheme::HitTestMenu/HitTestThemeControl
               → IsWin98ThemeActive()        → Win98Theme::HitTestMenuBar/HitTestTabs
               → else                         → HitTestTabs + per-tab handlers
```

### Win98Theme::Render() Pipeline
```
MakeCanvas(clientRect, fonts, assets)
→ EnsureScaledFonts(fonts, scale, rootDir)  // Creates 5 HFONTs at correct scale
→ Fill background
→ DrawTitleBar / DrawMenuBar / DrawTabBar
→ switch(activeTab):
    0: LOG     → DrawLogConsoleLeft + DrawAlertsRight
    1: TASKS   → DrawActiveTasks
    2: USAGE   → DrawSystemOverview + DrawRealTimeGraph + DrawAiContextEngine + DrawScramPanel
    3: AI      → DrawAiContextEngineFull + DrawAlertsRight
    4: SETTINGS → DrawSettingsTab
→ DrawStatusBar
→ DrawUpdateDialog (if visible)
```

---

## GDI Resource Audit

### Per-Frame GDI Objects
| Resource | Create Site | Destroy Site | Count/Frame | Risk |
|----------|------------|-------------|-------------|------|
| HBRUSH | `Fill()` → `CreateSolidBrush` | `Fill()` → `DeleteObject` | ~100+ | Low (create+destroy balanced) |
| HPEN | `Line()` → `CreatePen` | `Line()` → `DeleteObject` | ~200+ | Low (create+destroy balanced) |
| HPEN (direct) | `DrawMiniGraph`, `DrawLogConsoleLeft`, `DrawActiveTasks` | Same function | ~6 | Low |

**Verdict:** All GDI brushes/pens are created and destroyed within the same function call. No leaks. Performance cost is acceptable for a tool application.

### Persistent GDI Objects
| Object | Create Site | Destroy Site | Count |
|--------|------------|-------------|-------|
| HFONT (Win98ThemeFonts) | `EnsureScaledFonts()` | `Destroy()` in `~MonixApp` | 5 |
| Gdiplus::Bitmap (Win98Assets) | `LoadAssets()` | `Clear()` in `~MonixApp` | 25 |

**Verdict:** Properly managed. Fonts recreated on scale change, destroyed on shutdown.

### Font Safety
- `EnsureScaledFonts()` creates 5 fonts with `CreateFontW()` — returns NULL only on extreme system failure
- `Text()` calls `SelectObject(dc, font)` — if font is NULL, GDI uses default font (no crash)
- Fonts are recreated (old ones destroyed) when window scale changes by >0.01
- No nullptr dereference risk

---

## Scaling Analysis

### Canvas System
- Design size: 1536x1024
- Scale: `min(clientW/1536, clientH/1024)` — uniform scaling, preserves aspect ratio
- Canvas is centered within client rect (letterboxing)

### Scaling Helpers
| Helper | Purpose | Used By |
|--------|---------|---------|
| `X(c, designX)` | Design→screen X | All draw/hit functions |
| `Y(c, designY)` | Design→screen Y | All draw/hit functions |
| `S(c, designSize)` | Design→screen size | All draw/hit functions |
| `R(c, x, y, w, h)` | Design→screen RECT | All draw/hit functions |

### Verified Resolutions
- 1536x1024 (design native) — 1:1, all controls correct
- 1920x1080 — scaled, centered, all controls correct
- 1280x720 — scaled, centered, all controls correct
- 2560x1440 — scaled, centered, all controls correct
- Resized window — dynamic re-render correct

---

## Files Modified

| File | Changes |
|------|---------|
| `src/native/ui/Win98Theme.hpp` | **NEW** — Full Win98 renderer (2215 lines) copied from extended tree with DrawUpdateDialog/HitTestUpdateDialog fixes |
| `src/native/ui/Win98Types.hpp` | **NEW** — Win98Assets (25 GDI+ bitmaps) + Win98ThemeFonts (5 HFONTs) |
| `src/native/ui/AppState.hpp` | Added `UpdateState` struct |
| `src/native/ui/CoreMonitorTheme.hpp` | Added `kWin98ThemeMode`, `ThemeName(4)`, 6 new fields to `CoreMonitorThemeContext` |
| `src/native/telemetry/Snapshot.hpp` | Added `biosVersion` to Snapshot, `createTime100ns` to ProcessInfo |
| `src/native/main.cpp` | Added Win98Theme include, members, `IsWin98ThemeActive()`, `RenderWin98Theme()`, theme switching, hit testing, font/asset cleanup |
| `Monix/Monix/src/native/ui/Win98Theme.hpp` | Same DrawUpdateDialog/HitTestUpdateDialog fixes applied to extended tree copy |

---

## Remaining Issues (Not Blocking)

### MEDIUM: Per-Frame GDI Object Creation
- `Fill()` and `Line()` create+destroy brushes/pens every call
- ~300+ GDI objects per frame
- Acceptable for tool application, but could be optimized with cached brushes/pens if profiling shows issues

### LOW: HitTest Functions Create Empty Canvas
- `HitTestTabs`, `HitTestSettingsCategories`, etc. create `Canvas` with empty `Win98ThemeFonts{}` and `Win98Assets{}`
- Works correctly (only uses rect/scale), but allocates empty structs unnecessarily
- Could be refactored to take scale/rect directly

### LOW: Win98ThemeAssets Not Used
- 25 GDI+ bitmap assets are loaded but most are not used by the renderer
- Only a few icons/bitmaps are referenced in draw functions
- Could reduce asset count to only those actually used

### INFO: Update Dialog Geometry
- Update dialog uses fixed design coords (420x280) centered in canvas
- Button positions calculated from dialog origin — correct after fix
- Hit test matches draw geometry exactly after fix

---

## Build Verification

```
cmake --build build --config Release --target monix
→ monix.exe compiles with 0 errors
→ Pre-existing warnings only (C4189 unused vars, C4505 unreferenced func)
```

---

## Archive Status

This audit report and all fixes are committed to:
- Main repo: `https://github.com/belfegor442/Monix-31ago-stable` (commits `66e0173`, `f2889f8`)
