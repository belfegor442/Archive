#include <stdint.h>
#include "sensors.h"

#include <winsock2.h>
#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <pdh.h>
#include <powrprof.h>
#include <intrin.h>
#include <string.h>
#include <stdio.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "powrprof.lib")


/* ============================================================
   RAM
   ============================================================ */

void hw_read_ram(RamInfo* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(RamInfo));

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);

    if (GlobalMemoryStatusEx(&mem))
    {
        out->totalBytes = mem.ullTotalPhys;
        out->availableBytes = mem.ullAvailPhys;

        out->usedBytes =
            mem.ullTotalPhys > mem.ullAvailPhys ?
            mem.ullTotalPhys - mem.ullAvailPhys :
            0;

        out->loadPercent = mem.dwMemoryLoad;
    }
}



/* ============================================================
   CPU TIMES
   ============================================================ */

void hw_read_cpu_times(CpuTimes* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(CpuTimes));

    FILETIME idle;
    FILETIME kernel;
    FILETIME user;


    if (GetSystemTimes(&idle, &kernel, &user))
    {
        out->idleTime =
            ((uint64_t)idle.dwHighDateTime << 32) |
            idle.dwLowDateTime;


        out->kernelTime =
            ((uint64_t)kernel.dwHighDateTime << 32) |
            kernel.dwLowDateTime;


        out->userTime =
            ((uint64_t)user.dwHighDateTime << 32) |
            user.dwLowDateTime;
    }
}



/* ============================================================
   UPTIME
   ============================================================ */

uint64_t hw_get_uptime_ms(void)
{
    return GetTickCount64();
}



/* ============================================================
   DISK IO
   ============================================================ */

DiskIoRate hw_read_disk_io(void)
{
    DiskIoRate rate;

    memset(&rate, 0, sizeof(rate));


    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0
        );


    if (snapshot == INVALID_HANDLE_VALUE)
        return rate;



    PROCESSENTRY32W entry;

    entry.dwSize = sizeof(entry);



    uint64_t totalRead = 0;
    uint64_t totalWrite = 0;



    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            HANDLE process =
                OpenProcess(
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    FALSE,
                    entry.th32ProcessID
                );


            if (process)
            {
                IO_COUNTERS io;


                if (GetProcessIoCounters(process, &io))
                {
                    totalRead += io.ReadTransferCount;
                    totalWrite += io.WriteTransferCount;
                }


                CloseHandle(process);
            }


        } while(Process32NextW(snapshot, &entry));
    }


    CloseHandle(snapshot);



    rate.readBytesPerSec = totalRead;
    rate.writeBytesPerSec = totalWrite;


    return rate;
}



/* ============================================================
   NETWORK
   ============================================================ */

NetStats hw_read_network(void)
{
    NetStats stats;

    memset(&stats, 0, sizeof(stats));



    ULONG size = 0;

    GetIfTable(NULL, &size, FALSE);



    if (size)
    {
        BYTE* buffer =
            (BYTE*)HeapAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                size
            );


        if (buffer)
        {
            MIB_IFTABLE* table =
                (MIB_IFTABLE*)buffer;



            if (GetIfTable(table, &size, FALSE) == NO_ERROR)
            {
                for (DWORD i = 0;
                     i < table->dwNumEntries;
                     i++)
                {
                    MIB_IFROW* row =
                        &table->table[i];


                    if (row->dwOperStatus ==
                        IF_OPER_STATUS_OPERATIONAL)
                    {
                        stats.inBytesPerSec +=
                            row->dwInOctets;

                        stats.outBytesPerSec +=
                            row->dwOutOctets;
                    }
                }
            }


            HeapFree(
                GetProcessHeap(),
                0,
                buffer
            );
        }
    }




    DWORD tcpSize = 0;


    GetExtendedTcpTable(
        NULL,
        &tcpSize,
        FALSE,
        AF_INET,
        TCP_TABLE_OWNER_PID_ALL,
        0
    );


    if (tcpSize)
    {
        BYTE* buffer =
            (BYTE*)HeapAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                tcpSize
            );


        if (buffer)
        {
            MIB_TCPTABLE_OWNER_PID* table =
                (MIB_TCPTABLE_OWNER_PID*)buffer;


            if (GetExtendedTcpTable(
                table,
                &tcpSize,
                FALSE,
                AF_INET,
                TCP_TABLE_OWNER_PID_ALL,
                0
            ) == NO_ERROR)
            {
                for (DWORD i = 0;
                     i < table->dwNumEntries;
                     i++)
                {
                    DWORD state =
                        table->table[i].dwState;


                    if (state == MIB_TCP_STATE_ESTAB)
                        stats.inboundConnections++;

                    else if (state == MIB_TCP_STATE_LISTEN)
                        stats.outboundConnections++;
                }
            }


            HeapFree(
                GetProcessHeap(),
                0,
                buffer
            );
        }
    }


    return stats;
}



/* ============================================================
   CPU META
   ============================================================ */

void hw_read_cpu_meta(CpuMeta* out)
{
    if (!out)
        return;


    memset(out, 0, sizeof(CpuMeta));


    CpuInfo cpu;

    memset(&cpu, 0, sizeof(cpu));


    cpu_identify(&cpu);



    out->family = cpu.family;
    out->model = cpu.model;
    out->stepping = cpu.stepping;


    out->featuresEdx = cpu.featuresEdx;
    out->featuresEcx = cpu.featuresEcx;
    out->extFeatures = cpu.extFeatures;



    SYSTEM_INFO sys;

    GetSystemInfo(&sys);


    out->logicalCpus =
        sys.dwNumberOfProcessors;



    out->htEnabled =
        (cpu.featuresEdx & (1 << 28))
        ? 1 : 0;



    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;



    cpu_cpuid(
        0,
        0,
        &eax,
        &ebx,
        &ecx,
        &edx
    );


    out->maxLeaf = eax;



    cpu_cpuid(
        0x80000000,
        0,
        &eax,
        &ebx,
        &ecx,
        &edx
    );


    out->maxExtLeaf = eax;
}



/* ============================================================
   CPU CLOCK
   ============================================================ */

void hw_estimate_base_clock(CpuMeta* meta)
{
    if (!meta)
        return;


    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER freq;


    QueryPerformanceFrequency(&freq);


    uint64_t tscStart =
        cpu_read_tsc();



    QueryPerformanceCounter(&start);



    volatile uint64_t dummy = 0;


    for (int i = 0; i < 10000000; i++)
        dummy += i;



    QueryPerformanceCounter(&end);



    uint64_t tscEnd =
        cpu_read_tsc();



    double seconds =
        (double)(end.QuadPart - start.QuadPart)
        /
        (double)freq.QuadPart;



    if (seconds > 0)
    {
        meta->tscPerSec =
            (uint64_t)
            ((double)(tscEnd - tscStart) /
            seconds);
    }
}



/* ============================================================
   MEMORY INFO
   ============================================================ */

void hw_read_memory_info(MemoryInfo* out)
{
    if (!out)
        return;


    memset(out, 0, sizeof(MemoryInfo));



    PERFORMANCE_INFORMATION info;

    info.cb = sizeof(info);



    if (GetPerformanceInfo(
        &info,
        sizeof(info)
    ))
    {
        out->commitUsedBytes =
            (uint64_t)info.CommitTotal *
            info.PageSize;


        out->commitLimitBytes =
            (uint64_t)info.CommitLimit *
            info.PageSize;


        out->commitPeakBytes =
            (uint64_t)info.CommitPeak *
            info.PageSize;


        out->kernelPagedBytes =
            (uint64_t)info.KernelPaged *
            info.PageSize;


        out->kernelNonPagedBytes =
            (uint64_t)info.KernelNonpaged *
            info.PageSize;


        out->systemCacheBytes =
            (uint64_t)info.SystemCache *
            info.PageSize;
    }



    MEMORYSTATUSEX mem;

    mem.dwLength =
        sizeof(mem);


    if (GlobalMemoryStatusEx(&mem))
    {
        out->availablePhysicalBytes =
            mem.ullAvailPhys;
    }



    if (out->commitLimitBytes)
    {
        out->commitPressurePct =
            ((double)out->commitUsedBytes /
             (double)out->commitLimitBytes)
             * 100.0;
    }
}



/* ============================================================
   STORAGE
   ============================================================ */

void hw_read_storage_info(StorageInfo* out)
{
    if (!out)
        return;


    memset(out, 0, sizeof(StorageInfo));



    ULARGE_INTEGER freeBytes;
    ULARGE_INTEGER totalBytes;
    ULARGE_INTEGER available;



    if (GetDiskFreeSpaceExW(
        L"C:\\",
        &available,
        &totalBytes,
        &freeBytes
    ))
    {
        out->usage.totalBytes =
            totalBytes.QuadPart;


        out->usage.freeBytes =
            freeBytes.QuadPart;


        out->usage.usedBytes =
            totalBytes.QuadPart -
            freeBytes.QuadPart;



        if (totalBytes.QuadPart)
        {
            out->usage.usedPercent =
                ((double)out->usage.usedBytes /
                (double)totalBytes.QuadPart)
                * 100.0;
        }
    }



    out->health.smartHealthOk = 1;
}