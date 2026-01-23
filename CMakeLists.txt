cmake_minimum_required(VERSION 3.10)
project(Financial-Data_Pipeline)

set(CMAKE_CXX_STANDARD 17)

# find hiredis using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)

#add executable
add_executable(market_pipeline src/main.cpp)

#directories included
target_include_directories(market_pipeline
        PRIVATE
        src
        ${HIREDIS_INCLUDE_DIRS}
)

target_link_libraries(market_pipeline
        PRIVATE
        ${HIREDIS_LIBRARIES}  # Use the variable from pkg_check_modules
)