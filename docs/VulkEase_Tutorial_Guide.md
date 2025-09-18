# VulkEase Tutorial Guide

**Step-by-Step Learning Path for VulkEase Development**

---

## Table of Contents

1. [Tutorial Overview](#tutorial-overview)
2. [Tutorial 1: Hello VulkEase](#tutorial-1-hello-vulkease)
3. [Tutorial 2: Your First Triangle](#tutorial-2-your-first-triangle)
4. [Tutorial 3: Textures and Sampling](#tutorial-3-textures-and-sampling)
5. [Tutorial 4: 3D Rendering](#tutorial-4-3d-rendering)
6. [Tutorial 5: Compute Shaders](#tutorial-5-compute-shaders)
7. [Tutorial 6: Advanced Techniques](#tutorial-6-advanced-techniques)
8. [Tutorial 7: Production Application](#tutorial-7-production-application)

---

## Tutorial Overview

This tutorial series will take you from complete beginner to expert VulkEase developer. Each tutorial builds on the previous ones, gradually introducing more advanced concepts.

### Prerequisites
- Basic C programming knowledge
- Understanding of 3D graphics concepts (vectors, matrices)
- Vulkan 1.2+ compatible GPU and drivers
- VulkEase SDK installed

### Learning Path
1. **Hello VulkEase** - Basic initialization and cleanup
2. **Your First Triangle** - Basic rendering pipeline
3. **Textures and Sampling** - Working with images
4. **3D Rendering** - Matrices and transformations
5. **Compute Shaders** - GPU parallel processing
6. **Advanced Techniques** - Production-ready features
7. **Production Application** - Complete game/app structure

---

## Tutorial 1: Hello VulkEase

**Goal:** Learn basic VulkEase initialization and understand the core concepts.

### Step 1: Project Setup

Create a new C project with the following structure:
```
hello_vulkease/
├── src/
│   └── main.c
├── shaders/
└── assets/
```

### Step 2: Basic Initialization

```c
// src/main.c
#include "vulkease.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== Hello VulkEase Tutorial ===\n");
    
    // Step 1: Create VulkEase context
    printf("1. Creating VulkEase context...\n");
    VEContext* context = veCreateContext("Hello VulkEase Tutorial");
    if (!context) {
        printf("FAILED: Could not create VulkEase context\n");
        printf("Error: %s\n", veGetLastError());
        return -1;
    }
    printf("✓ Context created successfully\n");
    
    // Step 2: Create device (auto-select best GPU)
    printf("2. Creating VulkEase device...\n");
    VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);
    if (!device) {
        printf("FAILED: Could not create VulkEase device\n");
        printf("Error: %s\n", veGetLastError());
        veDestroyContext(context);
        return -1;
    }
    printf("✓ Device created successfully\n");
    
    // Step 3: Get device information
    printf("3. Querying device information...\n");
    char deviceName[256];
    uint32_t maxTextureSize;
    VESampleCount maxSampleCount;
    float timestampPeriod;
    
    VEResult result = veGetDeviceInfo(device, deviceName, &maxTextureSize, &maxSampleCount, &timestampPeriod);
    if (result == VE_SUCCESS) {
        printf("✓ Device: %s\n", deviceName);
        printf("✓ Max texture size: %u x %u\n", maxTextureSize, maxTextureSize);
        printf("✓ Max MSAA samples: %d\n", maxSampleCount);
        printf("✓ GPU timestamp period: %.2f ns\n", timestampPeriod);
    } else {
        printf("WARNING: Could not query device info\n");
    }
    
    // Step 4: Test resource creation
    printf("4. Testing basic resource creation...\n");
    
    // Create a simple texture
    uint8_t textureData[] = {255, 0, 0, 255};  // Single red pixel
    VETextureIndex texture = veCreateTexture2D(device, 1, 1, VE_FORMAT_RGBA8_UNORM,
                                              VE_TEXTURE_USAGE_SAMPLED, textureData, sizeof(textureData));
    
    if (texture == VE_INVALID_TEXTURE_INDEX) {
        printf("FAILED: Could not create test texture\n");
    } else {
        printf("✓ Created test texture at index: %u\n", texture);
        
        // Clean up texture
        veDestroyTexture(device, texture);
        printf("✓ Destroyed test texture\n");
    }
    
    // Create a simple buffer
    float bufferData[] = {1.0f, 2.0f, 3.0f, 4.0f};
    VEBufferIndex buffer = veCreateVertexBuffer(device, bufferData, sizeof(bufferData), 0);
    
    if (buffer == VE_INVALID_BUFFER_INDEX) {
        printf("FAILED: Could not create test buffer\n");
    } else {
        printf("✓ Created test buffer at index: %u\n", buffer);
        
        // Clean up buffer
        veDestroyBuffer(device, buffer);
        printf("✓ Destroyed test buffer\n");
    }
    
    // Step 5: Cleanup
    printf("5. Cleaning up...\n");
    veDestroyDevice(device);
    printf("✓ Device destroyed\n");
    
    veDestroyContext(context);
    printf("✓ Context destroyed\n");
    
    printf("\nTutorial 1 completed successfully!\n");
    printf("Key concepts learned:\n");
    printf("  • VulkEase initialization (context + device)\n");
    printf("  • Resource creation returns indices, not handles\n");
    printf("  • Proper cleanup order (device before context)\n");
    printf("  • Error handling with veGetLastError()\n");
    
    return 0;
}
```

### Step 3: Compile and Run

```bash
gcc -o hello_vulkease src/main.c -lvulkease
./hello_vulkease
```

**Expected Output:**
```
=== Hello VulkEase Tutorial ===
1. Creating VulkEase context...
✓ Context created successfully
2. Creating VulkEase device...
✓ Device created successfully
3. Querying device information...
✓ Device: NVIDIA GeForce RTX 4070
✓ Max texture size: 32768 x 32768
✓ Max MSAA samples: 64
✓ GPU timestamp period: 1.00 ns
4. Testing basic resource creation...
✓ Created test texture at index: 0
✓ Destroyed test texture
✓ Created test buffer at index: 0
✓ Destroyed test buffer
5. Cleaning up...
✓ Device destroyed
✓ Context destroyed

Tutorial 1 completed successfully!
```

---

## Tutorial 2: Your First Triangle

**Goal:** Render your first triangle using VulkEase's bindless approach.

### Step 1: Project Structure

```
first_triangle/
├── src/
│   ├── main.c
│   └── window.c
├── shaders/
│   ├── triangle.vert
│   └── triangle.frag
└── assets/
```

### Step 2: Window Creation (Simplified)

```c
// src/window.c
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

void* createSimpleWindow(uint32_t width, uint32_t height) {
    // Simplified - in real app, use proper window library
    return GetConsoleWindow();  // Placeholder
}
#else
void* createSimpleWindow(uint32_t width, uint32_t height) {
    // Simplified - in real app, use X11/Wayland
    return (void*)1;  // Placeholder
}
#endif
```

### Step 3: Vertex Shader

```glsl
// shaders/triangle.vert
#version 450 core

// VulkEase automatically injects bindless declarations here

layout(push_constant) uniform PushConstants {
    uint vertexBuffer;
    uint colorIndex;
    float time;
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    // Triangle vertices (hardcoded for simplicity)
    vec2 positions[3] = vec2[](
        vec2(-0.6, -0.4),
        vec2( 0.6, -0.4),
        vec2( 0.0,  0.6)
    );
    
    // Colors for each vertex
    vec3 colors[3] = vec3[](
        vec3(1.0, 0.0, 0.0),  // Red
        vec3(0.0, 1.0, 0.0),  // Green
        vec3(0.0, 0.0, 1.0)   // Blue
    );
    
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    
    // Animate colors based on time
    vec3 baseColor = colors[gl_VertexIndex];
    float pulse = 0.5 + 0.5 * sin(pc.time * 2.0);
    fragColor = vec4(baseColor * pulse, 1.0);
}
```

### Step 4: Fragment Shader

```glsl
// shaders/triangle.frag
#version 450 core

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
```

### Step 5: Main Application

```c
// src/main.c
#include "vulkease.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// External window creation
extern void* createSimpleWindow(uint32_t width, uint32_t height);

// Push constants structure
typedef struct {
    uint32_t vertexBuffer;
    uint32_t colorIndex;
    float time;
} PushConstants;

int main() {
    printf("=== First Triangle Tutorial ===\n");
    
    // Initialize VulkEase
    VEContext* context = veCreateContext("First Triangle Tutorial");
    VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);
    
    if (!context || !device) {
        printf("Failed to initialize VulkEase: %s\n", veGetLastError());
        return -1;
    }
    printf("✓ VulkEase initialized\n");
    
    // Create window and swapchain
    void* window = createSimpleWindow(800, 600);
    VESwapchain* swapchain = veCreateSwapchain(device, window, 800, 600);
    
    if (!swapchain) {
        printf("Note: Swapchain creation failed (expected without real window): %s\n", veGetLastError());
        printf("Continuing with offline rendering demo...\n");
    }
    
    // Create vertex data (even though shader doesn't use it, we still pass an index)
    float vertices[] = {
        -0.6f, -0.4f, 0.0f,  // Bottom left
         0.6f, -0.4f, 0.0f,  // Bottom right
         0.0f,  0.6f, 0.0f   // Top center
    };
    
    VEBufferIndex vertexBuffer = veCreateVertexBuffer(device, vertices, sizeof(vertices), 0);
    if (vertexBuffer == VE_INVALID_BUFFER_INDEX) {
        printf("Failed to create vertex buffer: %s\n", veGetLastError());
        return -1;
    }
    printf("✓ Created vertex buffer at index: %u\n", vertexBuffer);
    
    // Load and compile shaders
    printf("Loading shaders...\n");
    
    // Read shader files (simplified - assumes files exist)
    FILE* vertFile = fopen("shaders/triangle.vert", "r");
    FILE* fragFile = fopen("shaders/triangle.frag", "r");
    
    if (!vertFile || !fragFile) {
        printf("Could not open shader files - using embedded shaders\n");
        
        // Use embedded shader source
        const char* vertSource = 
            "#version 450 core\n"
            "layout(push_constant) uniform PushConstants { uint vertexBuffer; uint colorIndex; float time; } pc;\n"
            "layout(location = 0) out vec4 fragColor;\n"
            "void main() {\n"
            "    vec2 pos[3] = vec2[](vec2(-0.6,-0.4), vec2(0.6,-0.4), vec2(0.0,0.6));\n"
            "    vec3 colors[3] = vec3[](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));\n"
            "    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);\n"
            "    float pulse = 0.5 + 0.5 * sin(pc.time * 2.0);\n"
            "    fragColor = vec4(colors[gl_VertexIndex] * pulse, 1.0);\n"
            "}\n";
        
        const char* fragSource =
            "#version 450 core\n"
            "layout(location = 0) in vec4 fragColor;\n"
            "layout(location = 0) out vec4 outColor;\n"
            "void main() { outColor = fragColor; }\n";
        
        VEShader* vertexShader = veCreateShaderFromSource(device, vertSource, VE_SHADER_VERTEX, "main");
        VEShader* fragmentShader = veCreateShaderFromSource(device, fragSource, VE_SHADER_FRAGMENT, "main");
        
        if (!vertexShader || !fragmentShader) {
            printf("Failed to compile shaders: %s\n", veGetLastError());
            return -1;
        }
        printf("✓ Compiled shaders successfully\n");
        
        // Create graphics pipeline
        VEPipeline* pipeline = veCreateSimpleGraphicsPipeline(device, vertexShader, fragmentShader);
        if (!pipeline) {
            printf("Failed to create pipeline: %s\n", veGetLastError());
            return -1;
        }
        printf("✓ Created graphics pipeline\n");
        
        // Render frames
        printf("Rendering frames...\n");
        
        clock_t startTime = clock();
        
        for (int frame = 0; frame < 60; frame++) {  // 60 frames
            float currentTime = frame * 0.016f;  // 60 FPS
            
            // Get command buffer
            VECommandBuffer* cmd = veGetSecondaryCommandBuffer(device);
            if (!cmd) {
                printf("Failed to get command buffer\n");
                continue;
            }
            
            // Begin rendering
            vePushDebugGroup(cmd, "Triangle Rendering");
            
            // Set viewport (conceptual - would work with real swapchain)
            veSetViewport(cmd, 0, 0, 800, 600, 0.0f, 1.0f);
            
            // Bind pipeline - NO DESCRIPTOR SETS NEEDED!
            veBindPipeline(cmd, pipeline);
            
            // Push constants - THIS IS THE KEY!
            PushConstants pushData = {
                .vertexBuffer = vertexBuffer,
                .colorIndex = 0,
                .time = currentTime
            };
            vePushConstants(cmd, &pushData, sizeof(pushData), 0);
            
            // Draw triangle - NO VERTEX BUFFER BINDING!
            veDraw(cmd, vertexBuffer, 3, 1, 0, 0);
            
            vePopDebugGroup(cmd);
            
            // Submit command buffer
            VEResult result = veSubmitCommandBuffer(cmd, NULL);
            if (result != VE_SUCCESS) {
                printf("Frame %d submission failed: %d\n", frame, result);
            }
            
            // Wait for completion (for demo purposes)
            veDeviceWaitIdle(device);
            
            if (frame % 10 == 0) {
                printf("  Rendered frame %d (time: %.2fs)\n", frame, currentTime);
            }
        }
        
        clock_t endTime = clock();
        float totalTime = ((float)(endTime - startTime) / CLOCKS_PER_SEC) * 1000.0f;
        printf("✓ Rendered 60 frames in %.2f ms (avg: %.2f ms/frame)\n", totalTime, totalTime / 60.0f);
        
        // Cleanup
        veDestroyPipeline(pipeline);
        veDestroyShader(fragmentShader);
        veDestroyShader(vertexShader);
    } else {
        if (vertFile) fclose(vertFile);
        if (fragFile) fclose(fragFile);
        printf("External shader files found but not implemented in this tutorial\n");
    }
    
    veDestroyBuffer(device, vertexBuffer);
    
    if (swapchain) {
        veDestroySwapchain(swapchain);
    }
    
    veDestroyDevice(device);
    veDestroyContext(context);
    
    printf("\nTutorial 2 completed successfully!\n");
    printf("Key concepts learned:\n");
    printf("  • Shader compilation from source code\n");
    printf("  • Graphics pipeline creation\n");
    printf("  • Push constants for passing data to shaders\n");
    printf("  • Draw commands with NO BINDING OPERATIONS!\n");
    printf("  • Command buffer recording and submission\n");
    
    return 0;
}
```

### Step 6: Compile and Run

```bash
mkdir -p shaders
gcc -o first_triangle src/main.c src/window.c -lvulkease -lm
./first_triangle
```

**Expected Output:**
```
=== First Triangle Tutorial ===
✓ VulkEase initialized
Note: Swapchain creation failed (expected without real window)
Continuing with offline rendering demo...
✓ Created vertex buffer at index: 0
Loading shaders...
Could not open shader files - using embedded shaders
✓ Compiled shaders successfully
✓ Created graphics pipeline
Rendering frames...
  Rendered frame 0 (time: 0.00s)
  Rendered frame 10 (time: 0.16s)
  Rendered frame 20 (time: 0.32s)
  Rendered frame 30 (time: 0.48s)
  Rendered frame 40 (time: 0.64s)
  Rendered frame 50 (time: 0.80s)
✓ Rendered 60 frames in 45.23 ms (avg: 0.75 ms/frame)

Tutorial 2 completed successfully!
Key concepts learned:
  • Shader compilation from source code
  • Graphics pipeline creation
  • Push constants for passing data to shaders
  • Draw commands with NO BINDING OPERATIONS!
  • Command buffer recording and submission
```

---

## Tutorial 3: Textures and Sampling

**Goal:** Learn to create, load, and sample textures using VulkEase's bindless model.

### Step 1: Texture Creation

```c
// Generate a procedural checkerboard texture
void generateCheckerboardTexture(uint8_t* data, uint32_t width, uint32_t height, uint32_t checkSize) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t pixelIndex = (y * width + x) * 4;
            
            // Checkerboard pattern
            VEBool isWhite = ((x / checkSize) + (y / checkSize)) % 2 == 0;
            uint8_t value = isWhite ? 255 : 64;
            
            data[pixelIndex + 0] = value;      // R
            data[pixelIndex + 1] = value;      // G
            data[pixelIndex + 2] = value;      // B
            data[pixelIndex + 3] = 255;        // A
        }
    }
}

int main() {
    printf("=== Textures and Sampling Tutorial ===\n");
    
    // Initialize VulkEase
    VEContext* context = veCreateContext("Textures Tutorial");
    VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);
    
    // Create checkerboard texture
    const uint32_t textureSize = 256;
    const size_t dataSize = textureSize * textureSize * 4;
    uint8_t* textureData = malloc(dataSize);
    generateCheckerboardTexture(textureData, textureSize, textureSize, 32);
    
    VETextureIndex diffuseTexture = veCreateTexture2D(device, textureSize, textureSize,
                                                     VE_FORMAT_RGBA8_SRGB, VE_TEXTURE_USAGE_SAMPLED,
                                                     textureData, dataSize);
    
    if (diffuseTexture == VE_INVALID_TEXTURE_INDEX) {
        printf("Failed to create texture: %s\n", veGetLastError());
        return -1;
    }
    printf("✓ Created checkerboard texture at index: %u\n", diffuseTexture);
    
    // Create different samplers
    VESamplerIndex linearSampler = veCreateLinearSampler(device);
    VESamplerIndex nearestSampler = veCreateNearestSampler(device);
    
    printf("✓ Created samplers: linear=%u, nearest=%u\n", linearSampler, nearestSampler);
    
    // Generate mipmaps
    VECommandBuffer* cmd = veGetSecondaryCommandBuffer(device);
    veGenerateMipmaps(cmd, diffuseTexture);
    veSubmitCommandBuffer(cmd, NULL);
    veDeviceWaitIdle(device);
    printf("✓ Generated mipmaps for texture\n");
    
    // Create textured quad shaders
    const char* texturedVertexShader = 
        "#version 450 core\n"
        "layout(push_constant) uniform PushConstants {\n"
        "    uint diffuseTexture;\n"
        "    uint sampler;\n"
        "    float time;\n"
        "} pc;\n"
        "layout(location = 0) out vec2 fragTexCoord;\n"
        "void main() {\n"
        "    vec2 positions[6] = vec2[](\n"
        "        vec2(-0.8, -0.8), vec2(0.8, -0.8), vec2(-0.8, 0.8),\n"
        "        vec2(0.8, -0.8), vec2(0.8, 0.8), vec2(-0.8, 0.8)\n"
        "    );\n"
        "    vec2 texCoords[6] = vec2[](\n"
        "        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),\n"
        "        vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)\n"
        "    );\n"
        "    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
        "    fragTexCoord = texCoords[gl_VertexIndex] * (2.0 + sin(pc.time));\n"  // Animate UV
        "}\n";
    
    const char* texturedFragmentShader =
        "#version 450 core\n"
        "layout(push_constant) uniform PushConstants {\n"
        "    uint diffuseTexture;\n"
        "    uint sampler;\n"
        "    float time;\n"
        "} pc;\n"
        "layout(location = 0) in vec2 fragTexCoord;\n"
        "layout(location = 0) out vec4 outColor;\n"
        "void main() {\n"
        "    // This is the key - bindless texture access!\n"
        "    // vec4 texColor = texture(sampler2D(globalTextures[pc.diffuseTexture], globalSamplers[pc.sampler]), fragTexCoord);\n"
        "    \n"
        "    // For demo, we'll simulate texture sampling with a pattern\n"
        "    vec2 uv = fract(fragTexCoord);\n"
        "    float checker = step(0.5, mod(floor(fragTexCoord.x * 8.0) + floor(fragTexCoord.y * 8.0), 2.0));\n"
        "    vec3 color = mix(vec3(0.2), vec3(0.8), checker);\n"
        "    \n"
        "    outColor = vec4(color, 1.0);\n"
        "}\n";
    
    // Create pipeline
    VEShader* vs = veCreateShaderFromSource(device, texturedVertexShader, VE_SHADER_VERTEX, "main");
    VEShader* fs = veCreateShaderFromSource(device, texturedFragmentShader, VE_SHADER_FRAGMENT, "main");
    VEPipeline* texturedPipeline = veCreateSimpleGraphicsPipeline(device, vs, fs);
    
    if (!texturedPipeline) {
        printf("Failed to create textured pipeline: %s\n", veGetLastError());
        return -1;
    }
    printf("✓ Created textured rendering pipeline\n");
    
    // Render textured quad
    printf("Rendering textured quad...\n");
    
    typedef struct {
        uint32_t diffuseTexture;
        uint32_t sampler;
        float time;
    } TexturedPushConstants;
    
    for (int frame = 0; frame < 30; frame++) {
        float currentTime = frame * 0.1f;
        
        VECommandBuffer* cmd = veGetSecondaryCommandBuffer(device);
        
        vePushDebugGroup(cmd, "Textured Quad");
        veBindPipeline(cmd, texturedPipeline);
        
        // Test both samplers
        VESamplerIndex currentSampler = (frame < 15) ? linearSampler : nearestSampler;
        
        TexturedPushConstants pushData = {
            .diffuseTexture = diffuseTexture,
            .sampler = currentSampler,
            .time = currentTime
        };
        
        vePushConstants(cmd, &pushData, sizeof(pushData), 0);
        
        // Draw quad (6 vertices for 2 triangles)
        veDraw(cmd, 0, 6, 1, 0, 0);  // No vertex buffer needed for this shader
        
        vePopDebugGroup(cmd);
        
        veSubmitCommandBuffer(cmd, NULL);
        veDeviceWaitIdle(device);
        
        if (frame % 10 == 0) {
            printf("  Frame %d: Using %s sampler\n", frame, 
                   (currentSampler == linearSampler) ? "linear" : "nearest");
        }
    }
    
    printf("✓ Rendered 30 frames with different sampling modes\n");
    
    // Cleanup
    veDestroyPipeline(texturedPipeline);
    veDestroyShader(fs);
    veDestroyShader(vs);
    veDestroySampler(device, nearestSampler);
    veDestroySampler(device, linearSampler);
    veDestroyTexture(device, diffuseTexture);
    
    free(textureData);
    veDestroyDevice(device);
    veDestroyContext(context);
    
    printf("\nTutorial 3 completed successfully!\n");
    printf("Key concepts learned:\n");
    printf("  • Texture creation with different formats\n");
    printf("  • Sampler creation (linear vs nearest filtering)\n");
    printf("  • Mipmap generation using GPU\n");
    printf("  • Bindless texture access in shaders\n");
    printf("  • Push constants for texture and sampler indices\n");
    
    return 0;
}
```

---

## Tutorial 4: 3D Rendering

**Goal:** Implement 3D rendering with transformations and perspective projection.

```c
// Matrix and vector math (simplified)
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef float mat4[16];

void mat4_identity(mat4 m) {
    for (int i = 0; i < 16; i++) {
        m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

void mat4_perspective(mat4 m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    mat4_identity(m);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
    m[15] = 0.0f;
}

void mat4_rotate_y(mat4 m, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    mat4_identity(m);
    m[0] = c; m[2] = s;
    m[8] = -s; m[10] = c;
}

int main() {
    printf("=== 3D Rendering Tutorial ===\n");
    
    // Initialize VulkEase
    VEContext* context = veCreateContext("3D Rendering Tutorial");
    VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);
    
    // Create cube geometry
    float cubeVertices[] = {
        // Front face
        -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,
        // Back face
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, -1.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   0.0f, 0.0f, -1.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f, 0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 0.0f, -1.0f,  1.0f, 1.0f,
    };
    
    uint32_t cubeIndices[] = {
        0, 1, 2, 2, 3, 0,  // Front
        4, 5, 6, 6, 7, 4,  // Back
        // Add other faces...
    };
    
    VEBufferIndex vertexBuffer = veCreateVertexBuffer(device, cubeVertices, sizeof(cubeVertices), 0);
    VEBufferIndex indexBuffer = veCreateIndexBuffer(device, cubeIndices, sizeof(cubeIndices), 0);
    
    // Create uniform buffer for matrices
    typedef struct {
        mat4 modelMatrix;
        mat4 viewMatrix;
        mat4 projMatrix;
        vec4 lightPos;
        vec4 lightColor;
    } SceneUniforms;
    
    VEBufferIndex uniformBuffer = veCreateUniformBuffer(device, sizeof(SceneUniforms), 0, NULL);
    
    printf("✓ Created 3D geometry buffers\n");
    
    // Create 3D shaders
    const char* vertex3DShader = 
        "#version 450 core\n"
        "layout(push_constant) uniform PushConstants {\n"
        "    uint vertexBuffer;\n"
        "    uint uniformBuffer;\n"
        "    float time;\n"
        "} pc;\n"
        "layout(location = 0) out vec3 fragWorldPos;\n"
        "layout(location = 1) out vec3 fragNormal;\n"
        "layout(location = 2) out vec2 fragTexCoord;\n"
        "void main() {\n"
        "    // In real implementation, would access globalBuffers[pc.vertexBuffer]\n"
        "    vec3 positions[8] = vec3[](\n"
        "        vec3(-0.5,-0.5, 0.5), vec3( 0.5,-0.5, 0.5), vec3( 0.5, 0.5, 0.5), vec3(-0.5, 0.5, 0.5),\n"
        "        vec3(-0.5,-0.5,-0.5), vec3( 0.5,-0.5,-0.5), vec3( 0.5, 0.5,-0.5), vec3(-0.5, 0.5,-0.5)\n"
        "    );\n"
        "    vec3 position = positions[gl_VertexIndex % 8];\n"
        "    \n"
        "    // Simple rotation animation\n"
        "    float angle = pc.time;\n"
        "    mat3 rotY = mat3(cos(angle), 0, sin(angle), 0, 1, 0, -sin(angle), 0, cos(angle));\n"
        "    position = rotY * position;\n"
        "    \n"
        "    // Move back for perspective\n"
        "    position.z -= 2.0;\n"
        "    \n"
        "    // Simple perspective projection\n"
        "    gl_Position = vec4(position.xy / (1.0 + position.z * 0.5), position.z * 0.1, 1.0);\n"
        "    \n"
        "    fragWorldPos = position;\n"
        "    fragNormal = rotY * vec3(0, 0, 1);\n"
        "    fragTexCoord = position.xy + 0.5;\n"
        "}\n";
    
    const char* fragment3DShader =
        "#version 450 core\n"
        "layout(location = 0) in vec3 fragWorldPos;\n"
        "layout(location = 1) in vec3 fragNormal;\n"
        "layout(location = 2) in vec2 fragTexCoord;\n"
        "layout(location = 0) out vec4 outColor;\n"
        "void main() {\n"
        "    vec3 lightDir = normalize(vec3(1, 1, 1));\n"
        "    float lighting = max(dot(normalize(fragNormal), lightDir), 0.1);\n"
        "    vec3 color = vec3(0.7, 0.4, 0.2) * lighting;\n"
        "    outColor = vec4(color, 1.0);\n"
        "}\n";
    
    VEShader* vs3D = veCreateShaderFromSource(device, vertex3DShader, VE_SHADER_VERTEX, "main");
    VEShader* fs3D = veCreateShaderFromSource(device, fragment3DShader, VE_SHADER_FRAGMENT, "main");
    VEPipeline* pipeline3D = veCreateSimpleGraphicsPipeline(device, vs3D, fs3D);
    
    printf("✓ Created 3D rendering pipeline\n");
    
    // Render rotating cube
    printf("Rendering rotating 3D cube...\n");
    
    typedef struct {
        uint32_t vertexBuffer;
        uint32_t uniformBuffer;
        float time;
    } Render3DPushConstants;
    
    for (int frame = 0; frame < 180; frame++) {  // 3 seconds at 60fps
        float currentTime = frame * 0.016f;
        
        // Update uniforms
        SceneUniforms uniforms = {0};
        mat4_identity(uniforms.modelMatrix);
        mat4_identity(uniforms.viewMatrix);
        mat4_perspective(uniforms.projMatrix, 45.0f * 3.14159f / 180.0f, 800.0f/600.0f, 0.1f, 100.0f);
        uniforms.lightPos = (vec4){2.0f, 2.0f, 2.0f, 1.0f};
        uniforms.lightColor = (vec4){1.0f, 1.0f, 1.0f, 1.0f};
        
        veUpdateBuffer(device, uniformBuffer, &uniforms, sizeof(uniforms), 0);
        
        // Render frame
        VECommandBuffer* cmd = veGetSecondaryCommandBuffer(device);
        
        vePushDebugGroup(cmd, "3D Cube Rendering");
        veBindPipeline(cmd, pipeline3D);
        
        Render3DPushConstants pushData = {
            .vertexBuffer = vertexBuffer,
            .uniformBuffer = uniformBuffer,
            .time = currentTime
        };
        
        vePushConstants(cmd, &pushData, sizeof(pushData), 0);
        
        // Draw cube faces
        veDrawIndexed(cmd, vertexBuffer, indexBuffer, 12, 1, 0, 0, 0);  // 2 triangles per face, 6 faces
        
        vePopDebugGroup(cmd);
        
        veSubmitCommandBuffer(cmd, NULL);
        veDeviceWaitIdle(device);
        
        if (frame % 30 == 0) {
            printf("  Frame %d: rotation %.1f degrees\n", frame, currentTime * 180.0f / 3.14159f);
        }
    }
    
    printf("✓ Rendered 180 frames of rotating cube\n");
    
    // Cleanup
    veDestroyPipeline(pipeline3D);
    veDestroyShader(fs3D);
    veDestroyShader(vs3D);
    veDestroyBuffer(device, uniformBuffer);
    veDestroyBuffer(device, indexBuffer);
    veDestroyBuffer(device, vertexBuffer);
    
    veDestroyDevice(device);
    veDestroyContext(context);
    
    printf("\nTutorial 4 completed successfully!\n");
    printf("Key concepts learned:\n");
    printf("  • 3D vertex data with positions, normals, UVs\n");
    printf("  • Index buffers for efficient geometry\n");
    printf("  • Uniform buffers for transformation matrices\n");
    printf("  • Perspective projection and 3D transformations\n");
    printf("  • Basic lighting calculations\n");
    
    return 0;
}
```

This tutorial guide provides a comprehensive learning path from basic VulkEase concepts to advanced 3D rendering. Each tutorial builds on the previous ones while introducing new concepts gradually. The remaining tutorials (5-7) would continue with compute shaders, advanced techniques, and production application structure.