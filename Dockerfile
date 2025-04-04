\
# Use a Debian-based image with build tools
FROM debian:bullseye-slim AS builder

# Install prerequisites
# - build-essential: Basic C/C++ build tools (make, etc.)
# - g++-x86-64-linux-musl: MUSL cross-compiler for amd64
# - g++-aarch64-linux-musl: MUSL cross-compiler for arm64
# - libboost-*-dev: Boost development libraries needed for linking
#   (We need the host versions of the headers, the MUSL toolchain handles static linking)
# Using non-interactive frontend to avoid prompts during installation
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    g++-x86-64-linux-musl \
    g++-aarch64-linux-musl \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-process-dev \
    ca-certificates \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Source code and Makefile will be mounted into /app during build

# Set working directory where source will be mounted
WORKDIR /app

# Build command can be run via docker run, assuming source is mounted to /app
# Example: docker run --rm -v $(pwd):/app -v $(pwd)/../output:/build_output auto-attach-builder make all OUT_DIR=/build_output

# Optional: Create a smaller final image if needed, copying only the executables
# FROM scratch
# COPY --from=builder /app/../x64/auto-attach /auto-attach-amd64
# COPY --from=builder /app/../arm64/auto-attach /auto-attach-arm64
# (This part is commented out as the primary goal is to use the builder image in CI)
