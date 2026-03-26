# This is a comment explaining the purpose of this variable
ASSEMBLY := src

# Build directory where the output files will be placed
BUILD_DIR := bin

UNAME_S := $(shell uname -s)
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS := $(shell pkg-config --libs glfw3 2>/dev/null)

# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20

# Include directories
INCLUDE_FLAGS := -Isrc -Isrc/imgui $(GLFW_CFLAGS)

# Linker options
LINKER_FLAGS := $(GLFW_LIBS) -lvulkan

ifeq ($(UNAME_S),Darwin)
	CXXFLAGS += -x objective-c++
	INCLUDE_FLAGS += -I/opt/homebrew/include
	LINKER_FLAGS += -L/opt/homebrew/lib -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo -Wl,-rpath,/usr/local/lib
endif

ifeq ($(UNAME_S),Linux)
	LINKER_FLAGS += -ldl -lpthread
endif

ifneq ($(strip $(VULKAN_SDK)),)
	INCLUDE_FLAGS += -I$(VULKAN_SDK)/include
	LINKER_FLAGS += -L$(VULKAN_SDK)/lib
endif

# Source files
SRC_FILES := $(shell find $(ASSEMBLY) -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.m" \))

# The build target
.PHONY: build
build: $(BUILD_DIR)
	sh compileShaders.sh
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