#include "ProcessCapture.hpp"

#include "../../MonixApp.hpp"

#include <windows.h>
#include <array>
#include <string>

std::string MonixApp::RunProcessCapture(const std::wstring& commandLine) const {
  SECURITY_ATTRIBUTES sa {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = nullptr;

  HANDLE readPipe = nullptr;
  HANDLE writePipe = nullptr;
  if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
    return {};
  }

  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = writePipe;
  si.hStdError = writePipe;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION pi {};
  std::wstring mutableCommand = commandLine;
  const BOOL created = CreateProcessW(
    nullptr,
    mutableCommand.data(),
    nullptr,
    nullptr,
    TRUE,
    CREATE_NO_WINDOW,
    nullptr,
    nullptr,
    &si,
    &pi
  );

  CloseHandle(writePipe);
  if (!created) {
    CloseHandle(readPipe);
    return {};
  }

  std::string output;
  std::array<char, 4096> buffer {};
  DWORD bytesRead = 0;
  while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0) {
    output.append(buffer.data(), buffer.data() + bytesRead);
  }

  WaitForSingleObject(pi.hProcess, 15000);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(readPipe);
  return output;
}
