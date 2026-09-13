#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winsvc.h>
#include <psapi.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
int main() {
    // Registry scan for kernel drivers
    HKEY sk = nullptr;
    RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_READ, &sk);
    DWORD count = 0;
    RegQueryInfoKeyW(sk, nullptr, nullptr, nullptr, &count, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    for (DWORD i = 0; i < count; i++) {
        wchar_t name[256]{}; DWORD len = 256;
        RegEnumKeyExW(sk, i, name, &len, nullptr, nullptr, nullptr, nullptr);
        HKEY ck = nullptr;
        RegOpenKeyExW(sk, name, 0, KEY_READ, &ck);
        DWORD type = 0, sz = sizeof(type);
        RegQueryValueExW(ck, L"Type", nullptr, nullptr, (LPBYTE)&type, &sz);
        RegCloseKey(ck);
        if (type & 0x1) {
            char cname[256]{};
            WideCharToMultiByte(CP_UTF8, 0, name, -1, cname, sizeof(cname), nullptr, nullptr);
            std::string sname(cname);
            if (sname.find("ntoskrnl") != std::string::npos || sname.find("Ntfs") != std::string::npos || sname == "ACPI" || sname == "disk") {
                std::cerr << "  kernel: " << sname << std::endl;
            }
        }
    }
    RegCloseKey(sk);

    // API scan
    DWORD needed = 0;
    EnumDeviceDrivers(nullptr, 0, &needed);
    DWORD cnt = needed / sizeof(LPVOID);
    std::vector<LPVOID> bases(cnt);
    DWORD ret = 0;
    EnumDeviceDrivers(bases.data(), needed, &ret);
    cnt = ret / sizeof(LPVOID);
    for (DWORD i = 0; i < min(cnt, 10u); i++) {
        char bname[256]{};
        GetDeviceDriverBaseNameA(bases[i], bname, sizeof(bname));
        std::cerr << "  api: " << bname << std::endl;
    }
    return 0;
}
