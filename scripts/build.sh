#!/bin/bash

set -e

# Configuration
PROJECT_NAME="myproject"
BUILD_DIR="build"
SRC_DIR="src"
INC_DIR="include"

# Compiler settings
CXX="g++"
CXX_STD="-std=c++17"
CXX_FLAGS="-Wall -Wextra -Wpedantic -O2 -g"

# Dependencies
POSTGRES_FLAGS="-I/usr/include/postgresql -lpq"
REDIS_FLAGS="-I/usr/include/hiredis -lhiredis"

# Create build directory
mkdir -p "$BUILD_DIR"

# Find all source files
SRC_FILES=$(find "$SRC_DIR" -name "*.cpp" | tr '\n' ' ')

echo "Building $PROJECT_NAME..."

# Compile and link
$CXX $CXX_STD $CXX_FLAGS \
    -I"$INC_DIR" \
    $POSTGRES_FLAGS \
    $REDIS_FLAGS \
    $SRC_FILES \
    -o "$BUILD_DIR/$PROJECT_NAME"

echo "Build complete: $BUILD_DIR/$PROJECT_NAME"
