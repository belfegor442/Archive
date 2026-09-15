#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "SystemEvent.hpp"

namespace monix {

class EventBus {
public:
  using EventCallback = std::function<void(const SystemEvent&)>;

  uint64_t Subscribe(EventCategory category, EventCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = nextSubId_++;
    categorySubs_.push_back({id, category, std::move(cb)});
    return id;
  }

  uint64_t Subscribe(const std::wstring& eventType, EventCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = nextSubId_++;
    typeSubs_.push_back({id, eventType, std::move(cb)});
    return id;
  }

  uint64_t SubscribeAll(EventCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = nextSubId_++;
    globalSubs_.push_back({id, std::move(cb)});
    return id;
  }

  void Unsubscribe(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto removeIt = [subscriptionId](const auto& sub) {
      return sub.id == subscriptionId;
    };
    categorySubs_.erase(
      std::remove_if(categorySubs_.begin(), categorySubs_.end(), removeIt),
      categorySubs_.end());
    typeSubs_.erase(
      std::remove_if(typeSubs_.begin(), typeSubs_.end(), removeIt),
      typeSubs_.end());
    globalSubs_.erase(
      std::remove_if(globalSubs_.begin(), globalSubs_.end(), removeIt),
      globalSubs_.end());
  }

  void Emit(SystemEvent event) {
    std::vector<EventCallback> toCall;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      event.id = nextEventId_++;
      events_.push_back(event);
      while (static_cast<int>(events_.size()) > maxEvents_) {
        events_.pop_front();
      }
      for (const auto& sub : categorySubs_) {
        if (sub.category == event.category) {
          toCall.push_back(sub.callback);
        }
      }
      for (const auto& sub : typeSubs_) {
        if (sub.eventType == event.type) {
          toCall.push_back(sub.callback);
        }
      }
      for (const auto& sub : globalSubs_) {
        toCall.push_back(sub.callback);
      }
    }
    for (const auto& cb : toCall) {
      cb(event);
    }
  }

  void EmitBatch(std::vector<SystemEvent> events) {
    for (auto& e : events) {
      Emit(std::move(e));
    }
  }

  const SystemEvent& Event(uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    static const SystemEvent empty;
    for (const auto& e : events_) {
      if (e.id == id) return e;
    }
    return empty;
  }

  std::vector<SystemEvent> RecentEvents(int count = 100) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SystemEvent> result;
    int toReturn = std::min(count, static_cast<int>(events_.size()));
    result.reserve(toReturn);
    for (int i = static_cast<int>(events_.size()) - toReturn;
         i < static_cast<int>(events_.size()); ++i) {
      result.push_back(events_[i]);
    }
    return result;
  }

  std::vector<SystemEvent> EventsInTimeRange(uint64_t startNs, uint64_t endNs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SystemEvent> result;
    for (const auto& e : events_) {
      if (e.timestampNs >= startNs && e.timestampNs <= endNs) {
        result.push_back(e);
      }
    }
    return result;
  }

  std::vector<SystemEvent> EventsByCategory(EventCategory cat, int count = 100) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SystemEvent> result;
    for (int i = static_cast<int>(events_.size()) - 1;
         i >= 0 && static_cast<int>(result.size()) < count; --i) {
      if (events_[i].category == cat) {
        result.push_back(events_[i]);
      }
    }
    return result;
  }

  std::vector<SystemEvent> EventsByProcess(int pid, int count = 100) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SystemEvent> result;
    for (int i = static_cast<int>(events_.size()) - 1;
         i >= 0 && static_cast<int>(result.size()) < count; --i) {
      if (events_[i].processId == pid) {
        result.push_back(events_[i]);
      }
    }
    return result;
  }

  std::vector<SystemEvent> EventsByCorrelation(const std::wstring& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SystemEvent> result;
    for (const auto& e : events_) {
      if (e.correlationId == id) {
        result.push_back(e);
      }
    }
    return result;
  }

  int EventCount(EventCategory cat) const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto& e : events_) {
      if (e.category == cat) ++count;
    }
    return count;
  }

  int EventsPerMinute() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.empty()) return 0;
    uint64_t newest = events_.back().timestampNs;
    uint64_t oldest = events_.front().timestampNs;
    if (newest <= oldest) return 0;
    double minutes = static_cast<double>(newest - oldest) / 60000000000.0;
    if (minutes < 0.001) minutes = 0.001;
    return static_cast<int>(static_cast<double>(events_.size()) / minutes);
  }

  std::map<EventCategory, int> CategoryCounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<EventCategory, int> counts;
    for (const auto& e : events_) {
      counts[e.category]++;
    }
    return counts;
  }

  void SetMaxEvents(int max) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxEvents_ = max;
  }

  std::size_t TotalEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
  }

private:
  struct CategorySub {
    uint64_t id;
    EventCategory category;
    EventCallback callback;
  };

  struct TypeSub {
    uint64_t id;
    std::wstring eventType;
    EventCallback callback;
  };

  struct GlobalSub {
    uint64_t id;
    EventCallback callback;
  };

  std::deque<SystemEvent> events_;
  std::vector<CategorySub> categorySubs_;
  std::vector<TypeSub> typeSubs_;
  std::vector<GlobalSub> globalSubs_;
  uint64_t nextEventId_ = 1;
  uint64_t nextSubId_ = 1;
  int maxEvents_ = 10000;
  mutable std::mutex mutex_;
};

}
