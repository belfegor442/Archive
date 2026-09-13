#pragma once

#include "../../Snapshot.hpp"

namespace monix {

void ScanDirectory(const wchar_t* path, int& totalFiles, int& hiddenFiles, int& systemFiles, int& reparsePoints, int& sparseFiles);
int CountAlternateDataStreams(const wchar_t* filePath);
void CollectFilesystemData(Snapshot& snapshot);

}
