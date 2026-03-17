cmake_minimum_required(VERSION 3.10)
project(Financial-Data_Pipeline)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(REDISPP REQUIRED redis++)

#include directories
include_directories(include)
include_directories(${REDISPP_INCLUDE_DIRS})

add_library(market_pipeline
    src/api/alpha_vantage_client.cpp
    src/core/order_book.cpp
    src/storage/redis_client.cpp
)

# Sources
set(SOURCES
        src/core/main.cpp
        src/storage/redis_client.cpp
)

# Create executable
add_executable(market_pipeline src/core/main.cpp)

target_link_libraries(market_pipeline fincore_core)

# Include directories
target_include_directories(market_pipeline PRIVATE src include)
target_include_directories(market_pipeline SYSTEM PRIVATE ${HIREDIS_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})

target_link_libraries(market_pipeline PRIVATE
        ${HIREDIS_LIBRARIES}
        pthread
)
