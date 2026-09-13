#include <iostream>
#include <vector>
#include <windows.h>
#include <winsvc.h>
#pragma comment(lib, "advapi32.lib")
int main() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) { std::cerr << "OpenSCManager failed: " << GetLastError() << std::endl; return 1; }
    std::cerr << "SCM opened OK" << std::endl;

    DWORD needed = 0, returned = 0, resume = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &needed, &returned, &resume, nullptr);
    std::cerr << "Need " << needed << " bytes" << std::endl;

    std::vector<BYTE> buf(needed);
    BOOL ok = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf.data(), needed, &needed, &returned, &resume, nullptr);
    std::cerr << "Enum result: " << ok << " returned: " << returned << std::endl;

    if (ok) {
        auto* svc = reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESSW>(buf.data());
        for (DWORD i = 0; i < min(returned, 5u); i++) {
            char name[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, svc[i].lpServiceName, -1, name, sizeof(name), nullptr, nullptr);
            std::cerr << "  " << name << " state=" << svc[i].ServiceStatusProcess.dwCurrentState << std::endl;
        }
    }
    CloseServiceHandle(scm);
    return 0;
}
