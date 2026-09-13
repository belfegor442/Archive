#pragma once

#include <string>

class MonixApp;

namespace monix::runtime {

std::string RunProcessCapture(const MonixApp* app, const std::wstring& commandLine);

}  // namespace monix::runtime
