#pragma once
#ifndef MONIX_SENSORS_H
#define MONIX_SENSORS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* ============================================================
   VERSION
   ============================================================ */

#define MONIX_SENSORS_API_VERSION 6



/* ============================================================
   CPU INFORMATION
   ============================================================ */

typedef struct
{
    char vendor[13];
    char brand[49];

    uint32_t family;
    uint32_t model;
    uint32_t stepping;

    uint32_t physicalCores;
    uint32_t logicalCores;

    uint32_t featuresEdx;
    uint32_t featuresEcx;
    uint32_t extFeatures;

    uint32_t cacheLineSize;

    uint32_t l1CacheKB;
    uint32_t l2CacheKB;
    uint32_t l3CacheKB;

    uint32_t apicId;
    uint32_t numaNode;

    uint32_t instructionSetFlags;

    uint32_t virtualizationSupported;
    uint32_t avxSupported;
    uint32_t avx2Supported;
    uint32_t avx512Supported;

    uint32_t aesSupported;
    uint32_t shaSupported;

} CpuInfo;



/* ============================================================
   CPU METADATA
   ============================================================ */

typedef struct
{
    uint32_t logicalCpus;
    uint32_t physicalCpus;

    uint32_t htEnabled;

    uint32_t maxLeaf;
    uint32_t maxExtLeaf;

    uint32_t microcodeRev;

    uint32_t featuresEdx;
    uint32_t featuresEcx;
    uint32_t extFeatures;

    uint64_t tscPerSec;

    uint64_t nominalFrequencyHz;
    uint64_t maxFrequencyHz;

    uint32_t family;
    uint32_t model;
    uint32_t stepping;

    uint64_t invariantTsc;

    uint32_t packageCount;
    uint32_t coreCountPerPackage;

    uint32_t numaNodes;

    uint32_t cacheLevels;

} CpuMeta;



/* ============================================================
   CPU TIMING
   ============================================================ */

typedef struct
{
    uint64_t tsc;

    uint32_t aux;
    uint32_t coreId;

    uint32_t flags;

} CpuTscResult;



typedef struct
{
    uint64_t idleTime;
    uint64_t kernelTime;
    uint64_t userTime;

} CpuTimes;



typedef struct
{
    uint64_t totalTicks;

    uint64_t kernelTicks;
    uint64_t userTicks;
    uint64_t idleTicks;

} CpuTimesDelta;



/* ============================================================
   MEMORY
   ============================================================ */

typedef struct
{
    uint64_t totalBytes;

    uint64_t usedBytes;
    uint64_t availableBytes;

    uint32_t loadPercent;

    uint64_t speedMHz;

    uint32_t channels;

} RamInfo;



typedef struct
{
    uint64_t commitUsedBytes;
    uint64_t commitLimitBytes;
    uint64_t commitPeakBytes;

    uint64_t kernelPagedBytes;
    uint64_t kernelNonPagedBytes;

    uint64_t systemCacheBytes;

    uint64_t availablePhysicalBytes;

    uint64_t standbyListBytes;
    uint64_t modifiedListBytes;
    uint64_t freeListBytes;
    uint64_t zeroListBytes;

    uint64_t pageFaultCount;

    uint64_t workingSetTotal;

    double pagefilePctUsed;
    double commitPressurePct;

} MemoryInfo;



/* ============================================================
   TEMPERATURE
   ============================================================ */

typedef struct
{
    double tempC;

    int valid;
    int estimated;

    char source[32];

} Temperature;



/* ============================================================
   GPU
   ============================================================ */

typedef struct
{
    char name[128];

    double gpuUsagePct;

    double gpuTempC;
    int gpuTempValid;

    double vramUsedMB;
    double vramTotalMB;

    double gpuClockMHz;
    double memoryClockMHz;

    double powerWatts;

    double fanPercent;

    uint32_t driverVersion;

    uint32_t vendorId;
    uint32_t deviceId;

    uint32_t pciBus;
    uint32_t pciDevice;

    uint32_t pciFunction;

} GpuInfo;



/* ============================================================
   STORAGE
   ============================================================ */

typedef struct
{
    uint64_t readBytesPerSec;
    uint64_t writeBytesPerSec;

    uint64_t readOpsPerSec;
    uint64_t writeOpsPerSec;

    double diskQueueLength;

    double readLatencyMs;
    double writeLatencyMs;

} DiskIoRate;



typedef struct
{
    uint64_t totalBytes;

    uint64_t freeBytes;
    uint64_t usedBytes;

    double usedPercent;

} StorageUsage;



typedef struct
{
    uint64_t readBytesPerSec;
    uint64_t writeBytesPerSec;

    uint64_t readOpsPerSec;
    uint64_t writeOpsPerSec;

    double diskQueueLength;

    double readLatencyMs;
    double writeLatencyMs;

} StoragePerformance;



typedef struct
{
    int smartHealthOk;

    double temperatureC;

    int temperatureValid;

    uint32_t errorCode;

    uint32_t smartStatus;

} StorageHealth;



typedef struct
{
    char model[128];
    char serial[128];
    char firmware[64];

    StorageUsage usage;

    StoragePerformance performance;

    StorageHealth health;

    int deviceCount;

} StorageInfo;



/* ============================================================
   NETWORK
   ============================================================ */

typedef struct
{
    uint64_t inBytesPerSec;
    uint64_t outBytesPerSec;

    uint64_t packetsInPerSec;
    uint64_t packetsOutPerSec;

    uint64_t errorsIn;
    uint64_t errorsOut;

    uint64_t droppedIn;
    uint64_t droppedOut;

    uint64_t latencyMs;

    uint32_t interfaceCount;

    int inboundConnections;
    int outboundConnections;

} NetStats;



/* ============================================================
   SYSTEM
   ============================================================ */

typedef struct
{
    char computerName[64];

    char userName[64];

    char osName[64];

    char osVersion[64];

    uint64_t bootTime;

    uint32_t windowsBuild;

    uint32_t architecture;

    uint32_t uptimeDays;

    uint32_t processCount;

    uint32_t threadCount;

} SystemInfo;



/* ============================================================
   MOTHERBOARD
   ============================================================ */

typedef struct
{
    char manufacturer[64];

    char product[64];

    char version[32];

    char serial[64];

    char biosVersion[64];

    char biosDate[32];

} MotherboardInfo;



/* ============================================================
   BATTERY
   ============================================================ */

typedef struct
{
    int present;

    int charging;

    int levelPercent;

    int cycleCount;

    double voltage;

    double capacityWh;

    double remainingWh;

    double designCapacityWh;

    double healthPercent;

    double chargeRateWatts;

} BatteryInfo;



/* ============================================================
   CPU ASM FUNCTIONS
   ============================================================ */

uint64_t cpu_read_tsc(void);

uint64_t cpu_read_tscp(uint32_t* auxOut);


void cpu_cpuid(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t* eax,
    uint32_t* ebx,
    uint32_t* ecx,
    uint32_t* edx
);


void cpu_identify(
    CpuInfo* out
);


uint64_t cpu_read_perf_counter(
    uint32_t counter
);


int cpu_supports_rdtscp(void);


int cpu_read_msr(
    uint32_t msr,
    uint64_t* value
);


uint64_t cpu_read_tsc_delta(
    uint32_t delayMs
);



/* ============================================================
   HARDWARE FUNCTIONS
   ============================================================ */

void hw_read_ram(
    RamInfo* out
);


void hw_read_cpu_times(
    CpuTimes* out
);


void hw_read_cpu_meta(
    CpuMeta* out
);


void hw_estimate_base_clock(
    CpuMeta* meta
);


void hw_read_memory_info(
    MemoryInfo* out
);


void hw_read_storage_info(
    StorageInfo* out
);


void hw_read_gpu_info(
    GpuInfo* out
);


void hw_read_system_info(
    SystemInfo* out
);


void hw_read_motherboard_info(
    MotherboardInfo* out
);


void hw_read_battery_info(
    BatteryInfo* out
);


DiskIoRate hw_read_disk_io(void);


NetStats hw_read_network(void);


uint64_t hw_get_uptime_ms(void);



#ifdef __cplusplus
}
#endif

#endif /* MONIX_SENSORS_H */