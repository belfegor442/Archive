cmake_minimum_required(VERSION 3.20)

add_executable(archive_tests
    main.cpp
    core/test_enums.cpp
    core/test_models.cpp
    core/test_types.cpp
    storage/test_database.cpp
    hashing/test_hasher.cpp
    services/test_services.cpp
    integration/test_import_flow.cpp
    integration/test_atomic_ops.cpp
    test_new_enums.cpp
    test_new_models.cpp
    test_new_storage.cpp
    test_classifier.cpp
    test_scanner.cpp
    test_planner.cpp
    test_executor.cpp
    test_file_analysis.cpp
    integration/test_organize_flow.cpp
)

target_link_libraries(archive_tests PRIVATE archive_lib)

target_include_directories(archive_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
)

add_test(NAME archive_tests COMMAND archive_tests)
