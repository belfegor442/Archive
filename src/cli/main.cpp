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

static constexpr const char* APP_VERSION = "0.1.0";
#include "services/SearchService.h"
#include "services/ProjectDetector.h"
#include "core/utils/Logger.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

using namespace archive;

static void print_usage() {
    std::cout <<
        "Archive — file archiving and version management\n"
        "\n"
        "Usage: archive <command> [options]\n"
        "\n"
        "Commands:\n"
        "  import <path>       Archive a file or folder\n"
        "  list                List all archived items\n"
        "  show <id>           Show details of an archived item\n"
        "  versions <id>       List versions of an item\n"
        "  verify [id]         Verify integrity (all items or specific item)\n"
        "  restore <id>        Restore an item to its original location\n"
        "  trash <id>          Move an item to trash\n"
        "  untrash <id>        Restore an item from trash\n"
        "  delete <id>         Permanently delete an item\n"
        "  help                Show this help message\n"
        "\n"
        "Options:\n"
        "  --data-dir <path>   Override data directory (default: ~/.archive-data)\n"
        "  --help              Show this help message\n"
        "\n"
        "Examples:\n"
        "  archive import ./my-project\n"
        "  archive import ./document.pdf\n"
        "  archive list\n"
        "  archive show abc123\n"
        "  archive verify\n"
        "  archive restore abc123\n"
        "\n"
        "Exit codes:\n"
        "  0   Success\n"
        "  1   General error\n"
        "  2   Invalid arguments\n"
        "  3   Item not found\n"
        "  4   Verification failed\n"
        "  5   Import failed\n"
        "  6   Restore failed\n";
}

static void print_version() {
    std::cout << "Archive v" << APP_VERSION << "\n";
}

struct CliContext {
    app::AppConfig config;
    std::string command;
    std::vector<std::string> args;
};

static bool parse_args(int argc, char* argv[], CliContext& ctx) {
    ctx.config = app::AppConfig::default_config();

    std::vector<std::string> positional;
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
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        ctx.command = "help";
        return true;
    }

    ctx.command = positional[0];
    ctx.args = std::vector<std::string>(positional.begin() + 1, positional.end());
    return true;
}

static int cmd_import(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: import requires a path\n";
        std::cerr << "Usage: archive import <path>\n";
        return 2;
    }

    ctx.config.ensure_directories();

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::CategoryRepository categories(db);
    storage::TagRepository tags(db);
    storage::ActivityRepository activities(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored_objects(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);
    services::ProjectDetector detector;

    services::ImportService import_service(db, items, categories, tags, activities,
                                           versions, stored_objects, storage, detector);

    int total_imported = 0;
    int total_errors = 0;

    for (const auto& path : ctx.args) {
        core::ImportRequest req;
        req.paths.push_back(path);

        auto result = import_service.import(req);

        for (const auto& item : result.items) {
            std::cout << "Imported: " << item.name
                      << " [" << item.id << "]"
                      << " (" << item.size << " bytes)\n";
            total_imported++;
        }

        for (const auto& err : result.errors) {
            std::cerr << "Error importing " << err.path << ": " << err.error << "\n";
            total_errors++;
        }
    }

    std::cout << "\n" << total_imported << " item(s) imported";
    if (total_errors > 0) {
        std::cout << ", " << total_errors << " error(s)";
    }
    std::cout << "\n";

    return total_errors > 0 ? 5 : 0;
}

static int cmd_list(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    auto all = items.find_all();

    if (all.empty()) {
        std::cout << "No archived items.\n";
        return 0;
    }

    std::cout << "Archived items (" << all.size() << "):\n\n";
    for (const auto& item : all) {
        std::string status_str = core::to_string(item.status);

        std::cout << "  " << item.id
                  << "  " << item.name
                  << "  [" << status_str << "]"
                  << "  v" << item.current_version
                  << "  " << item.size << " bytes"
                  << "\n";
    }

    return 0;
}

static int cmd_show(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: show requires an item ID\n";
        std::cerr << "Usage: archive show <id>\n";
        return 2;
    }

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);

    auto item = items.find_by_id(ctx.args[0]);
    if (!item) {
        std::cerr << "Error: item not found: " << ctx.args[0] << "\n";
        return 3;
    }

    std::string type_str;
    switch (item->type) {
        case core::ItemType::File:    type_str = "File";    break;
        case core::ItemType::Folder:  type_str = "Folder";  break;
        case core::ItemType::Project: type_str = "Project"; break;
        case core::ItemType::Document: type_str = "Document"; break;
    }

    std::string status_str = core::to_string(item->status);

    std::cout << "Item: " << item->name << "\n"
              << "  ID:          " << item->id << "\n"
              << "  Type:        " << type_str << "\n"
              << "  Status:      " << status_str << "\n"
              << "  Size:        " << item->size << " bytes\n"
              << "  Version:     " << item->current_version << "\n"
              << "  Checksum:    " << item->checksum << "\n"
              << "  Original:    " << item->original_path << "\n"
              << "  Storage:     " << item->storage_path << "\n"
              << "  Created:     " << item->created_at << "\n"
              << "  Archived:    " << item->archived_at << "\n";

    auto vers = versions.find_by_item(item->id);
    if (!vers.empty()) {
        std::cout << "  Versions:    " << vers.size() << "\n";
        for (const auto& v : vers) {
            std::cout << "    v" << v.version_number
                      << "  " << v.size << " bytes"
                      << "  " << v.created_at
                      << "  " << v.checksum.substr(0, 16) << "..."
                      << "\n";
        }
    }

    return 0;
}

static int cmd_versions(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: versions requires an item ID\n";
        std::cerr << "Usage: archive versions <id>\n";
        return 2;
    }

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);

    auto item = items.find_by_id(ctx.args[0]);
    if (!item) {
        std::cerr << "Error: item not found: " << ctx.args[0] << "\n";
        return 3;
    }

    auto vers = versions.find_by_item(item->id);
    if (vers.empty()) {
        std::cout << "No versions for " << item->name << "\n";
        return 0;
    }

    std::cout << "Versions of " << item->name << " (" << vers.size() << "):\n\n";
    for (const auto& v : vers) {
        std::cout << "  " << v.id
                  << "  v" << v.version_number
                  << "  " << v.size << " bytes"
                  << "  " << v.created_at
                  << "\n";
        if (!v.notes.empty()) {
            std::cout << "    Notes: " << v.notes << "\n";
        }
    }

    return 0;
}

static int cmd_verify(CliContext& ctx) {
    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored_objects(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);

    services::IntegrityService integrity(items, versions, stored_objects, activities, storage);

    core::VerificationResult result;

    if (ctx.args.empty()) {
        std::cout << "Verifying all items...\n\n";
        result = integrity.verify_all();
    } else {
        auto item = items.find_by_id(ctx.args[0]);
        if (!item) {
            std::cerr << "Error: item not found: " << ctx.args[0] << "\n";
            return 3;
        }
        std::cout << "Verifying " << item->name << "...\n\n";
        result = integrity.verify_item(ctx.args[0]);
    }

    for (const auto& vi : result.items) {
        std::string state_str;
        switch (vi.state) {
            case core::IntegrityState::Valid:    state_str = "OK"; break;
            case core::IntegrityState::Modified: state_str = "MODIFIED"; break;
            case core::IntegrityState::Missing:  state_str = "MISSING"; break;
            case core::IntegrityState::Corrupted: state_str = "CORRUPTED"; break;
            case core::IntegrityState::Unknown:  state_str = "UNKNOWN"; break;
        }

        std::cout << "  " << vi.name << " [" << state_str << "]";
        if (vi.state != core::IntegrityState::Valid) {
            std::cout << " " << vi.details;
        }
        std::cout << "\n";
    }

    std::cout << "\nResults: " << result.valid_count << " valid";
    if (result.modified_count > 0) std::cout << ", " << result.modified_count << " modified";
    if (result.missing_count > 0) std::cout << ", " << result.missing_count << " missing";
    if (result.corrupted_count > 0) std::cout << ", " << result.corrupted_count << " corrupted";
    std::cout << "\n";

    return result.all_valid() ? 0 : 4;
}

static int cmd_restore(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: restore requires an item ID\n";
        std::cerr << "Usage: archive restore <id>\n";
        return 2;
    }

    ctx.config.ensure_directories();

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::VersionRepository versions(db);
    storage::StoredObjectRepository stored_objects(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);

    services::VersionService version_service(db, versions, items, activities, stored_objects, storage);

    auto item = items.find_by_id(ctx.args[0]);
    if (!item) {
        std::cerr << "Error: item not found: " << ctx.args[0] << "\n";
        return 3;
    }

    auto latest = versions.find_latest(item->id);
    if (!latest) {
        std::cerr << "Error: no versions available for " << item->name << "\n";
        return 6;
    }

    try {
        version_service.restore(item->id, latest->id);
        std::cout << "Restored: " << item->name
                  << " (version " << latest->version_number << ")\n"
                  << "  Checksum: " << latest->checksum << "\n"
                  << "  Size: " << latest->size << " bytes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error restoring " << item->name << ": " << e.what() << "\n";
        return 6;
    }
}

static int cmd_trash(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: trash requires an item ID\n";
        std::cerr << "Usage: archive trash <id>\n";
        return 2;
    }

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);

    services::UpdateService update(items, activities, storage);

    auto item = items.find_by_id(ctx.args[0]);
    if (!item) {
        std::cerr << "Error: item not found: " << ctx.args[0] << "\n";
        return 3;
    }

    try {
        update.move_to_trash(item->id);
        std::cout << "Trashed: " << item->name << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error trashing " << item->name << ": " << e.what() << "\n";
        return 1;
    }
}

static int cmd_untrash(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: untrash requires an item ID\n";
        std::cerr << "Usage: archive untrash <id>\n";
        return 2;
    }

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);

    services::UpdateService update(items, activities, storage);

    auto item = items.find_by_id(ctx.args[0]);
    if (!item) {
        std::cerr << "Error: item not found: " << ctx.args[0] << "\n";
        return 3;
    }

    try {
        update.restore_from_trash(item->id);
        std::cout << "Restored from trash: " << item->name << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error restoring " << item->name << ": " << e.what() << "\n";
        return 1;
    }
}

static int cmd_delete(CliContext& ctx) {
    if (ctx.args.empty()) {
        std::cerr << "Error: delete requires an item ID\n";
        std::cerr << "Usage: archive delete <id>\n";
        return 2;
    }

    storage::DatabaseManager db(ctx.config.db_path);
    db.initialize();

    storage::ArchiveItemRepository items(db);
    storage::ActivityRepository activities(db);
    filesystem::StorageManager storage(ctx.config.data_dir, ctx.config.items_dir);

    services::UpdateService update(items, activities, storage);

    auto item = items.find_by_id(ctx.args[0]);
    if (!item) {
        std::cerr << "Error: item not found: " << ctx.args[0] << "\n";
        return 3;
    }

    try {
        update.permanent_delete(item->id);
        std::cout << "Deleted: " << item->name << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting " << item->name << ": " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char* argv[]) {
    CliContext ctx;

    if (!parse_args(argc, argv, ctx)) {
        return 2;
    }

    if (ctx.command == "help" || ctx.command == "-h" || ctx.command == "--help") {
        print_usage();
        return 0;
    }

    if (ctx.command == "version" || ctx.command == "--version") {
        print_version();
        return 0;
    }

    if (ctx.command == "import")     return cmd_import(ctx);
    if (ctx.command == "list")       return cmd_list(ctx);
    if (ctx.command == "show")       return cmd_show(ctx);
    if (ctx.command == "versions")   return cmd_versions(ctx);
    if (ctx.command == "verify")     return cmd_verify(ctx);
    if (ctx.command == "restore")    return cmd_restore(ctx);
    if (ctx.command == "trash")      return cmd_trash(ctx);
    if (ctx.command == "untrash")    return cmd_untrash(ctx);
    if (ctx.command == "delete")     return cmd_delete(ctx);

    std::cerr << "Unknown command: " << ctx.command << "\n";
    std::cerr << "Run 'archive help' for usage.\n";
    return 2;
}
