FROM ubuntu:22.04

# Install everything in one layer
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    gdb \
    git \
    libpq-dev \
    libhiredis-dev \
    postgresql-client \
    redis-tools \
    clang-format \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash developer
USER developer
WORKDIR /home/developer/project

# Set environment variables
ENV CC=gcc
ENV CXX=g++

# Copy build script
COPY --chown=developer:developer scripts/build.sh /home/developer/build.sh

CMD ["/bin/bash"]
