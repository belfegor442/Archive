cmake_minimum_required(VERSION 3.20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(OPENSSL REQUIRED openssl)

add_executable(archive_tests
    main.cpp
    core/test_enums.cpp
    core/test_models.cpp
    core/test_types.cpp
    storage/test_database.cpp
    hashing/test_hasher.cpp
)

target_link_libraries(archive_tests PRIVATE archive_lib)
target_include_directories(archive_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
)

add_test(NAME archive_tests COMMAND archive_tests)
