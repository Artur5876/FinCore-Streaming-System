#minimum version of CMake required to run a program
cmake_minimum_required(VERSION 3.10)

#Name of the project
project(Finantial-Data_Pipeline)

#C++ version
set(CMAKE_CXX_STANDARD 17)

# You're using hiredis (C client), NOT redis++ (C++ client)
# Find hiredis
find_package(hiredis REQUIRED)

# Add executable
add_executable(market_pipeline src/main.cpp)

# Include directories - tell compiler where your headers are
target_include_directories(market_pipeline
        PRIVATE
        src
        ${hiredis_INCLUDE_DIR}  # Add hiredis headers
)

# Link libraries
target_link_libraries(market_pipeline
        PRIVATE
        hiredis::hiredis  # Link with hiredis library
)