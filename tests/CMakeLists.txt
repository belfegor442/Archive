cmake_minimum_required(VERSION 3.20)

add_executable(archive_tests
    main.cpp
    core/test_enums.cpp
    core/test_models.cpp
    core/test_types.cpp
)

target_include_directories(archive_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

add_test(NAME archive_tests COMMAND archive_tests)
