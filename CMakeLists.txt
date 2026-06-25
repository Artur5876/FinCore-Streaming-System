cmake_minimum_required(VERSION 3.20)
project(fincore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)


find_package(CURL REQUIRED)

find_package(redis++ QUIET)
if(NOT redis++_FOUND)
    #If dependency is not found, assume headers in /usr/local/include, lib in /usr/local/lib
    message(STATUS "redis++ cmake config not found --> using manual paths")
    add_library(redis++::redis++ INTERFACE IMPORTED)
    target_include_directories(redis++::redis++ INTERFACE /usr/local/include)
    target_link_libraries(redis++::redis++ INTERFACE
        /usr/local/lib/libredis++.a
        hiredis
    )
endif()

#Libraries
# Shared include path used by all targets
set(FINCORE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# order_book — no external deps
add_library(order_book STATIC
    order_book.cpp
)
target_include_directories(order_book PUBLIC ${FINCORE_INCLUDE_DIR})

# alpha_vantage_client will need libcurl
add_library(alpha_vantage_client STATIC
    alpha_vantage_client.cpp
)
target_include_directories(alpha_vantage_client PUBLIC ${FINCORE_INCLUDE_DIR})
target_link_libraries(alpha_vantage_client PUBLIC CURL::libcurl)

# redis_client --> needs redis-plus-plus + hiredis
add_library(redis_client STATIC
    redis_client.cpp
)
target_include_directories(redis_client PUBLIC ${FINCORE_INCLUDE_DIR})
target_link_libraries(redis_client PUBLIC redis++::redis++)

#Main executable file
add_executable(fincore_app main.cpp)
target_link_libraries(fincore_app
    PRIVATE
        order_book
        alpha_vantage_client
        redis_client
)

#Tests
enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# test_order_book - no Network(Redis/curl) needed
#add_executable(test_order_book test_order_book.cpp)
#target_link_libraries(test_order_book
#    PRIVATE
#        order_book
#        GTest::gtest_main
#)
#gtest_discover_tests(test_order_book)

# test_redis_client — also pure logic (uses the self-contained stub in the file)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_redis_client.cpp")
    add_executable(test_redis_client test_redis_client.cpp)
    target_link_libraries(test_redis_client
        PRIVATE
            GTest::gtest_main
    )
    gtest_discover_tests(test_redis_client)
endif()

#Run all tests:         cmake --build build && ctest --test-dir build -V
#Run only order_book:   ctest --test-dir build -R test_order_book -V
