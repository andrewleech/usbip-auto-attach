# SPDX-FileCopyrightText: 2025 The Anon Kode Authors
# SPDX-License-Identifier: GPL-3.0-only

# Use MUSL-based cross-compilers for static linking
# Assumes compilers like x86_64-linux-musl-g++ and aarch64-linux-musl-g++ are in the PATH
# (These are provided by the Dockerfile or need to be installed manually)
CXX_AMD64 ?= x86_64-linux-musl-g++
CXX_ARM64 ?= aarch64-linux-musl-g++

# Common flags for static linking with MUSL and optimization
# The -static flag is crucial for MUSL linking. Boost libraries are linked statically by default when found.
# Ensure Boost development headers/static libs for MUSL are available in the build environment.
COMMON_FLAGS = -std=c++17 -static -Os -Wall -Wextra -pthread -lboost_program_options -lboost_system -lboost_process

# Target specific flags (can add target-specific optimizations if needed)
AMD64_FLAGS = $(COMMON_FLAGS)
ARM64_FLAGS = $(COMMON_FLAGS)

# Source files
SRCS = src/main.cpp

# Output directories (relative to the Makefile location)
# Can be overridden e.g., OUT_DIR=/some/other/path make all
OUT_DIR ?= ../
TARGET_AMD64 = $(OUT_DIR)/x64/auto-attach
TARGET_ARM64 = $(OUT_DIR)/arm64/auto-attach

# Default target: build both architectures
all: $(TARGET_AMD64) $(TARGET_ARM64)

# Target for amd64
$(TARGET_AMD64): $(SRCS)
	@echo "Building static MUSL executable for amd64..."
	@mkdir -p $(dir $@)
	$(CXX_AMD64) $(AMD64_FLAGS) $(SRCS) -o $@
	@echo "Built $@ successfully."

# Target for arm64
$(TARGET_ARM64): $(SRCS)
	@echo "Building static MUSL executable for arm64..."
	@mkdir -p $(dir $@)
	$(CXX_ARM64) $(ARM64_FLAGS) $(SRCS) -o $@
	@echo "Built $@ successfully."

# Clean target
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET_AMD64) $(TARGET_ARM64)
	@echo "Clean complete."

.PHONY: all clean
