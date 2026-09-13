#pragma once

#include "../ICollector.hpp"
#include "../CollectorConfig.hpp"
#include "FilesystemTypes.hpp"
#include "FilesystemWatcher.hpp"
#include "FilesystemHasher.hpp"

#include <atomic>
#include <thread>
#include <mutex>

namespace monix::collectors {

class FilesystemCollector : public ICollector {
public:
  FilesystemCollector();
  ~FilesystemCollector() override;

  const CollectorId& id() const override;
  const CollectorInfo& info() const override;
  CollectorStatus status() const override;
  CollectorCapability capabilities() const override;

  bool start(const CollectorConfig& config) override;
  bool stop() override;
  void setEventCallback(EventCallback callback) override;
  void setFilesystemConfig(fs::FilesystemConfig fsCfg);

  const fs::FilesystemConfig& fsConfig() const;
  std::size_t eventsCollected() const;
  std::size_t duplicatesSkipped() const;
  std::size_t coalescedCount() const;

private:
  void onFilesystemEvent(fs::FilesystemEvent fsEvent);
  events::Event convertToEvent(const fs::FilesystemEvent& fsEvent);
  void parseFsConfig(const CollectorConfig& base);

  CollectorId id_ = "filesystem";
  CollectorInfo info_;
  std::atomic<CollectorLifecycle> lifecycle_{CollectorLifecycle::Created};
  EventCallback eventCallback_;
  fs::FilesystemConfig fsConfig_;
  fs::FilesystemWatcher watcher_;

  std::atomic<std::size_t> eventsCollected_{0};
  std::atomic<std::size_t> duplicatesSkipped_{0};
  std::atomic<std::size_t> coalescedCount_{0};
};

}  // namespace monix::collectors
