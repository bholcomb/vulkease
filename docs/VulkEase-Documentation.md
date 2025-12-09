# VulkEase 2.0 User Documentation

**VulkEase 2.0** is a modern Vulkan 1.3+ graphics API that eliminates the complexity of traditional Vulkan while unlocking its full power. It features bindless resources, buffer device addresses, dynamic rendering, shader objects, and GPU-driven rendering capabilities.

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Core Concepts](#core-concepts)
4. [Getting Started](#getting-started)
5. [API Reference](#api-reference)
6. [Examples](#examples)
7. [Advanced Topics](#advanced-topics)
8. [Performance Tips](#performance-tips)
9. [Debugging](#debugging)
10. [Migration Guide](#migration-guide)

---

## Overview

### Key Features

- **Buffer Device Address**: Direct GPU pointer access to buffers - no descriptor sets needed
- **Bindless Textures & Samplers**: Access thousands of textures via array indices
- **Dynamic Rendering**: No render passes - just begin rendering and draw
- **Shader Objects**: Hot-reloadable shaders without pipeline recreation
- **Render Configurations**: Lightweight state management inspired by Graphics Pipeline Libraries
- **GPU-Driven Rendering**: Indirect draws and compute dispatches
- **Automatic Memory Management**: VMA integration handles allocations
- **Built-in Texture Loading**: STB Image integration for common formats

### What VulkEase Eliminates

- Render passes and framebuffers
- Graphics pipelines (replaced by shader objects + render configs)
- Descriptor sets and layouts (replaced by bindless + buffer addresses)
- Manual memory management
- Complex synchronization barriers
- SPIR-V compilation complexity

---

## System Requirements

### Minimum Requirements
- **Vulkan 1.3** or newer
- **GPU**: Any modern graphics card supporting Vulkan 1.3
- **Required Extensions**: 
  - `VK_EXT_shader_object`
  - `VK_EXT_extended_dynamic_state3`
- **Optional Extensions**:
  - `VK_EXT_vertex_input_dynamic_state` (recommended)
  - `VK_EXT_scalar_block_layout` (recommended for uniform buffer access)
  - `VK_EXT_nonuniform_qualifier` (for bindless texture access)

### Supported Platforms
- Windows (7+)
- Linux (Ubuntu 18.04+, or equivalent)
- macOS (via MoltenVK)

---

## Core Concepts

### 1. Context and Device

The **Context** initializes Vulkan and enables required extensions. The **Device** represents your GPU and manages all resources.

```c
VEContext* context = veCreateContext("My Application");
VEDevice* device = veCreateDevice(context);
```

### 2. Buffer Device Addresses

Buffers return **64-bit GPU addresses** instead of handles. Pass these addresses directly in push constants to shaders.

```c
VEBufferDesc bufferDesc = {
    .size = sizeof(vertices),
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .initialData = vertices,
    .initialDataSize = sizeof(vertices)
};
VEBufferAddress vertexBuffer = veCreateBuffer(device, &bufferDesc);

// Use in push constants
pushConstants.vertexBufferAddress = vertexBuffer;
```

### 3. Bindless Resources

Textures and samplers use **32-bit indices** that can be accessed from any shader using predefined descriptor sets.

```c
VETextureIndex texture = veLoadTexture(device, "texture.png", 
    VK_IMAGE_USAGE_SAMPLED_BIT, true);
VESamplerIndex sampler = veCreateLinearSampler(device);

// Access in shader via indices in push constants
pushConstants.textures[0] = texture;
pushConstants.samplers[0] = sampler;
```

### 4. Dynamic Rendering

No render passes needed - just specify what to render to:

```c
VERenderingAttachment colorAttachment = {
    .texture = backbuffer,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {0.0f, 0.0f, 0.0f, 1.0f}
};

VERenderingInfo renderingInfo = {
    .renderAreaWidth = width,
    .renderAreaHeight = height,
    .colorAttachmentCount = 1,
    .colorAttachments = &colorAttachment
};

veBeginRendering(cmd, &renderingInfo);
// Draw commands here
veEndRendering(cmd);
```

### 5. Shader Objects

Load shaders individually and bind them dynamically:

```c
VEShader* vertexShader = veLoadShader(device, "vertex.spv", 
    VK_SHADER_STAGE_VERTEX_BIT, "main", "VertexShader");
VEShader* fragmentShader = veLoadShader(device, "fragment.spv", 
    VK_SHADER_STAGE_FRAGMENT_BIT, "main", "FragmentShader");

// Bind during rendering
veBindShader(cmd, vertexShader);
veBindShader(cmd, fragmentShader);
```

### 6. Render Configurations

Lightweight state objects that replace graphics pipelines:

```c
// Use built-in configurations
VERenderConfig* opaqueConfig = veCreateOpaqueRenderConfig(device, "Opaque");
VERenderConfig* transparentConfig = veCreateTransparentRenderConfig(device, "Transparent");

// Apply during rendering
veApplyRenderConfig(cmd, opaqueConfig);
```

---

## Getting Started

### Basic Application Structure

```c
#include "vulkease.h"

int main() {
    // 1. Initialize VulkEase
    VEContext* context = veCreateContext("Hello VulkEase");
    VEDevice* device = veCreateDevice(context);
    
    // 2. Create window and swapchain (using GLFW example)
    VESwapchain* swapchain = veCreateSwapchain(device, windowHandle, 
        800, 600, VK_FORMAT_B8G8R8A8_SRGB, true);
    
    // 3. Load shaders
    VEShader* vertexShader = veLoadShader(device, "vertex.spv", 
        VK_SHADER_STAGE_VERTEX_BIT, "main", NULL);
    VEShader* fragmentShader = veLoadShader(device, "fragment.spv", 
        VK_SHADER_STAGE_FRAGMENT_BIT, "main", NULL);
    
    // 4. Create resources
    VEBufferAddress vertexBuffer = veCreateVertexBuffer(device, vertices, 
        sizeof(vertices), "Vertices");
    VERenderConfig* renderConfig = veCreateOpaqueRenderConfig(device, NULL);
    
    // 5. Render loop
    while (!shouldQuit) {
        VETextureIndex backbuffer = veAcquireNextImage(swapchain);
        VECommandBuffer* cmd = veBeginCommandBuffer(device);
        
        // Setup rendering
        VERenderingAttachment colorAttachment = {
            .texture = backbuffer,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {0.1f, 0.2f, 0.3f, 1.0f}
        };
        
        VERenderingInfo renderingInfo = {
            .renderAreaWidth = width,
            .renderAreaHeight = height,
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment
        };
        
        // Render
        veBeginRendering(cmd, &renderingInfo);
        veBindShader(cmd, vertexShader);
        veBindShader(cmd, fragmentShader);
        veApplyRenderConfig(cmd, renderConfig);
        
        // Set push constants with buffer address
        struct {
            float mvpMatrix[16];
            uint64_t vertexBufferAddress;
        } pushConstants = {
            .vertexBufferAddress = vertexBuffer,
            // ... initialize mvpMatrix
        };
        vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
        
        veDraw(cmd, 3, 1, 0, 0); // Draw triangle
        veEndRendering(cmd);
        
        vePresentImage(swapchain, cmd);
    }
    
    // 6. Cleanup
    veDestroyBuffer(device, vertexBuffer);
    veDestroyShader(vertexShader);
    veDestroyShader(fragmentShader);
    veDestroyRenderConfig(renderConfig);
    veDestroySwapchain(swapchain);
    veDestroyDevice(device);
    veDestroyContext(context);
    
    return 0;
}
```

### Shader Examples (GLSL)

VulkEase shaders use buffer device addresses and bindless resources. Here are examples from the provided examples:

#### Triangle Vertex Shader
```glsl
#version 450 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Vertex structure matching our C struct
struct Vertex {
    vec3 position;
    vec4 color;
};

// Buffer reference for bindless vertex access
layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

// Output to fragment shader
layout(location = 0) out vec4 fragColor;

// Push constants for transformation and vertex buffer address
layout(push_constant) uniform PushConstants {
    mat4 mvpMatrix;
    uint64_t vertexBufferAddress;
} pc;

void main() {
    // Get vertex buffer reference from address
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    
    // Fetch vertex data
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
    
    // Transform position and pass color
    gl_Position = pc.mvpMatrix * vec4(vertex.position, 1.0);
    fragColor = vertex.color;
}
```

#### Triangle Fragment Shader
```glsl
#version 450 core

// Input from vertex shader
layout(location = 0) in vec4 fragColor;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
```

---

## API Reference

### Context and Device Management

```c
// Create context and device
VEContext* veCreateContext(const char* applicationName);
VEDevice* veCreateDevice(VEContext* context);
void veDestroyContext(VEContext* context);
void veDestroyDevice(VEDevice* device);

// Device information
const char* veGetDeviceName(VEDevice* device);
const char* veGetDriverVersion(VEDevice* device);
uint32_t veGetVulkanVersion(VEDevice* device);
```

### Buffer Management

```c
// Generic buffer creation
VEBufferAddress veCreateBuffer(VEDevice* device, const VEBufferDesc* desc);
void veDestroyBuffer(VEDevice* device, VEBufferAddress address);

// Convenience functions
VEBufferAddress veCreateVertexBuffer(VEDevice* device, const void* vertices, 
    size_t size, const char* debugName);
VEBufferAddress veCreateIndexBuffer(VEDevice* device, const void* indices, 
    size_t size, const char* debugName);
VEBufferAddress veCreateUniformBuffer(VEDevice* device, size_t size, 
    bool persistentlyMapped, const char* debugName);

// Buffer operations
VEResult veMapBuffer(VEDevice* device, VEBufferAddress address, void** mappedData);
void veUnmapBuffer(VEDevice* device, VEBufferAddress address);
VEResult veUpdateBuffer(VEDevice* device, VEBufferAddress address, 
    const void* data, size_t size, size_t offset);
```

### Texture Management

```c
// Texture creation
VETextureIndex veCreateTexture(VEDevice* device, const VETextureDesc* desc);
void veDestroyTexture(VEDevice* device, VETextureIndex index);

// Convenience functions
VETextureIndex veCreateTexture2D(VEDevice* device, uint32_t width, uint32_t height,
    VkFormat format, VkImageUsageFlags usage, const char* debugName);
VETextureIndex veLoadTexture(VEDevice* device, const char* filename,
    VkImageUsageFlags usage, bool generateMips);

// Texture operations
VEResult veGenerateMipmaps(VEDevice* device, VECommandBuffer* cmd, VETextureIndex texture);
VEResult veSaveTexture(VEDevice* device, VETextureIndex texture, const char* filename);
```

### Sampler Management

```c
VESamplerIndex veCreateSampler(VEDevice* device, const VESamplerDesc* desc);
void veDestroySampler(VEDevice* device, VESamplerIndex index);

// Convenience functions
VESamplerIndex veCreateLinearSampler(VEDevice* device);
VESamplerIndex veCreateNearestSampler(VEDevice* device);
VESamplerIndex veCreateAnisotropicSampler(VEDevice* device, float maxAnisotropy);
```

### Shader Management

```c
// Shader creation
VEShader* veCreateShaderFromSPIRV(VEDevice* device, VkShaderStageFlags stage,
    const uint32_t* code, size_t codeSize, const char* entryPoint, const char* debugName);
VEShader* veLoadShader(VEDevice* device, const char* filename,
    VkShaderStageFlags stage, const char* entryPoint, const char* debugName);
void veDestroyShader(VEShader* shader);

// Hot reload support
VEResult veEnableShaderHotReload(VEShader* shader, const char* sourceFile);
VEResult veReloadShader(VEShader* shader);
```

### Render Configuration

```c
// Configuration management
VERenderConfig* veCreateRenderConfig(VEDevice* device, const VERenderConfigDesc* desc);
void veDestroyRenderConfig(VERenderConfig* config);

// Built-in configurations
VERenderConfig* veCreateOpaqueRenderConfig(VEDevice* device, const char* debugName);
VERenderConfig* veCreateTransparentRenderConfig(VEDevice* device, const char* debugName);
VERenderConfig* veCreateWireframeRenderConfig(VEDevice* device, const char* debugName);
```

### Command Buffer and Rendering

```c
// Command buffer management
VECommandBuffer* veBeginCommandBuffer(VEDevice* device);
VEResult veSubmitCommandBuffer(VECommandBuffer* cmd, bool waitForCompletion);

// Dynamic rendering
void veBeginRendering(VECommandBuffer* cmd, const VERenderingInfo* renderingInfo);
void veEndRendering(VECommandBuffer* cmd);

// State binding
void veBindShader(VECommandBuffer* cmd, VEShader* shader);
void veApplyRenderConfig(VECommandBuffer* cmd, VERenderConfig* config);
void vePushConstants(VECommandBuffer* cmd, const void* data, size_t size, size_t offset);

// Drawing
void veDraw(VECommandBuffer* cmd, uint32_t vertexCount, uint32_t instanceCount,
    uint32_t firstVertex, uint32_t firstInstance);
void veDrawIndexed(VECommandBuffer* cmd, uint32_t indexCount, uint32_t instanceCount,
    uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

// Compute
void veDispatch(VECommandBuffer* cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
```

### Swapchain Management

```c
VESwapchain* veCreateSwapchain(VEDevice* device, void* windowHandle,
    uint32_t width, uint32_t height, VkFormat format, bool vsync);
void veDestroySwapchain(VESwapchain* swapchain);

VETextureIndex veAcquireNextImage(VESwapchain* swapchain);
VEResult vePresentImage(VESwapchain* swapchain, VECommandBuffer* cmd);
VEResult veResizeSwapchain(VESwapchain* swapchain, uint32_t width, uint32_t height);
```

---

## Examples

### Example 1: Spinning Triangle

The spinning triangle demonstrates the basics of VulkEase with animated rendering and shows how buffer device addresses work.

**Key Features:**
- Basic window setup with GLFW
- Vertex buffer creation with buffer device addresses
- Shader loading and binding
- Push constants for transformation matrices and buffer addresses
- Simple vertex data access via `buffer_reference`

**Shader Structure:**
The vertex shader uses `GL_EXT_buffer_reference` to directly access vertex data via 64-bit addresses passed through push constants:

```glsl
layout(push_constant) uniform PushConstants {
    mat4 mvpMatrix;
    uint64_t vertexBufferAddress;
} pc;

// Buffer reference allows direct GPU memory access
layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

void main() {
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
    // Transform and render...
}
```

### Example 2: Compute Particle System

This example shows GPU-driven particle simulation using compute shaders with buffer device addresses for shared data access.

**Key Features:**
- Compute shader for particle physics simulation using `buffer_reference`
- Storage buffers accessible via device addresses from both compute and graphics shaders
- Instanced rendering for thousands of particles
- GPU-to-GPU data flow (no CPU involvement)
- Memory barriers between compute and graphics stages

**Compute Shader Structure:**
```glsl
#version 450 core
#extension GL_EXT_buffer_reference : require

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Particle buffer accessible by device address
layout(buffer_reference, std430) restrict buffer ParticleBuffer {
    Particle particles[];
};

layout(push_constant) uniform ComputePushConstants {
    uint64_t particleBufferAddress;
    float deltaTime;
    float time;
    // ... other parameters
} pc;

void main() {
    uint index = gl_GlobalInvocationID.x;
    ParticleBuffer particleBuffer = ParticleBuffer(pc.particleBufferAddress);
    
    // Directly modify particle data via buffer reference
    Particle particle = particleBuffer.particles[index];
    // ... update physics
    particleBuffer.particles[index] = particle;
}
```

**Graphics Shader Structure:**
The graphics shaders access the same particle buffer for instanced rendering:

```glsl
// Access same particle buffer from graphics shaders
layout(push_constant) uniform GraphicsPushConstants {
    uint64_t vertexBufferAddress;
    uint64_t particleBufferAddress;  // Same buffer as compute!
    vec2 screenSize;
} pc;

void main() {
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    ParticleBuffer particleBuffer = ParticleBuffer(pc.particleBufferAddress);
    
    vec2 quadVertex = vertexBuffer.vertices[gl_VertexIndex];
    Particle particle = particleBuffer.particles[gl_InstanceIndex];
    
    // Instance rendering - each particle is an instance of a quad
    vec2 worldPos = particle.position + (quadVertex * particle.size);
    gl_Position = vec4(worldPos, 0.0, 1.0);
}
```

### Example 3: Textured Cube

The textured cube demonstrates 3D rendering with bindless textures and complex push constant structures.

**Key Features:**
- Index buffer usage for efficient geometry
- Texture loading and bindless sampling via predefined descriptor sets
- 3D transformations (model-view-projection) via uniform buffers
- Complex push constant structure matching `VEGraphicsPushConstants`
- Scalar block layout for optimal uniform buffer access

### Example 4: Multithreaded Asteroid Field

Located in `examples/04_asteroid/` (C++) and `dotnet/examples/04_MultithreadedAsteroids/` (C#), this demo showcases the threading-friendly portions of the API by rendering thousands of instanced asteroids while streaming chunk updates on a background thread.

**Key Features:**
- Background producer thread simulates asteroid updates and queues chunk transfers into a staging ring
- Main thread double-buffers instance data into a storage buffer via `veUpdateBuffer`
- Worker threads record secondary command buffers in parallel (`veBeginSecondaryCommandBuffer` + `veExecuteSecondaryCommandBuffers`)
- Primary command buffer stitches together the worker output during dynamic rendering
- Built-in CPU metrics pipeline with console logging, window title summaries, and CSV export hook
- Toggle (`M`) to compare single-threaded vs multithreaded command buffer builds at runtime

**Secondary Command Buffer Pattern:**

```cpp
VESecondaryCommandBufferDesc desc{};
desc.usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                  VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
desc.inheritanceInfo = cachedInheritanceInfo; // includes VkCommandBufferInheritanceRenderingInfo

VECommandBuffer* workerCmd = veBeginSecondaryCommandBuffer(device, &desc);
veBindShaderConfig(workerCmd, shaderConfig);
veApplyRenderConfig(workerCmd, renderConfig);
vePushConstants(workerCmd, &pushConstants, sizeof(pushConstants), 0);
veBindIndexBuffer(workerCmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
veDrawIndexed(workerCmd, indexCount, chunk.instanceCount, 0, 0, chunk.firstInstance);
veEndCommandBuffer(workerCmd);
```

Workers push their handles into a thread-safe queue; the primary command buffer later executes them:

```cpp
veBeginRendering(primaryCmd, &renderingInfo);
veExecuteSecondaryCommandBuffers(primaryCmd, secondaryCount, secondaryArray.data());
veEndRendering(primaryCmd);
```

The C# version mirrors the flow using `Task.Run` for worker jobs and the newly exposed `VulkEase.BeginSecondaryCommandBuffer` / `VulkEase.ExecuteSecondaryCommandBuffers` helpers.

**Vertex Shader Structure:**
```glsl
#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// Uniform buffer access via device address
layout(buffer_reference, scalar) buffer UniformBuffer {
    mat4 mvpMatrix;
};

// Complex push constants structure
layout(push_constant) uniform PushConstants {
    uint64_t vertexBufferAddress;    // 8 bytes
    uint64_t indexBuffer;           // 8 bytes  
    uint64_t uniformBuffers[4];     // 32 bytes
    uint textures[8];               // 32 bytes (bindless indices)
    uint samplers[8];               // 32 bytes (bindless indices)
    float objectScale;              // 4 bytes
    uint activeTextureCount;        // 4 bytes
    uint activeSamplerCount;        // 4 bytes
    uint activeUniformCount;        // 4 bytes
} pc;

void main() {
    UniformBuffer uniformBuf = UniformBuffer(pc.uniformBuffers[0]);
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = uniformBuf.mvpMatrix * vec4(vertex.position * pc.objectScale, 1.0);
    fragTexCoord = vertex.uv;
}
```

**Fragment Shader Structure:**
```glsl
#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Bindless texture/sampler arrays - managed by VulkEase engine
layout(set = 0, binding = 0) uniform texture2D ve_textures[];
layout(set = 1, binding = 0) uniform sampler ve_samplers[];

void main() {
    // Get bindless indices from push constants
    uint textureIndex = pc.textures[0];
    uint samplerIndex = pc.samplers[0];
    
    // Sample bindless texture with nonuniform indexing
    vec4 textureColor = texture(
        sampler2D(ve_textures[nonuniformEXT(textureIndex)], 
                  ve_samplers[nonuniformEXT(samplerIndex)]),
        fragTexCoord
    );
    
    outColor = textureColor;
}
```

---

## Advanced Topics

### Buffer Device Addresses in Shaders

VulkEase shaders extensively use the `GL_EXT_buffer_reference` extension for direct GPU memory access:

```glsl
// Different buffer layouts for different use cases
layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) buffer UniformBuffer {
    mat4 matrices[];
};

layout(buffer_reference, std430) restrict buffer StorageBuffer {
    float data[];
};
```

**Layout Qualifiers:**
- `std430`: Standard shader storage buffer layout
- `scalar`: Relaxed alignment (requires `GL_EXT_scalar_block_layout`)
- `restrict`: Indicates exclusive access (compute shaders)
- `readonly`: Read-only access

### Bindless Texture Access

VulkEase provides predefined descriptor sets for bindless resources:

```glsl
// Engine-provided bindless arrays
layout(set = 0, binding = 0) uniform texture2D ve_textures[];
layout(set = 1, binding = 0) uniform sampler ve_samplers[];

// Access via indices in push constants
uint textureIndex = pc.textures[0];
vec4 color = texture(sampler2D(ve_textures[textureIndex], ve_samplers[0]), uv);
```

### Custom Render Configurations

Create custom render states by combining configuration components:

```c
// Custom vertex input
VEVertexBinding bindings[] = {
    { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }
};
VEVertexAttribute attributes[] = {
    { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
    { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 12 }
};

VEVertexInputConfig vertexInput = {
    .bindingCount = 1, .bindings = bindings,
    .attributeCount = 2, .attributes = attributes,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
};

// Custom render configuration
VERenderConfigDesc configDesc = {
    .configTypes = VE_CONFIG_TYPE_VERTEX_INPUT | VE_CONFIG_TYPE_RASTERIZATION,
    .vertexInputConfig = &vertexInput,
    .debugName = "CustomConfig"
};
VERenderConfig* config = veCreateRenderConfig(device, &configDesc);
```

### GPU-Driven Rendering

Use indirect draws for GPU-controlled rendering:

```c
// Compute shader fills indirect draw buffer
VEBufferAddress indirectBuffer = veCreateIndirectBuffer(device, 
    sizeof(VkDrawIndirectCommand) * maxDraws, "IndirectCommands");

// Graphics draws from indirect buffer
veDrawIndirect(cmd, indirectBuffer, 0, drawCount, sizeof(VkDrawIndirectCommand));
```

### Multi-Threading

VulkEase supports multi-threaded command buffer recording:

```c
// Each thread gets its own command buffer
VECommandBuffer* cmd = veBeginCommandBuffer(device);
// Record commands...
VEResult result = veSubmitCommandBuffer(cmd, false);  // Don't wait
```

---

## Performance Tips

### Buffer Usage

1. **Use persistent mapping** for frequently updated uniform buffers
2. **Use scalar layout** when possible for uniform buffers (requires `GL_EXT_scalar_block_layout`)
3. **Batch buffer updates** to reduce API overhead
4. **Use storage buffers** instead of uniform buffers for large datasets
5. **Align buffer data** to 16-byte boundaries for optimal performance

### Bindless Resources

1. **Minimize texture switches** - use texture arrays when possible
2. **Group similar samplers** to reduce binding overhead  
3. **Use texture atlases** for small textures to reduce bindless pressure
4. **Use `nonuniformEXT()` qualifier** for dynamic indexing in shaders

### Render Configuration

1. **Minimize config switches** - group draw calls by render state
2. **Use built-in configs** when possible - they're optimized
3. **Override only necessary state** - don't change what doesn't need changing

### Command Buffer Optimization

1. **Record command buffers in parallel** on multiple threads
2. **Batch draw calls** to reduce command buffer overhead
3. **Use indirect draws** for GPU-driven rendering
4. **Minimize state changes** within a command buffer

### Shader Performance

1. **Use buffer_reference** instead of vertex input attributes when possible
2. **Use scalar layout** for uniform buffers to avoid padding
3. **Minimize push constant size** - they have limited space
4. **Use restrict qualifier** for exclusive buffer access in compute shaders

---

## Debugging

### Validation Layers

VulkEase automatically enables validation layers in debug builds:

```c
// Check for validation errors
const char* lastError = veGetLastError();
if (lastError) {
    printf("VulkEase Error: %s\n", lastError);
}
```

### Debug Labels

Annotate command buffers for better debugging:

```c
VEColor debugColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
veBeginDebugLabel(cmd, "Particle Rendering", debugColor);
// Render particles...
veEndDebugLabel(cmd);
```

### Resource Names

Set debug names for better identification:

```c
veSetBufferDebugName(device, vertexBuffer, "MainVertexBuffer");
veSetTextureDebugName(device, texture, "DiffuseTexture");
```

### Common Shader Issues

1. **Buffer reference alignment**: Ensure C structs match GLSL struct layouts
2. **Push constant size limits**: Keep under 128 bytes typically
3. **Bindless index validation**: Check texture/sampler indices are valid
4. **Scalar layout usage**: Requires both extension and matching C struct layout

### Performance Statistics

Monitor GPU performance:

```c
VEPerformanceStats stats;
veGetPerformanceStats(device, &stats);
printf("Frame time: %llu ns, Draw calls: %u\n", stats.frameTime, stats.drawCalls);

VEMemoryStats memStats;
veGetMemoryStats(device, &memStats);
printf("Total memory: %llu bytes\n", memStats.totalAllocated);
```

---

## Migration Guide

### From Traditional Vulkan

**Old Vulkan Approach:**
```c
// Create render pass, framebuffer, pipeline...
vkCreateRenderPass();
vkCreateFramebuffer();
vkCreateGraphicsPipeline();
vkCreateDescriptorSetLayout();
vkAllocateDescriptorSets();
vkUpdateDescriptorSets();

// Render
vkCmdBeginRenderPass();
vkCmdBindPipeline();
vkCmdBindDescriptorSets();
vkCmdBindVertexBuffers();
vkCmdDraw();
vkCmdEndRenderPass();
```

**VulkEase Approach:**
```c
// Create shader and config once
VEShader* shader = veLoadShader(...);
VERenderConfig* config = veCreateOpaqueRenderConfig(...);

// Render
veBeginRendering(cmd, &renderingInfo);
veBindShader(cmd, shader);
veApplyRenderConfig(cmd, config);
vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
veDraw(cmd, vertexCount, 1, 0, 0);
veEndRendering(cmd);
```

### From OpenGL

**OpenGL Approach:**
```c
glUseProgram(program);
glBindVertexArray(vao);
glBindTexture(GL_TEXTURE_2D, texture);
glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, mvpMatrix);
glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, 0);
```

**VulkEase Approach:**
```c
veBindShader(cmd, vertexShader);
veBindShader(cmd, fragmentShader);
struct {
    uint64_t vertexBuffer;
    uint64_t uniformBuffer;  // Or embed matrix directly in push constants
    uint32_t textureIndex;
    uint32_t samplerIndex;
} pushConstants = { vertexBuffer, uniformBuffer, textureIndex, samplerIndex };
vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
veDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
```

### Key Differences

1. **No VAOs** - vertex data accessed via buffer addresses in shaders
2. **No texture binding** - textures accessed via bindless indices  
3. **No uniform locations** - uniforms accessed via buffer addresses or embedded in push constants
4. **No state binding** - state applied via render configurations
5. **Command buffer based** - all rendering goes through command buffers
6. **Buffer references in shaders** - direct memory access via `GL_EXT_buffer_reference`

---

This documentation covers the core concepts and usage patterns of VulkEase 2.0, with corrected shader examples based on the actual implementation. For more detailed information, refer to the header file and example code provided with the library.