# VulkEase API Documentation

**Version 1.0**  
**High-Performance Bindless Graphics API**

---

## Table of Contents

1. [Overview](#overview)
2. [Core Concepts](#core-concepts)
3. [Getting Started](#getting-started)
4. [API Reference](#api-reference)
5. [Shader Integration](#shader-integration)
6. [Performance Guide](#performance-guide)
7. [Best Practices](#best-practices)
8. [Error Handling](#error-handling)
9. [Platform Support](#platform-support)
10. [Examples](#examples)

---

## Overview

VulkEase is a revolutionary graphics API that eliminates the complexity of traditional graphics programming while delivering maximum performance on modern desktop GPUs. Built on top of Vulkan, it provides a **true bindless resource model** where all resources are globally accessible via simple indices.

### Key Features

🚀 **True Bindless Resource Access**
- 65,536+ textures globally accessible
- 32,768+ buffers globally accessible  
- 1,024+ samplers globally accessible
- Zero CPU overhead - no binding operations

🎯 **Zero-Complexity Programming Model**
- No descriptor sets or binding points
- Resources accessed via simple uint32_t indices
- Push constants for all resource references
- Automatic shader enhancement

🔥 **Modern GPU Features**
- Dynamic rendering (no legacy render passes)
- Timeline semaphores for advanced sync
- Indirect rendering and compute dispatch
- Expert-level memory management

⚡ **Performance-First Design**
- Direct Vulkan backend for maximum speed
- Minimal CPU overhead
- GPU-optimal resource layout
- Advanced debugging and profiling tools

---

## Core Concepts

### Bindless Resource Model

Traditional graphics APIs require you to "bind" resources before using them:

```c
// Traditional API - COMPLEX and ERROR-PRONE
vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, NULL);
vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
```

VulkEase eliminates ALL binding operations:

```c
// VulkEase - SIMPLE and FAST
struct { uint32_t vertexBuffer, indexBuffer, texture, sampler; } push = {vbuf, ibuf, tex, samp};
vePushConstants(cmd, &push, sizeof(push), 0);
veDrawIndexed(cmd, vertexBuffer, indexBuffer, indexCount, instanceCount, 0, 0, 0);
```

### Resource Indices

All VulkEase resources are identified by simple indices:

```c
VETextureIndex texture = veCreateTexture2D(device, 512, 512, format, usage, data, size);
VEBufferIndex buffer = veCreateVertexBuffer(device, vertices, size, 0);
VESamplerIndex sampler = veCreateLinearSampler(device);

// These are just uint32_t values that can be used directly in shaders!
printf("Texture index: %u, Buffer index: %u, Sampler index: %u\n", texture, buffer, sampler);
```

### Automatic Shader Enhancement

VulkEase automatically injects bindless resource declarations into your shaders:

```glsl
#version 450 core

// VulkEase automatically adds these declarations:
// layout(set = 0, binding = 0) uniform sampler2D globalTextures[];
// layout(set = 0, binding = 1) uniform sampler globalSamplers[];
// layout(set = 0, binding = 2) buffer VertexBuffer { float data[]; } globalVertexBuffers[];
// layout(set = 0, binding = 3) buffer UniformBuffer { float data[]; } globalUniformBuffers[];
// layout(set = 0, binding = 4) buffer StorageBuffer { float data[]; } globalStorageBuffers[];

layout(push_constant) uniform PushConstants {
    uint diffuseTexture;
    uint sampler;
    uint vertexBuffer;
} pc;

void main() {
    // Access resources directly by index!
    vec4 color = texture(sampler2D(globalTextures[pc.diffuseTexture], globalSamplers[pc.sampler]), uv);
    
    // Or use convenience macros:
    vec4 color2 = ACCESS_TEXTURE_SAMPLER(pc.diffuseTexture, pc.sampler, uv);
}
```

---

## Getting Started

### Installation

#### Windows
```cmd
# Download VulkEase SDK from releases
# Extract to C:\VulkEase\
# Add C:\VulkEase\bin to PATH
# Add C:\VulkEase\lib to library path
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt install libvulkease-dev

# Arch Linux
sudo pacman -S vulkease

# From source
git clone https://github.com/vulkease/vulkease.git
cd vulkease && mkdir build && cd build
cmake .. && make && sudo make install
```

#### macOS
```bash
# Homebrew
brew install vulkease

# MacPorts
sudo port install vulkease
```

### Your First VulkEase Application

```c
#include "vulkease.h"
#include <stdio.h>

int main() {
    // 1. Initialize VulkEase
    VEContext* context = veCreateContext("Hello VulkEase");
    VEDevice* device = veCreateDevice(context, VE_DEVICE_TYPE_AUTO);
    
    // 2. Create resources (returns indices!)
    float vertices[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f};
    VEBufferIndex vertexBuffer = veCreateVertexBuffer(device, vertices, sizeof(vertices), 0);
    
    uint8_t textureData[] = {255, 0, 0, 255}; // Red pixel
    VETextureIndex texture = veCreateTexture2D(device, 1, 1, VE_FORMAT_RGBA8_UNORM, 
                                              VE_TEXTURE_USAGE_SAMPLED, textureData, sizeof(textureData));
    
    VESamplerIndex sampler = veCreateLinearSampler(device);
    
    printf("Created resources: vertex buffer %u, texture %u, sampler %u\n", 
           vertexBuffer, texture, sampler);
    
    // 3. Cleanup
    veDestroyTexture(device, texture);
    veDestroyBuffer(device, vertexBuffer);
    veDestroySampler(device, sampler);
    veDestroyDevice(device);
    veDestroyContext(context);
    
    return 0;
}
```

Compile and run:
```bash
gcc -o hello hello.c -lvulkease
./hello
```

---

## API Reference

### Context and Device Management

#### veCreateContext
```c
VEContext* veCreateContext(const char* applicationName);
```
Creates a VulkEase context with the specified application name.

**Parameters:**
- `applicationName`: Name of your application (used for debugging)

**Returns:** Context handle or NULL on failure

**Example:**
```c
VEContext* context = veCreateContext("My Game Engine");
if (!context) {
    fprintf(stderr, "Failed to create context: %s\n", veGetLastError());
    exit(1);
}
```

#### veCreateDevice
```c
VEDevice* veCreateDevice(VEContext* context, VEDeviceType deviceType);
```
Creates a VulkEase device from the specified context.

**Parameters:**
- `context`: Valid VulkEase context
- `deviceType`: Device selection preference
  - `VE_DEVICE_TYPE_AUTO`: Automatically select best GPU
  - `VE_DEVICE_TYPE_DISCRETE`: Prefer discrete GPU
  - `VE_DEVICE_TYPE_INTEGRATED`: Prefer integrated GPU

**Returns:** Device handle or NULL on failure

#### veGetDeviceInfo
```c
VEResult veGetDeviceInfo(VEDevice* device, char* deviceName, uint32_t* maxTextureSize, 
                        VESampleCount* maxSampleCount, float* timestampPeriod);
```
Retrieves device capabilities and information.

### Resource Creation

#### Texture Creation

##### veCreateTexture2D
```c
VETextureIndex veCreateTexture2D(VEDevice* device, uint32_t width, uint32_t height, 
                                VEFormat format, VETextureUsage usage, 
                                const void* data, size_t dataSize);
```
Creates a 2D texture.

**Parameters:**
- `device`: VulkEase device
- `width`, `height`: Texture dimensions
- `format`: Pixel format (see VEFormat enum)
- `usage`: Texture usage flags (see VETextureUsage enum)
- `data`: Initial texture data (can be NULL)
- `dataSize`: Size of data in bytes

**Returns:** Texture index or VE_INVALID_TEXTURE_INDEX on failure

**Formats:**
```c
typedef enum {
    VE_FORMAT_RGBA8_UNORM,     // 8-bit RGBA
    VE_FORMAT_RGBA8_SRGB,      // 8-bit sRGB RGBA
    VE_FORMAT_RGBA16_FLOAT,    // 16-bit float RGBA
    VE_FORMAT_RGBA32_FLOAT,    // 32-bit float RGBA
    VE_FORMAT_RGB8_UNORM,      // 8-bit RGB
    VE_FORMAT_RG8_UNORM,       // 8-bit RG
    VE_FORMAT_R8_UNORM,        // 8-bit R
    VE_FORMAT_D32_FLOAT,       // 32-bit depth
    VE_FORMAT_D24_UNORM_S8_UINT, // 24-bit depth, 8-bit stencil
    // ... more formats
} VEFormat;
```

**Usage Flags:**
```c
typedef enum {
    VE_TEXTURE_USAGE_SAMPLED        = 0x00000001, // Can be sampled in shaders
    VE_TEXTURE_USAGE_COLOR_TARGET   = 0x00000002, // Can be used as color render target
    VE_TEXTURE_USAGE_DEPTH_TARGET   = 0x00000004, // Can be used as depth render target
    VE_TEXTURE_USAGE_STORAGE        = 0x00000008, // Can be used for compute storage
    VE_TEXTURE_USAGE_TRANSFER_SRC   = 0x00000010, // Can be source of copy operations
    VE_TEXTURE_USAGE_TRANSFER_DST   = 0x00000020, // Can be destination of copy operations
} VETextureUsage;
```

##### veCreateTexture1D
```c
VETextureIndex veCreateTexture1D(VEDevice* device, uint32_t width, VEFormat format,
                                VETextureUsage usage, const void* data, size_t dataSize);
```
Creates a 1D texture for gradients, lookup tables, etc.

##### veCreateTexture3D
```c
VETextureIndex veCreateTexture3D(VEDevice* device, uint32_t width, uint32_t height, uint32_t depth,
                                VEFormat format, VETextureUsage usage, const void* data, size_t dataSize);
```
Creates a 3D texture for volumetric data.

##### veCreateTextureCube
```c
VETextureIndex veCreateTextureCube(VEDevice* device, uint32_t size, VEFormat format,
                                  VETextureUsage usage, const void* faceData[6], size_t faceDataSize);
```
Creates a cube texture for environment mapping.

**Parameters:**
- `faceData`: Array of 6 pointers to face data (+X, -X, +Y, -Y, +Z, -Z)
- `faceDataSize`: Size of each face's data

##### veCreateTexture2DArray
```c
VETextureIndex veCreateTexture2DArray(VEDevice* device, uint32_t width, uint32_t height, uint32_t layerCount,
                                     VEFormat format, VETextureUsage usage, const void* data, size_t dataSize);
```
Creates a 2D texture array.

##### veCreateTexture2DMultisample
```c
VETextureIndex veCreateTexture2DMultisample(VEDevice* device, uint32_t width, uint32_t height, 
                                           VEFormat format, VESampleCount sampleCount, VETextureUsage usage);
```
Creates a multisample 2D texture for MSAA.

**Sample Counts:**
```c
typedef enum {
    VE_SAMPLE_COUNT_1  = 1,
    VE_SAMPLE_COUNT_2  = 2,
    VE_SAMPLE_COUNT_4  = 4,
    VE_SAMPLE_COUNT_8  = 8,
    VE_SAMPLE_COUNT_16 = 16,
    VE_SAMPLE_COUNT_32 = 32,
    VE_SAMPLE_COUNT_64 = 64
} VESampleCount;
```

#### Buffer Creation

##### veCreateVertexBuffer
```c
VEBufferIndex veCreateVertexBuffer(VEDevice* device, const void* data, size_t size, VEBufferUsage usage);
```
Creates a vertex buffer.

**Parameters:**
- `device`: VulkEase device
- `data`: Vertex data
- `size`: Buffer size in bytes
- `usage`: Additional usage flags

**Buffer Usage Flags:**
```c
typedef enum {
    VE_BUFFER_USAGE_TRANSFER_SRC = 0x00000001,
    VE_BUFFER_USAGE_TRANSFER_DST = 0x00000002,
    VE_BUFFER_USAGE_STORAGE      = 0x00000008,
} VEBufferUsage;
```

##### veCreateIndexBuffer
```c
VEBufferIndex veCreateIndexBuffer(VEDevice* device, const void* data, size_t size, VEBufferUsage usage);
```
Creates an index buffer for indexed rendering.

##### veCreateUniformBuffer
```c
VEBufferIndex veCreateUniformBuffer(VEDevice* device, size_t size, VEBufferUsage usage, const void* initialData);
```
Creates a uniform buffer for shader constants.

##### veCreateStorageBuffer
```c
VEBufferIndex veCreateStorageBuffer(VEDevice* device, size_t size, VEBufferUsage usage, const void* initialData);
```
Creates a storage buffer for compute shader data.

##### veCreateIndirectBuffer
```c
VEBufferIndex veCreateIndirectBuffer(VEDevice* device, size_t size, VEBufferUsage usage, const void* initialData);
```
Creates a buffer for indirect rendering commands.

#### Sampler Creation

##### veCreateLinearSampler
```c
VESamplerIndex veCreateLinearSampler(VEDevice* device);
```
Creates a sampler with linear filtering and repeat addressing.

##### veCreateNearestSampler
```c
VESamplerIndex veCreateNearestSampler(VEDevice* device);
```
Creates a sampler with nearest filtering.

##### veCreateSampler
```c
VESamplerIndex veCreateSampler(VEDevice* device, VEFilter minFilter, VEFilter magFilter, VEFilter mipmapFilter,
                              VEAddressMode addressModeU, VEAddressMode addressModeV, VEAddressMode addressModeW,
                              float maxAnisotropy);
```
Creates a custom sampler with full control.

**Filter Types:**
```c
typedef enum {
    VE_FILTER_NEAREST = 0,
    VE_FILTER_LINEAR = 1
} VEFilter;
```

**Address Modes:**
```c
typedef enum {
    VE_ADDRESS_MODE_REPEAT = 0,
    VE_ADDRESS_MODE_MIRRORED_REPEAT = 1,
    VE_ADDRESS_MODE_CLAMP_TO_EDGE = 2,
    VE_ADDRESS_MODE_CLAMP_TO_BORDER = 3
} VEAddressMode;
```

### Shader and Pipeline Creation

#### Shader Creation

##### veCreateShaderFromSource
```c
VEShader* veCreateShaderFromSource(VEDevice* device, const char* source, VEShaderStage stage, const char* entryPoint);
```
Creates a shader from GLSL source code.

**Parameters:**
- `source`: GLSL source code
- `stage`: Shader stage
- `entryPoint`: Entry point function name (usually "main")

**Shader Stages:**
```c
typedef enum {
    VE_SHADER_VERTEX = 0,
    VE_SHADER_FRAGMENT = 1,
    VE_SHADER_GEOMETRY = 2,
    VE_SHADER_TESSELLATION_CONTROL = 3,
    VE_SHADER_TESSELLATION_EVALUATION = 4,
    VE_SHADER_COMPUTE = 5
} VEShaderStage;
```

##### veCreateShaderFromFile
```c
VEShader* veCreateShaderFromFile(VEDevice* device, const char* filename, VEShaderStage stage, const char* entryPoint);
```
Creates a shader from a file.

##### veCreateShaderFromSpirv
```c
VEShader* veCreateShaderFromSpirv(VEDevice* device, const uint32_t* spirvData, size_t spirvSize, 
                                 VEShaderStage stage, const char* entryPoint);
```
Creates a shader from pre-compiled SPIR-V bytecode.

#### Hot Reload
```c
VEResult veEnableShaderHotReload(VEShader* shader);
VEBool veCheckShaderReload(VEShader* shader);
```
Enable and check for shader file changes during development.

#### Pipeline Creation

##### veCreateSimpleGraphicsPipeline
```c
VEPipeline* veCreateSimpleGraphicsPipeline(VEDevice* device, VEShader* vertexShader, VEShader* fragmentShader);
```
Creates a graphics pipeline with default settings.

##### veCreateGraphicsPipeline
```c
VEPipeline* veCreateGraphicsPipeline(VEDevice* device, const VEGraphicsPipelineDesc* desc);
```
Creates a graphics pipeline with full configuration.

**Pipeline Description:**
```c
typedef struct {
    VEShader* vertexShader;
    VEShader* fragmentShader;
    VEShader* geometryShader;           // Optional
    VEShader* tessControlShader;        // Optional
    VEShader* tessEvalShader;          // Optional
    
    VEVertexAttribute* vertexAttributes;
    uint32_t vertexAttributeCount;
    
    VEPrimitiveTopology topology;
    VEBool primitiveRestart;
    VEBool depthClampEnable;
    VEBool rasterizerDiscardEnable;
    VEBool depthBiasEnable;
    float lineWidth;
    
    VESampleCount sampleCount;
    VEBool sampleShadingEnable;
    float minSampleShading;
    
    VEBool depthTestEnable;
    VEBool depthWriteEnable;
    VECompareOp depthCompareOp;
    
    VEBool blendEnable;
    VEBlendOp colorBlendOp;
    VEBlendOp alphaBlendOp;
    
    const char* debugName;
} VEGraphicsPipelineDesc;
```

**Vertex Attributes:**
```c
typedef struct {
    uint32_t location;
    VEFormat format;
    uint32_t offset;
} VEVertexAttribute;
```

##### veCreateComputePipeline
```c
VEComputePipeline* veCreateComputePipeline(VEDevice* device, VEShader* computeShader, const char* debugName);
```
Creates a compute pipeline.

### Command Recording

#### Command Buffer Management

##### veGetSecondaryCommandBuffer
```c
VECommandBuffer* veGetSecondaryCommandBuffer(VEDevice* device);
```
Gets a command buffer for recording commands.

##### veSubmitCommandBuffer
```c
VEResult veSubmitCommandBuffer(VECommandBuffer* cmd, VEFence* fence);
```
Submits a command buffer for execution.

##### veSubmitCommandBuffers
```c
VEResult veSubmitCommandBuffers(VECommandBuffer** cmds, uint32_t count, VEFence* fence);
```
Submits multiple command buffers as a batch.

#### Drawing Commands

##### veDraw
```c
void veDraw(VECommandBuffer* cmd, VEBufferIndex vertexBuffer, uint32_t vertexCount, 
           uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
```
Record a draw command. **NO BINDING REQUIRED** - just pass the vertex buffer index!

##### veDrawIndexed
```c
void veDrawIndexed(VECommandBuffer* cmd, VEBufferIndex vertexBuffer, VEBufferIndex indexBuffer,
                  uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, 
                  int32_t vertexOffset, uint32_t firstInstance);
```
Record an indexed draw command.

##### veDrawIndirect
```c
void veDrawIndirect(VECommandBuffer* cmd, VEBufferIndex indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride);
```
Record an indirect draw command.

#### Compute Commands

##### veBindComputePipeline
```c
void veBindComputePipeline(VECommandBuffer* cmd, VEComputePipeline* pipeline);
```
Bind a compute pipeline.

##### veDispatch
```c
void veDispatch(VECommandBuffer* cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
```
Dispatch compute work groups.

##### veDispatchIndirect
```c
void veDispatchIndirect(VECommandBuffer* cmd, VEBufferIndex indirectBuffer, uint32_t offset);
```
Dispatch compute work groups indirectly from buffer.

#### Pipeline and State Commands

##### veBindPipeline
```c
void veBindPipeline(VECommandBuffer* cmd, VEPipeline* pipeline);
```
Bind a graphics pipeline.

##### vePushConstants
```c
void vePushConstants(VECommandBuffer* cmd, const void* data, uint32_t size, uint32_t offset);
```
Push constants containing resource indices and parameters.

**Example:**
```c
struct {
    uint32_t vertexBuffer;
    uint32_t diffuseTexture;
    uint32_t sampler;
    float time;
} pushData = {vbuf, tex, samp, currentTime};

vePushConstants(cmd, &pushData, sizeof(pushData), 0);
```

##### veSetViewport
```c
void veSetViewport(VECommandBuffer* cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height, float minDepth, float maxDepth);
```
Set the viewport.

##### veSetScissor
```c
void veSetScissor(VECommandBuffer* cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
```
Set scissor rectangle.

### Synchronization

#### Fences

##### veCreateFence
```c
VEFence* veCreateFence(VEDevice* device, VEBool signaled);
```
Create a fence for CPU-GPU synchronization.

##### veWaitForFence
```c
VEResult veWaitForFence(VEFence* fence, uint64_t timeoutNs);
```
Wait for a fence to be signaled.

##### veWaitForFences
```c
VEResult veWaitForFences(VEFence** fences, uint32_t count, VEBool waitAll, uint64_t timeoutNs);
```
Wait for multiple fences.

##### veResetFence
```c
void veResetFence(VEFence* fence);
```
Reset a fence to unsignaled state.

#### Semaphores

##### veCreateBinarySemaphore
```c
VESemaphore* veCreateBinarySemaphore(VEDevice* device);
```
Create a binary semaphore for GPU-GPU synchronization.

##### veCreateTimelineSemaphore
```c
VESemaphore* veCreateTimelineSemaphore(VEDevice* device, uint64_t initialValue);
```
Create a timeline semaphore with initial value.

##### veSignalTimelineSemaphore
```c
VEResult veSignalTimelineSemaphore(VESemaphore* semaphore, uint64_t value);
```
Signal a timeline semaphore to a specific value.

##### veWaitForTimelineSemaphore
```c
VEResult veWaitForTimelineSemaphore(VESemaphore* semaphore, uint64_t value, uint64_t timeoutNs);
```
Wait for a timeline semaphore to reach a value.

### Swapchain and Presentation

#### veCreateSwapchain
```c
VESwapchain* veCreateSwapchain(VEDevice* device, void* windowHandle, uint32_t width, uint32_t height);
```
Create a swapchain for presenting to a window.

#### veCreateSwapchainAdvanced
```c
VESwapchain* veCreateSwapchainAdvanced(VEDevice* device, void* window, uint32_t width, uint32_t height,
                                      VEFormat format, VEPresentMode presentMode, VEBool vsync);
```
Create a swapchain with advanced options.

#### veResizeSwapchain
```c
VEResult veResizeSwapchain(VESwapchain* swapchain, uint32_t width, uint32_t height);
```
Resize a swapchain when window size changes.

#### veBeginFrame
```c
VECommandBuffer* veBeginFrame(VESwapchain* swapchain);
```
Begin a frame and get a command buffer.

#### veEndFrameAndPresent
```c
VEResult veEndFrameAndPresent(VECommandBuffer* cmd, VESwapchain* swapchain);
```
End frame recording and present to screen.

### Resource Updates

#### veUpdateTexture
```c
VEResult veUpdateTexture(VEDevice* device, VETextureIndex texture, const void* data, size_t dataSize,
                        uint32_t mipLevel, uint32_t arrayLayer);
```
Update texture data at runtime.

#### veUpdateBuffer
```c
VEResult veUpdateBuffer(VEDevice* device, VEBufferIndex buffer, const void* data, size_t dataSize, size_t offset);
```
Update buffer data at runtime.

### Advanced Features

#### Memory Pools

##### veCreateMemoryPool
```c
VEMemoryPool* veCreateMemoryPool(VEDevice* device, size_t size, VEMemoryUsage usage);
```
Create a memory pool for expert-level memory management.

##### veAllocateFromPool
```c
uint64_t veAllocateFromPool(VEMemoryPool* pool, size_t size, size_t alignment);
```
Allocate memory from pool.

##### veFreeFromPool
```c
void veFreeFromPool(VEMemoryPool* pool, uint64_t offset);
```
Free memory back to pool.

#### Advanced Operations

##### veGenerateMipmaps
```c
void veGenerateMipmaps(VECommandBuffer* cmd, VETextureIndex texture);
```
Generate mipmaps using GPU blitting.

##### veResolveTexture
```c
void veResolveTexture(VECommandBuffer* cmd, VETextureIndex msaaTexture, VETextureIndex resolveTexture);
```
Resolve MSAA texture to single-sample texture.

##### veCopyBufferToTexture
```c
void veCopyBufferToTexture(VECommandBuffer* cmd, VEBufferIndex srcBuffer, VETextureIndex dstTexture,
                          uint32_t width, uint32_t height, uint32_t mipLevel, uint32_t arrayLayer);
```
Copy buffer data to texture.

### Debug and Profiling

#### Debug Groups and Labels

##### vePushDebugGroup
```c
void vePushDebugGroup(VECommandBuffer* cmd, const char* name);
```
Begin a debug group for profilers.

##### vePopDebugGroup
```c
void vePopDebugGroup(VECommandBuffer* cmd);
```
End current debug group.

##### veInsertDebugLabel
```c
void veInsertDebugLabel(VECommandBuffer* cmd, const char* label);
```
Insert a debug label.

#### GPU Timing

##### veBeginGpuTimer
```c
uint32_t veBeginGpuTimer(VECommandBuffer* cmd, const char* name);
```
Begin GPU timing measurement.

##### veEndGpuTimer
```c
void veEndGpuTimer(VECommandBuffer* cmd, uint32_t timerId);
```
End GPU timing measurement.

##### veGetGpuTimerResult
```c
uint64_t veGetGpuTimerResult(VEDevice* device, uint32_t timerId);
```
Get GPU timing result in nanoseconds.

#### Performance Statistics

##### veGetPerformanceStats
```c
VEResult veGetPerformanceStats(VEDevice* device, VEPerformanceStats* stats);
```
Get comprehensive performance statistics.

```c
typedef struct {
    float frameTimeMs;
    uint32_t framesPerSecond;
    uint32_t drawCalls;
    uint32_t vertices;
    uint32_t triangles;
    uint32_t texturesAllocated;
    uint32_t buffersAllocated;
    uint32_t samplersAllocated;
    uint64_t textureMemoryUsed;
    uint64_t bufferMemoryUsed;
    uint64_t totalMemoryUsed;
} VEPerformanceStats;
```

#### Debug Information

##### vePrintDebugInfo
```c
void vePrintDebugInfo(VEDevice* device);
```
Print comprehensive device and resource information.

##### veSetResourceDebugName
```c
void veSetResourceDebugName(VEDevice* device, const char* resourceType, uint32_t resourceIndex, const char* name);
```
Set debug name for resource.

##### veTakeScreenshot
```c
VEResult veTakeScreenshot(VESwapchain* swapchain, const char* filename);
```
Capture current frame to image file.

### Error Handling

##### veGetLastError
```c
const char* veGetLastError(void);
```
Get description of last error.

##### Result Codes
```c
typedef enum {
    VE_SUCCESS = 0,
    VE_ERROR_INITIALIZATION_FAILED = 1,
    VE_ERROR_OUT_OF_MEMORY = 2,
    VE_ERROR_DEVICE_LOST = 3,
    VE_ERROR_INVALID_PARAMETER = 4,
    VE_ERROR_SHADER_COMPILATION_FAILED = 5,
    VE_ERROR_SWAPCHAIN_OUT_OF_DATE = 6,
    VE_ERROR_UNKNOWN = 7
} VEResult;
```

---

## Shader Integration

### Automatic Resource Injection

VulkEase automatically adds these declarations to your shaders:

```glsl
// Textures - up to 65,536
layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

// Samplers - up to 1,024  
layout(set = 0, binding = 1) uniform sampler globalSamplers[];

// Vertex buffers - bindless vertex fetching
layout(set = 0, binding = 2) buffer VertexBuffer {
    float data[];
} globalVertexBuffers[];

// Uniform buffers
layout(set = 0, binding = 3) buffer UniformBuffer {
    float data[];
} globalUniformBuffers[];

// Storage buffers - up to 32,768
layout(set = 0, binding = 4) buffer StorageBuffer {
    float data[];
} globalStorageBuffers[];
```

### Helper Macros

VulkEase provides convenient macros for resource access:

```glsl
// Texture access
#define ACCESS_TEXTURE(textureIndex, uv) texture(globalTextures[textureIndex], uv)
#define ACCESS_TEXTURE_SAMPLER(textureIndex, samplerIndex, uv) texture(sampler2D(globalTextures[textureIndex], globalSamplers[samplerIndex]), uv)

// Buffer access
#define ACCESS_VERTEX_BUFFER(bufferIndex, elementIndex) globalVertexBuffers[bufferIndex].data[elementIndex]
#define ACCESS_UNIFORM_BUFFER(bufferIndex, elementIndex) globalUniformBuffers[bufferIndex].data[elementIndex]
#define ACCESS_STORAGE_BUFFER(bufferIndex, elementIndex) globalStorageBuffers[bufferIndex].data[elementIndex]
```

### Example Shaders

#### Vertex Shader
```glsl
#version 450 core

layout(push_constant) uniform PushConstants {
    uint vertexBuffer;
    uint uniformBuffer;
    float time;
} pc;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

void main() {
    // Access vertex data bindlessly (conceptual - actual implementation varies)
    // In practice, you still use traditional vertex attributes for performance,
    // but can supplement with bindless buffer access
    
    vec3 position = vec3(ACCESS_VERTEX_BUFFER(pc.vertexBuffer, gl_VertexIndex * 3 + 0),
                        ACCESS_VERTEX_BUFFER(pc.vertexBuffer, gl_VertexIndex * 3 + 1), 
                        ACCESS_VERTEX_BUFFER(pc.vertexBuffer, gl_VertexIndex * 3 + 2));
    
    // Access uniform data
    mat4 mvpMatrix = mat4(ACCESS_UNIFORM_BUFFER(pc.uniformBuffer, 0),  // First column
                         ACCESS_UNIFORM_BUFFER(pc.uniformBuffer, 4),  // Second column
                         ACCESS_UNIFORM_BUFFER(pc.uniformBuffer, 8),  // Third column  
                         ACCESS_UNIFORM_BUFFER(pc.uniformBuffer, 12)); // Fourth column
    
    gl_Position = mvpMatrix * vec4(position, 1.0);
    fragTexCoord = position.xy * 0.5 + 0.5;
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
```

#### Fragment Shader
```glsl
#version 450 core

layout(push_constant) uniform PushConstants {
    uint vertexBuffer;
    uint uniformBuffer;
    float time;
    uint diffuseTexture;
    uint normalTexture;
    uint sampler;
} pc;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Bindless texture access - THE KEY FEATURE!
    vec4 diffuseColor = ACCESS_TEXTURE_SAMPLER(pc.diffuseTexture, pc.sampler, fragTexCoord);
    vec4 normalColor = ACCESS_TEXTURE_SAMPLER(pc.normalTexture, pc.sampler, fragTexCoord);
    
    // Process normal map
    vec3 normal = normalColor.rgb * 2.0 - 1.0;
    
    // Simple lighting
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float lighting = max(dot(normal, lightDir), 0.1);
    
    outColor = diffuseColor * lighting;
}
```

#### Compute Shader
```glsl
#version 450 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushConstants {
    uint inputBuffer;
    uint outputBuffer;
    uint elementCount;
    float multiplier;
} pc;

void main() {
    uint index = gl_GlobalInvocationID.x;
    
    if (index >= pc.elementCount) {
        return;
    }
    
    // Bindless storage buffer access
    float inputValue = ACCESS_STORAGE_BUFFER(pc.inputBuffer, index);
    float outputValue = inputValue * pc.multiplier;
    
    // Write to output buffer
    globalStorageBuffers[pc.outputBuffer].data[index] = outputValue;
}
```

---

This covers the first major section of the API documentation. Would you like me to continue with the remaining sections (Performance Guide, Best Practices, etc.)?