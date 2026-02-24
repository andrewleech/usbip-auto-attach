# Use a Debian-based image with build tools
FROM debian:bookworm AS builder

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
# Need git and python3-pip for git-versioner
# Need gdb for VS Code Dev Container debugging
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    wget \
    xz-utils \
    unzip \
    ca-certificates \
    git \
    python3 \
    python3-pip \
    gdb \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install git-versioner
RUN pip3 install --break-system-packages git-versioner

# Download and extract MUSL cross-compilers
# musl.cc is unreliable; use just-containers/musl-cross-make GitHub releases instead.
# These provide the same x86_64-linux-musl / aarch64-linux-musl toolchain naming.
RUN mkdir -p ${CROSS_PREFIX} \
    && cd /tmp \
    && WGET_OPTS="--progress=dot:mega -nv" \
    && MUSL_CROSS_BASE="https://github.com/just-containers/musl-cross-make/releases/download/v15" \
    && echo "Downloading MUSL toolchain for ${MUSL_ARCH_AMD64}..." \
    && wget ${WGET_OPTS} ${MUSL_CROSS_BASE}/gcc-9.2.0-${MUSL_ARCH_AMD64}.tar.xz \
    && echo "Downloading MUSL toolchain for ${MUSL_ARCH_ARM64}..." \
    && wget ${WGET_OPTS} ${MUSL_CROSS_BASE}/gcc-9.2.0-${MUSL_ARCH_ARM64}.tar.xz \
    && echo "Extracting toolchains..." \
    && tar -xJf gcc-9.2.0-${MUSL_ARCH_AMD64}.tar.xz -C ${CROSS_PREFIX} --strip-components=1 \
    && tar -xJf gcc-9.2.0-${MUSL_ARCH_ARM64}.tar.xz -C ${CROSS_PREFIX} --strip-components=1 \
    && rm -f gcc-9.2.0-${MUSL_ARCH_AMD64}.tar.xz gcc-9.2.0-${MUSL_ARCH_ARM64}.tar.xz

# Download and extract usbip binary (x64)
RUN cd /tmp \
    && WGET_OPTS="--progress=dot:kilo -nv" \
    && echo "Downloading usbip binaries..." \
    && wget ${WGET_OPTS} https://github.com/dorssel/usbipd-win-wsl/releases/download/v1.1.0/linux-binaries.zip \
    && echo "Extracting binaries..." \
    && unzip linux-binaries.zip -d linux-binaries \
    && echo "Keeping x64 binary..." \
    && mv linux-binaries/x64/usbip /bin \
    && rm -rf linux-binaries linux-binaries.zip

# Add cross-compiler bin directories AND Python user bin to PATH
ENV PATH=/root/.local/bin:${CROSS_PREFIX}/bin:${PATH}

# Set working directory (will be overridden by docker run -w)
WORKDIR /app

# Optional: Verify compilers
# RUN which x86_64-linux-musl-g++ && which aarch64-linux-musl-g++
