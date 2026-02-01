cmake_minimum_required(VERSION 3.10)
project(Financial-Data_Pipeline)

set(CMAKE_CXX_STANDARD 17)

# Find pkg-config
find_package(PkgConfig REQUIRED)

# Use pkg-config for hiredis and curl
pkg_check_modules(HIREDIS hiredis)
pkg_check_modules(CURL libcurl)

# Sources
set(SOURCES
        src/main.cpp
        src/redis_client.cpp
)

# Create executable
add_executable(market_pipeline ${SOURCES})

# Include directories
target_include_directories(market_pipeline PRIVATE src include)
target_include_directories(market_pipeline SYSTEM PRIVATE ${HIREDIS_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})

# Link libraries - match your working g++ command
target_link_libraries(market_pipeline PRIVATE
        ${HIREDIS_LIBRARIES}
        ${CURL_LIBRARIES}
        redis++  # Direct library name
        pthread
)