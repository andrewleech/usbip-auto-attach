# Build configuration
BUILD_DIR := ./build
SRC_DIR := ./src
TEST_DIR := ./tests

# Source files
SRCS := $(SRC_DIR)/main.cpp $(SRC_DIR)/parser.cpp
TEST_SRCS := $(TEST_DIR)/parser_test.cpp $(SRC_DIR)/parser.cpp # Test includes parser implementation

# Object files (intermediate build step for clarity and correctness)
OBJS_AMD64 := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/obj/amd64/%.o,$(SRCS))
OBJS_ARM64 := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/obj/arm64/%.o,$(SRCS))

# Test object files (using host compiler)
# Need to map source paths correctly to object paths
TEST_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/obj/test/src_%.o,$(filter $(SRC_DIR)/%.cpp,$(TEST_SRCS))) \
             $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/obj/test/test_%.o,$(filter $(TEST_DIR)/%.cpp,$(TEST_SRCS)))

# Target executables
TARGET_AMD64 := $(BUILD_DIR)/x64/usbip-auto-attach
TARGET_ARM64 := $(BUILD_DIR)/arm64/usbip-auto-attach
TEST_TARGET := $(BUILD_DIR)/test/parser_test

# Cross compilers (default to musl paths if running in the builder container)
CXX_AMD64 ?= /opt/cross/bin/x86_64-linux-musl-g++
CXX_ARM64 ?= /opt/cross/bin/aarch64-linux-musl-g++
CXX_TEST ?= g++ # Use host compiler for tests

# Compiler and Linker flags
CPPFLAGS := -I$(SRC_DIR) -Wall -Wextra -std=c++17 -Os
LDFLAGS := -pthread -static
# Test flags might differ if needed, e.g., no static linking needed for host tests
TEST_CPPFLAGS := -I$(SRC_DIR) -Wall -Wextra -std=c++17 # Same CPPFLAGS for test compilation
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
	$(CXX_AMD64) $(OBJS_AMD64) -o $@ $(LDFLAGS)

$(BUILD_DIR)/obj/amd64/%.o: $(SRC_DIR)/%.cpp $(VERSION_HEADER) | $(BUILD_DIR)/obj/amd64
	@echo "Compiling AMD64 object: $<"
	$(CXX_AMD64) $(CPPFLAGS) -c $< -o $@

# --- ARM64 Build ---
$(TARGET_ARM64): $(OBJS_ARM64) | $(BUILD_DIR)/arm64
	@echo "Linking ARM64 target..."
	$(CXX_ARM64) $(OBJS_ARM64) -o $@ $(LDFLAGS)

$(BUILD_DIR)/obj/arm64/%.o: $(SRC_DIR)/%.cpp $(VERSION_HEADER) | $(BUILD_DIR)/obj/arm64
	@echo "Compiling ARM64 object: $<"
	$(CXX_ARM64) $(CPPFLAGS) -c $< -o $@

# --- Test Build ---
test: $(TEST_TARGET)
	@echo "Running tests..."
	@$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) | $(BUILD_DIR)/test
	@echo "Linking test target..."
	$(CXX_TEST) $(TEST_OBJS) -o $@ $(TEST_LDFLAGS)

# Test object compilation rules
# Rule for test files in TEST_DIR
$(BUILD_DIR)/obj/test/test_%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)/obj/test
	@echo "Compiling test object: $<"
	$(CXX_TEST) $(TEST_CPPFLAGS) -c $< -o $@

# Rule for source files in SRC_DIR needed by tests
$(BUILD_DIR)/obj/test/src_%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)/obj/test
	@echo "Compiling test dependency object: $<"
	$(CXX_TEST) $(TEST_CPPFLAGS) -c $< -o $@


# --- Directory Creation ---
$(BUILD_DIR)/x64 $(BUILD_DIR)/arm64 $(BUILD_DIR)/test \
$(BUILD_DIR)/obj/amd64 $(BUILD_DIR)/obj/arm64 $(BUILD_DIR)/obj/test:
	@mkdir -p $@

# --- Clean ---
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(VERSION_HEADER)
