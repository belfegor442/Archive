#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class IntegrityState {
    Valid,
    Modified,
    Missing,
    Corrupted,
    Unknown
};

inline std::string to_string(IntegrityState state) {
    switch (state) {
        case IntegrityState::Valid:     return "valid";
        case IntegrityState::Modified:  return "modified";
        case IntegrityState::Missing:   return "missing";
        case IntegrityState::Corrupted: return "corrupted";
        case IntegrityState::Unknown:   return "unknown";
    }
    throw std::invalid_argument("Unknown IntegrityState");
}

inline IntegrityState integrity_state_from_string(const std::string& s) {
    if (s == "valid")     return IntegrityState::Valid;
    if (s == "modified")  return IntegrityState::Modified;
    if (s == "missing")   return IntegrityState::Missing;
    if (s == "corrupted") return IntegrityState::Corrupted;
    return IntegrityState::Unknown;
}

} // namespace archive::core
