#include "test_helpers.h"

#include "core/enums/ScanStatus.h"
#include "core/enums/MoveStatus.h"
#include "core/enums/PlanStatus.h"
#include "core/enums/FileRole.h"
#include "core/enums/UndoStatus.h"

using namespace archive::core;

// ============================================================
// ScanStatus tests
// ============================================================

static void test_scan_status_to_string_pending() {
    TEST_BEGIN("ScanStatus::Pending -> \"pending\"");
    ASSERT_EQ(to_string(ScanStatus::Pending), "pending");
    TEST_PASS();
}

static void test_scan_status_to_string_running() {
    TEST_BEGIN("ScanStatus::Running -> \"running\"");
    ASSERT_EQ(to_string(ScanStatus::Running), "running");
    TEST_PASS();
}

static void test_scan_status_to_string_completed() {
    TEST_BEGIN("ScanStatus::Completed -> \"completed\"");
    ASSERT_EQ(to_string(ScanStatus::Completed), "completed");
    TEST_PASS();
}

static void test_scan_status_to_string_failed() {
    TEST_BEGIN("ScanStatus::Failed -> \"failed\"");
    ASSERT_EQ(to_string(ScanStatus::Failed), "failed");
    TEST_PASS();
}

static void test_scan_status_to_string_cancelled() {
    TEST_BEGIN("ScanStatus::Cancelled -> \"cancelled\"");
    ASSERT_EQ(to_string(ScanStatus::Cancelled), "cancelled");
    TEST_PASS();
}

static void test_scan_status_from_string_pending() {
    TEST_BEGIN("\"pending\" -> ScanStatus::Pending");
    ASSERT_EQ(scan_status_from_string("pending"), ScanStatus::Pending);
    TEST_PASS();
}

static void test_scan_status_from_string_running() {
    TEST_BEGIN("\"running\" -> ScanStatus::Running");
    ASSERT_EQ(scan_status_from_string("running"), ScanStatus::Running);
    TEST_PASS();
}

static void test_scan_status_from_string_completed() {
    TEST_BEGIN("\"completed\" -> ScanStatus::Completed");
    ASSERT_EQ(scan_status_from_string("completed"), ScanStatus::Completed);
    TEST_PASS();
}

static void test_scan_status_from_string_failed() {
    TEST_BEGIN("\"failed\" -> ScanStatus::Failed");
    ASSERT_EQ(scan_status_from_string("failed"), ScanStatus::Failed);
    TEST_PASS();
}

static void test_scan_status_from_string_cancelled() {
    TEST_BEGIN("\"cancelled\" -> ScanStatus::Cancelled");
    ASSERT_EQ(scan_status_from_string("cancelled"), ScanStatus::Cancelled);
    TEST_PASS();
}

static void test_scan_status_from_string_invalid() {
    TEST_BEGIN("invalid string -> throws ScanStatus");
    ASSERT_THROW(scan_status_from_string("bogus"));
    TEST_PASS();
}

static void test_scan_status_round_trip() {
    TEST_BEGIN("ScanStatus round-trip to_string/from_string");
    ScanStatus values[] = {
        ScanStatus::Pending, ScanStatus::Running, ScanStatus::Completed,
        ScanStatus::Failed, ScanStatus::Cancelled
    };
    for (auto v : values) {
        ASSERT_EQ(scan_status_from_string(to_string(v)), v);
    }
    TEST_PASS();
}

// ============================================================
// MoveStatus tests
// ============================================================

static void test_move_status_to_string_planned() {
    TEST_BEGIN("MoveStatus::Planned -> \"planned\"");
    ASSERT_EQ(to_string(MoveStatus::Planned), "planned");
    TEST_PASS();
}

static void test_move_status_to_string_executing() {
    TEST_BEGIN("MoveStatus::Executing -> \"executing\"");
    ASSERT_EQ(to_string(MoveStatus::Executing), "executing");
    TEST_PASS();
}

static void test_move_status_to_string_completed() {
    TEST_BEGIN("MoveStatus::Completed -> \"completed\"");
    ASSERT_EQ(to_string(MoveStatus::Completed), "completed");
    TEST_PASS();
}

static void test_move_status_to_string_failed() {
    TEST_BEGIN("MoveStatus::Failed -> \"failed\"");
    ASSERT_EQ(to_string(MoveStatus::Failed), "failed");
    TEST_PASS();
}

static void test_move_status_to_string_skipped() {
    TEST_BEGIN("MoveStatus::Skipped -> \"skipped\"");
    ASSERT_EQ(to_string(MoveStatus::Skipped), "skipped");
    TEST_PASS();
}

static void test_move_status_from_string_planned() {
    TEST_BEGIN("\"planned\" -> MoveStatus::Planned");
    ASSERT_EQ(move_status_from_string("planned"), MoveStatus::Planned);
    TEST_PASS();
}

static void test_move_status_from_string_executing() {
    TEST_BEGIN("\"executing\" -> MoveStatus::Executing");
    ASSERT_EQ(move_status_from_string("executing"), MoveStatus::Executing);
    TEST_PASS();
}

static void test_move_status_from_string_completed() {
    TEST_BEGIN("\"completed\" -> MoveStatus::Completed");
    ASSERT_EQ(move_status_from_string("completed"), MoveStatus::Completed);
    TEST_PASS();
}

static void test_move_status_from_string_failed() {
    TEST_BEGIN("\"failed\" -> MoveStatus::Failed");
    ASSERT_EQ(move_status_from_string("failed"), MoveStatus::Failed);
    TEST_PASS();
}

static void test_move_status_from_string_skipped() {
    TEST_BEGIN("\"skipped\" -> MoveStatus::Skipped");
    ASSERT_EQ(move_status_from_string("skipped"), MoveStatus::Skipped);
    TEST_PASS();
}

static void test_move_status_from_string_invalid() {
    TEST_BEGIN("invalid string -> throws MoveStatus");
    ASSERT_THROW(move_status_from_string("invalid_move"));
    TEST_PASS();
}

static void test_move_status_round_trip() {
    TEST_BEGIN("MoveStatus round-trip to_string/from_string");
    MoveStatus values[] = {
        MoveStatus::Planned, MoveStatus::Executing, MoveStatus::Completed,
        MoveStatus::Failed, MoveStatus::Skipped
    };
    for (auto v : values) {
        ASSERT_EQ(move_status_from_string(to_string(v)), v);
    }
    TEST_PASS();
}

// ============================================================
// PlanStatus tests
// ============================================================

static void test_plan_status_to_string_draft() {
    TEST_BEGIN("PlanStatus::Draft -> \"draft\"");
    ASSERT_EQ(to_string(PlanStatus::Draft), "draft");
    TEST_PASS();
}

static void test_plan_status_to_string_ready() {
    TEST_BEGIN("PlanStatus::Ready -> \"ready\"");
    ASSERT_EQ(to_string(PlanStatus::Ready), "ready");
    TEST_PASS();
}

static void test_plan_status_to_string_executing() {
    TEST_BEGIN("PlanStatus::Executing -> \"executing\"");
    ASSERT_EQ(to_string(PlanStatus::Executing), "executing");
    TEST_PASS();
}

static void test_plan_status_to_string_completed() {
    TEST_BEGIN("PlanStatus::Completed -> \"completed\"");
    ASSERT_EQ(to_string(PlanStatus::Completed), "completed");
    TEST_PASS();
}

static void test_plan_status_to_string_failed() {
    TEST_BEGIN("PlanStatus::Failed -> \"failed\"");
    ASSERT_EQ(to_string(PlanStatus::Failed), "failed");
    TEST_PASS();
}

static void test_plan_status_from_string_draft() {
    TEST_BEGIN("\"draft\" -> PlanStatus::Draft");
    ASSERT_EQ(plan_status_from_string("draft"), PlanStatus::Draft);
    TEST_PASS();
}

static void test_plan_status_from_string_ready() {
    TEST_BEGIN("\"ready\" -> PlanStatus::Ready");
    ASSERT_EQ(plan_status_from_string("ready"), PlanStatus::Ready);
    TEST_PASS();
}

static void test_plan_status_from_string_executing() {
    TEST_BEGIN("\"executing\" -> PlanStatus::Executing");
    ASSERT_EQ(plan_status_from_string("executing"), PlanStatus::Executing);
    TEST_PASS();
}

static void test_plan_status_from_string_completed() {
    TEST_BEGIN("\"completed\" -> PlanStatus::Completed");
    ASSERT_EQ(plan_status_from_string("completed"), PlanStatus::Completed);
    TEST_PASS();
}

static void test_plan_status_from_string_failed() {
    TEST_BEGIN("\"failed\" -> PlanStatus::Failed");
    ASSERT_EQ(plan_status_from_string("failed"), PlanStatus::Failed);
    TEST_PASS();
}

static void test_plan_status_from_string_invalid() {
    TEST_BEGIN("invalid string -> throws PlanStatus");
    ASSERT_THROW(plan_status_from_string("not_real"));
    TEST_PASS();
}

static void test_plan_status_round_trip() {
    TEST_BEGIN("PlanStatus round-trip to_string/from_string");
    PlanStatus values[] = {
        PlanStatus::Draft, PlanStatus::Ready, PlanStatus::Executing,
        PlanStatus::Completed, PlanStatus::Failed
    };
    for (auto v : values) {
        ASSERT_EQ(plan_status_from_string(to_string(v)), v);
    }
    TEST_PASS();
}

// ============================================================
// FileRole tests
// ============================================================

static void test_file_role_to_string_source() {
    TEST_BEGIN("FileRole::Source -> \"Source\"");
    ASSERT_EQ(to_string(FileRole::Source), "Source");
    TEST_PASS();
}

static void test_file_role_to_string_header() {
    TEST_BEGIN("FileRole::Header -> \"Header\"");
    ASSERT_EQ(to_string(FileRole::Header), "Header");
    TEST_PASS();
}

static void test_file_role_to_string_script() {
    TEST_BEGIN("FileRole::Script -> \"Script\"");
    ASSERT_EQ(to_string(FileRole::Script), "Script");
    TEST_PASS();
}

static void test_file_role_to_string_config() {
    TEST_BEGIN("FileRole::Config -> \"Config\"");
    ASSERT_EQ(to_string(FileRole::Config), "Config");
    TEST_PASS();
}

static void test_file_role_to_string_build() {
    TEST_BEGIN("FileRole::Build -> \"Build\"");
    ASSERT_EQ(to_string(FileRole::Build), "Build");
    TEST_PASS();
}

static void test_file_role_to_string_documentation() {
    TEST_BEGIN("FileRole::Documentation -> \"Documentation\"");
    ASSERT_EQ(to_string(FileRole::Documentation), "Documentation");
    TEST_PASS();
}

static void test_file_role_to_string_asset() {
    TEST_BEGIN("FileRole::Asset -> \"Asset\"");
    ASSERT_EQ(to_string(FileRole::Asset), "Asset");
    TEST_PASS();
}

static void test_file_role_to_string_data() {
    TEST_BEGIN("FileRole::Data -> \"Data\"");
    ASSERT_EQ(to_string(FileRole::Data), "Data");
    TEST_PASS();
}

static void test_file_role_to_string_binary() {
    TEST_BEGIN("FileRole::Binary -> \"Binary\"");
    ASSERT_EQ(to_string(FileRole::Binary), "Binary");
    TEST_PASS();
}

static void test_file_role_to_string_unknown() {
    TEST_BEGIN("FileRole::Unknown -> \"Unknown\"");
    ASSERT_EQ(to_string(FileRole::Unknown), "Unknown");
    TEST_PASS();
}

static void test_file_role_from_string_source() {
    TEST_BEGIN("\"Source\" -> FileRole::Source");
    ASSERT_EQ(file_role_from_string("Source"), FileRole::Source);
    TEST_PASS();
}

static void test_file_role_from_string_header() {
    TEST_BEGIN("\"Header\" -> FileRole::Header");
    ASSERT_EQ(file_role_from_string("Header"), FileRole::Header);
    TEST_PASS();
}

static void test_file_role_from_string_script() {
    TEST_BEGIN("\"Script\" -> FileRole::Script");
    ASSERT_EQ(file_role_from_string("Script"), FileRole::Script);
    TEST_PASS();
}

static void test_file_role_from_string_config() {
    TEST_BEGIN("\"Config\" -> FileRole::Config");
    ASSERT_EQ(file_role_from_string("Config"), FileRole::Config);
    TEST_PASS();
}

static void test_file_role_from_string_build() {
    TEST_BEGIN("\"Build\" -> FileRole::Build");
    ASSERT_EQ(file_role_from_string("Build"), FileRole::Build);
    TEST_PASS();
}

static void test_file_role_from_string_documentation() {
    TEST_BEGIN("\"Documentation\" -> FileRole::Documentation");
    ASSERT_EQ(file_role_from_string("Documentation"), FileRole::Documentation);
    TEST_PASS();
}

static void test_file_role_from_string_asset() {
    TEST_BEGIN("\"Asset\" -> FileRole::Asset");
    ASSERT_EQ(file_role_from_string("Asset"), FileRole::Asset);
    TEST_PASS();
}

static void test_file_role_from_string_data() {
    TEST_BEGIN("\"Data\" -> FileRole::Data");
    ASSERT_EQ(file_role_from_string("Data"), FileRole::Data);
    TEST_PASS();
}

static void test_file_role_from_string_binary() {
    TEST_BEGIN("\"Binary\" -> FileRole::Binary");
    ASSERT_EQ(file_role_from_string("Binary"), FileRole::Binary);
    TEST_PASS();
}

static void test_file_role_from_string_unknown() {
    TEST_BEGIN("\"Unknown\" -> FileRole::Unknown");
    ASSERT_EQ(file_role_from_string("Unknown"), FileRole::Unknown);
    TEST_PASS();
}

static void test_file_role_from_string_invalid() {
    TEST_BEGIN("invalid string -> throws FileRole");
    ASSERT_THROW(file_role_from_string("not_a_role"));
    TEST_PASS();
}

static void test_file_role_round_trip() {
    TEST_BEGIN("FileRole round-trip to_string/from_string");
    FileRole values[] = {
        FileRole::Source, FileRole::Header, FileRole::Script,
        FileRole::Config, FileRole::Build, FileRole::Documentation,
        FileRole::Asset, FileRole::Data, FileRole::Binary, FileRole::Unknown
    };
    for (auto v : values) {
        ASSERT_EQ(file_role_from_string(to_string(v)), v);
    }
    TEST_PASS();
}

// ============================================================
// UndoStatus tests
// ============================================================

static void test_undo_status_to_string_available() {
    TEST_BEGIN("UndoStatus::Available -> \"available\"");
    ASSERT_EQ(to_string(UndoStatus::Available), "available");
    TEST_PASS();
}

static void test_undo_status_to_string_used() {
    TEST_BEGIN("UndoStatus::Used -> \"used\"");
    ASSERT_EQ(to_string(UndoStatus::Used), "used");
    TEST_PASS();
}

static void test_undo_status_to_string_failed() {
    TEST_BEGIN("UndoStatus::Failed -> \"failed\"");
    ASSERT_EQ(to_string(UndoStatus::Failed), "failed");
    TEST_PASS();
}

static void test_undo_status_from_string_available() {
    TEST_BEGIN("\"available\" -> UndoStatus::Available");
    ASSERT_EQ(undo_status_from_string("available"), UndoStatus::Available);
    TEST_PASS();
}

static void test_undo_status_from_string_used() {
    TEST_BEGIN("\"used\" -> UndoStatus::Used");
    ASSERT_EQ(undo_status_from_string("used"), UndoStatus::Used);
    TEST_PASS();
}

static void test_undo_status_from_string_failed() {
    TEST_BEGIN("\"failed\" -> UndoStatus::Failed");
    ASSERT_EQ(undo_status_from_string("failed"), UndoStatus::Failed);
    TEST_PASS();
}

static void test_undo_status_from_string_invalid() {
    TEST_BEGIN("invalid string -> throws UndoStatus");
    ASSERT_THROW(undo_status_from_string("not_valid"));
    TEST_PASS();
}

static void test_undo_status_round_trip() {
    TEST_BEGIN("UndoStatus round-trip to_string/from_string");
    UndoStatus values[] = {
        UndoStatus::Available, UndoStatus::Used, UndoStatus::Failed
    };
    for (auto v : values) {
        ASSERT_EQ(undo_status_from_string(to_string(v)), v);
    }
    TEST_PASS();
}

void run_new_enum_tests() {
    std::cout << "=== New Enum Tests ===" << std::endl;

    test_scan_status_to_string_pending();
    test_scan_status_to_string_running();
    test_scan_status_to_string_completed();
    test_scan_status_to_string_failed();
    test_scan_status_to_string_cancelled();
    test_scan_status_from_string_pending();
    test_scan_status_from_string_running();
    test_scan_status_from_string_completed();
    test_scan_status_from_string_failed();
    test_scan_status_from_string_cancelled();
    test_scan_status_from_string_invalid();
    test_scan_status_round_trip();

    test_move_status_to_string_planned();
    test_move_status_to_string_executing();
    test_move_status_to_string_completed();
    test_move_status_to_string_failed();
    test_move_status_to_string_skipped();
    test_move_status_from_string_planned();
    test_move_status_from_string_executing();
    test_move_status_from_string_completed();
    test_move_status_from_string_failed();
    test_move_status_from_string_skipped();
    test_move_status_from_string_invalid();
    test_move_status_round_trip();

    test_plan_status_to_string_draft();
    test_plan_status_to_string_ready();
    test_plan_status_to_string_executing();
    test_plan_status_to_string_completed();
    test_plan_status_to_string_failed();
    test_plan_status_from_string_draft();
    test_plan_status_from_string_ready();
    test_plan_status_from_string_executing();
    test_plan_status_from_string_completed();
    test_plan_status_from_string_failed();
    test_plan_status_from_string_invalid();
    test_plan_status_round_trip();

    test_file_role_to_string_source();
    test_file_role_to_string_header();
    test_file_role_to_string_script();
    test_file_role_to_string_config();
    test_file_role_to_string_build();
    test_file_role_to_string_documentation();
    test_file_role_to_string_asset();
    test_file_role_to_string_data();
    test_file_role_to_string_binary();
    test_file_role_to_string_unknown();
    test_file_role_from_string_source();
    test_file_role_from_string_header();
    test_file_role_from_string_script();
    test_file_role_from_string_config();
    test_file_role_from_string_build();
    test_file_role_from_string_documentation();
    test_file_role_from_string_asset();
    test_file_role_from_string_data();
    test_file_role_from_string_binary();
    test_file_role_from_string_unknown();
    test_file_role_from_string_invalid();
    test_file_role_round_trip();

    test_undo_status_to_string_available();
    test_undo_status_to_string_used();
    test_undo_status_to_string_failed();
    test_undo_status_from_string_available();
    test_undo_status_from_string_used();
    test_undo_status_from_string_failed();
    test_undo_status_from_string_invalid();
    test_undo_status_round_trip();
}
