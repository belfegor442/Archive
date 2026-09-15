cmake_minimum_required(VERSION 3.20)
project(Archive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release build
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Release optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<CONFIG:Release>:-O3>)
    add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)
    add_compile_options($<$<CONFIG:Release>:-flto>)
    add_link_options($<$<CONFIG:Release>:-flto>)
    add_link_options($<$<CONFIG:Release>:-s>)
endif()

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(OPENSSL REQUIRED openssl)

# Common sources (core library — no Qt)
set(ARCHIVE_LIB_SOURCES
    src/core/models/ArchiveItem.cpp
    src/core/models/Category.cpp
    src/core/models/Tag.cpp
    src/core/models/Version.cpp
    src/core/models/Note.cpp
    src/core/models/Activity.cpp
    src/core/models/StoredObject.cpp
    src/storage/DatabaseManager.cpp
    src/storage/ArchiveItemRepository.cpp
    src/storage/CategoryRepository.cpp
    src/storage/TagRepository.cpp
    src/storage/VersionRepository.cpp
    src/storage/NoteRepository.cpp
    src/storage/ActivityRepository.cpp
    src/storage/StoredObjectRepository.cpp
    src/storage/ScanRepository.cpp
    src/storage/ScanItemRepository.cpp
    src/storage/ClassificationRepository.cpp
    src/storage/TaxonomyRepository.cpp
    src/storage/ClassificationRuleRepository.cpp
    src/storage/OrgPlanRepository.cpp
    src/storage/OrgMoveRepository.cpp
    src/storage/UndoRepository.cpp
    src/hashing/FileHasher.cpp
    src/filesystem/FileUtils.cpp
    src/filesystem/StorageManager.cpp
    src/filesystem/FilesystemTracker.cpp
    src/filesystem/StagingManager.cpp
    src/services/ProjectDetector.cpp
    src/services/ImportService.cpp
    src/services/SearchService.cpp
    src/services/DashboardService.cpp
    src/services/NoteService.cpp
    src/services/VersionService.cpp
    src/services/ActivityService.cpp
    src/services/IntegrityService.cpp
    src/services/CategoryService.cpp
    src/services/TagService.cpp
    src/services/UpdateService.cpp
    src/services/Scanner.cpp
    src/services/Classifier.cpp
    src/services/OrganizationPlanner.cpp
    src/services/OrganizationExecutor.cpp
    src/core/utils/Logger.cpp
    src/app/AppConfig.cpp
)

# Static library for all core code
add_library(archive_lib STATIC ${ARCHIVE_LIB_SOURCES})

target_include_directories(archive_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${SQLITE3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

target_link_libraries(archive_lib PUBLIC
    ${SQLITE3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ws2_32
    crypt32
)

target_link_directories(archive_lib PUBLIC
    ${SQLITE3_LIBRARY_DIRS}
    ${OPENSSL_LIBRARY_DIRS}
)

# CLI executable (no Qt dependency)
add_executable(archive-cli src/cli/main.cpp)
target_link_libraries(archive-cli PRIVATE archive_lib)
target_include_directories(archive-cli PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_options(archive-cli PRIVATE -static -static-libgcc -static-libstdc++)

# Win32 native GUI (no Qt dependency)
add_executable(archive-gui WIN32 src/gui/main.cpp src/gui/archive-gui.rc)
target_link_libraries(archive-gui PRIVATE archive_lib comctl32 shell32)
target_include_directories(archive-gui PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(archive-gui PRIVATE UNICODE _UNICODE)
target_compile_options(archive-gui PRIVATE -municode -static)
target_link_options(archive-gui PRIVATE -municode -static -static-libgcc -static-libstdc++)

# Qt6 (optional — UI requires Qt6)
find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    set(ARCHIVE_UI_SOURCES
        src/app/Application.cpp
        src/ui/MainWindow.cpp
        src/ui/SidebarWidget.cpp
        src/ui/DashboardWidget.cpp
        src/ui/ItemsListWidget.cpp
        src/ui/ItemDetailWidget.cpp
        src/ui/ImportDialog.cpp
    )

    add_executable(archive
        src/main.cpp
        ${ARCHIVE_UI_SOURCES}
    )

    target_link_libraries(archive PRIVATE
        archive_lib
        Qt6::Widgets
    )

    target_include_directories(archive PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    include(GNUInstallDirs)
    install(TARGETS archive archive-cli archive-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    message(STATUS "Qt6 not found — skipping UI build (CLI, Win32 GUI, and core library still built)")
    include(GNUInstallDirs)
    install(TARGETS archive-cli archive-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Tests
option(ARCHIVE_BUILD_TESTS "Build tests" ON)
if(ARCHIVE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
