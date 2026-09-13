#include <iostream>
#include <chrono>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
int main() {
    auto t0 = std::chrono::steady_clock::now();

    // Test EnumDeviceDrivers
    DWORD needed = 0;
    EnumDeviceDrivers(nullptr, 0, &needed);
    auto t1 = std::chrono::steady_clock::now();
    std::cerr << "EnumDeviceDrivers (1st call): " << std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count() << " ms, needed=" << needed << std::endl;

    std::vector<LPVOID> bases(needed / sizeof(LPVOID));
    DWORD count = 0;
    EnumDeviceDrivers(bases.data(), needed, &count);
    auto t2 = std::chrono::steady_clock::now();
    std::cerr << "EnumDeviceDrivers (2nd call): " << std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() << " ms, count=" << count << std::endl;

    // Test registry enumeration
    HKEY key = nullptr;
    RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_READ, &key);
    DWORD subkeys = 0;
    RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    auto t3 = std::chrono::steady_clock::now();
    std::cerr << "Registry open + query: " << std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count() << " ms, subkeys=" << subkeys << std::endl;

    // Enumerate a few
    int count_drv = 0;
    for (DWORD i = 0; i < subkeys; i++) {
        wchar_t name[256]{}; DWORD len = 256;
        if (RegEnumKeyExW(key, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) continue;
        HKEY svckey = nullptr;
        RegOpenKeyExW(key, name, 0, KEY_READ, &svckey);
        DWORD type = 0, sz = sizeof(type);
        RegQueryValueExW(svckey, L"Type", nullptr, nullptr, (LPBYTE)&type, &sz);
        RegCloseKey(svckey);
        if (type & 0x1) count_drv++;
    }
    RegCloseKey(key);
    auto t4 = std::chrono::steady_clock::now();
    std::cerr << "Registry full scan: " << std::chrono::duration_cast<std::chrono::milliseconds>(t4-t3).count() << " ms, kernel drivers=" << count_drv << std::endl;

    return 0;
}
