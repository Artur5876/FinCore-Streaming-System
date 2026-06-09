cmake_minimum_required(VERSION 3.10)
project(Financial_Data_Pipeline)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
<<<<<<< HEAD
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(REDISPP REQUIRED redis++)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)   # for pthread

<<<<<<< HEAD
# Use pkg-config for hiredis and curl
pkg_check_modules(HIREDIS hiredis)
pkg_check_modules(CURL libcurl)

# Sources
set(SOURCES
        src/core/main.cpp
        src/core/order_book.cpp
        src/api/alpha_vantage_client.cpp
        src/storage/redis_client.cpp
=======
# Create executable directly
add_executable(market_pipeline
    src/core/main.cpp
    src/api/alpha_vantage_client.cpp
    src/core/order_book.cpp
    src/storage/redis_client.cpp
>>>>>>> feature/cmake-setup
)

# Include directories
target_include_directories(market_pipeline PRIVATE
    include
    ${REDISPP_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(market_pipeline PRIVATE
    ${REDISPP_LIBRARIES}
    ${CURL_LIBRARIES}
    Threads::Threads
)

# Tests
enable_testing()
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Fixed test executable for redis_client
add_executable(test_redis_client
    test_redis_client.cpp
)
target_link_libraries(test_redis_client PRIVATE
    GTest::gtest_main
    ${REDISPP_LIBRARIES}
    Threads::Threads
)
target_include_directories(test_redis_client PRIVATE
    ${REDISPP_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(test_redis_client)
=======

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
>>>>>>> feature/unit-tests
