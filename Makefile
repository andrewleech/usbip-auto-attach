# SPDX-FileCopyrightText: 2025 The Anon Kode Authors
# SPDX-License-Identifier: GPL-3.0-only

# Use MUSL-based cross-compilers for static linking
CXX_AMD64 ?= x86_64-linux-musl-g++
CXX_ARM64 ?= aarch64-linux-musl-g++

# Common flags for static linking with MUSL and optimization
COMMON_FLAGS = -std=c++17 -static -Os -Wall -Wextra -pthread

# Target specific flags
AMD64_FLAGS = $(COMMON_FLAGS)
ARM64_FLAGS = $(COMMON_FLAGS)

# Source files and generated header
SRCS = src/main.cpp
VERSION_HEADER = src/version.h
TEMPLATE_HEADER = version.h.in

# Output directories
OUT_DIR ?= ./build
TARGET_AMD64 = $(OUT_DIR)/x64/auto-attach
TARGET_ARM64 = $(OUT_DIR)/arm64/auto-attach

# Default target: build both architectures
all: $(TARGET_AMD64) $(TARGET_ARM64)

# Rule to generate version.h from template using git-versioner
$(VERSION_HEADER): $(TEMPLATE_HEADER)
	@echo "Generating $(VERSION_HEADER)..."
	@mkdir -p $(dir $@)
	@git-versioner --template $(TEMPLATE_HEADER) $@ # Use --template <template> <output>

# Target for amd64
$(TARGET_AMD64): $(SRCS) $(VERSION_HEADER)
	@echo "Building static MUSL executable for amd64..."
	@mkdir -p $(dir $@)
	$(CXX_AMD64) $(AMD64_FLAGS) $(SRCS) -o $@
	@echo "Built $@ successfully."

# Target for arm64
$(TARGET_ARM64): $(SRCS) $(VERSION_HEADER)
	@echo "Building static MUSL executable for arm64..."
	@mkdir -p $(dir $@)
	$(CXX_ARM64) $(ARM64_FLAGS) $(SRCS) -o $@
	@echo "Built $@ successfully."

# Clean target
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET_AMD64) $(TARGET_ARM64) $(VERSION_HEADER)
	@echo "Clean complete."

.PHONY: all clean
