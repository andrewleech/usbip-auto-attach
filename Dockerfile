# Use a Debian-based image with build tools
FROM debian:bullseye-slim AS builder

ENV MUSL_VERSION=1.2.3
ENV MUSL_ARCH_AMD64=x86_64-linux-musl
ENV MUSL_ARCH_ARM64=aarch64-linux-musl
ENV CROSS_PREFIX=/opt/cross

# Install prerequisites
# - build-essential: Basic build tools
# - wget, xz-utils: For downloading and extracting toolchains
# - libboost-*-dev: Boost development headers (linking handled by MUSL toolchain)
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    wget \
    xz-utils \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-process-dev \
    ca-certificates \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Download and extract MUSL cross-compilers
RUN mkdir -p ${CROSS_PREFIX} \
    && cd /tmp \
    && WGET_OPTS="--progress=dot:mega -nv"
    && echo "Downloading MUSL toolchain for ${MUSL_ARCH_AMD64}..." \
    && wget ${WGET_OPTS} https://musl.cc/${MUSL_ARCH_AMD64}-cross.tgz \
    && echo "Downloading MUSL toolchain for ${MUSL_ARCH_ARM64}..." \
    && wget ${WGET_OPTS} https://musl.cc/${MUSL_ARCH_ARM64}-cross.tgz \
    && echo "Extracting toolchains..." \
    && tar -xzf ${MUSL_ARCH_AMD64}-cross.tgz -C ${CROSS_PREFIX} --strip-components=1 \
    && tar -xzf ${MUSL_ARCH_ARM64}-cross.tgz -C ${CROSS_PREFIX} --strip-components=1 \
    && rm -f ${MUSL_ARCH_AMD64}-cross.tgz ${MUSL_ARCH_ARM64}-cross.tgz

# Add cross-compiler bin directories to PATH
ENV PATH=${CROSS_PREFIX}/bin:${PATH}

# Set working directory where source will be mounted
WORKDIR /app

# Build command can be run via docker run, assuming source is mounted to /app
# Example: docker run --rm -v $(pwd):/app -v $(pwd)/../output:/build_output <image_name> make all OUT_DIR=/build_output

# Optional: Verify compilers are found
# RUN which x86_64-linux-musl-g++ && which aarch64-linux-musl-g++
