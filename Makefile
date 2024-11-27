# This is a comment explaining the purpose of this variable
ASSEMBLY := src

# Build directory where the output files will be placed
BUILD_DIR := bin

# Compiler and flags
CXX := g++                             # Use g++ as the compiler
CXXFLAGS := -std=c++20                 # Enable C++20 standard

# Include directories
INCLUDE_FLAGS := -Isrc -I/opt/homebrew/include -I$(VULKAN_SDK)/include $(shell pkg-config --cflags glfw3)

# Linker options
LINKER_FLAGS := -L/opt/homebrew/lib -L$(VULKAN_SDK)/lib $(shell pkg-config --libs glfw3) -lvulkan -rpath /usr/local/lib

# Source files
SRC_FILES := $(shell find $(ASSEMBLY) -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.m" \))

# The build target
.PHONY: build
build: $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SRC_FILES) -o $(BUILD_DIR)/app $(INCLUDE_FLAGS) $(LINKER_FLAGS)

# Create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean target to remove built files
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

# Run the application after building it
.PHONY: run
run: build
	@$(BUILD_DIR)/app