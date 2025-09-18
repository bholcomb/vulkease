# VulkEase Troubleshooting & FAQ

**Common Issues, Solutions, and Frequently Asked Questions**

---

## Table of Contents

1. [Installation Issues](#installation-issues)
2. [Common Runtime Errors](#common-runtime-errors)
3. [Performance Issues](#performance-issues)
4. [Shader Compilation Problems](#shader-compilation-problems)
5. [Resource Management Issues](#resource-management-issues)
6. [Platform-Specific Issues](#platform-specific-issues)
7. [Frequently Asked Questions](#frequently-asked-questions)
8. [Debug Tools and Techniques](#debug-tools-and-techniques)

---

## Installation Issues

### Problem: VulkEase library not found during compilation

**Symptoms:**
```bash
gcc: error: -lvulkease: No such file or directory
/usr/bin/ld: cannot find -lvulkease
```

**Solutions:**

1. **Check Installation:**
   ```bash
   # Linux
   ldconfig -p | grep vulkease
   pkg-config --cflags --libs vulkease
   
   # macOS
   brew list vulkease
   
   # Windows
   dir "C:\VulkEase\lib\vulkease.lib"
   ```

2. **Manual Library Path:**
   ```bash
   gcc -L/usr/local/lib -I/usr/local/include -o myapp main.c -lvulkease
   ```

3. **Environment Variables:**
   ```bash
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
   export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
   ```

### Problem: Vulkan drivers not found

**Symptoms:**
```
VulkEase initialization failed: Vulkan loader library not found
Failed to create VulkEase context: VE_ERROR_INITIALIZATION_FAILED
```

**Solutions:**

1. **Install Vulkan Runtime:**
   ```bash
   # Ubuntu/Debian
   sudo apt install vulkan-tools vulkan-utils vulkan-validationlayers
   
   # Arch Linux  
   sudo pacman -S vulkan-tools vulkan-validation-layers
   
   # Windows: Download from LunarG or GPU vendor
   # macOS: Install MoltenVK
   brew install molten-vk
   ```

2. **Verify Vulkan Installation:**
   ```bash
   vulkaninfo
   vkcube  # Should display spinning cube
   ```

3. **Update GPU Drivers:**
   - **NVIDIA:** Download latest drivers from nvidia.com
   - **AMD:** Download latest drivers from amd.com  
   - **Intel:** Update through Windows Update or intel.com

### Problem: VulkEase headers not found

**Symptoms:**
```c
main.c:1:10: fatal error: vulkease.h: No such file or directory
```

**Solutions:**

1. **Check Header Installation:**
   ```bash
   find /usr -name "vulkease.h" 2>/dev/null
   ```

2. **Add Include Path:**
   ```bash
   gcc -I/usr/local/include -o myapp main.c -lvulkease
   ```

3. **CMake Configuration:**
   ```cmake
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(VULKEASE REQUIRED vulkease)
   
   target_include_directories(myapp PRIVATE ${VULKEASE_INCLUDE_DIRS})
   target_link_libraries(myapp ${VULKEASE_LIBRARIES})
   ```

---

## Common Runtime Errors

### Error: VE_ERROR_INITIALIZATION_FAILED

**Symptoms:**
```c
VEContext* context = veCreateContext("MyApp");
if (!context) {
    printf("Error: %s\n", veGetLastError());
    // Output: "Failed to initialize Vulkan instance"
}
```

**Common Causes and Solutions:**

1. **Missing Validation Layers (Debug Builds):**
   ```bash
   # Install validation layers
   # Ubuntu
   sudo apt install vulkan-validationlayers-dev
   # Or run without validation
   export VE_DISABLE_VALIDATION=1
   ```

2. **Insufficient GPU Support:**
   ```c
   // Check GPU capabilities
   char deviceName[256];
   uint32_t maxTextureSize;
   VESampleCount maxSampleCount;
   float timestampPeriod;
   
   if (veGetDeviceInfo(device, deviceName, &maxTextureSize, &maxSampleCount, &timestampPeriod) == VE_SUCCESS) {
       printf("Device: %s supports VulkEase\n", deviceName);
   } else {
       printf("Device does not meet minimum requirements\n");
   }
   ```

3. **Headless Environment:**
   ```c
   // For servers/containers without display
   VEContext* context = veCreateContextHeadless("Server App");  // Hypothetical
   ```

### Error: VE_ERROR_DEVICE_LOST

**Symptoms:**
```
Command buffer submission failed: VE_ERROR_DEVICE_LOST
```

**Recovery Steps:**

1. **Immediate Actions:**
   ```c
   VEResult handleDeviceLoss(VEDevice* device) {
       printf("Device lost detected - attempting recovery\n");
       
       // Wait for device to stabilize
       veDeviceWaitIdle(device);
       
       // Check if device is recoverable
       VEResult status = veCheckDeviceStatus(device);  // Hypothetical
       if (status == VE_ERROR_DEVICE_LOST) {
           printf("Device permanently lost - restart required\n");
           return VE_ERROR_DEVICE_LOST;
       }
       
       // Recreate critical resources
       recreateSwapchain();
       recreatePipelines();
       
       return VE_SUCCESS;
   }
   ```

2. **Prevention:**
   ```c
   // Avoid infinite loops
   while (rendering && deviceLossCount < 3) {
       VEResult result = renderFrame();
       if (result == VE_ERROR_DEVICE_LOST) {
           if (handleDeviceLoss(device) != VE_SUCCESS) {
               deviceLossCount++;
           }
       }
   }
   ```

### Error: VE_ERROR_OUT_OF_MEMORY

**Symptoms:**
```
Failed to create texture: VE_ERROR_OUT_OF_MEMORY
Failed to create buffer: VE_ERROR_OUT_OF_MEMORY
```

**Solutions:**

1. **Check Available Memory:**
   ```c
   void checkMemoryUsage(VEDevice* device) {
       VEPerformanceStats stats;
       if (veGetPerformanceStats(device, &stats) == VE_SUCCESS) {
           float memoryMB = stats.totalMemoryUsed / (1024.0f * 1024.0f);
           printf("Current memory usage: %.1f MB\n", memoryMB);
           
           if (memoryMB > 1024.0f) {  // Over 1GB
               printf("WARNING: High memory usage detected\n");
               // Trigger garbage collection or reduce quality
           }
       }
   }
   ```

2. **Resource Management:**
   ```c
   // Implement resource pooling
   typedef struct {
       VETextureIndex textures[MAX_POOLED_TEXTURES];
       VEBool inUse[MAX_POOLED_TEXTURES];
       uint32_t count;
   } TexturePool;
   
   VETextureIndex getPooledTexture(TexturePool* pool, uint32_t width, uint32_t height) {
       // Reuse existing texture if possible
       for (uint32_t i = 0; i < pool->count; i++) {
           if (!pool->inUse[i]) {
               // Check if dimensions match
               pool->inUse[i] = VE_TRUE;
               return pool->textures[i];
           }
       }
       
       // Create new if pool not full
       if (pool->count < MAX_POOLED_TEXTURES) {
           VETextureIndex texture = veCreateTexture2D(device, width, height, format, usage, NULL, 0);
           if (texture != VE_INVALID_TEXTURE_INDEX) {
               pool->textures[pool->count] = texture;
               pool->inUse[pool->count] = VE_TRUE;
               pool->count++;
               return texture;
           }
       }
       
       return VE_INVALID_TEXTURE_INDEX;
   }
   ```

3. **Fallback Strategies:**
   ```c
   VETextureIndex createTextureWithFallback(VEDevice* device, uint32_t width, uint32_t height, 
                                           VEFormat format, const void* data) {
       // Try original format first
       VETextureIndex texture = veCreateTexture2D(device, width, height, format, 
                                                 VE_TEXTURE_USAGE_SAMPLED, data, width * height * 4);
       
       if (texture == VE_INVALID_TEXTURE_INDEX) {
           // Try smaller size
           uint32_t fallbackWidth = width / 2;
           uint32_t fallbackHeight = height / 2;
           printf("Trying fallback size: %ux%u\n", fallbackWidth, fallbackHeight);
           
           // Would need to resize data here
           texture = veCreateTexture2D(device, fallbackWidth, fallbackHeight, format,
                                      VE_TEXTURE_USAGE_SAMPLED, NULL, 0);
       }
       
       if (texture == VE_INVALID_TEXTURE_INDEX) {
           // Try different format
           printf("Trying fallback format: R8\n");
           texture = veCreateTexture2D(device, width, height, VE_FORMAT_R8_UNORM,
                                      VE_TEXTURE_USAGE_SAMPLED, NULL, 0);
       }
       
       return texture;
   }
   ```

### Error: Invalid Resource Index

**Symptoms:**
```
Draw command failed: Invalid texture index 65537
Buffer update failed: Invalid buffer index 32769
```

**Debugging:**

1. **Check Index Validity:**
   ```c
   VEBool isValidTexture(VETextureIndex texture) {
       return texture != VE_INVALID_TEXTURE_INDEX && texture < 65536;
   }
   
   void safeDraw(VECommandBuffer* cmd, VEBufferIndex vertexBuffer, VETextureIndex texture, VESamplerIndex sampler) {
       if (!isValidTexture(texture)) {
           printf("ERROR: Invalid texture index %u\n", texture);
           return;
       }
       
       struct { uint32_t vbuf, tex, samp; } push = {vertexBuffer, texture, sampler};
       vePushConstants(cmd, &push, sizeof(push), 0);
       veDraw(cmd, vertexBuffer, 3, 1, 0, 0);
   }
   ```

2. **Resource Tracking:**
   ```c
   #ifdef DEBUG
   static VEBool allocatedTextures[65536] = {0};
   static VEBool allocatedBuffers[32768] = {0};
   
   VETextureIndex debugCreateTexture(VEDevice* device, /* ... */) {
       VETextureIndex texture = veCreateTexture2D(device, /* ... */);
       if (texture != VE_INVALID_TEXTURE_INDEX) {
           allocatedTextures[texture] = VE_TRUE;
           printf("DEBUG: Allocated texture %u\n", texture);
       }
       return texture;
   }
   
   void debugDestroyTexture(VEDevice* device, VETextureIndex texture) {
       if (allocatedTextures[texture]) {
           allocatedTextures[texture] = VE_FALSE;
           veDestroyTexture(device, texture);
           printf("DEBUG: Freed texture %u\n", texture);
       } else {
           printf("ERROR: Attempting to free invalid texture %u\n", texture);
       }
   }
   #endif
   ```

---

## Performance Issues

### Problem: Low Frame Rate

**Diagnosis Steps:**

1. **GPU Timing Analysis:**
   ```c
   void analyzePerformance(VECommandBuffer* cmd) {
       uint32_t frameTimer = veBeginGpuTimer(cmd, "Full Frame");
       uint32_t shadowTimer = veBeginGpuTimer(cmd, "Shadow Pass");
       
       renderShadows(cmd);
       veEndGpuTimer(cmd, shadowTimer);
       
       uint32_t geometryTimer = veBeginGpuTimer(cmd, "Geometry Pass");
       renderGeometry(cmd);
       veEndGpuTimer(cmd, geometryTimer);
       
       uint32_t lightingTimer = veBeginGpuTimer(cmd, "Lighting Pass");
       renderLighting(cmd);
       veEndGpuTimer(cmd, lightingTimer);
       
       veEndGpuTimer(cmd, frameTimer);
       
       // Submit and get results
       veSubmitCommandBuffer(cmd, NULL);
       veDeviceWaitIdle(device);
       
       uint64_t frameTimeNs = veGetGpuTimerResult(device, frameTimer);
       uint64_t shadowTimeNs = veGetGpuTimerResult(device, shadowTimer);
       uint64_t geometryTimeNs = veGetGpuTimerResult(device, geometryTimer);
       uint64_t lightingTimeNs = veGetGpuTimerResult(device, lightingTimer);
       
       printf("Performance Breakdown:\n");
       printf("  Total: %.2f ms\n", frameTimeNs / 1000000.0);
       printf("  Shadows: %.2f ms (%.1f%%)\n", shadowTimeNs / 1000000.0, (float)shadowTimeNs / frameTimeNs * 100);
       printf("  Geometry: %.2f ms (%.1f%%)\n", geometryTimeNs / 1000000.0, (float)geometryTimeNs / frameTimeNs * 100);
       printf("  Lighting: %.2f ms (%.1f%%)\n", lightingTimeNs / 1000000.0, (float)lightingTimeNs / frameTimeNs * 100);
   }
   ```

2. **CPU vs GPU Bottleneck:**
   ```c
   void detectBottleneck() {
       uint64_t cpuStart = getCurrentTimeNs();
       
       // Record commands
       VECommandBuffer* cmd = veGetSecondaryCommandBuffer(device);
       recordAllCommands(cmd);
       
       uint64_t cpuEnd = getCurrentTimeNs();
       uint64_t cpuTime = cpuEnd - cpuStart;
       
       // Submit and time GPU
       uint64_t gpuStart = getCurrentTimeNs();
       veSubmitCommandBuffer(cmd, NULL);
       veDeviceWaitIdle(device);
       uint64_t gpuEnd = getCurrentTimeNs();
       uint64_t gpuTime = gpuEnd - gpuStart;
       
       printf("CPU time: %.2f ms\n", cpuTime / 1000000.0);
       printf("GPU time: %.2f ms\n", gpuTime / 1000000.0);
       
       if (cpuTime > gpuTime * 1.5) {
           printf("CPU BOTTLENECK: Optimize command recording\n");
       } else if (gpuTime > cpuTime * 1.5) {
           printf("GPU BOTTLENECK: Optimize shaders/reduce draw calls\n");
       } else {
           printf("BALANCED: CPU and GPU are well-matched\n");
       }
   }
   ```

### Problem: High Memory Usage

**Analysis and Solutions:**

1. **Memory Leak Detection:**
   ```c
   void trackMemoryLeaks() {
       static uint64_t lastMemoryUsed = 0;
       
       VEPerformanceStats stats;
       veGetPerformanceStats(device, &stats);
       
       uint64_t currentMemory = stats.totalMemoryUsed;
       
       if (currentMemory > lastMemoryUsed + 10 * 1024 * 1024) {  // 10MB increase
           printf("MEMORY LEAK WARNING: Usage increased by %llu MB\n", 
                  (currentMemory - lastMemoryUsed) / (1024 * 1024));
           
           // Print resource counts
           printf("Resources: %u textures, %u buffers\n", 
                  stats.texturesAllocated, stats.buffersAllocated);
       }
       
       lastMemoryUsed = currentMemory;
   }
   ```

2. **Memory Usage Optimization:**
   ```c
   void optimizeMemoryUsage() {
       // Use texture compression
       VETextureIndex compressedTexture = veCreateTexture2D(device, 1024, 1024, 
                                                            VE_FORMAT_BC7_SRGB,  // Compressed format
                                                            VE_TEXTURE_USAGE_SAMPLED, 
                                                            compressedData, compressedSize);
       
       // Generate mipmaps to reduce bandwidth
       VECommandBuffer* cmd = veGetSecondaryCommandBuffer(device);
       veGenerateMipmaps(cmd, compressedTexture);
       veSubmitCommandBuffer(cmd, NULL);
       
       // Use memory pools for frequent allocations
       VEMemoryPool* pool = veCreateMemoryPool(device, 64 * 1024 * 1024, VE_MEMORY_USAGE_GPU_ONLY);
       uint64_t poolAllocation = veAllocateFromPool(pool, bufferSize, 256);
   }
   ```

---

## Shader Compilation Problems

### Error: Shader compilation failed

**Common Issues and Solutions:**

1. **GLSL Version Mismatch:**
   ```glsl
   // WRONG
   #version 330 core  // Too old for VulkEase
   
   // CORRECT
   #version 450 core  // Required for VulkEase
   ```

2. **Missing Bindless Declarations:**
   ```glsl
   // VulkEase automatically injects these - don't add manually:
   // layout(set = 0, binding = 0) uniform sampler2D globalTextures[];
   // layout(set = 0, binding = 1) uniform sampler globalSamplers[];
   
   // Your shader just needs push constants:
   layout(push_constant) uniform PushConstants {
       uint diffuseTexture;
       uint sampler;
   } pc;
   ```

3. **Debugging Shader Compilation:**
   ```c
   VEShader* shader = veCreateShaderFromSource(device, source, VE_SHADER_VERTEX, "main");
   if (!shader) {
       printf("Shader compilation failed: %s\n", veGetLastError());
       
       // Get detailed compilation log
       const char* log = veGetShaderLog(shader);  // Hypothetical
       if (log) {
           printf("Compilation log:\n%s\n", log);
       }
       return -1;
   }
   ```

4. **Shader Hot Reload Issues:**
   ```c
   // Enable hot reload carefully
   VEResult result = veEnableShaderHotReload(shader);
   if (result != VE_SUCCESS) {
       printf("Hot reload not available: %s\n", veGetLastError());
       // Continue without hot reload
   }
   
   // Check for updates in main loop
   void checkShaderUpdates() {
       static uint32_t lastCheck = 0;
       uint32_t currentTime = getCurrentTimeMs();
       
       if (currentTime - lastCheck > 100) {  // Check every 100ms
           if (veCheckShaderReload(shader)) {
               printf("Shader reloaded successfully\n");
               // Recreate dependent pipelines
               recreatePipelines();
           }
           lastCheck = currentTime;
       }
   }
   ```

---

## Resource Management Issues

### Problem: Resource Leaks

**Detection:**
```c
typedef struct {
    uint32_t texturesCreated;
    uint32_t texturesDestroyed;
    uint32_t buffersCreated;
    uint32_t buffersDestroyed;
    uint32_t samplersCreated;
    uint32_t samplersDestroyed;
} ResourceStats;

static ResourceStats g_stats = {0};

// Wrapper functions for tracking
VETextureIndex trackedCreateTexture2D(VEDevice* device, /* ... */) {
    VETextureIndex texture = veCreateTexture2D(device, /* ... */);
    if (texture != VE_INVALID_TEXTURE_INDEX) {
        g_stats.texturesCreated++;
    }
    return texture;
}

void trackedDestroyTexture(VEDevice* device, VETextureIndex texture) {
    veDestroyTexture(device, texture);
    g_stats.texturesDestroyed++;
}

void printResourceLeaks() {
    printf("Resource Leak Report:\n");
    printf("  Textures: %u created, %u destroyed, %u leaked\n", 
           g_stats.texturesCreated, g_stats.texturesDestroyed,
           g_stats.texturesCreated - g_stats.texturesDestroyed);
    printf("  Buffers: %u created, %u destroyed, %u leaked\n",
           g_stats.buffersCreated, g_stats.buffersDestroyed,
           g_stats.buffersCreated - g_stats.buffersDestroyed);
}
```

### Problem: Running Out of Resource Slots

**Solutions:**
```c
// Check resource utilization
void checkResourceUtilization(VEDevice* device) {
    VEPerformanceStats stats;
    veGetPerformanceStats(device, &stats);
    
    float textureUtilization = (float)stats.texturesAllocated / 65536.0f * 100.0f;
    float bufferUtilization = (float)stats.buffersAllocated / 32768.0f * 100.0f;
    
    if (textureUtilization > 90.0f) {
        printf("WARNING: Texture slots 90%% full - implement texture streaming\n");
    }
    
    if (bufferUtilization > 90.0f) {
        printf("WARNING: Buffer slots 90%% full - implement buffer pooling\n");
    }
}

// Implement resource streaming
typedef struct {
    VETextureIndex* textures;
    uint32_t* lastUsedFrame;
    uint32_t count;
    uint32_t capacity;
} TextureStreamer;

void streamTextures(TextureStreamer* streamer, uint32_t currentFrame) {
    for (uint32_t i = 0; i < streamer->count; i++) {
        // Evict textures not used recently
        if (currentFrame - streamer->lastUsedFrame[i] > 300) {  // 5 seconds at 60 FPS
            printf("Evicting texture %u (unused for %u frames)\n", 
                   streamer->textures[i], currentFrame - streamer->lastUsedFrame[i]);
            veDestroyTexture(device, streamer->textures[i]);
            // Remove from array and load new textures as needed
        }
    }
}
```

---

## Platform-Specific Issues

### Windows Issues

1. **D3D12 Interop Problems:**
   ```c
   // Ensure proper Vulkan/D3D12 coexistence
   #ifdef _WIN32
   // Disable D3D12 debug layer when using VulkEase
   putenv("DXGI_DEBUG_LAYER_DISABLE=1");
   #endif
   ```

2. **Visual Studio Linker Issues:**
   ```cmake
   # CMakeLists.txt
   if(WIN32)
       target_link_libraries(myapp vulkease.lib)
       # Copy DLL to output directory
       add_custom_command(TARGET myapp POST_BUILD
           COMMAND ${CMAKE_COMMAND} -E copy_if_different
           "${VULKEASE_BINARY_DIR}/vulkease.dll"
           $<TARGET_FILE_DIR:myapp>)
   endif()
   ```

### Linux Issues

1. **Wayland vs X11:**
   ```c
   // Check for Wayland environment
   const char* waylandDisplay = getenv("WAYLAND_DISPLAY");
   const char* x11Display = getenv("DISPLAY");
   
   if (waylandDisplay) {
       printf("Running on Wayland\n");
       // Use Wayland-specific window creation
   } else if (x11Display) {
       printf("Running on X11\n");
       // Use X11-specific window creation
   }
   ```

2. **Missing Graphics Drivers:**
   ```bash
   # Check if proper drivers are installed
   lspci | grep VGA
   glxinfo | grep renderer
   vulkaninfo | head -20
   
   # Install mesa drivers for Intel/AMD
   sudo apt install mesa-vulkan-drivers
   
   # Install proprietary drivers
   sudo apt install nvidia-driver-470  # or latest version
   ```

### macOS Issues

1. **MoltenVK Specific:**
   ```c
   #ifdef __APPLE__
   // MoltenVK has some limitations
   printf("Running on macOS with MoltenVK\n");
   
   // Some features may not be available
   // Check device capabilities
   char deviceName[256];
   veGetDeviceInfo(device, deviceName, NULL, NULL, NULL);
   if (strstr(deviceName, "Intel")) {
       printf("Intel GPU detected - some features limited\n");
   }
   #endif
   ```

2. **Metal Performance Shaders Integration:**
   ```c
   // For optimal performance on macOS, consider using
   // Metal Performance Shaders for compute-heavy operations
   #ifdef __APPLE__
   #include <MetalPerformanceShaders/MetalPerformanceShaders.h>
   // Use MPS for matrix operations, image processing, etc.
   #endif
   ```

---

## Frequently Asked Questions

### Q: How do I convert from OpenGL/Direct3D to VulkEase?

**A:** Key differences to understand:

1. **No Binding Operations:**
   ```c
   // OpenGL way - COMPLEX
   glBindTexture(GL_TEXTURE_2D, texture);
   glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
   glDrawArrays(GL_TRIANGLES, 0, 3);
   
   // VulkEase way - SIMPLE
   struct { uint32_t texture, vertexBuffer, sampler; } push = {tex, vbuf, samp};
   vePushConstants(cmd, &push, sizeof(push), 0);
   veDraw(cmd, vertexBuffer, 3, 1, 0, 0);
   ```

2. **Resource Creation Returns Indices:**
   ```c
   // Traditional APIs return opaque handles
   GLuint texture;
   glGenTextures(1, &texture);
   
   // VulkEase returns simple indices
   VETextureIndex texture = veCreateTexture2D(device, ...);  // Returns 0, 1, 2, etc.
   ```

### Q: Can I use VulkEase with existing graphics libraries?

**A:** Yes, but with considerations:

```c
// VulkEase can coexist with other APIs
void hybridRendering() {
    // Initialize VulkEase
    VEContext* veContext = veCreateContext("Hybrid App");
    VEDevice* veDevice = veCreateDevice(veContext, VE_DEVICE_TYPE_AUTO);
    
    // Initialize OpenGL (for UI)
    // ... OpenGL context creation
    
    // Use VulkEase for 3D rendering
    veRender3DScene();
    
    // Use OpenGL for UI overlay
    glRenderUI();
}
```

### Q: How do I debug shader issues?

**A:** Several approaches:

1. **Printf Debugging in Shaders:**
   ```glsl
   // Enable debug printf (if supported)
   #extension GL_EXT_debug_printf : enable
   
   void main() {
       debugPrintfEXT("Vertex %d: position = (%f, %f, %f)\n", 
                      gl_VertexIndex, position.x, position.y, position.z);
   }
   ```

2. **Render Debugging:**
   ```c
   // Render intermediate results
   VETextureIndex debugTexture = veCreateTexture2D(device, 512, 512, VE_FORMAT_RGBA8_UNORM,
                                                   VE_TEXTURE_USAGE_COLOR_TARGET | VE_TEXTURE_USAGE_SAMPLED,
                                                   NULL, 0);
   
   // Render to debug texture
   renderToTexture(debugTexture);
   
   // Save debug texture
   saveTextureToFile(debugTexture, "debug_output.png");
   ```

### Q: What's the maximum number of resources I can create?

**A:** Current limits:
- **Textures:** 65,536 (can be increased if needed)
- **Buffers:** 32,768 (vertex, index, uniform, storage combined)
- **Samplers:** 1,024
- **Pipelines:** Unlimited (limited by memory)

### Q: Can I use VulkEase for compute-only applications?

**A:** Absolutely:

```c
int main() {
    VEContext* context = veCreateContext("Compute Only App");
    VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);
    
    // Create compute resources
    VEBufferIndex inputBuffer = veCreateStorageBuffer(device, dataSize, 0, inputData);
    VEBufferIndex outputBuffer = veCreateStorageBuffer(device, dataSize, 0, NULL);
    
    // Create compute pipeline
    VEShader* computeShader = veCreateShaderFromFile(device, "compute.comp", VE_SHADER_COMPUTE, "main");
    VEComputePipeline* pipeline = veCreateComputePipeline(device, computeShader, "Data Processing");
    
    // Dispatch compute work
    VECommandBuffer* cmd = veGetSecondaryCommandBuffer(device);
    veBindComputePipeline(cmd, pipeline);
    
    struct { uint32_t input, output, count; } push = {inputBuffer, outputBuffer, elementCount};
    vePushConstants(cmd, &push, sizeof(push), 0);
    veDispatch(cmd, (elementCount + 63) / 64, 1, 1);
    
    veSubmitCommandBuffer(cmd, NULL);
    veDeviceWaitIdle(device);
    
    // Read back results
    // ... readback code
    
    return 0;
}
```

---

## Debug Tools and Techniques

### Using VulkEase Debug Features

1. **Enable Validation:**
   ```c
   VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);
   veSetValidationEnabled(device, VE_TRUE);
   
   // All VulkEase calls will be validated
   VETextureIndex texture = veCreateTexture2D(device, 0, 0, VE_FORMAT_RGBA8_UNORM, 0, NULL, 0);
   // Will trigger validation error for invalid parameters
   ```

2. **Performance Monitoring:**
   ```c
   void setupPerformanceMonitoring(VEDevice* device) {
       // Enable detailed performance tracking
       veSetDebugMode(device, VE_DEBUG_MODE_PERFORMANCE);  // Hypothetical
       
       // Set up performance callbacks
       veSetPerformanceCallback(device, performanceCallback, NULL);  // Hypothetical
   }
   
   void performanceCallback(VEDevice* device, const VEPerformanceEvent* event, void* userData) {
       if (event->type == VE_PERFORMANCE_WARNING_MEMORY) {
           printf("Performance warning: High memory usage detected\n");
       }
   }
   ```

3. **Resource Debugging:**
   ```c
   void debugResourceUsage(VEDevice* device) {
       // Print comprehensive debug info
       vePrintDebugInfo(device);
       
       // Take screenshot for visual debugging
       if (swapchain) {
           veTakeScreenshot(swapchain, "debug_frame.ppm");
       }
       
       // Dump performance stats
       VEPerformanceStats stats;
       veGetPerformanceStats(device, &stats);
       
       FILE* logFile = fopen("performance_log.txt", "w");
       fprintf(logFile, "Frame %d:\n", frameNumber);
       fprintf(logFile, "  Frame Time: %.2f ms\n", stats.frameTimeMs);
       fprintf(logFile, "  Memory Usage: %.1f MB\n", stats.totalMemoryUsed / (1024.0f * 1024.0f));
       fclose(logFile);
   }
   ```

### External Debug Tools

1. **RenderDoc Integration:**
   ```c
   #ifdef USE_RENDERDOC
   #include "renderdoc_app.h"
   
   RENDERDOC_API_1_1_2* rdoc_api = NULL;
   
   void initRenderDoc() {
       if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
           pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
           int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
       }
   }
   
   void captureFrame() {
       if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
       renderFrame();
       if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
   }
   #endif
   ```

2. **Nsight Graphics Integration:**
   ```c
   // Use NVTX markers for Nsight profiling
   #ifdef USE_NVTX
   #include "nvtx3/nvToolsExt.h"
   
   void profiledRenderPass() {
       nvtxRangePushA("Geometry Pass");
       renderGeometry();
       nvtxRangePop();
       
       nvtxRangePushA("Lighting Pass");
       renderLighting();
       nvtxRangePop();
   }
   #endif
   ```

This comprehensive troubleshooting guide covers the most common issues developers encounter when using VulkEase, providing practical solutions and debugging techniques for each scenario.