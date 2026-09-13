#pragma once

#include <string>
#include <vector>

#include "../core/types/VerificationResult.h"
#include "../storage/ArchiveItemRepository.h"
#include "../hashing/FileHasher.h"

namespace archive::services {

class IntegrityService {
public:
    explicit IntegrityService(storage::ArchiveItemRepository& items);

    core::VerificationResult verify_all();
    core::VerificationResult verify_item(const std::string& item_id);

private:
    storage::ArchiveItemRepository& items_;
};

} // namespace archive::services
