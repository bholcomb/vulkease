# VulkEase SDL/GLFW Integration Guide

**Complete examples showing how to integrate VulkEase with SDL and GLFW window libraries**

---

## Overview

This guide provides complete working examples of integrating VulkEase with the two most popular cross-platform window management libraries:

- **SDL2** - Simple DirectMedia Layer (widely used in games and multimedia)
- **GLFW** - Graphics Library Framework (popular in graphics programming)

Both examples demonstrate:
- ✅ **Proper window creation** with Vulkan support
- ✅ **VulkEase swapchain setup** with automatic surface handling
- ✅ **Complete rendering loop** with frame timing
- ✅ **Window resize handling** and swapchain recreation
- ✅ **Input handling** (keyboard and mouse)
- ✅ **Resource management** and cleanup

---

## Key Integration Points

### **Swapchain Creation - The Critical Step**

Both examples show how VulkEase simplifies swapchain creation:

```c
// SDL Integration
SDL_Window* window = SDL_CreateWindow("App", x, y, w, h, SDL_WINDOW_VULKAN);
VESwapchain* swapchain = veCreateSwapchain(device, window, width, height);

// GLFW Integration  
GLFWwindow* window = glfwCreateWindow(width, height, "App", NULL, NULL);
VESwapchain* swapchain = veCreateSwapchain(device, window, width, height);
```

**VulkEase handles all the complex Vulkan surface creation internally!**

### **Frame Rendering Loop**

Both examples use the same VulkEase rendering pattern:

```c
// Begin frame
VECommandBuffer* cmd = veBeginFrame(swapchain);

// Begin render pass
VEColor clearColor = { 0.1f, 0.2f, 0.3f, 1.0f };
veBeginRenderPass(cmd, swapchain, &clearColor);

// Set viewport and scissor
veSetViewport(cmd, 0, 0, width, height, 0.0f, 1.0f);
veSetScissor(cmd, 0, 0, width, height);

// Bind pipeline and draw - NO DESCRIPTOR SETS!
veBindPipeline(cmd, pipeline);
vePushConstants(cmd, &pushData, sizeof(pushData), 0);
veDraw(cmd, vertexBuffer, vertexCount, 1, 0, 0);

// End render pass and present
veEndRenderPass(cmd);
VEResult result = veEndFrameAndPresent(cmd, swapchain);
```

---

## Example 1: VulkEase + SDL2

**File:** `vulkease_sdl_example.c`

### **Features Demonstrated:**
- SDL2 window creation with Vulkan support
- Animated triangle with procedural textures
- Window resize handling
- Simple input (ESC to exit)
- Performance timing and FPS display

### **Key Code Highlights:**

#### **SDL Window Setup:**
```c
// Initialize SDL with Vulkan support
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

SDL_Window* window = SDL_CreateWindow(
    "VulkEase + SDL Example",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    1280, 720,
    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE  // Key flag!
);
```

#### **VulkEase Integration:**
```c
// Create VulkEase context and device
VEContext* context = veCreateContext("VulkEase SDL Example");
VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);

// Create swapchain - VulkEase handles SDL surface internally
VESwapchain* swapchain = veCreateSwapchain(device, window, width, height);
```

#### **Bindless Rendering:**
```c
// Push constants with resource indices - THE BINDLESS WAY!
PushConstants pushData = {
    .vertexBuffer = vertexBuffer,
    .texture = texture,
    .sampler = sampler,
    .time = currentTime,
    .aspectRatio = (float)width / (float)height
};

vePushConstants(cmd, &pushData, sizeof(pushData), 0);
veDraw(cmd, vertexBuffer, 3, 1, 0, 0);  // NO BINDING!
```

### **Compilation:**
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libvulkease-dev
gcc -o vulkease_sdl vulkease_sdl_example.c -lSDL2 -lvulkease -lm

# macOS
brew install sdl2 vulkease
gcc -o vulkease_sdl vulkease_sdl_example.c -lSDL2 -lvulkease -lm

# Windows (MinGW)
pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-vulkease
gcc -o vulkease_sdl.exe vulkease_sdl_example.c -lSDL2 -lvulkease -lm
```

---

## Example 2: VulkEase + GLFW

**File:** `vulkease_glfw_example.c`

### **Features Demonstrated:**
- GLFW window creation with Vulkan support
- 3D cube rendering with lighting
- Multiple cubes with different scales
- Full input handling (keyboard and mouse)
- Procedural brick and normal map textures
- Advanced uniform buffer usage

### **Key Code Highlights:**

#### **GLFW Window Setup:**
```c
// Initialize GLFW
glfwInit();
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // No OpenGL!
glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

GLFWwindow* window = glfwCreateWindow(1280, 720, "VulkEase + GLFW", NULL, NULL);

// Set up callbacks
glfwSetKeyCallback(window, keyCallback);
glfwSetFramebufferSizeCallback(window, resizeCallback);
glfwSetCursorPosCallback(window, mouseCallback);
```

#### **VulkEase Integration:**
```c
// Create VulkEase context and device  
VEContext* context = veCreateContext("VulkEase GLFW Example");
VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);

// Create swapchain - VulkEase handles GLFW surface internally
VESwapchain* swapchain = veCreateSwapchain(device, window, width, height);
```

#### **Advanced Rendering:**
```c
// Multiple resources in uniform buffer
typedef struct {
    float mvpMatrix[16];
    float lightPos[4];
    uint32_t diffuseTexture;  // Bindless texture index
    uint32_t normalTexture;   // Bindless normal map index  
    uint32_t sampler;         // Bindless sampler index
} UniformData;

// Draw multiple cubes with different properties
for (int i = 0; i < 5; i++) {
    PushConstants pushData = {
        .vertexBuffer = vertexBuffer,
        .uniformBuffer = uniformBuffer,
        .objectScale = 0.5f + i * 0.2f
    };
    
    vePushConstants(cmd, &pushData, sizeof(pushData), 0);
    veDrawIndexed(cmd, vertexBuffer, 0, 36, 1, 0, 0, i);
}
```

### **Compilation:**
```bash
# Ubuntu/Debian
sudo apt install libglfw3-dev libvulkease-dev
gcc -o vulkease_glfw vulkease_glfw_example.c -lglfw -lvulkease -lm

# macOS
brew install glfw vulkease
gcc -o vulkease_glfw vulkease_glfw_example.c -lglfw -lvulkease -lm

# Windows (MinGW)
pacman -S mingw-w64-x86_64-glfw mingw-w64-x86_64-vulkease
gcc -o vulkease_glfw.exe vulkease_glfw_example.c -lglfw -lvulkease -lm
```

---

## Comparison: SDL vs GLFW with VulkEase

### **SDL2 Advantages:**
- **Multimedia support** - Audio, input devices, threading
- **Game-focused** - Joystick, haptic feedback, sensors  
- **Cross-platform** - Wide platform support including mobile
- **Established ecosystem** - Many games and engines use SDL

### **GLFW Advantages:**
- **Lightweight** - Focused specifically on windowing and input
- **Modern design** - Clean, consistent API
- **Better callbacks** - More granular event handling
- **Graphics-focused** - Designed for OpenGL/Vulkan applications

### **VulkEase Integration - Both are Simple:**

```c
// SDL
VESwapchain* swapchain = veCreateSwapchain(device, sdl_window, w, h);

// GLFW  
VESwapchain* swapchain = veCreateSwapchain(device, glfw_window, w, h);
```

**VulkEase abstracts away all the Vulkan surface complexity!**

---

## Window Resize Handling

Both examples handle window resizing properly:

### **SDL Resize:**
```c
void handleWindowResize(SDLVulkEaseApp* app, int newWidth, int newHeight) {
    // Resize VulkEase swapchain
    VEResult result = veResizeSwapchain(app->swapchain, newWidth, newHeight);
    if (result != VE_SUCCESS) {
        printf("Failed to resize swapchain: %d\n", result);
    }
}

// In event loop
case SDL_WINDOWEVENT:
    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        handleWindowResize(app, event.window.data1, event.window.data2);
    }
    break;
```

### **GLFW Resize:**
```c
static void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    GLFWVulkEaseApp* app = (GLFWVulkEaseApp*)glfwGetWindowUserPointer(window);
    
    if (width > 0 && height > 0) {
        VEResult result = veResizeSwapchain(app->swapchain, width, height);
        if (result != VE_SUCCESS) {
            printf("Failed to resize swapchain: %d\n", result);
        }
    }
}

glfwSetFramebufferSizeCallback(window, glfwFramebufferSizeCallback);
```

---

## Error Handling

Both examples include comprehensive error handling:

### **Swapchain Out of Date:**
```c
VEResult result = veEndFrameAndPresent(cmd, swapchain);
if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
    // Get current window size
    int width, height;
    SDL_GetWindowSize(window, &width, &height);  // or glfwGetFramebufferSize
    
    // Recreate swapchain
    veResizeSwapchain(swapchain, width, height);
}
```

### **Resource Creation Failures:**
```c
VETextureIndex texture = veCreateTexture2D(device, w, h, format, usage, data, size);
if (texture == VE_INVALID_TEXTURE_INDEX) {
    printf("Failed to create texture: %s\n", veGetLastError());
    return false;
}
```

---

## Running the Examples

### **Expected Output (SDL):**
```
=== VulkEase + SDL Integration Example ===
Initializing SDL...
✓ SDL window created (1280x720)
Initializing VulkEase...
✓ VulkEase device: NVIDIA GeForce RTX 4070
✓ VulkEase swapchain created
Creating rendering resources...
✓ All rendering resources created

Initialization complete!
Controls:
  ESC - Exit
  Window is resizable

Starting main loop...
Frame 60: 60.0 FPS, Time: 1.00s
Frame 120: 59.8 FPS, Time: 2.01s
...
```

### **Expected Output (GLFW):**
```
=== VulkEase + GLFW Integration Example ===
Initializing GLFW...
✓ GLFW window created (1280x720)
Initializing VulkEase...
✓ VulkEase device: NVIDIA GeForce RTX 4070
  Max texture: 32768x32768, Max MSAA: 64x, Timestamp: 1.00ns
✓ VulkEase swapchain created
Creating rendering resources...
✓ Generated mipmaps
✓ All rendering resources created

Initialization complete!
Starting main loop...
Controls:
  ESC - Exit
  Window is resizable
  Mouse supported

Frame 120: 60.1 FPS, Time: 2.00s
...
```

---

## CMake Build System

For more complex projects, use CMake:

### **CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.12)
project(VulkEaseExamples)

set(CMAKE_C_STANDARD 99)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(VULKEASE REQUIRED vulkease)

# SDL Example
find_package(SDL2 REQUIRED)
add_executable(vulkease_sdl vulkease_sdl_example.c)
target_link_libraries(vulkease_sdl ${SDL2_LIBRARIES} ${VULKEASE_LIBRARIES})
target_include_directories(vulkease_sdl PRIVATE ${SDL2_INCLUDE_DIRS} ${VULKEASE_INCLUDE_DIRS})

# GLFW Example
find_package(glfw3 REQUIRED)
add_executable(vulkease_glfw vulkease_glfw_example.c)
target_link_libraries(vulkease_glfw glfw ${VULKEASE_LIBRARIES})
target_include_directories(vulkease_glfw PRIVATE ${VULKEASE_INCLUDE_DIRS})

# Copy shaders if any
# file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
```

### **Build with CMake:**
```bash
mkdir build
cd build
cmake ..
make

./vulkease_sdl
./vulkease_glfw
```

---

## Key Takeaways

### **🚀 VulkEase Simplifies Everything:**
1. **No Surface Management** - VulkEase handles SDL/GLFW surface creation
2. **No Descriptor Sets** - All resources accessed via indices
3. **No Complex Setup** - Simple swapchain creation
4. **No Binding Operations** - Push constants contain all resource indices

### **📱 Cross-Platform Ready:**
Both examples compile and run on:
- **Windows** (with MinGW or MSVC)
- **macOS** (with Homebrew dependencies)
- **Linux** (with package manager dependencies)

### **⚡ Performance Benefits:**
- **Zero binding overhead** - Direct resource access
- **Minimal API calls** - Streamlined rendering loop
- **GPU-optimal** - Modern bindless resource access

### **🎮 Production Ready:**
These examples demonstrate patterns suitable for:
- **Game engines**
- **Scientific visualization**
- **CAD applications**  
- **Media processing tools**

**VulkEase makes modern GPU programming accessible while maintaining full performance!** 🎉