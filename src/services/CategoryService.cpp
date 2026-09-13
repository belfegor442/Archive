#include "CategoryService.h"

#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace archive::services {

CategoryService::CategoryService(
    storage::CategoryRepository& categories,
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities
) : categories_(categories)
  , items_(items)
  , activities_(activities)
{}

core::Category CategoryService::create(const std::string& name, const std::string& color,
                                        const std::string& description) {
    core::Category cat;
    cat.id = generate_id();
    cat.name = name;
    cat.color = color;
    cat.description = description;
    cat.created_at = now_iso();
    categories_.insert(cat);
    return cat;
}

void CategoryService::update(const std::string& id, const std::string& name, const std::string& color) {
    auto cat = categories_.find_by_id(id);
    if (!cat) return;
    cat->name = name;
    cat->color = color;
    categories_.update(*cat);
}

void CategoryService::remove(const std::string& id) {
    categories_.remove(id);
}

std::vector<core::Category> CategoryService::get_all() {
    return categories_.find_all();
}

std::optional<core::Category> CategoryService::get_by_id(const std::string& id) {
    return categories_.find_by_id(id);
}

int CategoryService::get_item_count(const std::string& category_id) {
    auto items = items_.find_by_category(category_id);
    return static_cast<int>(items.size());
}

std::string CategoryService::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
    std::ostringstream oss;
    oss << std::hex << dis(gen) << dis(gen);
    std::string id = oss.str();
    id.resize(32);
    return id;
}

std::string CategoryService::now_iso() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace archive::services
