#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace monix::telemetry {

struct EntityIdentity {
  enum class Kind {
    Process,
    Thread,
    Module,
    Driver,
    Device,
    NetworkConnection,
    File,
    RegistryKey,
    StorageDevice,
    Gpu,
    Cpu,
    Memory,
    Firmware,
    SecurityState
  };

  Kind kind = Kind::Process;
  std::uint64_t idLow = 0;
  std::uint64_t idHigh = 0;

  static EntityIdentity Process(std::uint32_t pid, std::uint64_t creationTime100ns) {
    EntityIdentity e;
    e.kind = Kind::Process;
    e.idLow = pid;
    e.idHigh = creationTime100ns;
    return e;
  }

  static EntityIdentity Thread(std::uint32_t tid, std::uint32_t ownerPid) {
    EntityIdentity e;
    e.kind = Kind::Thread;
    e.idLow = (static_cast<std::uint64_t>(ownerPid) << 32) | tid;
    return e;
  }

  static EntityIdentity Module(std::uint32_t pid, const std::wstring& path) {
    EntityIdentity e;
    e.kind = Kind::Module;
    e.idLow = pid;
    std::hash<std::wstring> h;
    e.idHigh = h(path);
    return e;
  }

  static EntityIdentity Driver(const std::wstring& name) {
    EntityIdentity e;
    e.kind = Kind::Driver;
    std::hash<std::wstring> h;
    e.idLow = h(name);
    return e;
  }

  bool operator==(const EntityIdentity& o) const {
    return kind == o.kind && idLow == o.idLow && idHigh == o.idHigh;
  }

  bool operator!=(const EntityIdentity& o) const { return !(*this == o); }
};

}
