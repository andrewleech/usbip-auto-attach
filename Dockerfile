# Use a Debian-based image with build tools
FROM debian:bookworm AS builder

ENV MUSL_VERSION=1.2.3
ENV MUSL_ARCH_AMD64=x86_64-linux-musl
ENV MUSL_ARCH_ARM64=aarch64-linux-musl
ENV CROSS_PREFIX=/opt/cross

# Ensure main contrib non-free are potentially available
# Using Bookworm sources
RUN echo "deb http://deb.debian.org/debian bookworm main contrib non-free non-free-firmware" > /etc/apt/sources.list && \
    echo "deb http://deb.debian.org/debian bookworm-updates main contrib non-free non-free-firmware" >> /etc/apt/sources.list && \
    echo "deb http://deb.debian.org/debian-security/ bookworm-security main contrib non-free non-free-firmware" >> /etc/apt/sources.list

# Update package lists first
RUN apt-get update

# Install prerequisites
# No boost needed anymore
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    wget \
    xz-utils \
    ca-certificates \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Download and extract MUSL cross-compilers
RUN mkdir -p ${CROSS_PREFIX} \
    && cd /tmp \
    && WGET_OPTS="--progress=dot:mega -nv" \
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

# Set working directory (will be overridden by docker run -w)
WORKDIR /app

# Optional: Verify compilers
# RUN which x86_64-linux-musl-g++ && which aarch64-linux-musl-g++
