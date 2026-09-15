#include "test_helpers.h"

#include "storage/DatabaseManager.h"
#include "storage/OrgPlanRepository.h"
#include "storage/OrgMoveRepository.h"
#include "storage/UndoRepository.h"
#include "services/OrganizationExecutor.h"

#include <fstream>
#include <filesystem>

using namespace archive::storage;
using namespace archive::services;
using namespace archive::core;

static const std::string TEST_BASE = "executor_test";

static void cleanup() {
    std::error_code ec;
    std::filesystem::remove_all(TEST_BASE, ec);
}

static void create_test_file(const std::string& path, const std::string& content) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

struct TestDB {
    DatabaseManager db;
    TestDB() : db(":memory:") {
        db.initialize();
        db.execute("PRAGMA foreign_keys=OFF");
    }
};
static void ensure_scan(DatabaseManager& db, const std::string& scan_id) {
    db.execute("INSERT OR IGNORE INTO scans (id, root_path, status) VALUES ('" + scan_id + "', '/fake', 'completed');");
}

static void ensure_plan(DatabaseManager& db, const std::string& plan_id, const std::string& scan_id) {
    db.execute("INSERT OR IGNORE INTO org_plans (id, scan_id, root_path, status) VALUES ('" + plan_id + "', '" + scan_id + "', '/fake', 'draft');");
}


static void setup_exec_environment() {
    cleanup();
    create_test_file(TEST_BASE + "/source/main.cpp", "int main() { return 0; }");
    create_test_file(TEST_BASE + "/source/script.py", "print('hello')");
    create_test_file(TEST_BASE + "/source/data.json", "{\"key\": \"value\"}");
}

// ============================================================
// OrganizationExecutor tests
// ============================================================

static void test_execute_moves_files() {
    TEST_BEGIN("execute moves files to correct locations");
    setup_exec_environment();
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = TEST_BASE + "/source";
    plan.status = PlanStatus::Ready;
    plan.total_files = 1;
    plan.moves_planned = 1;
    plan_repo.insert(plan);

    OrgMove move;
    move.id = "move-001";
    move.plan_id = "plan-001";
    move.source_path = TEST_BASE + "/source/main.cpp";
    move.dest_path = TEST_BASE + "/dest/Development/C++/main.cpp";
    move.confidence = 0.95;
    move.status = MoveStatus::Planned;
    move_repo.insert(move);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute("plan-001");

    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/dest/Development/C++/main.cpp"));
    ASSERT_TRUE(!std::filesystem::exists(TEST_BASE + "/source/main.cpp"));
    cleanup();
    TEST_PASS();
}

static void test_execute_creates_undo_record() {
    TEST_BEGIN("execute creates undo record");
    setup_exec_environment();
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = TEST_BASE + "/source";
    plan.status = PlanStatus::Ready;
    plan.total_files = 1;
    plan.moves_planned = 1;
    plan_repo.insert(plan);

    OrgMove move;
    move.id = "move-001";
    move.plan_id = "plan-001";
    move.source_path = TEST_BASE + "/source/script.py";
    move.dest_path = TEST_BASE + "/dest/Development/Python/script.py";
    move.confidence = 0.9;
    move.status = MoveStatus::Planned;
    move_repo.insert(move);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute("plan-001");

    ASSERT_TRUE(!undo.id.empty());
    ASSERT_EQ(undo.status, UndoStatus::Available);
    ASSERT_EQ(undo.moves_count, 1);

    auto entries = undo_repo.find_entries(undo.id);
    ASSERT_EQ(entries.size(), 1u);
    cleanup();
    TEST_PASS();
}

static void test_undo_restores_files() {
    TEST_BEGIN("undo restores files to original locations");
    setup_exec_environment();
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = TEST_BASE + "/source";
    plan.status = PlanStatus::Ready;
    plan.total_files = 1;
    plan.moves_planned = 1;
    plan_repo.insert(plan);

    OrgMove move;
    move.id = "move-001";
    move.plan_id = "plan-001";
    move.source_path = TEST_BASE + "/source/data.json";
    move.dest_path = TEST_BASE + "/dest/Data/JSON/data.json";
    move.confidence = 0.85;
    move.status = MoveStatus::Planned;
    move_repo.insert(move);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute("plan-001");

    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/dest/Data/JSON/data.json"));

    executor.undo(undo.id);

    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/source/data.json"));
    ASSERT_TRUE(!std::filesystem::exists(TEST_BASE + "/dest/Data/JSON/data.json"));
    cleanup();
    TEST_PASS();
}

static void test_execute_nonexistent_source_continues() {
    TEST_BEGIN("execute with non-existent source continues");
    setup_exec_environment();
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = TEST_BASE + "/source";
    plan.status = PlanStatus::Ready;
    plan.total_files = 2;
    plan.moves_planned = 2;
    plan_repo.insert(plan);

    OrgMove m1;
    m1.id = "move-001"; m1.plan_id = "plan-001";
    m1.source_path = TEST_BASE + "/source/main.cpp";
    m1.dest_path = TEST_BASE + "/dest/Development/C++/main.cpp";
    m1.confidence = 0.9; m1.status = MoveStatus::Planned;
    move_repo.insert(m1);

    OrgMove m2;
    m2.id = "move-002"; m2.plan_id = "plan-001";
    m2.source_path = TEST_BASE + "/nonexistent/file.txt";
    m2.dest_path = TEST_BASE + "/dest/Data/file.txt";
    m2.confidence = 0.8; m2.status = MoveStatus::Planned;
    move_repo.insert(m2);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute("plan-001");

    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/dest/Development/C++/main.cpp"));
    cleanup();
    TEST_PASS();
}

static void test_undo_missing_file_continues() {
    TEST_BEGIN("undo with missing file continues without crash");
    setup_exec_environment();
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = TEST_BASE + "/source";
    plan.status = PlanStatus::Ready;
    plan.total_files = 1;
    plan.moves_planned = 1;
    plan_repo.insert(plan);

    OrgMove move;
    move.id = "move-001";
    move.plan_id = "plan-001";
    move.source_path = TEST_BASE + "/source/main.cpp";
    move.dest_path = TEST_BASE + "/dest/Development/C++/main.cpp";
    move.confidence = 0.9;
    move.status = MoveStatus::Planned;
    move_repo.insert(move);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute("plan-001");

    std::error_code ec;
    std::filesystem::remove(TEST_BASE + "/dest/Development/C++/main.cpp", ec);

    executor.undo(undo.id);

    ASSERT_TRUE(!std::filesystem::exists(TEST_BASE + "/dest/Development/C++/main.cpp"));
    cleanup();
    TEST_PASS();
}

static void test_execute_multiple_files() {
    TEST_BEGIN("execute moves multiple files");
    setup_exec_environment();
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = TEST_BASE + "/source";
    plan.status = PlanStatus::Ready;
    plan.total_files = 3;
    plan.moves_planned = 3;
    plan_repo.insert(plan);

    OrgMove m1; m1.id = "m1"; m1.plan_id = "plan-001";
    m1.source_path = TEST_BASE + "/source/main.cpp";
    m1.dest_path = TEST_BASE + "/dest/Development/C++/main.cpp";
    m1.confidence = 0.9; m1.status = MoveStatus::Planned;
    move_repo.insert(m1);

    OrgMove m2; m2.id = "m2"; m2.plan_id = "plan-001";
    m2.source_path = TEST_BASE + "/source/script.py";
    m2.dest_path = TEST_BASE + "/dest/Development/Python/script.py";
    m2.confidence = 0.85; m2.status = MoveStatus::Planned;
    move_repo.insert(m2);

    OrgMove m3; m3.id = "m3"; m3.plan_id = "plan-001";
    m3.source_path = TEST_BASE + "/source/data.json";
    m3.dest_path = TEST_BASE + "/dest/Data/JSON/data.json";
    m3.confidence = 0.8; m3.status = MoveStatus::Planned;
    move_repo.insert(m3);

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    auto undo = executor.execute("plan-001");

    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/dest/Development/C++/main.cpp"));
    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/dest/Development/Python/script.py"));
    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/dest/Data/JSON/data.json"));

    executor.undo(undo.id);

    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/source/main.cpp"));
    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/source/script.py"));
    ASSERT_TRUE(std::filesystem::exists(TEST_BASE + "/source/data.json"));
    cleanup();
    TEST_PASS();
}

static void test_execute_preserves_content() {
    TEST_BEGIN("execute preserves file content after move");
    setup_exec_environment();
    TestDB tdb;
    auto& db = tdb.db;
    OrgPlanRepository plan_repo(db);
    OrgMoveRepository move_repo(db);
    UndoRepository undo_repo(db);

    OrgPlan plan;
    plan.id = "plan-001";
    plan.scan_id = "scan-001";
    plan.root_path = TEST_BASE + "/source";
    plan.status = PlanStatus::Ready;
    plan.total_files = 1;
    plan.moves_planned = 1;
    plan_repo.insert(plan);

    OrgMove move;
    move.id = "move-001";
    move.plan_id = "plan-001";
    move.source_path = TEST_BASE + "/source/main.cpp";
    move.dest_path = TEST_BASE + "/dest/Development/C++/main.cpp";
    move.confidence = 0.9;
    move.status = MoveStatus::Planned;
    move_repo.insert(move);

    std::string original_content = "int main() { return 0; }";

    OrganizationExecutor executor(db, plan_repo, move_repo, undo_repo);
    executor.execute("plan-001");

    std::string moved_content = read_file(TEST_BASE + "/dest/Development/C++/main.cpp");
    ASSERT_EQ(moved_content, original_content);
    cleanup();
    TEST_PASS();
}

void run_executor_tests() {
    std::cout << "=== OrganizationExecutor Tests ===" << std::endl;

    test_execute_moves_files();
    test_execute_creates_undo_record();
    test_undo_restores_files();
    test_execute_nonexistent_source_continues();
    test_undo_missing_file_continues();
    test_execute_multiple_files();
    test_execute_preserves_content();
}
