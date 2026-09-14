cmake_minimum_required(VERSION 3.20)
project(Monix LANGUAGES C CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT (/MT)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Root paths
set(MONIX_LEAF "${CMAKE_CURRENT_SOURCE_DIR}/Monix/Monix")
set(MONIX_NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src/native")
set(MONIX_CORE "${CMAKE_CURRENT_SOURCE_DIR}/src/core")
set(MONIX_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")

# Collect all core library sources
file(GLOB_RECURSE CORE_EVENTS_SRC     "${MONIX_CORE}/events/*.cpp")
file(GLOB_RECURSE CORE_EVENTBUS_SRC   "${MONIX_CORE}/eventbus/*.cpp")
file(GLOB_RECURSE CORE_VALIDATION_SRC "${MONIX_CORE}/validation/*.cpp")
file(GLOB_RECURSE CORE_COLLECTORS_SRC "${MONIX_CORE}/collectors/*.cpp")
file(GLOB_RECURSE CORE_SECURITY_SRC   "${MONIX_CORE}/security/*.cpp")
file(GLOB_RECURSE CORE_INTEGRATION_SRC "${MONIX_CORE}/integration/*.cpp")
file(GLOB_RECURSE CORE_PLATFORM_SRC   "${MONIX_CORE}/platform/*.cpp")

set(CORE_SOURCES
    ${CORE_EVENTS_SRC}
    ${CORE_EVENTBUS_SRC}
    ${CORE_VALIDATION_SRC}
    ${CORE_COLLECTORS_SRC}
    ${CORE_SECURITY_SRC}
    ${CORE_INTEGRATION_SRC}
    ${CORE_PLATFORM_SRC}
)

# Ultra Logger core library (header-heavy, but compile .cpp files)
add_library(monix_core STATIC ${CORE_SOURCES})
target_include_directories(monix_core PUBLIC
    "${MONIX_CORE}"
    "${MONIX_CORE}/events"
    "${MONIX_CORE}/eventbus"
    "${MONIX_CORE}/validation"
    "${MONIX_CORE}/collectors"
    "${MONIX_CORE}/security"
    "${MONIX_CORE}/integration"
    "${MONIX_CORE}/platform"
    "${MONIX_CORE}/platform/windows"
    "${MONIX_NATIVE}/kernel"
)
target_compile_definitions(monix_core PRIVATE
    WIN32_LEAN_AND_MEAN
    MONIX_KERNEL_BUILD
    UNICODE _UNICODE
)

# --- Main executable sources ---
set(MAIN_SOURCES
    "${MONIX_NATIVE}/main.cpp"
    "${MONIX_LEAF}/src/native/vulkan_renderer.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk_unity.cpp"
    "${MONIX_NATIVE}/settings/SettingsRegistry.cpp"
    "${MONIX_LEAF}/src/native/core/TextUtils.cpp"
    "${MONIX_NATIVE}/telemetry/Collectors.cpp"
    "${MONIX_NATIVE}/kernel/KernelDiagnostics.cpp"
    "${MONIX_NATIVE}/kernel/KernelSelfTest.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/opengl/GlBackend.cpp"
    "${MONIX_LEAF}/src/native/tests/renderer_vk/test_pipeline.cpp"
    "${MONIX_LEAF}/src/native/tests/renderer_vk/test_runtime.cpp"
)

# SCRAM engine (rule-based risk analysis)
file(GLOB SCRAM_SOURCES "${MONIX_LEAF}/src/native/scram/*.cpp")
file(GLOB SCRAM_RULE_SOURCES "${MONIX_LEAF}/src/native/scram/rules/*.cpp")
list(APPEND MAIN_SOURCES ${SCRAM_SOURCES} ${SCRAM_RULE_SOURCES})

# Vulkan decomposition modules (vk_globals, instance, device, swapchain, etc.)
file(GLOB_RECURSE VK_DECOMPOSE_SOURCES "${MONIX_LEAF}/src/native/renderer_vk/vk/*.cpp")
list(APPEND MAIN_SOURCES ${VK_DECOMPOSE_SOURCES})

# Logging subsystem (LogManager, SoundPlayer, NotificationQueue)
list(APPEND MAIN_SOURCES
    "${MONIX_LEAF}/src/native/logging/LogManager.cpp"
    "${MONIX_LEAF}/src/native/logging/SoundPlayer.cpp"
    "${MONIX_LEAF}/src/native/logging/NotificationQueue.cpp"
)

# Login subsystem
set(LOGIN_SOURCES
    "${MONIX_LEAF}/login/LoginOverlay.cpp"
    "${MONIX_LEAF}/login/MonixKernel.cpp"
    "${MONIX_LEAF}/login/KernelDisplay.cpp"
    "${MONIX_LEAF}/login/BootUp.cpp"
    "${MONIX_LEAF}/login/BootUpDisplay.cpp"
    "${MONIX_LEAF}/login/GdiPlusLoader.cpp"
)

# Transitions subsystem
file(GLOB TRANSITION_SOURCES "${MONIX_NATIVE}/transitions/*.cpp")
list(REMOVE_ITEM TRANSITION_SOURCES "${MONIX_NATIVE}/transitions/tests/transition_tests.cpp")

# Hardware sensors (C + ASM)
set(SENSOR_SOURCES
    "${MONIX_LEAF}/sensors/hardware.c"
)

# Windows resource file
set(MONIX_RC "${MONIX_LEAF}/src/native/resources.rc")

add_executable(monix WIN32
    ${MAIN_SOURCES}
    ${LOGIN_SOURCES}
    ${TRANSITION_SOURCES}
    ${SENSOR_SOURCES}
    ${MONIX_RC}
)

target_include_directories(monix PRIVATE
    "${MONIX_LEAF}/login"
    "${MONIX_LEAF}/sensors"
    "${MONIX_NATIVE}"
    "${MONIX_NATIVE}/core"
    "${MONIX_NATIVE}/kernel"
    "${MONIX_NATIVE}/settings"
    "${MONIX_NATIVE}/telemetry"
    "${MONIX_NATIVE}/transitions"
    "${MONIX_NATIVE}/renderer_vk"
    "${MONIX_NATIVE}/scram"
    "${MONIX_LEAF}/src/native"
    "${MONIX_LEAF}/src/native/renderer_vk"
)

target_link_libraries(monix PRIVATE monix_core)

target_compile_definitions(monix PRIVATE
    UNICODE _UNICODE
    MONIX_ARCH_X64
    WIN32_LEAN_AND_MEAN
)

# MASM x64 assembly for cpu.asm
enable_language(ASM_MASM)
set_source_files_properties("${MONIX_LEAF}/sensors/cpu.asm" PROPERTIES
    LANGUAGE ASM_MASM
)

target_sources(monix PRIVATE "${MONIX_LEAF}/sensors/cpu.asm")

# --- Platform libraries (from build.ps1 link line) ---
target_link_libraries(monix PRIVATE
    user32 gdi32 psapi comdlg32 shell32 shlwapi
    iphlpapi ws2_32 ole32 opengl32 uuid winmm
    kernel32 ntdll setupapi bcrypt wintrust crypt32
    advapi32 powrprof gdiplus pdh wbemuuid shcore
    wtsapi32
)

# MSVC-specific flags matching build.ps1
target_compile_options(monix PRIVATE
    $<$<COMPILE_LANGUAGE:CXX,C>:/MT /Zm800 /bigobj /EHsc /utf-8 /w>
)
target_compile_options(monix_core PRIVATE
    /MT /EHsc /utf-8 /w /DWIN32_LEAN_AND_MEAN /DMONIX_KERNEL_BUILD
)

# --- Test executable ---
file(GLOB_RECURSE TEST_SOURCES "${MONIX_TESTS}/core/**/*.cpp")

add_executable(monix_tests ${TEST_SOURCES})
target_link_libraries(monix_tests PRIVATE monix_core)
target_include_directories(monix_tests PRIVATE
    "${MONIX_CORE}"
    "${MONIX_CORE}/events"
    "${MONIX_CORE}/eventbus"
    "${MONIX_CORE}/validation"
    "${MONIX_CORE}/collectors"
    "${MONIX_CORE}/security"
    "${MONIX_CORE}/integration"
    "${MONIX_CORE}/platform"
    "${MONIX_CORE}/platform/windows"
    "${MONIX_NATIVE}/kernel"
)
target_compile_definitions(monix_tests PRIVATE
    WIN32_LEAN_AND_MEAN
    MONIX_KERNEL_BUILD
    MONIX_KERNEL_TEST_BUILD
    UNICODE _UNICODE
)
target_compile_options(monix_tests PRIVATE /MT /EHsc /utf-8 /w)
