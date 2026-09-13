#include "ui/render/panels/hardware/RenderHardware.hpp"

void MonixApp::DrawHardwareView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);

  const double ramPct = state_.snapshot.ramTotalBytes == 0 ? 0.0 :
    (static_cast<double>(state_.snapshot.ramUsedBytes) / static_cast<double>(state_.snapshot.ramTotalBytes)) * 100.0;

  RECT topPanel = outer;
  topPanel.bottom = outer.top + 240;
  RECT midLeft = outer;
  midLeft.top = topPanel.bottom + 14;
  midLeft.right = outer.left + (outer.right - outer.left) / 2 - 7;
  midLeft.bottom = midLeft.top + 200;
  RECT midRight = outer;
  midRight.top = topPanel.bottom + 14;
  midRight.left = midLeft.right + 14;
  midRight.bottom = midRight.top + 200;
  RECT botLeft = outer;
  botLeft.top = midLeft.bottom + 14;
  botLeft.right = midLeft.right;
  botLeft.bottom = botLeft.top + 160;
  RECT botRight = outer;
  botRight.top = midRight.bottom + 14;
  botRight.left = midLeft.right + 14;
  botRight.bottom = botRight.top + 160;

  DrawPanel(dc, topPanel, L"HARDWARE OVERVIEW", ColorRole::Primary, smallFont_);
  DrawPanel(dc, midLeft, L"CPU & THERMAL", ColorRole::Warning, smallFont_);
  DrawPanel(dc, midRight, L"MEMORY", ColorRole::Thermal, smallFont_);
  DrawPanel(dc, botLeft, L"STORAGE & DISK", ColorRole::Storage, smallFont_);
  DrawPanel(dc, botRight, L"NETWORK ADAPTER", ColorRole::Network, smallFont_);

  RECT cpuBar { topPanel.left + 22, topPanel.top + 64, topPanel.right - 320, topPanel.top + 88 };
  RECT ramBar { topPanel.left + 22, topPanel.top + 112, topPanel.right - 320, topPanel.top + 136 };
  RECT gpuBar { topPanel.left + 22, topPanel.top + 160, topPanel.right - 320, topPanel.top + 184 };
  DrawTextLine(dc, cpuBar.left, cpuBar.top - 24, 320, L"CPU  " + BuildBar(state_.snapshot.cpuPct) + L"  " + FormatPercent(state_.snapshot.cpuPct), ColorRole::Primary, bodyFont_);
  DrawProgressBar(dc, cpuBar, state_.snapshot.cpuPct, state_.snapshot.cpuPct >= 85.0 ? ColorRole::Warning : ColorRole::Success);
  DrawTextLine(dc, ramBar.left, ramBar.top - 24, 320, L"RAM  " + BuildBar(ramPct) + L"  " + FormatPercent(ramPct), ColorRole::Primary, bodyFont_);
  DrawProgressBar(dc, ramBar, ramPct, ramPct >= 82.0 ? ColorRole::Warning : ColorRole::Success);
  DrawTextLine(dc, gpuBar.left, gpuBar.top - 24, 320, L"GPU  " + BuildBar(state_.snapshot.gpuPct) + L"  " + FormatPercent(state_.snapshot.gpuPct), ColorRole::Primary, bodyFont_);
  DrawProgressBar(dc, gpuBar, state_.snapshot.gpuPct, state_.snapshot.gpuPct >= 85.0 ? ColorRole::Warning : ColorRole::Success);

  RECT cpuSpark { topPanel.right - 290, topPanel.top + 56, topPanel.right - 24, topPanel.top + 102 };
  RECT ramSpark { topPanel.right - 290, topPanel.top + 108, topPanel.right - 24, topPanel.top + 154 };
  RECT gpuSpark { topPanel.right - 290, topPanel.top + 160, topPanel.right - 24, topPanel.top + 206 };
  DrawSparkline(dc, cpuSpark, state_.history.cpu, 100.0, ColorRole::Success);
  DrawSparkline(dc, ramSpark, state_.history.ram, 100.0, ColorRole::Thermal);
  DrawSparkline(dc, gpuSpark, state_.history.gpu, 100.0, ColorRole::Engine);

  DrawTextLine(dc, topPanel.left + 22, topPanel.top + 210, topPanel.right - topPanel.left - 44,
    L"Host: " + state_.snapshot.host + L"  |  Uptime: " + FormatDuration(state_.snapshot.uptimeSeconds),
    ColorRole::Dim, smallFont_);

  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 56, midLeft.right - midLeft.left - 40, L"CPU temp: " + FormatTemperature(state_.snapshot.cpuCoreTempC, false), ColorRole::Warning, bodyFont_);
  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 96, midLeft.right - midLeft.left - 40, L"GPU temp: " + FormatTemperature(state_.snapshot.gpuTempC, state_.snapshot.gpuTempEstimated), ColorRole::Warning, bodyFont_);
  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 136, midLeft.right - midLeft.left - 40, L"Storage temp: " + FormatTemperature(state_.snapshot.storageTempC, state_.snapshot.storageTempEstimated), ColorRole::Warning, bodyFont_);
  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 176, midLeft.right - midLeft.left - 40, L"Queue: " + std::to_wstring(state_.snapshot.processorQueueLength) + L"  |  Ctx sw/s: " + std::to_wstring(state_.snapshot.contextSwitchesPerSec), ColorRole::Kernel, bodyFont_);

  DrawTextLine(dc, midRight.left + 20, midRight.top + 56, midRight.right - midRight.left - 40, L"Total: " + FormatBytes(state_.snapshot.ramTotalBytes), ColorRole::Primary, bodyFont_);
  DrawTextLine(dc, midRight.left + 20, midRight.top + 96, midRight.right - midRight.left - 40, L"Used: " + FormatBytes(state_.snapshot.ramUsedBytes), ColorRole::Warning, bodyFont_);
  DrawTextLine(dc, midRight.left + 20, midRight.top + 136, midRight.right - midRight.left - 40, L"Pagefile: " + FormatBytes(state_.snapshot.pageFileUsedBytes) + L" / " + FormatBytes(state_.snapshot.pageFileTotalBytes), ColorRole::Dim, bodyFont_);
  DrawTextLine(dc, midRight.left + 20, midRight.top + 176, midRight.right - midRight.left - 40, L"IRPs/s: " + std::to_wstring(state_.snapshot.interruptsPerSec) + L"  |  Syscalls: " + std::to_wstring(state_.snapshot.systemCallsPerSec), ColorRole::Kernel, bodyFont_);

  DrawTextLine(dc, botLeft.left + 20, botLeft.top + 56, botLeft.right - botLeft.left - 40, L"Disk read: " + FormatRate(state_.snapshot.diskReadBytesPerSec), ColorRole::Primary, bodyFont_);
  DrawTextLine(dc, botLeft.left + 20, botLeft.top + 96, botLeft.right - botLeft.left - 40, L"Disk write: " + FormatRate(state_.snapshot.diskWriteBytesPerSec), ColorRole::Primary, bodyFont_);
  DrawTextLine(dc, botLeft.left + 20, botLeft.top + 136, botLeft.right - botLeft.left - 40, L"Processes: " + std::to_wstring(state_.snapshot.processCount) + L"  |  Threads: " + std::to_wstring(state_.snapshot.threadCount) + L"  |  Handles: " + std::to_wstring(state_.snapshot.handleCount), ColorRole::Dim, bodyFont_);

  DrawTextLine(dc, botRight.left + 20, botRight.top + 56, botRight.right - botRight.left - 40, L"Upload: " + FormatRate(state_.snapshot.netUpBytesPerSec), ColorRole::Network, bodyFont_);
  DrawTextLine(dc, botRight.left + 20, botRight.top + 96, botRight.right - botRight.left - 40, L"Download: " + FormatRate(state_.snapshot.netDownBytesPerSec), ColorRole::Network, bodyFont_);
  DrawTextLine(dc, botRight.left + 20, botRight.top + 136, botRight.right - botRight.left - 40, L"Inbound: " + std::to_wstring(state_.snapshot.inboundConnections) + L"  |  Outbound: " + std::to_wstring(state_.snapshot.outboundConnections), ColorRole::Dim, bodyFont_);
}
