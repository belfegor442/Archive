#include "test_helpers.h"

#include "core/enums/ItemType.h"
#include "core/enums/ItemStatus.h"
#include "core/enums/IntegrityState.h"
#include "core/enums/ActivityAction.h"

using namespace archive::core;

static void test_item_type_to_string_file() {
    TEST_BEGIN("ItemType::File -> string");
    ASSERT_EQ(to_string(ItemType::File), "file");
    TEST_PASS();
}

static void test_item_type_to_string_folder() {
    TEST_BEGIN("ItemType::Folder -> string");
    ASSERT_EQ(to_string(ItemType::Folder), "folder");
    TEST_PASS();
}

static void test_item_type_to_string_project() {
    TEST_BEGIN("ItemType::Project -> string");
    ASSERT_EQ(to_string(ItemType::Project), "project");
    TEST_PASS();
}

static void test_item_type_to_string_document() {
    TEST_BEGIN("ItemType::Document -> string");
    ASSERT_EQ(to_string(ItemType::Document), "document");
    TEST_PASS();
}

static void test_item_type_from_string_file() {
    TEST_BEGIN("string -> ItemType::File");
    ASSERT_EQ(item_type_from_string("file"), ItemType::File);
    TEST_PASS();
}

static void test_item_type_from_string_invalid() {
    TEST_BEGIN("invalid string -> throws");
    ASSERT_THROW(item_type_from_string("invalid"));
    TEST_PASS();
}

static void test_item_status_to_string() {
    TEST_BEGIN("ItemStatus -> string");
    ASSERT_EQ(to_string(ItemStatus::Archived), "archived");
    ASSERT_EQ(to_string(ItemStatus::Deleted), "deleted");
    TEST_PASS();
}

static void test_item_status_from_string() {
    TEST_BEGIN("string -> ItemStatus");
    ASSERT_EQ(item_status_from_string("archived"), ItemStatus::Archived);
    ASSERT_EQ(item_status_from_string("deleted"), ItemStatus::Deleted);
    TEST_PASS();
}

static void test_item_status_from_string_invalid() {
    TEST_BEGIN("invalid ItemStatus string -> throws");
    ASSERT_THROW(item_status_from_string("favorite"));
    TEST_PASS();
}

static void test_integrity_state_to_string() {
    TEST_BEGIN("IntegrityState -> string");
    ASSERT_EQ(to_string(IntegrityState::Valid), "valid");
    ASSERT_EQ(to_string(IntegrityState::Modified), "modified");
    ASSERT_EQ(to_string(IntegrityState::Missing), "missing");
    ASSERT_EQ(to_string(IntegrityState::Corrupted), "corrupted");
    ASSERT_EQ(to_string(IntegrityState::Unknown), "unknown");
    TEST_PASS();
}

static void test_integrity_state_from_string() {
    TEST_BEGIN("string -> IntegrityState");
    ASSERT_EQ(integrity_state_from_string("valid"), IntegrityState::Valid);
    ASSERT_EQ(integrity_state_from_string("modified"), IntegrityState::Modified);
    ASSERT_EQ(integrity_state_from_string("missing"), IntegrityState::Missing);
    ASSERT_EQ(integrity_state_from_string("corrupted"), IntegrityState::Corrupted);
    ASSERT_EQ(integrity_state_from_string("unknown"), IntegrityState::Unknown);
    TEST_PASS();
}

static void test_activity_action_to_string() {
    TEST_BEGIN("ActivityAction -> string");
    ASSERT_EQ(to_string(ActivityAction::Imported), "Imported");
    ASSERT_EQ(to_string(ActivityAction::Deleted), "Deleted");
    ASSERT_EQ(to_string(ActivityAction::VersionCreated), "Version created");
    ASSERT_EQ(to_string(ActivityAction::IntegrityVerified), "Integrity verified");
    TEST_PASS();
}

static void test_activity_action_from_string() {
    TEST_BEGIN("string -> ActivityAction");
    ASSERT_EQ(activity_action_from_string("Imported"), ActivityAction::Imported);
    ASSERT_EQ(activity_action_from_string("Version created"), ActivityAction::VersionCreated);
    TEST_PASS();
}

void run_enum_tests() {
    std::cout << "=== Enum Tests ===" << std::endl;

    test_item_type_to_string_file();
    test_item_type_to_string_folder();
    test_item_type_to_string_project();
    test_item_type_to_string_document();
    test_item_type_from_string_file();
    test_item_type_from_string_invalid();
    test_item_status_to_string();
    test_item_status_from_string();
    test_item_status_from_string_invalid();
    test_integrity_state_to_string();
    test_integrity_state_from_string();
    test_activity_action_to_string();
    test_activity_action_from_string();
}
