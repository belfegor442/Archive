#include "EventSerializer.hpp"

#include <sstream>
#include <iomanip>
#include <chrono>

namespace monix::events {

namespace {

void writeIndent(std::ostringstream& ss, int depth) {
  for (int i = 0; i < depth; ++i) ss << "  ";
}

void writeStr(std::ostringstream& ss, const std::string& key,
              const std::string& val, int d, bool last = true) {
  writeIndent(ss, d);
  ss << "\"" << EventSerializer::escapeJsonString(key) << "\": \""
     << EventSerializer::escapeJsonString(val) << "\""
     << (last ? "" : ",") << "\n";
}

void writeU64(std::ostringstream& ss, const std::string& key,
              std::uint64_t val, int d, bool last = true) {
  writeIndent(ss, d);
  ss << "\"" << EventSerializer::escapeJsonString(key) << "\": " << val
     << (last ? "" : ",") << "\n";
}

void writeBool(std::ostringstream& ss, const std::string& key,
               bool val, int d, bool last = true) {
  writeIndent(ss, d);
  ss << "\"" << EventSerializer::escapeJsonString(key) << "\": "
     << (val ? "true" : "false")
     << (last ? "" : ",") << "\n";
}

void writeVal(std::ostringstream& ss, const PayloadValue& val) {
  std::visit([&](const auto& v) {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, std::string>) {
      ss << "\"" << EventSerializer::escapeJsonString(v) << "\"";
    } else if constexpr (std::is_same_v<T, bool>) {
      ss << (v ? "true" : "false");
    } else if constexpr (std::is_same_v<T, double>) {
      ss << std::fixed << std::setprecision(6) << v;
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
      ss << v;
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
      ss << v;
    } else if constexpr (std::is_same_v<T, Timestamp>) {
      ss << v;
    } else {
      ss << "null";
    }
  }, val);
}

}  // namespace

std::string EventSerializer::escapeJsonString(const std::string& s) {
  std::ostringstream out;
  for (char c : s) {
    switch (c) {
      case '\"':  out << "\\\""; break;
      case '\\':  out << "\\\\"; break;
      case '\n':  out << "\\n"; break;
      case '\r':  out << "\\r"; break;
      case '\t':  out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(c));
        } else {
          out << c;
        }
    }
  }
  return out.str();
}

std::string EventSerializer::timestampToJson(Timestamp ts) {
  auto tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts));
  auto time_t_val = std::chrono::system_clock::to_time_t(tp);
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &time_t_val);
#else
  gmtime_r(&time_t_val, &utc);
#endif
  auto ms = ts % 1000;
  std::ostringstream ss;
  ss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << "."
     << std::setw(3) << std::setfill('0') << ms << "Z";
  return ss.str();
}

std::string EventSerializer::serialize(const Event& event) {
  std::string output;
  EventErrorDetail err = serializeTo(event, output);
  if (err.hasError()) return "{}";
  return output;
}

EventErrorDetail EventSerializer::serializeTo(const Event& event, std::string& output) {
  if (!event.id.isValid())
    return EventErrorDetail(EventError::InvalidEventId, "Event ID is invalid");
  if (!event.type.isValid())
    return EventErrorDetail(EventError::MissingEventType, "Event type is invalid");

  std::ostringstream ss;
  int d = 0;
  const std::string ind = "  ";

  ss << "{\n";
  ++d;
  writeIndent(ss, d);
  ss << "\"schema\": {\n";
  ++d;
  writeStr(ss, "name", event.provenance.schema_name, d, false);
  writeU64(ss, "version", event.provenance.schema_version, d);
  --d;
  writeIndent(ss, d);
  ss << "},\n";

  writeStr(ss, "id", event.id.toString(), d, false);
  writeU64(ss, "sequence", event.sequence, d, false);

  ss << "\"time\": {\n";
  ++d;
  writeStr(ss, "occurrence", timestampToJson(event.time.occurrence), d, false);
  writeStr(ss, "ingestion", timestampToJson(event.time.ingestion), d);
  --d;
  writeIndent(ss, d);
  ss << "},\n";

  ss << "\"type\": {\n";
  ++d;
  writeStr(ss, "namespace", event.type.namespace_name, d, false);
  writeStr(ss, "name", event.type.name, d, false);
  writeU64(ss, "id", event.type.numeric_id, d);
  --d;
  writeIndent(ss, d);
  ss << "},\n";

  writeStr(ss, "severity", EventSeverityName(event.severity), d, false);

  ss << "\"source\": {\n";
  ++d;
  writeStr(ss, "id", event.source.id, d, false);
  writeStr(ss, "name", event.source.name, d, false);
  writeStr(ss, "version", event.source.version, d, false);
  writeStr(ss, "kind", SourceKindName(event.source.kind), d);
  --d;
  writeIndent(ss, d);
  ss << "},\n";

  if (event.actor) {
    ss << "\"actor\": {\n";
    ++d;
    writeStr(ss, "id", event.actor->id, d, false);
    writeStr(ss, "kind", ActorKindName(event.actor->kind), d, false);
    if (event.actor->name) writeStr(ss, "name", *event.actor->name, d, false);
    if (event.actor->username) writeStr(ss, "username", *event.actor->username, d, false);
    if (event.actor->process_id) {
      writeIndent(ss, d);
      ss << "\"process_id\": " << *event.actor->process_id << ",\n";
    }
    writeBool(ss, "_has_session", event.actor->session_id.has_value(), d);
    --d;
    writeIndent(ss, d);
    ss << "},\n";
  }

  if (event.entity) {
    ss << "\"entity\": {\n";
    ++d;
    writeStr(ss, "id", event.entity->id, d, false);
    writeStr(ss, "kind", EntityKindName(event.entity->kind), d, false);
    if (event.entity->name) writeStr(ss, "name", *event.entity->name, d);
    --d;
    writeIndent(ss, d);
    ss << "},\n";
  }

  if (event.action) {
    ss << "\"action\": {\n";
    ++d;
    writeStr(ss, "name", event.action->name, d);
    --d;
    writeIndent(ss, d);
    ss << "},\n";
  }

  if (event.correlation.hasAny()) {
    ss << "\"correlation\": {\n";
    ++d;
    bool first = true;
    if (event.correlation.correlation_id) {
      writeStr(ss, "correlation_id", *event.correlation.correlation_id, d, false);
      first = false;
    }
    if (event.correlation.parent_event_id) {
      writeStr(ss, "parent_event_id", event.correlation.parent_event_id->toString(), d, false);
      first = false;
    }
    if (event.correlation.activity_id) {
      writeStr(ss, "activity_id", *event.correlation.activity_id, d, first);
      first = false;
    }
    --d;
    writeIndent(ss, d);
    ss << "},\n";
  }

  ss << "\"provenance\": {\n";
  ++d;
  writeStr(ss, "source_id", event.provenance.source_id, d, false);
  writeStr(ss, "collector", event.provenance.collector_name, d, false);
  writeStr(ss, "collector_version", event.provenance.collector_version, d, false);
  writeStr(ss, "kind", ProvenanceKindName(event.provenance.kind), d);
  --d;
  writeIndent(ss, d);
  ss << "},\n";

  if (event.flags != EventFlags::None) {
    writeU64(ss, "flags", static_cast<std::uint32_t>(event.flags), d, false);
  }

  ss << "\"payload\": {\n";
  ++d;
  for (std::size_t i = 0; i < event.payload.size(); ++i) {
    writeIndent(ss, d);
    ss << "\"" << escapeJsonString(event.payload.entries[i].first) << "\": ";
    writeVal(ss, event.payload.entries[i].second);
    ss << (i + 1 < event.payload.size() ? "," : "") << "\n";
  }
  --d;
  writeIndent(ss, d);
  ss << "},\n";

  ss << "\"metadata\": {\n";
  ++d;
  for (std::size_t i = 0; i < event.metadata.size(); ++i) {
    writeIndent(ss, d);
    ss << "\"" << escapeJsonString(event.metadata.entries[i].first) << "\": ";
    writeVal(ss, event.metadata.entries[i].second);
    ss << (i + 1 < event.metadata.size() ? "," : "") << "\n";
  }
  --d;
  writeIndent(ss, d);
  ss << "}\n";

  --d;
  ss << "}\n";

  output = ss.str();
  return EventErrorDetail();
}

}  // namespace monix::events
