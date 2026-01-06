#minimum version of CMake required to run a program
cmake_minimum_required(VERSION 3.10)

#Name of the project 
project(Finantial-Data_Pipeline)

#C++ version
set(CMAKE_CXX_STANDARD 17)

#find redis
find_package(redis++, REQUIRED)

#instruction to CMake to create executable file from main.cpp
add_executable(market_pipeline src/main.cpp)
