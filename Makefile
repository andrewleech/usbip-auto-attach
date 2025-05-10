# Build configuration
BUILD_DIR := ./build
SRC_DIR := ./src
TEST_DIR := ./tests

# Source files (C files now instead of C++)
SRCS := $(SRC_DIR)/main.c $(SRC_DIR)/parser.c
TEST_SRCS := $(TEST_DIR)/parser_test.c $(SRC_DIR)/parser.c # Test includes parser implementation

# Object files (intermediate build step for clarity and correctness)
OBJS_AMD64 := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/obj/amd64/%.o,$(SRCS))
OBJS_ARM64 := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/obj/arm64/%.o,$(SRCS))

# Test object files (using host compiler)
# Need to map source paths correctly to object paths
TEST_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/obj/test/src_%.o,$(filter $(SRC_DIR)/%.c,$(TEST_SRCS))) \
             $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/obj/test/test_%.o,$(filter $(TEST_DIR)/%.c,$(TEST_SRCS)))

# Target executables
TARGET_AMD64 := $(BUILD_DIR)/x64/usbip-auto-attach
TARGET_ARM64 := $(BUILD_DIR)/arm64/usbip-auto-attach
TEST_TARGET := $(BUILD_DIR)/test/parser_test

# Cross compilers (default to musl paths if running in the builder container)
# Use gcc instead of g++ for C code
CC_AMD64 ?= /opt/cross/bin/x86_64-linux-musl-gcc
OBJCOPY_AMD64 ?= /opt/cross/bin/x86_64-linux-musl-objcopy
STRIP_AMD64 ?= /opt/cross/bin/x86_64-linux-musl-strip
CC_ARM64 ?= /opt/cross/bin/aarch64-linux-musl-gcc
OBJCOPY_ARM64 ?= /opt/cross/bin/aarch64-linux-musl-objcopy
STRIP_ARM64 ?= /opt/cross/bin/aarch64-linux-musl-strip
CC_TEST ?= gcc # Use host compiler for tests

# Compiler and Linker flags for C (changed from C++)
CFLAGS := -I$(SRC_DIR) -Wall -Wextra -std=c99 -Os -g
LDFLAGS := -pthread -static
# Test flags
TEST_CFLAGS := -I$(SRC_DIR) -Wall -Wextra -std=c99 # Same CFLAGS for test compilation
TEST_LDFLAGS := -pthread    # Don't force static linking for tests

# Version generation
VERSION_HEADER := $(SRC_DIR)/version.h
VERSION_TEMPLATE := version.h.in

.PHONY: all clean test

all: $(TARGET_AMD64) $(TARGET_ARM64)

# --- Version Header Generation ---
$(VERSION_HEADER): $(VERSION_TEMPLATE) FORCE
	@echo "Generating version header..."
	@mkdir -p $(dir $@)
	@/usr/local/bin/git-versioner --template $(VERSION_TEMPLATE) $(VERSION_HEADER) || echo "#define AUTO_ATTACH_VERSION \"unknown\"" > $(VERSION_HEADER)

FORCE: ;

# --- AMD64 Build ---
$(TARGET_AMD64): $(OBJS_AMD64) | $(BUILD_DIR)/x64
	@echo "Linking AMD64 target..."
	$(CC_AMD64) $(OBJS_AMD64) -o $@ $(LDFLAGS)
	@echo "Splitting off the debug symbols..."
	$(OBJCOPY_AMD64) --only-keep-debug $(TARGET_AMD64) $(TARGET_AMD64).debug
	@echo "Stripping the target..."
	$(STRIP_AMD64) --strip-unneeded $(TARGET_AMD64)
	@echo "Adding link to debug symbols..."
	$(OBJCOPY_AMD64) --add-gnu-debuglink=$(TARGET_AMD64).debug $(TARGET_AMD64)

$(BUILD_DIR)/obj/amd64/%.o: $(SRC_DIR)/%.c $(VERSION_HEADER) | $(BUILD_DIR)/obj/amd64
	@echo "Compiling AMD64 object: $<"
	$(CC_AMD64) $(CFLAGS) -c $< -o $@

# --- ARM64 Build ---
$(TARGET_ARM64): $(OBJS_ARM64) | $(BUILD_DIR)/arm64
	@echo "Linking ARM64 target..."
	$(CC_ARM64) $(OBJS_ARM64) -o $@ $(LDFLAGS)
	@echo "Splitting off the debug symbols..."
	$(OBJCOPY_ARM64) --only-keep-debug $(TARGET_ARM64) $(TARGET_ARM64).debug
	@echo "Stripping the target..."
	$(OBJCOPY_ARM64) --strip-unneeded $(TARGET_ARM64)
	@echo "Adding link to debug symbols..."
	$(OBJCOPY_ARM64) --add-gnu-debuglink=$(TARGET_ARM64).debug $(TARGET_ARM64)

$(BUILD_DIR)/obj/arm64/%.o: $(SRC_DIR)/%.c $(VERSION_HEADER) | $(BUILD_DIR)/obj/arm64
	@echo "Compiling ARM64 object: $<"
	$(CC_ARM64) $(CFLAGS) -c $< -o $@

# --- Test Build ---
test: $(TEST_TARGET)
	@echo "Running tests..."
	@$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) | $(BUILD_DIR)/test
	@echo "Linking test target..."
	$(CC_TEST) $(TEST_OBJS) -o $@ $(TEST_LDFLAGS)

# Test object compilation rules
# Rule for test files in TEST_DIR
$(BUILD_DIR)/obj/test/test_%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)/obj/test
	@echo "Compiling test object: $<"
	$(CC_TEST) $(TEST_CFLAGS) -c $< -o $@

# Rule for source files in SRC_DIR needed by tests
$(BUILD_DIR)/obj/test/src_%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/obj/test
	@echo "Compiling test dependency object: $<"
	$(CC_TEST) $(TEST_CFLAGS) -c $< -o $@

# --- Directory Creation ---
$(BUILD_DIR)/x64 $(BUILD_DIR)/arm64 $(BUILD_DIR)/test \
$(BUILD_DIR)/obj/amd64 $(BUILD_DIR)/obj/arm64 $(BUILD_DIR)/obj/test:
	@mkdir -p $@

# --- Clean ---
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(VERSION_HEADER)
