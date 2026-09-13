#include <iostream>
#include <chrono>
#include <vector>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
int main() {
    auto t0 = std::chrono::steady_clock::now();
    DWORD needed = 0;
    EnumDeviceDrivers(nullptr, 0, &needed);
    auto t1 = std::chrono::steady_clock::now();
    std::cerr << "1st call: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count() << " ms, needed=" << needed << std::endl;

    DWORD count = needed / sizeof(LPVOID);
    std::vector<LPVOID> bases(count);
    DWORD actual = 0;
    EnumDeviceDrivers(bases.data(), needed, &actual);
    auto t2 = std::chrono::steady_clock::now();
    std::cerr << "2nd call: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() << " ms, actual=" << actual << std::endl;

    for (DWORD i = 0; i < min(actual, 10u); i++) {
        char bname[256]{}; char bpath[512]{};
        GetDeviceDriverBaseNameA(bases[i], bname, sizeof(bname));
        GetDeviceDriverFileNameA(bases[i], bpath, sizeof(bpath));
        std::cerr << "  " << bname << " @ " << bpath << std::endl;
    }
    auto t3 = std::chrono::steady_clock::now();
    std::cerr << "First 10: " << std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count() << " ms" << std::endl;

    // Time ALL
    for (DWORD i = 0; i < actual; i++) {
        char bname[256]{}; char bpath[512]{};
        GetDeviceDriverBaseNameA(bases[i], bname, sizeof(bname));
        GetDeviceDriverFileNameA(bases[i], bpath, sizeof(bpath));
    }
    auto t4 = std::chrono::steady_clock::now();
    std::cerr << "All " << actual << ": " << std::chrono::duration_cast<std::chrono::milliseconds>(t4-t3).count() << " ms" << std::endl;
    return 0;
}
