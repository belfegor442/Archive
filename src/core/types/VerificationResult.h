#pragma once

#include <string>
#include <vector>

#include "../enums/IntegrityState.h"

namespace archive::core {

struct VerificationItem {
    std::string id;
    std::string name;
    IntegrityState state = IntegrityState::Unknown;
    std::string expected_checksum;
    std::string actual_checksum;
    std::string details;

    VerificationItem() = default;
};

struct VerificationResult {
    std::vector<VerificationItem> items;
    int valid_count = 0;
    int modified_count = 0;
    int missing_count = 0;
    int corrupted_count = 0;

    VerificationResult() = default;

    [[nodiscard]] bool all_valid() const {
        return modified_count == 0 && missing_count == 0 && corrupted_count == 0;
    }
};

} // namespace archive::core
