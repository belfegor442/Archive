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

# Core library sources (explicit lists — no GLOB_RECURSE)
set(CORE_SOURCES
    # events
    "${MONIX_CORE}/events/ActorRef.cpp"
    "${MONIX_CORE}/events/EntityRef.cpp"
    "${MONIX_CORE}/events/EventBuilder.cpp"
    "${MONIX_CORE}/events/EventDeserializer.cpp"
    "${MONIX_CORE}/events/EventErrors.cpp"
    "${MONIX_CORE}/events/EventFactory.cpp"
    "${MONIX_CORE}/events/EventId.cpp"
    "${MONIX_CORE}/events/EventPayload.cpp"
    "${MONIX_CORE}/events/EventSerializer.cpp"
    "${MONIX_CORE}/events/EventSeverity.cpp"
    "${MONIX_CORE}/events/EventTime.cpp"
    "${MONIX_CORE}/events/Provenance.cpp"
    "${MONIX_CORE}/events/SourceRef.cpp"
    # eventbus
    "${MONIX_CORE}/eventbus/BackpressureController.cpp"
    "${MONIX_CORE}/eventbus/DeadLetterQueue.cpp"
    "${MONIX_CORE}/eventbus/EventBus.cpp"
    "${MONIX_CORE}/eventbus/EventBusErrors.cpp"
    "${MONIX_CORE}/eventbus/EventBusMetrics.cpp"
    "${MONIX_CORE}/eventbus/EventFilter.cpp"
    "${MONIX_CORE}/eventbus/EventQueue.cpp"
    "${MONIX_CORE}/eventbus/EventSubscription.cpp"
    # validation
    "${MONIX_CORE}/validation/EventSchema.cpp"
    "${MONIX_CORE}/validation/EventSchemaRegistry.cpp"
    "${MONIX_CORE}/validation/EventValidationPipeline.cpp"
    "${MONIX_CORE}/validation/IntegrityValidator.cpp"
    "${MONIX_CORE}/validation/QuarantineManager.cpp"
    "${MONIX_CORE}/validation/SchemaValidator.cpp"
    "${MONIX_CORE}/validation/SourceRegistry.cpp"
    "${MONIX_CORE}/validation/StructuralValidator.cpp"
    "${MONIX_CORE}/validation/SemanticValidator.cpp"
    "${MONIX_CORE}/validation/ValidationMetrics.cpp"
    "${MONIX_CORE}/validation/ValidationPolicy.cpp"
    "${MONIX_CORE}/validation/ValidationResult.cpp"
    "${MONIX_CORE}/validation/ValidationTypes.cpp"
    # collectors
    "${MONIX_CORE}/collectors/CollectorConfig.cpp"
    "${MONIX_CORE}/collectors/CollectorErrors.cpp"
    "${MONIX_CORE}/collectors/CollectorManager.cpp"
    "${MONIX_CORE}/collectors/CollectorMetrics.cpp"
    "${MONIX_CORE}/collectors/CollectorRegistry.cpp"
    "${MONIX_CORE}/collectors/CollectorStatus.cpp"
    "${MONIX_CORE}/collectors/activity/ActivityModel.cpp"
    "${MONIX_CORE}/collectors/activityview/ActivityView.cpp"
    "${MONIX_CORE}/collectors/camera/CameraAdapter.cpp"
    "${MONIX_CORE}/collectors/circuit/CircuitBreaker.cpp"
    "${MONIX_CORE}/collectors/config/ConfigCollector.cpp"
    "${MONIX_CORE}/collectors/config/ConfigTypes.cpp"
    "${MONIX_CORE}/collectors/context/DetectionContext.cpp"
    "${MONIX_CORE}/collectors/correlation/EventCorrelation.cpp"
    "${MONIX_CORE}/collectors/device/DeviceCollector.cpp"
    "${MONIX_CORE}/collectors/device/DeviceTypes.cpp"
    "${MONIX_CORE}/collectors/driver/DriverCollector.cpp"
    "${MONIX_CORE}/collectors/driver/DriverTypes.cpp"
    "${MONIX_CORE}/collectors/external/ExternalObservation.cpp"
    "${MONIX_CORE}/collectors/failureinject/FailureInjector.cpp"
    "${MONIX_CORE}/collectors/filesystem/FilesystemCoalescer.cpp"
    "${MONIX_CORE}/collectors/filesystem/FilesystemCollector.cpp"
    "${MONIX_CORE}/collectors/filesystem/FilesystemDeduplicator.cpp"
    "${MONIX_CORE}/collectors/filesystem/FilesystemHasher.cpp"
    "${MONIX_CORE}/collectors/filesystem/FilesystemScope.cpp"
    "${MONIX_CORE}/collectors/filesystem/FilesystemTypes.cpp"
    "${MONIX_CORE}/collectors/filesystem/FilesystemWatcher.cpp"
    "${MONIX_CORE}/collectors/forensic/ForensicMode.cpp"
    "${MONIX_CORE}/collectors/forensic/HashIntegrity.cpp"
    "${MONIX_CORE}/collectors/health/CollectorHealth.cpp"
    "${MONIX_CORE}/collectors/health/HealthTypes.cpp"
    "${MONIX_CORE}/collectors/health/ObservationGap.cpp"
    "${MONIX_CORE}/collectors/health/RestartPolicy.cpp"
    "${MONIX_CORE}/collectors/inspector/EventInspector.cpp"
    "${MONIX_CORE}/collectors/integrity_storage/IntegrityStorage.cpp"
    "${MONIX_CORE}/collectors/livestream/LiveStream.cpp"
    "${MONIX_CORE}/collectors/network/NetworkCollector.cpp"
    "${MONIX_CORE}/collectors/network/NetworkTypes.cpp"
    "${MONIX_CORE}/collectors/noise/NoiseControl.cpp"
    "${MONIX_CORE}/collectors/process/ProcessCollector.cpp"
    "${MONIX_CORE}/collectors/process/ProcessIdentity.cpp"
    "${MONIX_CORE}/collectors/process/ProcessSnapshot.cpp"
    "${MONIX_CORE}/collectors/process/ProcessTypes.cpp"
    "${MONIX_CORE}/collectors/quarantine/QuarantineIntegration.cpp"
    "${MONIX_CORE}/collectors/retention/RetentionPolicy.cpp"
    "${MONIX_CORE}/collectors/ruleengine/RuleEngine.cpp"
    "${MONIX_CORE}/collectors/script/ScriptCollector.cpp"
    "${MONIX_CORE}/collectors/script/ScriptTypes.cpp"
    "${MONIX_CORE}/collectors/selfmonitor/SelfMonitor.cpp"
    "${MONIX_CORE}/collectors/sensor/SensorCollector.cpp"
    "${MONIX_CORE}/collectors/sensor/SensorTypes.cpp"
    "${MONIX_CORE}/collectors/service/ServiceCollector.cpp"
    "${MONIX_CORE}/collectors/service/ServiceTypes.cpp"
    "${MONIX_CORE}/collectors/snapshot/SnapshotEngine.cpp"
    "${MONIX_CORE}/collectors/snapshot/SnapshotTypes.cpp"
    "${MONIX_CORE}/collectors/storage/EventStorage.cpp"
    "${MONIX_CORE}/collectors/storage/StorageCollector.cpp"
    "${MONIX_CORE}/collectors/storage/StorageTypes.cpp"
    "${MONIX_CORE}/collectors/storm/EventStormProtection.cpp"
    "${MONIX_CORE}/collectors/trust/DeviceTrust.cpp"
    "${MONIX_CORE}/collectors/usersession/UserSessionCollector.cpp"
    "${MONIX_CORE}/collectors/usersession/UserSessionTypes.cpp"
    # security
    "${MONIX_CORE}/security/SecretDetector.cpp"
    # integration
    "${MONIX_CORE}/integration/UltraPipeline.cpp"
    # platform
    "${MONIX_CORE}/platform/PlatformFactory.cpp"
    "${MONIX_CORE}/platform/PlatformInterfaces.cpp"
    "${MONIX_CORE}/platform/linux/LinuxPlatform.cpp"
    "${MONIX_CORE}/platform/windows/WindowsPlatform.cpp"
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
set(SCRAM_SOURCES
    "${MONIX_LEAF}/src/native/scram/ScramEngine.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/AudioMultimediaRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/CpuIdentityRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/CpuPressureRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/DiskStorageRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/DisplayConfigRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/FilesystemDataRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/FrameTimeRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/GpuDriverRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/GpuThermalPowerFanRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/GpuUtilizationRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/GpuVramRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/HandleObjectRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/HardwareSensorsRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/MemorySubsystemRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/NetworkLatencyRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/NetworkTopologyRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/NetworkTrafficRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/OsKernelRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/PowerBatteryRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/ProcessTriageRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/RamExhaustionRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/RegistryConfigRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/ReliabilityRecoveryRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/SecurityIntegrityRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/TdrStabilityRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/ThermalCoolingRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/ThermalDriftRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/ThreadSchedulingRule.cpp"
    "${MONIX_LEAF}/src/native/scram/rules/VpnTunnelRule.cpp"
)
list(APPEND MAIN_SOURCES ${SCRAM_SOURCES})

# Vulkan decomposition modules (vk_globals, instance, device, swapchain, etc.)
set(VK_DECOMPOSE_SOURCES
    "${MONIX_LEAF}/src/native/renderer_vk/vk/vk_globals.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/buffer/VkBuffer.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/cmd/VkCmdRecording.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/command/VkCommand.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/descriptor/VkDescriptor.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/device/VkDevice.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/image/VkImage.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/instance/VkInstance.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/memory/VkMemory.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/pipeline/VkPipeline.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/preset/VkPreset.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/sampler/VkSampler.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/screenshot/VkScreenshot.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/swapchain/VkSwapchain.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/sync/VkSync.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/upload/VkUploadTexture.cpp"
    "${MONIX_LEAF}/src/native/renderer_vk/vk/validation/VkValidation.cpp"
)
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
set(TRANSITION_SOURCES
    "${MONIX_NATIVE}/transitions/TransitionPipeline.cpp"
    "${MONIX_NATIVE}/transitions/TransitionManager.cpp"
    "${MONIX_NATIVE}/transitions/ScanlineTransition.cpp"
    "${MONIX_NATIVE}/transitions/ScanlineBlender.cpp"
)

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
    $<$<COMPILE_LANGUAGE:CXX,C>:/MT /Zm800 /bigobj /EHsc /utf-8 /W4 /permissive- /external:anglebrackets /external:W0>
)
target_compile_options(monix_core PRIVATE
    /MT /EHsc /utf-8 /W4 /permissive- /external:anglebrackets /external:W0 /DWIN32_LEAN_AND_MEAN /DMONIX_KERNEL_BUILD
)

# --- Test executables (each test file is a standalone executable) ---
file(GLOB_RECURSE TEST_FILES "${MONIX_TESTS}/core/**/*.cpp")

foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE monix_core)
    target_include_directories(${TEST_NAME} PRIVATE
        "${MONIX_CORE}"
        "${MONIX_CORE}/events"
        "${MONIX_CORE}/eventbus"
        "${MONIX_CORE}/validation"
        "${MONIX_CORE}/collectors"
        "${MONIX_CORE}/collectors/activity"
        "${MONIX_CORE}/collectors/activityview"
        "${MONIX_CORE}/collectors/camera"
        "${MONIX_CORE}/collectors/circuit"
        "${MONIX_CORE}/collectors/config"
        "${MONIX_CORE}/collectors/context"
        "${MONIX_CORE}/collectors/correlation"
        "${MONIX_CORE}/collectors/device"
        "${MONIX_CORE}/collectors/driver"
        "${MONIX_CORE}/collectors/external"
        "${MONIX_CORE}/collectors/failureinject"
        "${MONIX_CORE}/collectors/filesystem"
        "${MONIX_CORE}/collectors/forensic"
        "${MONIX_CORE}/collectors/health"
        "${MONIX_CORE}/collectors/inspector"
        "${MONIX_CORE}/collectors/integrity_storage"
        "${MONIX_CORE}/collectors/livestream"
        "${MONIX_CORE}/collectors/network"
        "${MONIX_CORE}/collectors/noise"
        "${MONIX_CORE}/collectors/process"
        "${MONIX_CORE}/collectors/quarantine"
        "${MONIX_CORE}/collectors/retention"
        "${MONIX_CORE}/collectors/ruleengine"
        "${MONIX_CORE}/collectors/script"
        "${MONIX_CORE}/collectors/selfmonitor"
        "${MONIX_CORE}/collectors/sensor"
        "${MONIX_CORE}/collectors/service"
        "${MONIX_CORE}/collectors/snapshot"
        "${MONIX_CORE}/collectors/storage"
        "${MONIX_CORE}/collectors/storm"
        "${MONIX_CORE}/collectors/trust"
        "${MONIX_CORE}/collectors/usersession"
        "${MONIX_CORE}/security"
        "${MONIX_CORE}/integration"
        "${MONIX_CORE}/platform"
        "${MONIX_CORE}/platform/windows"
        "${MONIX_NATIVE}/kernel"
    )
    target_compile_definitions(${TEST_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        MONIX_KERNEL_TEST_BUILD
        UNICODE _UNICODE
    )
    target_compile_options(${TEST_NAME} PRIVATE /MT /EHsc /utf-8 /W4 /permissive-)
endforeach()
