# ORIGINAL - UI Backed Up Before Redesign
# Fecha: 2026-08-25
# Total: 37 archivos / ~15,332 lineas

## Contenido

### login/ (15 archivos - Boot/Kernel UI)
- KernelDisplay.cpp/hpp - Main kernel screen renderer (GDI)
- LoginOverlay.cpp/hpp - Login overlay UI
- BootUpDisplay.cpp/hpp - Boot-up sequence screen
- DiagnosticsDisplay.cpp/hpp - Hardware diagnostics grid
- MonixKernel.cpp/hpp - Kernel state machine
- BootUp.cpp/hpp - Boot phase timing
- AuthManager.hpp - Authentication state
- GdiPlusLoader.cpp - GDI+ init/PNG drawing
- MainWindowInternal.hpp - Forward declarations

### main_original.cpp (1 archivo - 9,263 lineas)
- MonixApp class + WndProc
- All Draw* methods (DrawLogView, DrawTasksView, DrawHardwareView, DrawNetworkView, DrawScramView, DrawSettingsView, DrawIntro, DrawPanel, DrawToast, etc.)
- Font management, DPI scaling, hit testing

### transitions/ (10 archivos - Screen transitions)
- Transition.hpp - Abstract base
- TransitionType.hpp - Enum (Scanline, Glitch, CRT, Fade)
- ScanlineTransition.cpp/hpp - Animation logic
- ScanlineBlender.cpp/hpp - Pixel blending
- TransitionManager.cpp/hpp - Lifecycle management
- TransitionPipeline.cpp/hpp - Pipeline stub

### types/ (4 archivos - UI primitives)
- Types.hpp - Tab enum, ColorRole, drawing helpers
- AppState.hpp - IntroState, NotificationItem
- TextUtils.cpp/hpp - String formatting

### settings/ (4 archivos - Settings definitions)
- MonixConfigTypes.hpp - 80+ SettingId definitions
- SettingsRegistry.cpp/hpp - Labels, ranges, choices
- MonixConfig.hpp - Config file reader

### shader_panel/ (2 archivos - Shader browser)
- ShaderBrowserPanel.cpp/hpp - Shader list/detail panel

### Snapshot.hpp (1 archivo - Data model)
- ProcessInfo, NetworkFlow, StorageInfo, SensorReading
