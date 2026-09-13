#include "FrameTimeRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void FrameTimeRule::Evaluate(const Snapshot& current,
                             const Snapshot* previous,
                             std::vector<ScramFinding>& findings) {
  const bool renderBacklog = current.gpuPctValid == 1 && current.gpuPct >= 70.0 && current.frameTimeMs > 33.3;
  if (renderBacklog && !prevRenderBacklog_) {
    findings.push_back({
      L"Render queue backlog detected.",
      L"GPU utilization is high with elevated frame times.",
      (L"Frame time: " + std::to_wstring((int)current.frameTimeMs) + L"ms at " + std::to_wstring((int)current.gpuPct) + L"% GPU."),
      10
    });
  }
  prevRenderBacklog_ = renderBacklog;

  if (previous) {
    if (current.frameTimeMs > 50.0 && previous->frameTimeMs > 0 && previous->frameTimeMs < 20.0) {
      findings.push_back({
        L"Frame time spike detected.",
        L"Frame time jumped significantly from baseline.",
        (L"Frame time: " + std::to_wstring((int)previous->frameTimeMs) + L"ms -> " + std::to_wstring((int)current.frameTimeMs) + L"ms"),
        10
      });
    }

    if (previous->frameTimeMs > 0 && current.frameTimeMs <= 0) {
      findings.push_back({
        L"Frame drop event detected.",
        L"Frame time reset to zero, indicating a dropped frame.",
        L"Frame drop event: frame time reset to zero.",
        6
      });
    }
  }
}

} // namespace monix
