#!/bin/bash

echo "Installing dependencies..."
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    g++ \
    libpq-dev \
    libhiredis-dev \
    postgresql-client \
    redis-tools \
    clang-format

echo "Done!"
