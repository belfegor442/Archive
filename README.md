# Archive

A native C++ desktop application for archiving, preserving and organizing projects, files, versions and old material that is no longer active but should not be deleted.

## Stack

- **Language**: C++20
- **Build System**: CMake 3.20+ with Ninja
- **UI Framework**: Qt6 (Widgets)
- **Database**: SQLite3 (WAL mode)
- **Hashing**: OpenSSL EVP (SHA-256)
- **Compiler**: MinGW-w64 (GCC 16.1.0) or compatible

## Building

### Prerequisites

- MSYS2 with `mingw-w64-x86_64-gcc`, `mingw-w64-x86_64-cmake`, `mingw-w64-x86_64-ninja`, `mingw-w64-x86_64-qt6-base`, `mingw-w64-x86_64-sqlite3`, `mingw-w64-x86_64-openssl`

### Build

```bash
# Windows
build.bat

# Or manually
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++.exe
cmake --build build
build\bin\archive_tests.exe
build\bin\archive.exe
```

### Clean

```bash
clean.bat
```

## Features

- Import files, folders, and projects with automatic detection
- SHA-256 checksums for integrity verification
- Categories and tags for organization
- Full-text search with filters
- Version history with restore capability
- Notes and activity log
- Favorites and trash management
- Project type detection (C++, Python, Rust, Go, Java, Node.js, etc.)
- Dark Catppuccin theme UI

## Project Structure

```
Archive/
  src/
    core/           Domain models, enums, types, utilities
    storage/        SQLite repositories and database manager
    hashing/        SHA-256 file hashing (OpenSSL EVP)
    filesystem/     File operations and storage management
    services/       Business logic services
    ui/             Qt6 interface widgets
    app/            Application bootstrap and config
  tests/            Unit and integration tests
```

## Architecture

```
UI (Qt6)
  |
  +-- Application Layer
  |     ImportService, SearchService, VersionService, IntegrityService,
  |     CategoryService, TagService, ActivityService, UpdateService,
  |     NoteService, DashboardService
  |
  +-- Domain/Core
  |     ArchiveItem, StoredObject, Version, Category, Tag, Note, Activity
  |
  +-- Repositories
  |     ArchiveItemRepository, VersionRepository, CategoryRepository,
  |     TagRepository, NoteRepository, ActivityRepository
  |
  +-- Infrastructure
        DatabaseManager, StorageManager, FileUtils, FileHasher,
        ProjectDetector
```

## Testing

```bash
build\bin\archive_tests.exe
```

Currently 78 unit tests covering enums, models, types, storage, hashing, and services.

## License

MIT License
