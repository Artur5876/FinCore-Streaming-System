cmake_minimum_required(VERSION 3.10)
project(Finantial-Data_Pipeline)

set(CMAKE_CXX_STANDARD 17)

# Find hiredis using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)

# Add executable
add_executable(market_pipeline src/main.cpp)

# Include directories
target_include_directories(market_pipeline
        PRIVATE
        src
        ${HIREDIS_INCLUDE_DIRS}  # Note: HIREDIS_INCLUDE_DIRS (plural), not hiredis_INCLUDE_DIR
)

# Link libraries
target_link_libraries(market_pipeline
        PRIVATE
        ${HIREDIS_LIBRARIES}  # Use the variable from pkg_check_modules
)