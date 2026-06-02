cmake_minimum_required(VERSION 3.10)
project(Financial-Data_Pipeline)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#FetchContent to fetch fmt
include(FetchContent)
FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/10.1.0.zip
)
FetchContent_MakeAvailable(fmt)

#google tests
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

#find psql system library
find_package(PostgreSQL REQUIRED)

#database library
add_library(fincore_db STATIC
    src/storage/postgres_client.cpp
)

target_include_directories(fincore_db PRIVATE include)
target_link_libraries(fincore_db
    PRIVATE
    ${PostgreSQL_LIBRARIES}
    fmt::fmt
)

# tests
add_executable(test_postgres
    tests/unit/test_quote_repo.cpp
)

target_include_directories(test_postgres PRIVATE include)   # needed for headers
target_link_libraries(test_postgres
    PRIVATE
    fincore_db
    gtest_main
    ${PostgreSQL_LIBRARIES}
    fmt::fmt
)

add_test(NAME PostgresUnit COMMAND test_postgres)

# Test 2: Utilities tests
#add_executable(test_utils
#    tests/unit/test_validation.cpp
#    tests/unit/test_time_utils.cpp
#)
#
#target_link_libraries(test_utils
#    PRIVATE
#    gtest_main
#)

add_test(NAME UtilsTests COMMAND test_utils)
