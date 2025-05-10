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
