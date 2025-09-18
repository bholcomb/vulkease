# VulkEase Makefile
# Builds libvulkease.so shared library from C sources

# Project configuration
PROJECT_NAME = vulkease
VERSION = 2.0.0
SO_VERSION = 2

# Directories
SRC_DIR = src
EXTERNAL_DIR = external
BUILD_DIR = build
LIB_DIR = lib

# Output library
LIBRARY = libvulkease.so

# Compilers and flags
CC = gcc
CXX = g++
CFLAGS = -std=c11 -fPIC -Wall -Wextra -O2 -DNDEBUG
CXXFLAGS = -std=c++11 -fPIC -Wall -Wextra -O2 -DNDEBUG

# Include directories
INCLUDES = -I$(SRC_DIR) -I$(EXTERNAL_DIR) -I/usr/include/vulkan

# Libraries to link against
LIBS = -lvulkan -lm -ldl -lpthread -lstdc++

# Source files (exclude problematic debug file)
C_SOURCES = $(filter-out $(SRC_DIR)/ve_debug.c, $(wildcard $(SRC_DIR)/*.c))
CXX_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)

# Object files
C_OBJECTS = $(C_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CXX_OBJECTS = $(CXX_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
OBJECTS = $(C_OBJECTS) $(CXX_OBJECTS)

# Default target
all: $(LIB_DIR)/$(LIBRARY)

# Create build and lib directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

# Compile C source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling C file $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile C++ source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo "Compiling C++ file $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Link shared library
$(LIB_DIR)/$(LIBRARY): $(OBJECTS) | $(LIB_DIR)
	@echo "Linking shared library $(LIBRARY)"
	$(CXX) -shared -Wl,-soname,$(LIBRARY) $(OBJECTS) $(LIBS) -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts"
	rm -rf $(BUILD_DIR) $(LIB_DIR)

# Show build information
info:
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "Library: $(LIBRARY)"
	@echo "Sources: $(C_SOURCES) $(CXX_SOURCES)"
	@echo "Objects: $(OBJECTS)"

.PHONY: all clean info
