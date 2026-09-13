cmake_minimum_required(VERSION 3.20)
project(Archive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Sources (core library — no UI yet)
set(ARCHIVE_SOURCES
    src/main.cpp
    src/app/AppConfig.cpp
    src/app/Application.cpp
    src/core/models/ArchiveItem.cpp
    src/core/models/Category.cpp
    src/core/models/Tag.cpp
    src/core/models/Version.cpp
    src/core/models/Note.cpp
    src/core/models/Activity.cpp
    src/core/models/StoredObject.cpp
)

add_executable(archive ${ARCHIVE_SOURCES})

target_include_directories(archive PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Tests
option(ARCHIVE_BUILD_TESTS "Build tests" ON)
if(ARCHIVE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
