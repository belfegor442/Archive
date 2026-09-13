#include "ui/render/panels/tasks/RenderTasks.hpp"

void MonixApp::DrawTasksView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  RECT tablePanel = outer;
  tablePanel.right -= 380;
  RECT detailPanel = outer;
  detailPanel.left = tablePanel.right + 14;

  DrawPanel(dc, tablePanel, L"TASK SURFACE", ColorRole::Primary, smallFont_);
  DrawPanel(dc, detailPanel, L"PROCESS INSPECTOR", ColorRole::Scram, smallFont_);

  RECT tableRect = ShrinkRect(tablePanel, 14);
  tableRect.top += 34;
  const int headerHeight = 48;
  RECT header = tableRect;
  header.bottom = header.top + headerHeight;
  FillSolid(dc, header, RGB(8, 24, 10));
  DrawRectOutline(dc, header, ResolveColor(ColorRole::Accent));

  const int width = tableRect.right - tableRect.left;
  const int nameEnd = tableRect.left + static_cast<int>(width * 0.28);
  const int pidEnd = tableRect.left + static_cast<int>(width * 0.39);
  const int cpuEnd = tableRect.left + static_cast<int>(width * 0.51);
  const int ramEnd = tableRect.left + static_cast<int>(width * 0.67);
  const int gpuEnd = tableRect.left + static_cast<int>(width * 0.79);
  const int priEnd = tableRect.left + static_cast<int>(width * 0.89);

  DrawTextRect(dc, RECT { tableRect.left + 10, header.top, nameEnd - 6, header.bottom }, L"PROCESS", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { nameEnd + 6, header.top, pidEnd - 6, header.bottom }, L"PID", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { pidEnd + 6, header.top, cpuEnd - 6, header.bottom }, L"CPU", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { cpuEnd + 6, header.top, ramEnd - 6, header.bottom }, L"RAM", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { ramEnd + 6, header.top, gpuEnd - 6, header.bottom }, L"GPU", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { gpuEnd + 6, header.top, priEnd - 6, header.bottom }, L"PRI", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { priEnd + 6, header.top, tableRect.right - 6, header.bottom }, L"STATE", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  const int rowHeight = std::max(38, bodyLineHeight_ + 14);
  const int visibleRows = VisibleTaskRows(tableRect);
  const int maxScroll = std::max(0, static_cast<int>(state_.snapshot.processes.size()) - visibleRows);
  // DEFENSIVE CLAMP: Primary scroll mutation happens in WM_MOUSEWHEEL handler (AppWindowProc.cpp:995).
  // This clamp is a safety net to ensure scroll stays in bounds if the process list shrinks between frames.
  state_.taskScroll = std::clamp(state_.taskScroll, 0, maxScroll);
  const std::size_t subGigabyteProcesses = std::count_if(state_.snapshot.processes.begin(), state_.snapshot.processes.end(), [](const ProcessInfo& process) {
    return process.ramBytes < 1024ull * 1024ull * 1024ull;
  });
  const auto cpuColor = [&](const ProcessInfo& process) {
    if (process.cpuPct >= 35.0 || (state_.snapshot.cpuPct >= 70.0 && process.cpuPct >= 12.0)) return ColorRole::Warning;
    if (process.cpuPct >= 8.0) return ColorRole::Primary;
    return ColorRole::White;
  };
  const auto ramColor = [&](const ProcessInfo& process) {
    if (state_.snapshot.ramTotalBytes == 0) return ColorRole::White;
    const double share = (static_cast<double>(process.ramBytes) / static_cast<double>(state_.snapshot.ramTotalBytes)) * 100.0;
    if (process.ramBytes < 1024ull * 1024ull * 1024ull && subGigabyteProcesses >= 8) return ColorRole::White;
    if (share >= 10.0 || process.ramBytes >= 3ull * 1024ull * 1024ull * 1024ull) return ColorRole::Error;
    if (share >= 4.0 || process.ramBytes >= 1024ull * 1024ull * 1024ull) return ColorRole::Warning;
    return ColorRole::White;
  };
  const auto gpuColor = [&](const ProcessInfo& process) {
    if (process.gpuPct >= 50.0) return ColorRole::Error;
    if (process.gpuPct >= 15.0 || (state_.snapshot.gpuPct >= 70.0 && process.gpuPct >= 8.0)) return ColorRole::Warning;
    return ColorRole::White;
  };

  for (int row = 0; row < visibleRows; ++row) {
    const int processIndex = state_.taskScroll + row;
    if (processIndex >= static_cast<int>(state_.snapshot.processes.size())) {
      break;
    }

    RECT rowRect {
      tableRect.left,
      header.bottom + row * rowHeight,
      tableRect.right,
      header.bottom + (row + 1) * rowHeight
    };

    const auto& process = state_.snapshot.processes[processIndex];
    const bool selected = processIndex == state_.selectedTaskIndex;
    const bool tracked = process.pid == state_.scramState.trackedPid;

    FillSolid(dc, rowRect, selected ? RGB(18, 44, 20) : RGB(4, 12, 5));
    DrawRectOutline(dc, rowRect, tracked ? ResolveColor(ColorRole::Scram) : ResolveColor(ColorRole::Accent));

    DrawTextLine(dc, rowRect.left + 10, rowRect.top + 7, nameEnd - rowRect.left - 16, process.name, ColorRole::White, bodyFont_);
    DrawTextLine(dc, nameEnd + 6, rowRect.top + 7, pidEnd - nameEnd - 12, std::to_wstring(process.pid), ColorRole::Dim, bodyFont_);
    DrawTextLine(dc, pidEnd + 6, rowRect.top + 7, cpuEnd - pidEnd - 12, FormatPercent(process.cpuPct), cpuColor(process), bodyFont_);
    DrawTextLine(dc, cpuEnd + 6, rowRect.top + 7, ramEnd - cpuEnd - 12, FormatBytes(process.ramBytes), ramColor(process), bodyFont_);
    DrawTextLine(dc, ramEnd + 6, rowRect.top + 7, gpuEnd - ramEnd - 12, FormatPercent(process.gpuPct), gpuColor(process), bodyFont_);
    DrawTextLine(dc, gpuEnd + 6, rowRect.top + 7, priEnd - gpuEnd - 12, process.priority, ColorRole::Dim, bodyFont_);
    DrawTextLine(dc, priEnd + 6, rowRect.top + 7, tableRect.right - priEnd - 12, process.status, tracked ? ColorRole::Scram : ColorRole::Primary, bodyFont_);
  }

  const int selectedIndex = std::clamp(state_.selectedTaskIndex, 0, std::max(0, static_cast<int>(state_.snapshot.processes.size()) - 1));
  const ProcessInfo* selected = state_.snapshot.processes.empty() ? nullptr : &state_.snapshot.processes[selectedIndex];

  if (selected) {
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 56, detailPanel.right - detailPanel.left - 36, L"Name: " + selected->name, ColorRole::White, bodyFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 96, detailPanel.right - detailPanel.left - 36, L"PID: " + std::to_wstring(selected->pid), ColorRole::Dim, bodyFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 136, detailPanel.right - detailPanel.left - 36, L"PPID: " + std::to_wstring(selected->parentPid) + L" | Session: " + std::to_wstring(selected->sessionId), ColorRole::Dim, bodyFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 176, detailPanel.right - detailPanel.left - 36, L"CPU: " + FormatPercent(selected->cpuPct), cpuColor(*selected), bodyFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 216, detailPanel.right - detailPanel.left - 36, L"RAM: " + FormatBytes(selected->ramBytes), ramColor(*selected), bodyFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 256, detailPanel.right - detailPanel.left - 36, L"GPU: " + FormatPercent(selected->gpuPct), gpuColor(*selected), bodyFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 296, detailPanel.right - detailPanel.left - 36, L"Priority: " + selected->priority + L" | " + selected->status, ColorRole::White, bodyFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 336, detailPanel.right - detailPanel.left - 36, L"GUID: " + selected->processGuid, ColorRole::Dim, smallFont_);
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 370, detailPanel.right - detailPanel.left - 36, L"Tracked in S.C.R.A.M: " + std::wstring(selected->pid == state_.scramState.trackedPid ? L"YES" : L"NO"), selected->pid == state_.scramState.trackedPid ? ColorRole::Scram : ColorRole::Dim, bodyFont_);
  }

  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 414, detailPanel.right - detailPanel.left - 36, L"Process count: " + std::to_wstring(state_.snapshot.processCount), ColorRole::Dim, smallFont_);
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 446, detailPanel.right - detailPanel.left - 36, L"Thread count: " + std::to_wstring(state_.snapshot.threadCount), ColorRole::Dim, smallFont_);
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 478, detailPanel.right - detailPanel.left - 36, L"Handles: " + std::to_wstring(state_.snapshot.handleCount), ColorRole::Dim, smallFont_);
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 510, detailPanel.right - detailPanel.left - 36, L"Queue length: " + std::to_wstring(state_.snapshot.processorQueueLength), ColorRole::Kernel, smallFont_);

  DrawTextRect(dc, RECT { detailPanel.left + 16, detailPanel.bottom - 170, detailPanel.right - 16, detailPanel.bottom - 20 }, L"Custom menu:\nInspect\nTrack in S.C.R.A.M\nCopy PID\nPriority HIGH\nPriority LOW\nRefresh snapshot\n\nRight click any row to open it.", ColorRole::Dim, smallFont_, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
}
