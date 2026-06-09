cmake_minimum_required(VERSION 3.10)
project(Financial_Data_Pipeline)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(REDISPP REQUIRED redis++)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)   # for pthread

# Create executable directly
add_executable(market_pipeline
    src/core/main.cpp
    src/api/alpha_vantage_client.cpp
    src/core/order_book.cpp
    src/storage/redis_client.cpp
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

