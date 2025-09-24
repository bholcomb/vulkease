# VulkEase Makefile
# Builds libvulkease.so shared library and examples

# Project configuration
PROJECT_NAME = vulkease
VERSION = 2.0.0
SO_VERSION = 2

# Directories
SRC_DIR = src
EXTERNAL_DIR = external
EXAMPLES_DIR = examples
BUILD_DIR = build
BIN_DIR = bin
SHADERS_DIR = $(EXAMPLES_DIR)/shaders
DATA_DIR = $(EXAMPLES_DIR)/data
DATA_DEST_DIR = $(BIN_DIR)/$(DATA_DIR)


# Output library and examples
LIBRARY = libvulkease.so
EXAMPLE_TRIANGLE = 01_triangle
EXAMPLE_PARTICLES = 02_compute_particles
EXAMPLE_CUBE = 03_cube

# Build config: debug (default) or release
CONFIG ?= debug

# Compilers and flags
CC = gcc
CXX = g++

CFLAGS = -std=c11 -fPIC -Wall -Wextra
CXXFLAGS = -std=c++14 -fPIC -Wall -Wextra -std=c++17
EXAMPLE_CFLAGS = -std=c11 -Wall -Wextra


# Per-config flags
ifeq ($(CONFIG),debug)
  CFLAGS   += -O0 -DDEBUG -g
  CXXFLAGS += -O0 -DDEBUG -g
  EXAMPLE_CFLAGS += -O0 -DDEBUG -g
else ifeq ($(CONFIG),release)
  CFLAGS   += -O3
  CXXFLAGS += -O3
  EXAMPLE_CFLAGS += -O3
else
  $(error Unknown CONFIG '$(CONFIG)'; use CONFIG=debug or CONFIG=release)
endif


# Include directories
INCLUDES = -I$(SRC_DIR) -I$(EXTERNAL_DIR) -I/usr/include/vulkan

# Libraries to link against
LIBS = -lvulkan -lm -ldl -lpthread -lstdc++
EXAMPLE_LIBS = -L$(BIN_DIR) -lvulkease -lglfw -lm

# Source files (exclude stub debug file, use full implementation)
C_SOURCES = $(wildcard $(SRC_DIR)/*.c)
CXX_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)

# Object files
C_OBJECTS = $(C_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CXX_OBJECTS = $(CXX_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
OBJECTS = $(C_OBJECTS) $(CXX_OBJECTS)

# Shader files
SHADER_SOURCES = $(wildcard $(SHADERS_DIR)/*.vert $(SHADERS_DIR)/*.frag $(SHADERS_DIR)/*.comp)
SHADER_SPIRV = $(SHADER_SOURCES:%=$(BIN_DIR)/%.spv)

#Data files
PNG_FILES  := $(wildcard $(DATA_DIR)/*.png)
GLTF_FILES := $(wildcard $(DATA_DIR)/*.gltf)
ASSET_FILES := $(PNG_FILES) $(GLTF_FILES)

# Default target
all: library examples

# Build library
library: $(BIN_DIR)/$(LIBRARY)

# Build examples
examples: shaders assets $(BIN_DIR)/$(EXAMPLE_TRIANGLE) $(BIN_DIR)/$(EXAMPLE_PARTICLES) $(BIN_DIR)/$(EXAMPLE_CUBE)

# Build shaders
shaders: $(SHADER_SPIRV)

#copy data
# Map source files to destination paths
DEST_ASSETS := $(patsubst $(DATA_DIR)/%,$(DATA_DEST_DIR)/%,$(ASSET_FILES))

# Copy all assets to DEST_DIR
assets: $(DATA_DEST_DIR) $(DEST_ASSETS)
	@echo "Assets copied to $(DATA_DEST_DIR)"

# Pattern rule: copy each file, ensuring the directory exists
$(DATA_DEST_DIR)/%: $(DATA_DIR)/% | $(DATA_DEST_DIR)
	@cp $< $@

# Ensure destination directory exists
$(DATA_DEST_DIR):
	@mkdir -p $(DATA_DEST_DIR)

# Create build and bin directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Compile C source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling C file $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile C++ source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo "Compiling C++ file $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Link shared library
$(BIN_DIR)/$(LIBRARY): $(OBJECTS) | $(BIN_DIR)
	@echo "Linking shared library $(LIBRARY)"
	$(CXX) -shared -Wl,-soname,$(LIBRARY) $(OBJECTS) $(LIBS) -o $@

# Compile GLSL shaders to SPIR-V
$(BIN_DIR)/%.vert.spv: %.vert | $(BIN_DIR)
	@echo "Compiling vertex shader $<"
	@mkdir -p $(dir $@)
	glslangValidator -V --target-env vulkan1.3 -o $@ $<

$(BIN_DIR)/%.frag.spv: %.frag | $(BIN_DIR)
	@echo "Compiling fragment shader $<"
	@mkdir -p $(dir $@)
	glslangValidator -V --target-env vulkan1.3 -o $@ $<

$(BIN_DIR)/%.comp.spv: %.comp | $(BIN_DIR)
	@echo "Compiling compute shader $<"
	@mkdir -p $(dir $@)
	glslangValidator -V --target-env vulkan1.3 -o $@ $<

$(DATA_BIN): $(DATA_SRC_DIR)/$(TEXTURE_FILE) | $(DATA_BIN_DIR)
	@echo "Copying data: $< -> $@"
	@cp $< $@

# Build triangle example
$(BIN_DIR)/$(EXAMPLE_TRIANGLE): $(EXAMPLES_DIR)/$(EXAMPLE_TRIANGLE).c $(BIN_DIR)/$(LIBRARY) | $(BIN_DIR)
	@echo "Building example: $(EXAMPLE_TRIANGLE)"
	$(CC) $(EXAMPLE_CFLAGS) $(INCLUDES) -Wl,-rpath,'$$ORIGIN' $< $(EXAMPLE_LIBS) -o $@

# Build compute particles example
$(BIN_DIR)/$(EXAMPLE_PARTICLES): $(EXAMPLES_DIR)/$(EXAMPLE_PARTICLES).c $(BIN_DIR)/$(LIBRARY) | $(BIN_DIR)
	@echo "Building example: $(EXAMPLE_PARTICLES)"
	$(CC) $(EXAMPLE_CFLAGS) $(INCLUDES) -Wl,-rpath,'$$ORIGIN' $< $(EXAMPLE_LIBS) -o $@

# Build cube example
$(BIN_DIR)/$(EXAMPLE_CUBE): $(EXAMPLES_DIR)/$(EXAMPLE_CUBE).c $(BIN_DIR)/$(LIBRARY) | $(BIN_DIR)
	@echo "Building example: $(EXAMPLE_CUBE)"
	$(CC) $(EXAMPLE_CFLAGS) $(INCLUDES) -Wl,-rpath,'$$ORIGIN' $< $(EXAMPLE_LIBS) -o $@

# Install system dependencies (requires sudo)
install-deps:
	@echo "Installing system dependencies..."
	sudo apt-get update
	sudo apt-get install -y libglfw3-dev libvulkan-dev vulkan-tools glslang-tools

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts"
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Clean only shaders
clean-shaders:
	@echo "Cleaning compiled shaders"
	rm -f $(SHADER_SPIRV)

# Show build information
info:
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "Library: $(LIBRARY)"
	@echo "Examples: $(EXAMPLE_TRIANGLE) $(EXAMPLE_PARTICLES)"
	@echo "Sources: $(C_SOURCES) $(CXX_SOURCES)"
	@echo "Objects: $(OBJECTS)"

.PHONY: all library examples shaders install-deps clean clean-shaders info
