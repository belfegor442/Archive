#include "GpuVramRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void GpuVramRule::Evaluate(const Snapshot& current,
                           const Snapshot* previous,
                           std::vector<ScramFinding>& findings) {
  if (!previous) return;

  if (current.gpuVramTotalBytes > 0 && current.gpuVramUsedBytes > 0) {
    double vramPct = static_cast<double>(current.gpuVramUsedBytes) / static_cast<double>(current.gpuVramTotalBytes) * 100.0;
    const bool saturated = vramPct >= 90.0;
    if (saturated && !prevVramSaturated_) {
      findings.push_back({
        L"GPU memory saturation detected.",
        L"VRAM usage is critically high.",
        (L"VRAM: " + std::to_wstring((int)vramPct) + L"% (" + std::to_wstring(current.gpuVramUsedBytes / (1024*1024)) + L"/" + std::to_wstring(current.gpuVramTotalBytes / (1024*1024)) + L" MB)"),
        14
      });
    }
    prevVramSaturated_ = saturated;
  }

  if (current.gpuVramUsedBytes > previous->gpuVramUsedBytes + 100ull * 1024ull * 1024ull) {
    const uint64_t growth = current.gpuVramUsedBytes - previous->gpuVramUsedBytes;
    findings.push_back({
      L"VRAM allocation growth detected.",
      L"GPU memory usage increased by over 100 MB in one sample.",
      (L"VRAM grew by " + std::to_wstring(growth / (1024*1024)) + L" MB."),
      10
    });
  }

  if (previous->gpuVramUsedBytes > current.gpuVramUsedBytes + 200ull * 1024ull * 1024ull) {
    const uint64_t evicted = previous->gpuVramUsedBytes - current.gpuVramUsedBytes;
    findings.push_back({
      L"VRAM eviction event detected.",
      L"GPU memory was freed in large chunks.",
      (L"VRAM eviction: freed " + std::to_wstring(evicted / (1024*1024)) + L" MB."),
      4
    });
  }
}

} // namespace monix
