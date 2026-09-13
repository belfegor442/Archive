#include "EventDeserializer.hpp"

#include <vector>
#include <map>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace monix::events {

namespace {

struct JsonValue {
  enum Type { Null, Bool, Number, String, Array, Object };
  Type type = Null;
  bool boolVal = false;
  double numVal = 0.0;
  std::string strVal;
  std::vector<JsonValue> arrVal;
  std::map<std::string, JsonValue> objVal;

  const JsonValue* find(const std::string& key) const {
    if (type != Object) return nullptr;
    auto it = objVal.find(key);
    return it != objVal.end() ? &it->second : nullptr;
  }

  const char* str(const std::string& key, const char* def = "") const {
    auto* v = find(key);
    if (!v || v->type != String) return def;
    return v->strVal.c_str();
  }

  double num(const std::string& key, double def = 0.0) const {
    auto* v = find(key);
    if (!v || v->type != Number) return def;
    return v->numVal;
  }

  bool has(const std::string& key) const {
    return find(key) != nullptr;
  }
};

struct JsonParser {
  const char* data;
  std::size_t len;
  std::size_t pos = 0;

  void skipWs() {
    while (pos < len && (data[pos] == ' ' || data[pos] == '\n' ||
           data[pos] == '\r' || data[pos] == '\t')) ++pos;
  }

  char peek() { skipWs(); return pos < len ? data[pos] : '\0'; }
  char next() { skipWs(); return pos < len ? data[pos++] : '\0'; }

  bool match(const char* s) {
    skipWs();
    std::size_t slen = std::strlen(s);
    if (pos + slen > len) return false;
    if (std::memcmp(data + pos, s, slen) != 0) return false;
    pos += slen;
    return true;
  }

  std::string parseString() {
    if (next() != '\"') return "";
    std::string result;
    while (pos < len && data[pos] != '\"') {
      if (data[pos] == '\\') {
        ++pos;
        if (pos >= len) break;
        switch (data[pos]) {
          case '\"': result += '\"'; break;
          case '\\': result += '\\'; break;
          case '/':  result += '/';  break;
          case 'n':  result += '\n'; break;
          case 'r':  result += '\r'; break;
          case 't':  result += '\t'; break;
          case 'b':  result += '\b'; break;
          case 'f':  result += '\f'; break;
          case 'u': {
            if (pos + 4 < len) {
              char hex[5] = {data[pos+1], data[pos+2], data[pos+3], data[pos+4], 0};
              unsigned int cp = static_cast<unsigned int>(std::strtoul(hex, nullptr, 16));
              pos += 4;
              if (cp < 0x80) result += static_cast<char>(cp);
              else if (cp < 0x800) {
                result += static_cast<char>(0xC0 | (cp >> 6));
                result += static_cast<char>(0x80 | (cp & 0x3F));
              } else {
                result += static_cast<char>(0xE0 | (cp >> 12));
                result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (cp & 0x3F));
              }
            }
            break;
          }
          default: result += data[pos]; break;
        }
      } else {
        result += data[pos];
      }
      ++pos;
    }
    if (pos < len) ++pos;
    return result;
  }

  JsonValue parseValue() {
    char c = peek();
    if (c == '\"') {
      JsonValue v;
      v.type = JsonValue::String;
      v.strVal = parseString();
      return v;
    }
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == 't' || c == 'f') {
      JsonValue v;
      v.type = JsonValue::Bool;
      v.boolVal = match("true");
      if (!v.boolVal) match("false");
      return v;
    }
    if (c == 'n') {
      match("null");
      return JsonValue{};
    }
    JsonValue v;
    v.type = JsonValue::Number;
    std::size_t start = pos;
    skipWs();
    if (pos < len && data[pos] == '-') ++pos;
    while (pos < len && data[pos] >= '0' && data[pos] <= '9') ++pos;
    if (pos < len && data[pos] == '.') {
      ++pos;
      while (pos < len && data[pos] >= '0' && data[pos] <= '9') ++pos;
    }
    if (pos < len && (data[pos] == 'e' || data[pos] == 'E')) {
      ++pos;
      if (pos < len && (data[pos] == '+' || data[pos] == '-')) ++pos;
      while (pos < len && data[pos] >= '0' && data[pos] <= '9') ++pos;
    }
    v.numVal = std::strtod(data + start, nullptr);
    return v;
  }

  JsonValue parseArray() {
    JsonValue v;
    v.type = JsonValue::Array;
    next();
    if (peek() == ']') { ++pos; return v; }
    while (true) {
      v.arrVal.push_back(parseValue());
      if (peek() == ',') { ++pos; continue; }
      break;
    }
    next();
    return v;
  }

  JsonValue parseObject() {
    JsonValue v;
    v.type = JsonValue::Object;
    next();
    if (peek() == '}') { ++pos; return v; }
    while (true) {
      std::string key = parseString();
      next();
      v.objVal[key] = parseValue();
      if (peek() == ',') { ++pos; continue; }
      break;
    }
    next();
    return v;
  }
};

EventErrorDetail parseEventFromJson(const JsonValue& root, Event& out) {
  if (root.type != JsonValue::Object)
    return EventErrorDetail(EventError::DeserializationFailed, "Root is not an object");

  auto* idStr = root.str("id", nullptr);
  if (!idStr) return EventErrorDetail(EventError::InvalidEventId, "Missing id");
  out.id = EventId::fromString(idStr);
  if (!out.id.isValid())
    return EventErrorDetail(EventError::InvalidEventId, "Invalid id format");

  out.sequence = static_cast<EventSequence>(root.num("sequence", 0));

  auto* timeObj = root.find("time");
  if (timeObj && timeObj->type == JsonValue::Object) {
    out.time.occurrence = static_cast<Timestamp>(timeObj->num("occurrence", 0));
    out.time.ingestion = static_cast<Timestamp>(timeObj->num("ingestion", 0));
  }

  auto* typeObj = root.find("type");
  if (typeObj && typeObj->type == JsonValue::Object) {
    out.type.namespace_name = typeObj->str("namespace", "");
    out.type.name = typeObj->str("name", "");
    out.type.numeric_id = static_cast<std::uint32_t>(typeObj->num("id", 0));
  }
  if (!out.type.isValid())
    return EventErrorDetail(EventError::MissingEventType, "Missing or invalid type");

  out.severity = EventSeverityFromString(root.str("severity", "info"));

  auto* srcObj = root.find("source");
  if (srcObj && srcObj->type == JsonValue::Object) {
    out.source.id = srcObj->str("id", "");
    out.source.name = srcObj->str("name", "");
    out.source.version = srcObj->str("version", "");
    out.source.kind = SourceKindFromString(srcObj->str("kind", "unknown"));
  }
  if (!out.source.isValid())
    return EventErrorDetail(EventError::MissingSource, "Missing or invalid source");

  auto* actorObj = root.find("actor");
  if (actorObj && actorObj->type == JsonValue::Object) {
    ActorRef a;
    a.id = actorObj->str("id", "");
    a.kind = ActorKindFromString(actorObj->str("kind", "unknown"));
    if (actorObj->has("name")) a.name = actorObj->str("name");
    if (actorObj->has("username")) a.username = actorObj->str("username");
    if (actorObj->has("process_id"))
      a.process_id = static_cast<ProcessId>(actorObj->num("process_id", 0));
    out.actor = std::move(a);
  }

  auto* entObj = root.find("entity");
  if (entObj && entObj->type == JsonValue::Object) {
    EntityRef e;
    e.id = entObj->str("id", "");
    e.kind = EntityKindFromString(entObj->str("kind", "unknown"));
    if (entObj->has("name")) e.name = entObj->str("name");
    out.entity = std::move(e);
  }

  auto* actObj = root.find("action");
  if (actObj && actObj->type == JsonValue::Object) {
    Action a;
    a.name = actObj->str("name", "");
    out.action = std::move(a);
  }

  auto* provObj = root.find("provenance");
  if (provObj && provObj->type == JsonValue::Object) {
    out.provenance.source_id = provObj->str("source_id", "");
    out.provenance.collector_name = provObj->str("collector", "");
    out.provenance.collector_version = provObj->str("collector_version", "");
    out.provenance.kind = ProvenanceKindFromString(provObj->str("kind", "native_observation"));
  }
  if (!out.provenance.isValid())
    return EventErrorDetail(EventError::MissingProvenance, "Missing or invalid provenance");

  auto* payloadObj = root.find("payload");
  if (payloadObj && payloadObj->type == JsonValue::Object) {
    for (const auto& [k, v] : payloadObj->objVal) {
      switch (v.type) {
        case JsonValue::String:  out.payload.set(k, v.strVal); break;
        case JsonValue::Number:  out.payload.set(k, static_cast<std::int64_t>(v.numVal)); break;
        case JsonValue::Bool:    out.payload.set(k, v.boolVal); break;
        default: break;
      }
    }
  }

  auto* metaObj = root.find("metadata");
  if (metaObj && metaObj->type == JsonValue::Object) {
    for (const auto& [k, v] : metaObj->objVal) {
      switch (v.type) {
        case JsonValue::String:  out.metadata.set(k, v.strVal); break;
        case JsonValue::Number:  out.metadata.set(k, static_cast<std::int64_t>(v.numVal)); break;
        case JsonValue::Bool:    out.metadata.set(k, v.boolVal); break;
        default: break;
      }
    }
  }

  auto* corrObj = root.find("correlation");
  if (corrObj && corrObj->type == JsonValue::Object) {
    if (corrObj->has("correlation_id"))
      out.correlation.correlation_id = corrObj->str("correlation_id");
    if (corrObj->has("parent_event_id"))
      out.correlation.parent_event_id = EventId::fromString(corrObj->str("parent_event_id"));
    if (corrObj->has("activity_id"))
      out.correlation.activity_id = corrObj->str("activity_id");
  }

  auto* flagsVal = root.find("flags");
  if (flagsVal && flagsVal->type == JsonValue::Number)
    out.flags = static_cast<EventFlags>(static_cast<std::uint32_t>(flagsVal->numVal));

  return EventErrorDetail();
}

}  // namespace

std::variant<Event, EventErrorDetail> EventDeserializer::deserialize(const std::string& json) {
  return deserialize(json.data(), json.size());
}

std::variant<Event, EventErrorDetail> EventDeserializer::deserialize(const char* data, std::size_t len) {
  if (!data || len == 0)
    return EventErrorDetail(EventError::DeserializationFailed, "Empty input");

  JsonParser parser{data, len};
  JsonValue root = parser.parseValue();

  if (root.type != JsonValue::Object)
    return EventErrorDetail(EventError::DeserializationFailed, "Root is not a JSON object");

  Event event;
  EventErrorDetail err = parseEventFromJson(root, event);
  if (err.hasError()) return err;
  return std::move(event);
}

}  // namespace monix::events
