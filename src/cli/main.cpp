#include "app/AppConfig.h"
#include "storage/DatabaseManager.h"
#include "storage/ArchiveItemRepository.h"
#include "storage/CategoryRepository.h"
#include "storage/TagRepository.h"
#include "storage/VersionRepository.h"
#include "storage/NoteRepository.h"
#include "storage/ActivityRepository.h"
#include "storage/StoredObjectRepository.h"
#include "filesystem/StorageManager.h"
#include "hashing/FileHasher.h"
#include "services/ImportService.h"
#include "services/VersionService.h"
#include "services/IntegrityService.h"
#include "services/UpdateService.h"
#include "services/SearchService.h"
#include "services/ProjectDetector.h"
#include "services/ActivityService.h"
#include "services/CategoryService.h"
#include "services/TagService.h"
#include "services/NoteService.h"
#include "services/DashboardService.h"
#include "core/utils/Logger.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

static constexpr const char* APP_VERSION = "0.2.0";

using namespace archive;

struct CliContext {
    app::AppConfig config;
    std::string command;
    std::vector<std::string> sub_args;
    std::vector<std::string> positional;
};

static bool parse_args(int argc, char* argv[], CliContext& ctx) {
    ctx.config = app::AppConfig::default_config();

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            ctx.command = "help";
            return true;
        } else if (arg == "--version") {
            ctx.command = "version";
            return true;
        } else if (arg == "--data-dir" && i + 1 < argc) {
            ctx.config.data_dir = argv[++i];
            ctx.config.db_path = ctx.config.data_dir + "/archive.db";
            ctx.config.items_dir = ctx.config.data_dir + "/items";
        } else if (arg == "--type" || arg == "--category" || arg == "--tag"
                || arg == "--version-num" || arg == "--color" || arg == "--limit") {
            ctx.sub_args.push_back(arg);
            if (i + 1 < argc) ctx.sub_args.push_back(argv[++i]);
        } else if (arg == "--favorite") {
            ctx.sub_args.push_back("--favorite");
        } else if (arg[0] != '-' || ctx.command.empty()) {
            if (ctx.command.empty()) {
                ctx.command = arg;
            } else {
                ctx.positional.push_back(arg);
            }
        }
    }

    if (ctx.command.empty()) {
        ctx.command = "help";
    }

    return true;
}

static std::string get_sub_arg(const CliContext& ctx, const std::string& key) {
    for (size_t i = 0; i < ctx.sub_args.size(); i++) {
        if (ctx.sub_args[i] == key && i + 1 < ctx.sub_args.size()) {
            return ctx.sub_args[i + 1];
        }
    }
    return "";
}

static bool has_sub_arg(const CliContext& ctx, const std::string& key) {
    for (const auto& a : ctx.sub_args) {
        if (a == key) return true;
    }
    return false;
}

static void print_usage() {
    std::cout <<
        "Archive — file archiving and version management\n"
        "\n"
        "Usage: archive <command> [options]\n"
        "\n"
        "Item Commands:\n"
        "  import <path...>         Archive files or folders\n"
        "  list                     List all archived items\n"
        "  show <id>                Show details of an item\n"
        "  search <query>           Search items by name/description/path\n"
        "  restore <id>             Restore latest version\n"
        "  restore <id> --version N Restore specific version\n"
        "  trash <id>               Move to trash\n"
        "  untrash <id>             Restore from trash\n"
        "  delete <id>              Permanently delete\n"
        "  rename <id> <name>       Rename an item\n"
        "  favorite <id>            Toggle favorite status\n"
        "\n"
        "Version Commands:\n"
        "  versions <id>            List versions of an item\n"
        "  create-version <id> <path> Create new version from file\n"
        "\n"
        "Tag Commands:\n"
        "  tag create <name>        Create a tag\n"
        "  tag list                 List all tags\n"
        "  tag assign <item> <tag>  Assign tag to item\n"
        "  tag remove <item> <tag>  Remove tag from item\n"
        "\n"
        "Category Commands:\n"
        "  category create <name>   Create a category\n"
        "  category list            List all categories\n"
        "  category assign <item> <cat>  Assign category to item\n"
        "\n"
        "Note Commands:\n"
        "  note add <item> <text>   Add a note to an item\n"
        "  note list <item>         List notes for an item\n"
        "  note remove <note>       Remove a note\n"
        "\n"
        "Integrity Commands:\n"
        "  verify [id]              Verify checksum integrity\n"
        "  consistency              Run deep consistency check\n"
        "\n"
        "Info Commands:\n"
        "  stats                    Show archive statistics\n"
        "  history [limit]          Show recent activity\n"
        "  history item <id>        Show activity for an item\n"
        "\n"
        "Options:\n"
        "  --data-dir <path>        Override data directory\n"
        "  --type <type>            Filter by type (file/folder/project/document)\n"
        "  --category <id>          Filter by category\n"
        "  --tag <name>             Filter by tag name\n"
        "  --favorite               Filter by favorite status\n"
        "  --version-num <N>        Restore specific version number\n"
        "  --color <hex>            Color for tag/category creation\n"
        "  --limit <N>              Limit results\n"
        "  --help                   Show this help\n"
        "\n"
        "Exit Codes:\n"
        "  0  Success\n"
        "  1  General error\n"
        "  2  Invalid arguments\n"
        "  3  Item not found\n"
        "  4  Verification failed\n"
        "  5  Import failed\n"
        "  6  Restore failed\n";
}

static int cmd_import(CliContext& ctx) {
    if (ctx.positional.empty()) {
        std::cerr << "Error: import requires a path\n";
        return 2;
    }
    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::CategoryRepository categories(db);
    storage::TagRepository tags(db);
    storage::ActivityRepository activities(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::ProjectDetector detector;
    services::ImportService svc(db, items, categories, tags, activities, versions, stored, storage, detector);

    int imported = 0, errors = 0;
    for (const auto& path : ctx.positional) {
        core::ImportRequest req;
        req.paths.push_back(path);
        auto result = svc.import(req);
        for (const auto& item : result.items) {
            std::cout << "Imported: " << item.name << " [" << item.id << "] (" << item.size << " bytes)\n";
            imported++;
        }
        for (const auto& err : result.errors) {
            std::cerr << "Error: " << err.path << ": " << err.error << "\n";
            errors++;
        }
    }
    std::cout << "\n" << imported << " item(s) imported";
    if (errors > 0) std::cout << ", " << errors << " error(s)";
    std::cout << "\n";
    return errors > 0 ? 5 : 0;
}

static int cmd_list(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    auto all = items.find_all();
    if (all.empty()) { std::cout << "No archived items.\n"; return 0; }
    std::cout << "Archived items (" << all.size() << "):\n\n";
    for (const auto& item : all) {
        std::cout << "  " << item.id << "  " << item.name
                  << "  [" << core::to_string(item.status) << "]"
                  << "  v" << item.current_version
                  << "  " << item.size << " bytes\n";
    }
    return 0;
}

static int cmd_show(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: show requires an item ID\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    std::cout << "Item: " << item->name << "\n"
              << "  ID:          " << item->id << "\n"
              << "  Type:        " << core::to_string(item->type) << "\n"
              << "  Status:      " << core::to_string(item->status) << "\n"
              << "  Size:        " << item->size << " bytes\n"
              << "  Version:     " << item->current_version << "\n"
              << "  Checksum:    " << item->checksum << "\n"
              << "  Original:    " << item->original_path << "\n"
              << "  Storage:     " << item->storage_path << "\n"
              << "  Created:     " << item->created_at << "\n"
              << "  Archived:    " << item->archived_at << "\n"
              << "  Favorite:    " << (item->is_favorite ? "yes" : "no") << "\n";
    if (item->category_id) {
        storage::CategoryRepository cats(db);
        auto cat = cats.find_by_id(*item->category_id);
        if (cat) std::cout << "  Category:    " << cat->name << "\n";
    }
    if (!item->tags.empty()) {
        std::cout << "  Tags:        ";
        for (size_t i = 0; i < item->tags.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << item->tags[i].name;
        }
        std::cout << "\n";
    }
    auto vers = versions.find_by_item(item->id);
    if (!vers.empty()) {
        std::cout << "  Versions:    " << vers.size() << "\n";
        for (const auto& v : vers) {
            std::cout << "    v" << v.version_number << "  " << v.size << " bytes  " << v.created_at
                      << "  " << v.checksum.substr(0, 16) << "...\n";
        }
    }
    return 0;
}

static int cmd_search(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: search requires a query\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    services::SearchService svc(items);
    services::SearchFilters filters;
    auto type = get_sub_arg(ctx, "--type");
    if (!type.empty()) filters.type = type;
    auto cat = get_sub_arg(ctx, "--category");
    if (!cat.empty()) filters.category_id = cat;
    auto tag = get_sub_arg(ctx, "--tag");
    if (!tag.empty()) filters.tag = tag;
    if (has_sub_arg(ctx, "--favorite")) filters.is_favorite = true;
    std::string query = ctx.positional[0];
    auto result = svc.search(query, filters);
    if (result.items.empty()) {
        std::cout << "No results for '" << query << "'\n";
        return 0;
    }
    std::cout << "Results for '" << query << "' (" << result.total << "):\n\n";
    for (const auto& item : result.items) {
        std::cout << "  " << item.id << "  " << item.name
                  << "  [" << core::to_string(item.type) << "]"
                  << "  " << item.size << " bytes\n";
    }
    return 0;
}

static int cmd_versions(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: versions requires an item ID\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    auto vers = versions.find_by_item(item->id);
    if (vers.empty()) { std::cout << "No versions for " << item->name << "\n"; return 0; }
    std::cout << "Versions of " << item->name << " (" << vers.size() << "):\n\n";
    for (const auto& v : vers) {
        std::cout << "  " << v.id << "  v" << v.version_number
                  << "  " << v.size << " bytes  " << v.created_at << "\n";
        if (!v.notes.empty()) std::cout << "    Notes: " << v.notes << "\n";
    }
    return 0;
}

static int cmd_create_version(CliContext& ctx) {
    if (ctx.positional.size() < 2) { std::cerr << "Error: create-version requires <item_id> <file_path>\n"; return 2; }
    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::VersionService svc(db, versions, items, activities, stored, storage);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    try {
        auto ver = svc.create_version(ctx.positional[0], ctx.positional[1]);
        std::cout << "Created version v" << ver.version_number << " for " << item->name << "\n"
                  << "  ID: " << ver.id << "\n  Size: " << ver.size << " bytes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

static int cmd_restore(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: restore requires an item ID\n"; return 2; }
    ctx.config.ensure_directories();
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::VersionService svc(db, versions, items, activities, stored, storage);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }

    std::shared_ptr<core::Version> target_version;
    auto ver_num_str = get_sub_arg(ctx, "--version-num");
    if (!ver_num_str.empty()) {
        int ver_num = std::stoi(ver_num_str);
        auto vers = versions.find_by_item(item->id);
        for (const auto& v : vers) {
            if (v.version_number == ver_num) { target_version = std::make_shared<core::Version>(v); break; }
        }
        if (!target_version) { std::cerr << "Error: version " << ver_num << " not found\n"; return 3; }
    } else {
        target_version = std::make_shared<core::Version>(*versions.find_latest(item->id));
    }
    try {
        svc.restore(item->id, target_version->id);
        std::cout << "Restored: " << item->name << " (version " << target_version->version_number << ")\n"
                  << "  Checksum: " << target_version->checksum << "\n"
                  << "  Size: " << target_version->size << " bytes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error restoring " << item->name << ": " << e.what() << "\n";
        return 6;
    }
}

static int cmd_trash(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: trash requires an item ID\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::UpdateService svc(db, items, activities, storage);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    svc.move_to_trash(item->id);
    std::cout << "Trashed: " << item->name << "\n";
    return 0;
}

static int cmd_untrash(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: untrash requires an item ID\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::UpdateService svc(db, items, activities, storage);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    svc.restore_from_trash(item->id);
    std::cout << "Restored from trash: " << item->name << "\n";
    return 0;
}

static int cmd_delete(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: delete requires an item ID\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::UpdateService svc(db, items, activities, storage);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    svc.permanent_delete(item->id);
    std::cout << "Deleted: " << item->name << "\n";
    return 0;
}

static int cmd_rename(CliContext& ctx) {
    if (ctx.positional.size() < 2) { std::cerr << "Error: rename requires <id> <new_name>\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::UpdateService svc(db, items, activities, storage);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    svc.update_metadata(ctx.positional[0], ctx.positional[1], item->description);
    std::cout << "Renamed: " << item->name << " -> " << ctx.positional[1] << "\n";
    return 0;
}

static int cmd_favorite(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: favorite requires an item ID\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::UpdateService svc(db, items, activities, storage);
    auto item = items.find_by_id(ctx.positional[0]);
    if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
    svc.toggle_favorite(ctx.positional[0]);
    auto updated = items.find_by_id(ctx.positional[0]);
    std::cout << (updated->is_favorite ? "Favorited: " : "Unfavorited: ") << item->name << "\n";
    return 0;
}

static int cmd_tag(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: tag requires a subcommand (create/list/assign/remove)\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::TagRepository tag_repo(db);
    storage::ArchiveItemRepository items(db);
    services::TagService svc(tag_repo, items);
    std::string sub = ctx.positional[0];
    if (sub == "create") {
        if (ctx.positional.size() < 2) { std::cerr << "Error: tag create requires <name>\n"; return 2; }
        auto color = get_sub_arg(ctx, "--color");
        if (color.empty()) color = "#6366f1";
        auto tag = svc.create(ctx.positional[1], color);
        std::cout << "Created tag: " << tag.name << " [" << tag.id << "] (" << tag.color << ")\n";
    } else if (sub == "list") {
        auto tags = svc.get_all();
        if (tags.empty()) { std::cout << "No tags.\n"; return 0; }
        std::cout << "Tags (" << tags.size() << "):\n\n";
        for (const auto& t : tags) {
            int count = svc.get_item_count(t.id);
            std::cout << "  " << t.id << "  " << t.name << "  " << t.color << "  (" << count << " items)\n";
        }
    } else if (sub == "assign") {
        if (ctx.positional.size() < 3) { std::cerr << "Error: tag assign requires <item_id> <tag_id>\n"; return 2; }
        svc.add_to_item(ctx.positional[1], ctx.positional[2]);
        std::cout << "Tag assigned.\n";
    } else if (sub == "remove") {
        if (ctx.positional.size() < 3) { std::cerr << "Error: tag remove requires <item_id> <tag_id>\n"; return 2; }
        svc.remove_from_item(ctx.positional[1], ctx.positional[2]);
        std::cout << "Tag removed.\n";
    } else {
        std::cerr << "Unknown tag subcommand: " << sub << "\n";
        return 2;
    }
    return 0;
}

static int cmd_category(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: category requires a subcommand (create/list/assign)\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::CategoryRepository cat_repo(db);
    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    services::CategoryService svc(cat_repo, items, activities);
    std::string sub = ctx.positional[0];
    if (sub == "create") {
        if (ctx.positional.size() < 2) { std::cerr << "Error: category create requires <name>\n"; return 2; }
        auto color = get_sub_arg(ctx, "--color");
        if (color.empty()) color = "#6366f1";
        auto cat = svc.create(ctx.positional[1], color);
        std::cout << "Created category: " << cat.name << " [" << cat.id << "] (" << cat.color << ")\n";
    } else if (sub == "list") {
        auto cats = svc.get_all();
        if (cats.empty()) { std::cout << "No categories.\n"; return 0; }
        std::cout << "Categories (" << cats.size() << "):\n\n";
        for (const auto& c : cats) {
            int count = svc.get_item_count(c.id);
            std::cout << "  " << c.id << "  " << c.name << "  " << c.color << "  (" << count << " items)\n";
        }
    } else if (sub == "assign") {
        if (ctx.positional.size() < 3) { std::cerr << "Error: category assign requires <item_id> <category_id>\n"; return 2; }
        items.update_category(ctx.positional[1], ctx.positional[2]);
        std::cout << "Category assigned.\n";
    } else {
        std::cerr << "Unknown category subcommand: " << sub << "\n";
        return 2;
    }
    return 0;
}

static int cmd_note(CliContext& ctx) {
    if (ctx.positional.empty()) { std::cerr << "Error: note requires a subcommand (add/list/remove)\n"; return 2; }
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::NoteRepository note_repo(db);
    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    services::NoteService svc(note_repo, items, activities);
    std::string sub = ctx.positional[0];
    if (sub == "add") {
        if (ctx.positional.size() < 3) { std::cerr << "Error: note add requires <item_id> <text>\n"; return 2; }
        auto item = items.find_by_id(ctx.positional[1]);
        if (!item) { std::cerr << "Error: item not found: " << ctx.positional[1] << "\n"; return 3; }
        auto note = svc.add(ctx.positional[1], ctx.positional[2]);
        std::cout << "Note added: " << note.id << "\n";
    } else if (sub == "list") {
        if (ctx.positional.size() < 2) { std::cerr << "Error: note list requires <item_id>\n"; return 2; }
        auto notes = svc.get_notes(ctx.positional[1]);
        if (notes.empty()) { std::cout << "No notes.\n"; return 0; }
        std::cout << "Notes (" << notes.size() << "):\n\n";
        for (const auto& n : notes) {
            std::cout << "  " << n.id << "  " << n.created_at << "\n    " << n.content << "\n";
        }
    } else if (sub == "remove") {
        if (ctx.positional.size() < 3) { std::cerr << "Error: note remove requires <note_id> <item_id>\n"; return 2; }
        svc.remove(ctx.positional[1], ctx.positional[2]);
        std::cout << "Note removed.\n";
    } else {
        std::cerr << "Unknown note subcommand: " << sub << "\n";
        return 2;
    }
    return 0;
}

static int cmd_verify(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::IntegrityService svc(items, versions, stored, activities, storage);
    core::VerificationResult result;
    if (ctx.positional.empty()) {
        std::cout << "Verifying all items...\n\n";
        result = svc.verify_all();
    } else {
        auto item = items.find_by_id(ctx.positional[0]);
        if (!item) { std::cerr << "Error: item not found: " << ctx.positional[0] << "\n"; return 3; }
        std::cout << "Verifying " << item->name << "...\n\n";
        result = svc.verify_item(ctx.positional[0]);
    }
    for (const auto& vi : result.items) {
        std::string state;
        switch (vi.state) {
            case core::IntegrityState::Valid:    state = "OK"; break;
            case core::IntegrityState::Modified: state = "MODIFIED"; break;
            case core::IntegrityState::Missing:  state = "MISSING"; break;
            case core::IntegrityState::Corrupted: state = "CORRUPTED"; break;
            case core::IntegrityState::Unknown:  state = "UNKNOWN"; break;
        }
        std::cout << "  " << vi.name << " [" << state << "]";
        if (vi.state != core::IntegrityState::Valid) std::cout << " " << vi.details;
        std::cout << "\n";
    }
    std::cout << "\nResults: " << result.valid_count << " valid";
    if (result.modified_count > 0) std::cout << ", " << result.modified_count << " modified";
    if (result.missing_count > 0) std::cout << ", " << result.missing_count << " missing";
    if (result.corrupted_count > 0) std::cout << ", " << result.corrupted_count << " corrupted";
    std::cout << "\n";
    return result.all_valid() ? 0 : 4;
}

static int cmd_consistency(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::IntegrityService svc(items, versions, stored, activities, storage);
    std::cout << "Running consistency check...\n\n";
    auto report = svc.check_consistency();
    if (report.is_clean()) {
        std::cout << "Archive is consistent. No issues found.\n";
        return 0;
    }
    std::cout << "Issues found: " << report.error_count << " error(s), " << report.warning_count << " warning(s)\n\n";
    for (const auto& issue : report.issues) {
        std::string severity = (issue.severity == core::ConsistencyIssue::Severity::Error) ? "ERROR" : "WARN";
        std::cout << "  [" << severity << "] " << issue.details << "\n";
    }
    return report.error_count > 0 ? 4 : 0;
}

static int cmd_stats(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ArchiveItemRepository items(db);
    services::DashboardService svc(items);
    auto stats = svc.get_stats();
    std::cout << "Archive Statistics\n"
              << "==================\n\n"
              << "  Total items:     " << stats.total_items << "\n"
              << "  Archived:        " << stats.archived_items << "\n"
              << "  Favorites:       " << stats.favorite_items << "\n"
              << "  Deleted:         " << stats.deleted_items << "\n"
              << "  Total size:      " << stats.total_size << " bytes\n\n";
    if (!stats.category_counts.empty()) {
        std::cout << "  Categories:\n";
        for (const auto& cc : stats.category_counts) {
            std::cout << "    " << cc.name << " (" << cc.color << "): " << cc.count << " items\n";
        }
        std::cout << "\n";
    }
    if (!stats.type_counts.empty()) {
        std::cout << "  Types:\n";
        for (const auto& tc : stats.type_counts) {
            std::cout << "    " << tc.type << ": " << tc.count << " items\n";
        }
        std::cout << "\n";
    }
    if (!stats.recent_items.empty()) {
        std::cout << "  Recent items:\n";
        for (const auto& item : stats.recent_items) {
            std::cout << "    " << item.name << "  [" << core::to_string(item.type) << "]  " << item.size << " bytes\n";
        }
    }
    return 0;
}

static int cmd_history(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path); db.initialize();
    storage::ActivityRepository act_repo(db);
    storage::ArchiveItemRepository items(db);
    services::ActivityService svc(act_repo);
    if (!ctx.positional.empty() && ctx.positional[0] == "item") {
        if (ctx.positional.size() < 2) { std::cerr << "Error: history item requires <item_id>\n"; return 2; }
        auto acts = svc.get_for_item(ctx.positional[1]);
        auto item = items.find_by_id(ctx.positional[1]);
        std::string name = item ? item->name : ctx.positional[1];
        if (acts.empty()) { std::cout << "No activity for " << name << "\n"; return 0; }
        std::cout << "Activity for " << name << " (" << acts.size() << "):\n\n";
        for (const auto& a : acts) {
            std::cout << "  " << a.created_at << "  " << core::to_string(a.action);
            if (!a.details.empty()) std::cout << "  " << a.details;
            std::cout << "\n";
        }
    } else {
        int limit = 20;
        if (!ctx.positional.empty()) limit = std::stoi(ctx.positional[0]);
        auto acts = svc.get_recent(limit);
        if (acts.empty()) { std::cout << "No activity.\n"; return 0; }
        std::cout << "Recent activity (" << acts.size() << "):\n\n";
        for (const auto& a : acts) {
            auto item = items.find_by_id(a.item_id);
            std::string name = item ? item->name : a.item_id;
            std::cout << "  " << a.created_at << "  " << core::to_string(a.action)
                      << "  " << name;
            if (!a.details.empty()) std::cout << "  " << a.details;
            std::cout << "\n";
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    CliContext ctx;
    if (!parse_args(argc, argv, ctx)) return 2;
    if (ctx.command == "help" || ctx.command == "-h" || ctx.command == "--help") { print_usage(); return 0; }
    if (ctx.command == "version" || ctx.command == "--version") { std::cout << "Archive v" << APP_VERSION << "\n"; return 0; }
    if (ctx.command == "import")         return cmd_import(ctx);
    if (ctx.command == "list")           return cmd_list(ctx);
    if (ctx.command == "show")           return cmd_show(ctx);
    if (ctx.command == "search")         return cmd_search(ctx);
    if (ctx.command == "versions")       return cmd_versions(ctx);
    if (ctx.command == "create-version") return cmd_create_version(ctx);
    if (ctx.command == "restore")        return cmd_restore(ctx);
    if (ctx.command == "trash")          return cmd_trash(ctx);
    if (ctx.command == "untrash")        return cmd_untrash(ctx);
    if (ctx.command == "delete")         return cmd_delete(ctx);
    if (ctx.command == "rename")         return cmd_rename(ctx);
    if (ctx.command == "favorite")       return cmd_favorite(ctx);
    if (ctx.command == "tag")            return cmd_tag(ctx);
    if (ctx.command == "category")       return cmd_category(ctx);
    if (ctx.command == "note")           return cmd_note(ctx);
    if (ctx.command == "verify")         return cmd_verify(ctx);
    if (ctx.command == "consistency")    return cmd_consistency(ctx);
    if (ctx.command == "stats")          return cmd_stats(ctx);
    if (ctx.command == "history")        return cmd_history(ctx);
    std::cerr << "Unknown command: " << ctx.command << "\nRun 'archive help' for usage.\n";
    return 2;
}
