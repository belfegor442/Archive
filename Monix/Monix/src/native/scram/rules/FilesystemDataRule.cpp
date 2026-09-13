#include "FilesystemDataRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void FilesystemDataRule::Evaluate(const Snapshot& current,
                                  const Snapshot* previous,
                                  std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const auto& prev = *previous;

  const int fs32Delta = current.fsSystem32FileCount - prev.fsSystem32FileCount;

  // 1. File create event (System32 file count increase)
  if (fs32Delta > 20) {
    findings.push_back({
      L"File creation burst in System32.",
      L"System32 file count increased by over 20 files between samples.",
      L"System32: +" + std::to_wstring(fs32Delta) + L" files.",
      10
    });
  }

  // 2. File delete event (System32 file count decrease) — raised threshold to avoid normal churn
  if (prev.fsSystem32FileCount > current.fsSystem32FileCount + 50) {
    findings.push_back({
      L"File deletion event in System32.",
      L"System32 file count decreased by over 50 files.",
      L"System32: " + std::to_wstring(current.fsSystem32FileCount - prev.fsSystem32FileCount) + L" files.",
      12
    });
  }

  // 3. File rename event (file count unchanged but drivers changed)
  if (current.fsDriversFileCount != prev.fsDriversFileCount && current.fsSystem32FileCount == prev.fsSystem32FileCount) {
    findings.push_back({
      L"File rename event in drivers.",
      L"Driver file count changed while System32 count remained stable.",
      L"Drivers: " + std::to_wstring(prev.fsDriversFileCount) + L" -> " + std::to_wstring(current.fsDriversFileCount) + L".",
      8
    });
  }

  // 4. File move event (Program Files count increase)
  const int pfDelta = current.fsProgramFilesCount - prev.fsProgramFilesCount;
  if (pfDelta > 30) {
    findings.push_back({
      L"File move/copy event to Program Files.",
      L"Program Files directory count increased by over 30, indicating installation.",
      L"Program Files: +" + std::to_wstring(pfDelta) + L".",
      8
    });
  }

  // 5. File overwrite event (recycle bin delta)
  if (current.fsRecycleBinContentCount != prev.fsRecycleBinContentCount) {
    const int binDelta = current.fsRecycleBinContentCount - prev.fsRecycleBinContentCount;
    if (binDelta > 50) {
      findings.push_back({
        L"File overwrite/replace burst.",
        L"Recycle bin content increased by over 50, indicating mass file replacement.",
        L"Recycle bin: +" + std::to_wstring(binDelta) + L" items.",
        10
      });
    }
  }

  // 6. File truncation event (file count decrease in Program Files)
  if (prev.fsProgramFilesCount > current.fsProgramFilesCount + 20) {
    findings.push_back({
      L"File truncation/removal in Program Files.",
      L"Program Files directory count decreased by over 20.",
      L"Program Files: " + std::to_wstring(current.fsProgramFilesCount - prev.fsProgramFilesCount) + L".",
      10
    });
  }

  // 7. Hidden file creation
  if (current.fsSystem32HiddenCount > prev.fsSystem32HiddenCount + 3) {
    findings.push_back({
      L"Hidden file creation in System32.",
      L"Hidden file count in System32 increased by more than 3.",
      L"Hidden: " + std::to_wstring(prev.fsSystem32HiddenCount) + L" -> " + std::to_wstring(current.fsSystem32HiddenCount) + L".",
      12
    });
  }

  // 8. System file modification
  if (current.fsSystem32SystemCount != prev.fsSystem32SystemCount && prev.fsSystem32SystemCount > 0) {
    const int sysDelta = current.fsSystem32SystemCount - prev.fsSystem32SystemCount;
    findings.push_back({
      L"System file modification detected.",
      L"System file count in System32 has changed.",
      L"System files: " + std::to_wstring(prev.fsSystem32SystemCount) + L" -> " + std::to_wstring(current.fsSystem32SystemCount) + L".",
      16
    });
  }

  // 9. Hidden file in drivers
  if (current.fsDriversHiddenCount > prev.fsDriversHiddenCount) {
    findings.push_back({
      L"Hidden file in drivers directory.",
      L"Hidden file count in drivers directory has increased.",
      L"Hidden drivers: " + std::to_wstring(prev.fsDriversHiddenCount) + L" -> " + std::to_wstring(current.fsDriversHiddenCount) + L".",
      14
    });
  }

  // 10. Volume dirty bit set
  if (current.fsVolumeDirtyBit == 1 && prev.fsVolumeDirtyBit == 0) {
    findings.push_back({
      L"Volume dirty bit set.",
      L"Filesystem dirty bit has been set, indicating unclean shutdown or corruption.",
      L"Volume dirty bit: set.",
      14
    });
  }

  // 11. Volume label change
  if (!current.fsVolumeLabel.empty() && !prev.fsVolumeLabel.empty() && current.fsVolumeLabel != prev.fsVolumeLabel) {
    findings.push_back({
      L"Volume label changed.",
      L"Filesystem volume label has been modified.",
      L"Label: " + prev.fsVolumeLabel + L" -> " + current.fsVolumeLabel + L".",
      10
    });
  }

  // 12. Mount point change (delta-based: only significant changes)
  if (current.fsMountPointCount != prev.fsMountPointCount) {
    const int mpDelta = current.fsMountPointCount - prev.fsMountPointCount;
    if (mpDelta > 3 || mpDelta < -3) {
      findings.push_back({
        L"Mount point activity detected.",
        L"Active mount point count has changed significantly.",
        L"Mount points: " + std::to_wstring(prev.fsMountPointCount) + L" -> " + std::to_wstring(current.fsMountPointCount) + L" active.",
        4
      });
    }
  }

  // 13. Symbolic link/reparse point creation
  if (current.fsReparsePointCount > prev.fsReparsePointCount + 2) {
    findings.push_back({
      L"Reparse point creation detected.",
      L"Symbolic links or reparse points have been created in monitored directories.",
      L"Reparse: " + std::to_wstring(prev.fsReparsePointCount) + L" -> " + std::to_wstring(current.fsReparsePointCount) + L".",
      12
    });
  }

  // 14. Sparse file creation
  if (current.fsSparseFileCount > prev.fsSparseFileCount + 5) {
    findings.push_back({
      L"Sparse file creation detected.",
      L"Additional sparse files created in System32.",
      L"Sparse: " + std::to_wstring(prev.fsSparseFileCount) + L" -> " + std::to_wstring(current.fsSparseFileCount) + L".",
      8
    });
  }

  // 15. Alternate data stream creation
  if (current.fsAdsWithDataCount > prev.fsAdsWithDataCount) {
    findings.push_back({
      L"Alternate data stream creation detected.",
      L"ADS count on ntoskrnl.exe has increased, possibly indicating data hiding.",
      L"ADS: " + std::to_wstring(prev.fsAdsWithDataCount) + L" -> " + std::to_wstring(current.fsAdsWithDataCount) + L".",
      14
    });
  }

  // 16. Filesystem corruption warning (delta-based: only on transition, not persistent state)
  // Removed: duplicate of rule 10 (dirty bit transition 0->1)

  // 17. Chkdsk pending
  if (current.fsChkdskPending == 1 && prev.fsChkdskPending == 0) {
    findings.push_back({
      L"Chkdsk pending.",
      L"Filesystem dirty bit set, chkdsk should be run on next reboot.",
      L"Chkdsk: pending on C:.",
      12
    });
  }

  // 18. Directory enumeration burst
  if (fs32Delta > 100) {
    findings.push_back({
      L"Directory enumeration burst.",
      L"Over 100 files added to System32 in one sample, indicating mass deployment.",
      L"Enum burst: +" + std::to_wstring(fs32Delta) + L" files.",
      14
    });
  }

  // 19. System file deletion
  if (prev.fsSystem32SystemCount > current.fsSystem32SystemCount + 3) {
    findings.push_back({
      L"System file deletion detected.",
      L"Multiple system files have been removed from System32.",
      L"Sys files removed: " + std::to_wstring(prev.fsSystem32SystemCount - current.fsSystem32SystemCount) + L".",
      18
    });
  }

  // 20. File hash change (proxy: system file count + hidden count change)
  if (current.fsSystem32SystemCount != prev.fsSystem32SystemCount && current.fsSystem32HiddenCount != prev.fsSystem32HiddenCount) {
    findings.push_back({
      L"File integrity change detected.",
      L"Both system and hidden file counts changed in System32, indicating possible hash modification.",
      L"Integrity: system delta + hidden delta in System32.",
      16
    });
  }

  // 21. Duplicate file burst
  if (fs32Delta > 50 && current.fsSystem32HiddenCount == prev.fsSystem32HiddenCount) {
    findings.push_back({
      L"File duplicate burst detected.",
      L"Many new files in System32 without hidden attribute change suggests duplication.",
      L"Dup burst: +" + std::to_wstring(fs32Delta) + L" non-hidden files.",
      10
    });
  }

  // 22. Sparse file creation in drivers
  if (current.fsDriversHiddenCount > prev.fsDriversHiddenCount && current.fsDriversFileCount == prev.fsDriversFileCount) {
    findings.push_back({
      L"Hidden file modification in drivers.",
      L"Hidden file count changed in drivers without total count change.",
      L"Hidden drivers: " + std::to_wstring(prev.fsDriversHiddenCount) + L" -> " + std::to_wstring(current.fsDriversHiddenCount) + L".",
      12
    });
  }

  // 23. Data integrity check failure
  if (current.fsVolumeDirtyBit == 1 && current.fsCorruptionWarnings > prev.fsCorruptionWarnings) {
    findings.push_back({
      L"Data integrity check failure.",
      L"Volume corruption indicators increasing, data integrity at risk.",
      L"Integrity: dirty bit + corruption warnings.",
      18
    });
  }

  // 24. Recycle bin flush (mass deletion)
  if (prev.fsRecycleBinContentCount > current.fsRecycleBinContentCount + 50) {
    findings.push_back({
      L"Recycle bin flush detected.",
      L"Recycle bin emptied or mass deletion occurred.",
      L"Recycle bin: " + std::to_wstring(prev.fsRecycleBinContentCount) + L" -> " + std::to_wstring(current.fsRecycleBinContentCount) + L".",
      8
    });
  }

  // 25. Large installation detected
  if (pfDelta > 100) {
    findings.push_back({
      L"Large installation detected.",
      L"Program Files grew by over 100 files, indicating major software installation.",
      L"Install: +" + std::to_wstring(pfDelta) + L" files in Program Files.",
      6
    });
  }
}

} // namespace monix
