#include "test_helpers.h"

#include "core/types/SearchResult.h"
#include "core/types/DashboardStats.h"
#include "core/types/ImportRequest.h"
#include "core/types/ImportResult.h"
#include "core/types/VerificationResult.h"

using namespace archive::core;

static void test_search_result_default() {
    TEST_BEGIN("SearchResult default");
    SearchResult sr;
    ASSERT_TRUE(sr.items.empty());
    ASSERT_EQ(sr.total, 0);
    ASSERT_TRUE(sr.query.empty());
    TEST_PASS();
}

static void test_search_result_with_items() {
    TEST_BEGIN("SearchResult with items");
    ArchiveItem item;
    item.id = "1";
    item.name = "test";

    std::vector<ArchiveItem> items;
    items.push_back(item);

    SearchResult sr(std::move(items), 1, "test");
    ASSERT_EQ(sr.items.size(), 1u);
    ASSERT_EQ(sr.total, 1);
    ASSERT_EQ(sr.query, "test");
    TEST_PASS();
}

static void test_dashboard_stats_default() {
    TEST_BEGIN("DashboardStats default");
    DashboardStats ds;
    ASSERT_EQ(ds.total_items, 0);
    ASSERT_EQ(ds.archived_items, 0);
    ASSERT_EQ(ds.favorite_items, 0);
    ASSERT_EQ(ds.deleted_items, 0);
    ASSERT_EQ(ds.total_size, 0u);
    ASSERT_TRUE(ds.recent_items.empty());
    ASSERT_TRUE(ds.category_counts.empty());
    ASSERT_TRUE(ds.type_counts.empty());
    TEST_PASS();
}

static void test_category_count() {
    TEST_BEGIN("CategoryCount");
    CategoryCount cc;
    cc.category_id = "cat-1";
    cc.name = "Documents";
    cc.count = 5;
    cc.color = "#ff0000";
    ASSERT_EQ(cc.category_id, "cat-1");
    ASSERT_EQ(cc.count, 5);
    TEST_PASS();
}

static void test_type_count() {
    TEST_BEGIN("TypeCount");
    TypeCount tc;
    tc.type = "project";
    tc.count = 10;
    ASSERT_EQ(tc.type, "project");
    ASSERT_EQ(tc.count, 10);
    TEST_PASS();
}

static void test_import_request_default() {
    TEST_BEGIN("ImportRequest default");
    ImportRequest ir;
    ASSERT_TRUE(ir.paths.empty());
    ASSERT_TRUE(ir.category_id == std::nullopt);
    ASSERT_TRUE(ir.tags.empty());
    ASSERT_TRUE(ir.description.empty());
    TEST_PASS();
}

static void test_import_request_with_paths() {
    TEST_BEGIN("ImportRequest with paths");
    ImportRequest ir;
    ir.paths = {"/path/one", "/path/two"};
    ir.tags = {"cpp", "archive"};
    ir.description = "Test import";
    ASSERT_EQ(ir.paths.size(), 2u);
    ASSERT_EQ(ir.tags.size(), 2u);
    TEST_PASS();
}

static void test_import_result() {
    TEST_BEGIN("ImportResult success/error counting");
    ImportResult result;
    ArchiveItem item1;
    item1.id = "1";
    result.items.push_back(std::move(item1));

    ImportError err;
    err.path = "/bad/path";
    err.error = "Not found";
    result.errors.push_back(std::move(err));

    ASSERT_EQ(result.success_count(), 1);
    ASSERT_EQ(result.error_count(), 1);
    ASSERT_TRUE(result.has_errors());
    TEST_PASS();
}

static void test_import_result_no_errors() {
    TEST_BEGIN("ImportResult no errors");
    ImportResult result;
    ASSERT_EQ(result.success_count(), 0);
    ASSERT_EQ(result.error_count(), 0);
    ASSERT_TRUE(!result.has_errors());
    TEST_PASS();
}

static void test_verification_result_valid() {
    TEST_BEGIN("VerificationResult all valid");
    VerificationResult vr;
    vr.valid_count = 5;
    ASSERT_TRUE(vr.all_valid());
    TEST_PASS();
}

static void test_verification_result_failures() {
    TEST_BEGIN("VerificationResult with failures");
    VerificationResult vr;
    vr.valid_count = 3;
    vr.modified_count = 1;
    ASSERT_TRUE(!vr.all_valid());
    TEST_PASS();
}

static void test_verification_item() {
    TEST_BEGIN("VerificationItem");
    VerificationItem vi;
    vi.id = "item-1";
    vi.name = "test.txt";
    vi.state = IntegrityState::Valid;
    vi.expected_checksum = "abc123";
    vi.actual_checksum = "abc123";
    ASSERT_EQ(vi.state, IntegrityState::Valid);
    ASSERT_EQ(vi.expected_checksum, vi.actual_checksum);
    TEST_PASS();
}

void run_type_tests() {
    std::cout << "=== Type Tests ===" << std::endl;

    test_search_result_default();
    test_search_result_with_items();
    test_dashboard_stats_default();
    test_category_count();
    test_type_count();
    test_import_request_default();
    test_import_request_with_paths();
    test_import_result();
    test_import_result_no_errors();
    test_verification_result_valid();
    test_verification_result_failures();
    test_verification_item();
}
