# VulkEase 2.0 Documentation

## Table of Contents

1. [High-Level Overview](#1-high-level-overview)
2. [The Basics](#2-the-basics)
3. [Walkthrough: Textured Cube](#3-walkthrough-textured-cube)
4. [Multithreading and Secondary Command Buffers](#4-multithreading-and-secondary-command-buffers)
5. [API Reference](#5-api-reference)

---

# 1. High-Level Overview

## What is VulkEase?

VulkEase is a modern graphics API built on Vulkan 1.4+ that provides the power of low-level GPU programming without the complexity. It's designed for developers who want direct control over rendering without writing thousands of lines of boilerplate.

## Core Philosophy

**Simplicity without sacrificing power.** VulkEase is opinionated—there's one way to do things, and it's the right way. No legacy baggage, no compatibility modes, just clean modern graphics programming.

## Key Features

| Feature | Description |
|---------|-------------|
| **Buffer Device Addresses** | Access buffers via 64-bit GPU pointers. No descriptor sets for buffers. |
| **Bindless Textures** | Access thousands of textures via 32-bit indices in push constants. |
| **Dynamic Rendering** | No render passes or framebuffers. Just `veBeginRendering()` and draw. |
| **Shader Objects** | Hot-reloadable shaders. No pipeline recreation. |
| **Render Targets** | All rendering goes to offscreen targets, then blits to swapchain. |
| **Secondary Command Buffers** | Record command buffers in parallel across threads. |
| **Render Configurations** | Lightweight state objects replace monolithic pipelines. |
| **Custom Extensions** | Enable additional Vulkan extensions for advanced features. |
| **Multi-GPU Support** | Enumerate and select specific GPUs. |

## What VulkEase Eliminates

Traditional Vulkan requires you to manage:

- ❌ Render passes and subpasses
- ❌ Framebuffers
- ❌ Graphics pipelines (hundreds of lines of setup)
- ❌ Descriptor set layouts, pools, and sets
- ❌ Pipeline layouts
- ❌ Manual memory allocation
- ❌ Complex synchronization primitives

VulkEase replaces all of this with a clean, modern API.

## System Requirements

- **Vulkan 1.4+** runtime and drivers
- **GPU** supporting required extensions:
  - `VK_EXT_shader_object`
  - `VK_EXT_extended_dynamic_state3`
  - `VK_EXT_vertex_input_dynamic_state`
  - `VK_EXT_host_image_copy`
- **Platforms**: Windows, Linux (X11/Wayland), macOS (via MoltenVK)

---

# 2. The Basics

## Context and Device

Every VulkEase application starts by creating a **Context** (Vulkan instance) and a **Device** (GPU handle).

```c
// Create context - initializes Vulkan
VEContext* context = veCreateContext("My Application", NULL, 0);

// Create device - selects GPU (VK_NULL_HANDLE = auto-select best)
VEDevice* device = veCreateDevice(context, VK_NULL_HANDLE, NULL, 0);

// Get device info
printf("GPU: %s\n", veGetDeviceName(device));
printf("Driver: %s\n", veGetDriverVersion(device));
```

### Selecting a Specific GPU

```c
uint32_t gpuCount = 0;
veEnumeratePhysicalDevices(context, &gpuCount);

for (uint32_t i = 0; i < gpuCount; i++) {
    VkPhysicalDevice gpu;
    char name[256];
    VkPhysicalDeviceType type;
    veGetPhysicalDeviceInfo(context, i, &gpu, name, &type);
    printf("%u: %s\n", i, name);
}

// Select specific GPU
VkPhysicalDevice selectedGPU;
veGetPhysicalDeviceInfo(context, 0, &selectedGPU, NULL, NULL);
VEDevice* device = veCreateDevice(context, selectedGPU, NULL, 0);
```

### Enabling Custom Extensions

```c
// Check availability first
if (veIsDeviceExtensionAvailable(context, "VK_KHR_ray_tracing_pipeline")) {
    const char* extensions[] = { "VK_KHR_ray_tracing_pipeline" };
    VEDevice* device = veCreateDevice(context, VK_NULL_HANDLE, extensions, 1);
}
```

## Buffers

Buffers in VulkEase return **64-bit GPU addresses** instead of handles. Pass these addresses directly to shaders via push constants.

```c
// Create a vertex buffer with initial data
VEBufferAddress vertexBuffer = veCreateVertexBuffer(device, 
    vertices, sizeof(vertices), "MyVertices");

// Create an index buffer
VEBufferAddress indexBuffer = veCreateIndexBuffer(device,
    indices, sizeof(indices), "MyIndices");

// Create a uniform buffer (persistently mapped for easy updates)
VEBufferAddress uniformBuffer = veCreateUniformBuffer(device,
    sizeof(UniformData), true, "MyUniforms");

// Update buffer data
veUpdateBuffer(device, uniformBuffer, &data, sizeof(data), 0);

// The buffer address goes directly into push constants
pushConstants.vertexBuffer = vertexBuffer;
```

### Buffer Types

| Function | Usage |
|----------|-------|
| `veCreateVertexBuffer` | Vertex data |
| `veCreateIndexBuffer` | Index data |
| `veCreateUniformBuffer` | Per-frame uniform data |
| `veCreateStorageBuffer` | Large read/write data |
| `veCreateBuffer` | Custom usage flags |

## Textures and Samplers

Textures and samplers use **32-bit bindless indices**. VulkEase manages a global texture/sampler array that shaders access via these indices.

```c
// Load texture from file
VETextureIndex texture = veLoadTexture(device, "texture.png",
    VK_IMAGE_USAGE_SAMPLED_BIT, true);  // true = generate mipmaps

// Create sampler
VESamplerIndex sampler = veCreateLinearSampler(device);

// Use in push constants
pushConstants.textures[0] = texture;
pushConstants.samplers[0] = sampler;
```

### Sampler Types

| Function | Description |
|----------|-------------|
| `veCreateLinearSampler` | Bilinear filtering, clamp to edge |
| `veCreateNearestSampler` | Point sampling |
| `veCreateAnisotropicSampler` | Anisotropic filtering |
| `veCreateShadowSampler` | Depth comparison for shadow maps |

## Shaders

VulkEase uses **Shader Objects** (`VK_EXT_shader_object`). Load shaders individually and bind them dynamically.

```c
// Load shaders from SPIR-V files
VEShader* vertexShader = veLoadShaderFromFile(device, "shader.vert.spv",
    VK_SHADER_STAGE_VERTEX_BIT, "main", "MyVertexShader");

VEShader* fragmentShader = veLoadShaderFromFile(device, "shader.frag.spv",
    VK_SHADER_STAGE_FRAGMENT_BIT, "main", "MyFragmentShader");

// Bind shaders during rendering
veBindShader(cmd, vertexShader);
veBindShader(cmd, fragmentShader);
```

### Shader Configurations

Bundle related shaders together:

```c
VEShaderConfigDesc desc = {
    .vertexShader = vertexShader,
    .fragmentShader = fragmentShader,
    .debugName = "MyShaderConfig"
};
VEShaderConfig* shaderConfig = veCreateShaderConfig(device, &desc);

// Bind entire configuration
veBindShaderConfig(cmd, shaderConfig);
```

## Render Targets

**All rendering goes to offscreen render targets, then blits to the swapchain.** This provides:

- Resolution-independent rendering
- Easy post-processing
- Consistent behavior across all use cases

```c
// Create swapchain for the window
VESwapchainDesc swapchainDesc = {
    .surface = {
        .type = VE_SURFACE_TYPE_XLIB,
        .xlib = { .display = display, .window = window }
    },
    .width = 800,
    .height = 600,
    .colorFormat = VK_FORMAT_B8G8R8A8_SRGB,
    .vsync = true
};
VESwapchain* swapchain = veCreateSwapchain(device, &swapchainDesc);

// Create render target (with optional depth buffer)
VERenderTargetDesc rtDesc = {
    .width = 800,
    .height = 600,
    .colorFormat = VK_FORMAT_B8G8R8A8_SRGB,
    .depthFormat = VK_FORMAT_D32_SFLOAT,  // VK_FORMAT_UNDEFINED = no depth
    .sampleCount = 1
};
VERenderTarget* renderTarget = veCreateRenderTarget(device, &rtDesc);
```

## VERenderingInfo

The `VERenderingInfo` struct defines what you're rendering to. **Build it once at initialization, rebuild only on resize.**

```c
// Create at initialization
VERenderingInfo renderingInfo = veCreateRenderingInfo(width, height);

// Add color attachment (clear to dark blue)
veRenderingAddColorAttachment(&renderingInfo,
    veGetRenderTargetColorTexture(renderTarget),
    VK_ATTACHMENT_LOAD_OP_CLEAR,
    (VEColor){0.1f, 0.2f, 0.3f, 1.0f});

// Add depth attachment (clear to 1.0)
veRenderingSetDepthAttachment(&renderingInfo,
    veGetRenderTargetDepthTexture(renderTarget),
    VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f);
```

### Rebuilding on Resize

```c
void onResize(uint32_t width, uint32_t height) {
    veResizeSwapchain(swapchain, width, height);
    veResizeRenderTarget(renderTarget, width, height);
    
    // Rebuild with new texture handles
    renderingInfo = veCreateRenderingInfo(width, height);
    veRenderingAddColorAttachment(&renderingInfo,
        veGetRenderTargetColorTexture(renderTarget),
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        (VEColor){0.1f, 0.2f, 0.3f, 1.0f});
    veRenderingSetDepthAttachment(&renderingInfo,
        veGetRenderTargetDepthTexture(renderTarget),
        VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f);
}
```

## Render Configurations

Replace graphics pipelines with lightweight **render configurations**.

```c
// Use built-in configurations
VERenderConfig* opaqueConfig = veCreateOpaqueRenderConfig(device, "Opaque");
VERenderConfig* transparentConfig = veCreateTransparentRenderConfig(device, "Transparent");

// Apply during rendering
veApplyRenderConfig(cmd, opaqueConfig);
```

### Custom Configurations

```c
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

VERenderConfigDesc configDesc = {
    .configTypes = VE_CONFIG_TYPE_VERTEX_INPUT | VE_CONFIG_TYPE_DEPTH_STENCIL,
    .vertexInputConfig = &vertexInput
};

VERenderConfig* config = veCreateRenderConfig(device, &configDesc);
```

## The Render Loop

Every frame follows this pattern:

```c
while (running) {
    // 1. Begin command buffer
    VECommandBuffer* cmd = veBeginCommandBuffer(device);
    
    // 2. Begin rendering to render target
    veBeginRendering(cmd, &renderingInfo);
    
    // 3. Apply render state
    VERenderState renderState = {
        .shaderConfig = shaderConfig,
        .renderConfig = renderConfig,
        .viewport = NULL,  // NULL = full render area
        .scissor = NULL
    };
    veApplyRenderState(cmd, &renderState);
    
    // 4. Push constants and draw
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    veBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    veDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    
    // 5. End rendering
    veEndRendering(cmd);
    
    // 6. Blit to swapchain
    veBlitToSwapchain(cmd, renderTarget, swapchain, VK_FILTER_LINEAR);
    
    // 7. Present
    vePresentImage(swapchain, cmd, true);  // true = auto-release cmd
}
```

---

# 3. Walkthrough: Textured Cube

This section walks through the complete `03_cube.c` example, explaining each step.

## Project Structure

```
examples/
├── 03_cube.c              # Main application code
├── shaders/
│   ├── cube.vert          # Vertex shader (GLSL)
│   ├── cube.frag          # Fragment shader (GLSL)
│   ├── cube.vert.spv      # Compiled SPIR-V
│   └── cube.frag.spv      # Compiled SPIR-V
└── data/
    └── testCard.png       # Test texture
```

## Application State

```c
typedef struct CubeApp {
    GLFWwindow* window;
    VEContext* context;
    VEDevice* device;
    VESwapchain* swapchain;
    VERenderTarget* renderTarget;
    
    VEBufferAddress vertexBuffer;
    VEBufferAddress indexBuffer;
    VEBufferAddress uniformBuffer;
    VETextureIndex texture;
    VESamplerIndex sampler;
    
    VEShader* vertexShader;
    VEShader* fragmentShader;
    VEShaderConfig* shaderConfig;
    VERenderConfig* renderConfig;
    
    VERenderingInfo renderingInfo;  // Built once, reused every frame
    
    float rotationAngle;
} CubeApp;
```

## Step 1: Initialize GLFW

```c
glfwInit();
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // No OpenGL context
glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

app->window = glfwCreateWindow(800, 600, "VulkEase Cube", NULL, NULL);
glfwSetWindowUserPointer(app->window, app);
glfwSetWindowSizeCallback(app->window, framebufferSizeCallback);
```

## Step 2: Create Context and Device

```c
app->context = veCreateContext("VulkEase Cube Demo", NULL, 0);
app->device = veCreateDevice(app->context, VK_NULL_HANDLE, NULL, 0);

printf("GPU: %s\n", veGetDeviceName(app->device));
```

## Step 3: Create Swapchain with Platform Surface

```c
VESwapchainDesc swapchainDesc = {0};
swapchainDesc.width = 800;
swapchainDesc.height = 600;
swapchainDesc.colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
swapchainDesc.vsync = true;

#if defined(__linux__)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_XLIB;
    swapchainDesc.surface.xlib.display = glfwGetX11Display();
    swapchainDesc.surface.xlib.window = glfwGetX11Window(app->window);
#elif defined(_WIN32)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_WIN32;
    swapchainDesc.surface.win32.hwnd = glfwGetWin32Window(app->window);
#endif

app->swapchain = veCreateSwapchain(app->device, &swapchainDesc);
```

## Step 4: Create Render Target

```c
VERenderTargetDesc rtDesc = {
    .width = 800,
    .height = 600,
    .colorFormat = veGetSwapchainFormat(app->swapchain),
    .depthFormat = VK_FORMAT_D32_SFLOAT,  // Depth buffer for 3D
    .sampleCount = 1
};
app->renderTarget = veCreateRenderTarget(app->device, &rtDesc);
```

## Step 5: Build VERenderingInfo

```c
app->renderingInfo = veCreateRenderingInfo(800, 600);

veRenderingAddColorAttachment(&app->renderingInfo,
    veGetRenderTargetColorTexture(app->renderTarget),
    VK_ATTACHMENT_LOAD_OP_CLEAR,
    (VEColor){0.1f, 0.2f, 0.3f, 1.0f});  // Dark blue

veRenderingSetDepthAttachment(&app->renderingInfo,
    veGetRenderTargetDepthTexture(app->renderTarget),
    VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f);
```

## Step 6: Load Shaders

```c
app->vertexShader = veLoadShaderFromFile(app->device, 
    "examples/shaders/cube.vert.spv",
    VK_SHADER_STAGE_VERTEX_BIT, "main", "CubeVertex");

app->fragmentShader = veLoadShaderFromFile(app->device,
    "examples/shaders/cube.frag.spv",
    VK_SHADER_STAGE_FRAGMENT_BIT, "main", "CubeFragment");

VEShaderConfigDesc shaderConfigDesc = {
    .vertexShader = app->vertexShader,
    .fragmentShader = app->fragmentShader
};
app->shaderConfig = veCreateShaderConfig(app->device, &shaderConfigDesc);
```

## Step 7: Create Buffers

```c
// Vertex data
typedef struct Vertex {
    float pos[3];
    float texCoord[2];
} Vertex;

static const Vertex cubeVertices[] = {
    {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f}},  // Front face
    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}},
    // ... more vertices for other faces
};

static const uint16_t cubeIndices[] = {
    0, 1, 2,  2, 3, 0,   // Front
    4, 6, 5,  6, 4, 7,   // Back
    // ... more indices
};

app->vertexBuffer = veCreateVertexBuffer(app->device, 
    cubeVertices, sizeof(cubeVertices), "CubeVertices");

app->indexBuffer = veCreateIndexBuffer(app->device,
    cubeIndices, sizeof(cubeIndices), "CubeIndices");

app->uniformBuffer = veCreateUniformBuffer(app->device,
    sizeof(UniformData), true, "CubeUniforms");
```

## Step 8: Load Texture and Sampler

```c
app->texture = veLoadTexture(app->device, "examples/data/testCard.png",
    VK_IMAGE_USAGE_SAMPLED_BIT, true);

app->sampler = veCreateLinearSampler(app->device);
```

## Step 9: Create Render Configuration

```c
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

VERenderConfigDesc configDesc = {
    .configTypes = VE_CONFIG_TYPE_VERTEX_INPUT | VE_CONFIG_TYPE_DEPTH_STENCIL,
    .vertexInputConfig = &vertexInput
};

app->renderConfig = veCreateRenderConfig(app->device, &configDesc);
```

## Step 10: Render Loop

```c
while (!glfwWindowShouldClose(app->window)) {
    glfwPollEvents();
    
    // Update rotation
    app->rotationAngle += deltaTime * 0.8f;
    
    // Update uniform buffer with MVP matrix
    Mat4 projection = mat4Perspective(PI * 0.25f, aspect, 0.1f, 100.0f);
    Mat4 view = mat4Translate(0.0f, 0.0f, -5.0f);
    Mat4 model = mat4RotateY(app->rotationAngle);
    Mat4 mvp = mat4Multiply(&mat4Multiply(&model, &view), &projection);
    veUpdateBuffer(app->device, app->uniformBuffer, &mvp, sizeof(mvp), 0);
    
    // Render
    VECommandBuffer* cmd = veBeginCommandBuffer(app->device);
    
    veBeginRendering(cmd, &app->renderingInfo);
    
    VERenderState renderState = {
        .shaderConfig = app->shaderConfig,
        .renderConfig = app->renderConfig,
        .viewport = NULL,
        .scissor = NULL
    };
    veApplyRenderState(cmd, &renderState);
    
    VEGraphicsPushConstants pushConstants = VE_INIT_GRAPHICS_PUSH_CONSTANTS();
    pushConstants.vertexBuffer = app->vertexBuffer;
    pushConstants.uniformBuffers[0] = app->uniformBuffer;
    pushConstants.textures[0] = app->texture;
    pushConstants.samplers[0] = app->sampler;
    pushConstants.activeUniformCount = 1;
    pushConstants.activeTextureCount = 1;
    pushConstants.activeSamplerCount = 1;
    
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    veBindIndexBuffer(cmd, app->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    veDrawIndexed(cmd, 36, 1, 0, 0, 0);
    
    veEndRendering(cmd);
    
    veBlitToSwapchain(cmd, app->renderTarget, app->swapchain, VK_FILTER_LINEAR);
    vePresentImage(app->swapchain, cmd, true);
}
```

## Step 11: Cleanup

```c
veDeviceWaitIdle(app->device);

veDestroyRenderConfig(app->renderConfig);
veDestroyShaderConfig(app->shaderConfig);
veDestroyShader(app->fragmentShader);
veDestroyShader(app->vertexShader);

veDestroySampler(app->device, app->sampler);
veDestroyTexture(app->device, app->texture);
veDestroyBuffer(app->device, app->uniformBuffer);
veDestroyBuffer(app->device, app->indexBuffer);
veDestroyBuffer(app->device, app->vertexBuffer);

veDestroyRenderTarget(app->renderTarget);
veDestroySwapchain(app->swapchain);
veDestroyDevice(app->device);
veDestroyContext(app->context);
```

## The Shaders

### Vertex Shader (cube.vert)

```glsl
#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Vertex {
    vec3 position;
    vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) buffer UniformBuffer {
    mat4 mvpMatrix;
};

layout(location = 0) out vec2 fragTexCoord;

layout(push_constant) uniform PushConstants {
    uint64_t vertexBufferAddress;
    uint64_t indexBuffer;
    uint64_t uniformBuffers[8];
    uint textures[16];
    uint samplers[16];
    float objectScale;
    uint activeTextureCount;
    uint activeSamplerCount;
    uint activeUniformCount;
    uint reserved[7];
} pc;

void main() {
    UniformBuffer uniformBuf = UniformBuffer(pc.uniformBuffers[0]);
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    
    Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = uniformBuf.mvpMatrix * vec4(vertex.position * pc.objectScale, 1.0);
    fragTexCoord = vertex.uv;
}
```

### Fragment Shader (cube.frag)

```glsl
#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform texture2D ve_textures[];
layout(set = 1, binding = 0) uniform sampler ve_samplers[];

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    uint64_t vertexBufferAddress;
    uint64_t indexBuffer;
    uint64_t uniformBuffers[8];
    uint textures[16];
    uint samplers[16];
    float objectScale;
    uint activeTextureCount;
    uint activeSamplerCount;
    uint activeUniformCount;
    uint reserved[7];
} pc;

void main() {
    uint textureIndex = pc.textures[0];
    uint samplerIndex = pc.samplers[0];
    
    outColor = texture(
        sampler2D(ve_textures[nonuniformEXT(textureIndex)], 
                  ve_samplers[nonuniformEXT(samplerIndex)]),
        fragTexCoord);
}
```

---

# 4. Multithreading and Secondary Command Buffers

For complex scenes with many draw calls, you can record **secondary command buffers** in parallel across multiple threads.

## Why Use Secondary Command Buffers?

- **Parallel recording**: Each thread records its own command buffer
- **Chunk-based rendering**: Divide work into chunks (e.g., 512 objects each)
- **Reduced CPU bottleneck**: Main thread doesn't block on command recording

## The Pattern

1. Divide your scene into chunks
2. Record secondary command buffers for each chunk (parallel)
3. Execute all secondary buffers from the primary command buffer

## VESecondaryCommandBufferDesc

```c
VESecondaryCommandBufferDesc desc = {0};
desc.beginRecording = true;  // Auto-begin recording
desc.usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                  VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
desc.colorAttachmentCount = 1;
desc.colorAttachmentFormats[0] = swapchainFormat;
desc.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
desc.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
desc.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

// Render area for veApplyRenderState defaults
desc.renderAreaX = 0;
desc.renderAreaY = 0;
desc.renderAreaWidth = width;
desc.renderAreaHeight = height;
```

## Recording Secondary Buffers

```c
VECommandBuffer* recordChunk(uint32_t chunkIndex) {
    VESecondaryCommandBufferDesc desc = {0};
    desc.beginRecording = true;
    desc.usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                      VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    desc.colorAttachmentCount = 1;
    desc.colorAttachmentFormats[0] = VK_FORMAT_B8G8R8A8_SRGB;
    desc.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
    desc.renderAreaWidth = width;
    desc.renderAreaHeight = height;
    
    VECommandBuffer* cmd = veBeginSecondaryCommandBuffer(device, &desc);
    
    // IMPORTANT: Secondary buffers do NOT inherit dynamic state!
    // Each secondary must set ALL required state before drawing.
    VERenderState renderState = {
        .shaderConfig = shaderConfig,
        .renderConfig = renderConfig,
        .viewport = NULL,
        .scissor = NULL
    };
    veApplyRenderState(cmd, &renderState);
    
    // Push constants and draw this chunk
    VEGraphicsPushConstants push = VE_INIT_GRAPHICS_PUSH_CONSTANTS();
    push.vertexBuffer = vertexBuffer;
    push.uniformBuffers[0] = cameraBuffer;
    push.uniformBuffers[1] = instanceBuffer;
    push.activeUniformCount = 2;
    vePushConstants(cmd, &push, sizeof(push), 0);
    
    veBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    veDrawIndexed(cmd, indexCount, chunks[chunkIndex].count, 
                  0, 0, chunks[chunkIndex].firstInstance);
    
    veEndCommandBuffer(cmd);
    return cmd;
}
```

## Parallel Recording with Thread Pool

```c
std::vector<VECommandBuffer*> secondaryCmds;
std::mutex resultMutex;
std::condition_variable cv;
std::atomic<uint32_t> remaining(chunks.size());

// Enqueue all chunks to worker threads
for (uint32_t i = 0; i < chunks.size(); i++) {
    workerPool.enqueue([&, i]() {
        VECommandBuffer* cmd = recordChunk(i);
        
        std::lock_guard<std::mutex> lock(resultMutex);
        secondaryCmds.push_back(cmd);
        
        if (remaining.fetch_sub(1) == 1) {
            cv.notify_one();
        }
    });
}

// Wait for all workers
std::unique_lock<std::mutex> lock(resultMutex);
cv.wait(lock, [&]() { return remaining.load() == 0; });
```

## Executing Secondary Buffers

```c
VECommandBuffer* primary = veBeginCommandBuffer(device);

veBeginRendering(primary, &renderingInfo);

// Execute all secondary command buffers
veExecuteSecondaryCommandBuffers(primary,
    (uint32_t)secondaryCmds.size(),
    secondaryCmds.data(),
    true);  // true = auto-release secondary buffers

veEndRendering(primary);

veBlitToSwapchain(primary, renderTarget, swapchain, VK_FILTER_LINEAR);
vePresentImage(swapchain, primary, true);
```

## Important: State is NOT Inherited

With `VK_EXT_shader_object`, **dynamic state is NOT inherited by secondary command buffers**. Each secondary buffer must set:

- Shaders (via `veBindShaderConfig` or `veBindShader`)
- Render config (via `veApplyRenderConfig`)
- Viewport and scissor (via `veApplyRenderState` or `veSetViewport`/`veSetScissor`)
- Push constants

The `veApplyRenderState` function handles all of this in one call.

## The releaseCommandBuffers Parameter

```c
veExecuteSecondaryCommandBuffers(primary, count, cmds, releaseCommandBuffers);
```

- **true**: Automatically release secondary buffers after execution (recommended)
- **false**: You manage the lifetime manually (call `veReleaseCommandBuffer` later)

Similarly for `vePresentImage`:

```c
vePresentImage(swapchain, cmd, releaseCommandBuffer);
```

- **true**: Auto-release the command buffer after presentation
- **false**: Reuse the command buffer for multiple frames

---

# 5. API Reference

## Context & Device

### veCreateContext

```c
VEContext* veCreateContext(
    const char* applicationName,
    const char* const* additionalInstanceExtensions,
    uint32_t additionalInstanceExtensionCount);
```

Creates a VulkEase context (Vulkan instance). Pass additional instance extension names to enable features like ray tracing.

**Returns**: Context handle, or NULL on failure.

---

### veDestroyContext

```c
void veDestroyContext(VEContext* context);
```

Destroys a VulkEase context. Destroy all devices first.

---

### veEnumeratePhysicalDevices

```c
VEResult veEnumeratePhysicalDevices(VEContext* context, uint32_t* count);
```

Returns the number of available GPUs.

---

### veGetPhysicalDeviceInfo

```c
VEResult veGetPhysicalDeviceInfo(
    VEContext* context,
    uint32_t deviceIndex,
    VkPhysicalDevice* physicalDevice,
    char* deviceName,           // Buffer of at least 256 chars, or NULL
    VkPhysicalDeviceType* deviceType);  // Can be NULL
```

Gets information about a specific GPU. Use to select a GPU for `veCreateDevice`.

---

### veCreateDevice

```c
VEDevice* veCreateDevice(
    VEContext* context,
    VkPhysicalDevice preferredDevice,  // VK_NULL_HANDLE = auto-select
    const char* const* additionalDeviceExtensions,
    uint32_t additionalDeviceExtensionCount);
```

Creates a VulkEase device for the specified GPU. Pass `VK_NULL_HANDLE` to auto-select the best GPU.

**Returns**: Device handle, or NULL on failure.

---

### veDestroyDevice

```c
void veDestroyDevice(VEDevice* device);
```

Destroys a device. Destroy all resources created from this device first.

---

### veIsInstanceExtensionAvailable

```c
bool veIsInstanceExtensionAvailable(const char* extensionName);
```

Checks if an instance extension is available. Call before `veCreateContext`.

---

### veIsDeviceExtensionAvailable

```c
bool veIsDeviceExtensionAvailable(VEContext* context, const char* extensionName);
```

Checks if a device extension is available. Call after `veCreateContext`, before `veCreateDevice`.

---

### veGetDeviceName / veGetDriverVersion

```c
const char* veGetDeviceName(VEDevice* device);
const char* veGetDriverVersion(VEDevice* device);
```

Returns device information strings.

---

### veDeviceWaitIdle

```c
VEResult veDeviceWaitIdle(VEDevice* device);
```

Waits for all GPU work to complete. Call before destroying resources.

---

### veGetVkInstance / veGetVkDevice / veGetVkPhysicalDevice

```c
VkInstance veGetVkInstance(VEContext* context);
VkDevice veGetVkDevice(VEDevice* device);
VkPhysicalDevice veGetVkPhysicalDevice(VEDevice* device);
```

Returns underlying Vulkan handles for custom extension use.

---

### veGetLastError

```c
const char* veGetLastError(void);
```

Returns the last error message. Check after any function returns NULL or an error code.

---

## Swapchain

### veCreateSwapchain

```c
VESwapchain* veCreateSwapchain(VEDevice* device, const VESwapchainDesc* desc);
```

Creates a swapchain for a window.

**VESwapchainDesc**:
```c
typedef struct VESwapchainDesc {
    VESurfaceDesc surface;   // Platform-specific window handle
    uint32_t width;
    uint32_t height;
    VkFormat colorFormat;    // Usually VK_FORMAT_B8G8R8A8_SRGB
    bool vsync;
    const char* debugName;
} VESwapchainDesc;
```

**VESurfaceDesc** (union):
```c
typedef struct VESurfaceDesc {
    VESurfaceType type;  // VE_SURFACE_TYPE_WIN32, VE_SURFACE_TYPE_XLIB, etc.
    union {
        struct { void* hwnd; } win32;
        struct { void* display; unsigned long window; } xlib;
        struct { void* display; void* surface; } wayland;
        struct { void* window; } cocoa;
        struct { void* nativeWindow; } android;
    };
} VESurfaceDesc;
```

---

### veDestroySwapchain

```c
void veDestroySwapchain(VESwapchain* swapchain);
```

---

### veResizeSwapchain

```c
VEResult veResizeSwapchain(VESwapchain* swapchain, uint32_t width, uint32_t height);
```

Resizes the swapchain. Call in your window resize callback.

---

### veGetSwapchainSize / veGetSwapchainFormat

```c
VEResult veGetSwapchainSize(VESwapchain* swapchain, uint32_t* width, uint32_t* height);
VkFormat veGetSwapchainFormat(VESwapchain* swapchain);
```

---

## Render Targets

### veCreateRenderTarget

```c
VERenderTarget* veCreateRenderTarget(VEDevice* device, const VERenderTargetDesc* desc);
```

**VERenderTargetDesc**:
```c
typedef struct VERenderTargetDesc {
    uint32_t width;
    uint32_t height;
    VkFormat colorFormat;
    VkFormat depthFormat;           // VK_FORMAT_UNDEFINED = no depth
    VkSampleCountFlags sampleCount; // 0 or 1 = no MSAA
    bool hasResolveTarget;          // For MSAA
    const char* debugName;
} VERenderTargetDesc;
```

---

### veDestroyRenderTarget

```c
void veDestroyRenderTarget(VERenderTarget* renderTarget);
```

---

### veResizeRenderTarget

```c
VEResult veResizeRenderTarget(VERenderTarget* renderTarget, uint32_t width, uint32_t height);
```

Resizes the render target. Call in your window resize callback, then rebuild `VERenderingInfo`.

---

### veGetRenderTargetColorTexture / veGetRenderTargetDepthTexture

```c
VETextureIndex veGetRenderTargetColorTexture(VERenderTarget* renderTarget);
VETextureIndex veGetRenderTargetDepthTexture(VERenderTarget* renderTarget);
VETextureIndex veGetRenderTargetResolveTexture(VERenderTarget* renderTarget);
```

Returns texture handles for use in `VERenderingInfo`.

---

### veGetRenderTargetSize / veGetRenderTargetColorFormat

```c
VEResult veGetRenderTargetSize(VERenderTarget* renderTarget, uint32_t* width, uint32_t* height);
VkFormat veGetRenderTargetColorFormat(VERenderTarget* renderTarget);
VkFormat veGetRenderTargetDepthFormat(VERenderTarget* renderTarget);
```

---

### veBlitToSwapchain

```c
VEResult veBlitToSwapchain(
    VECommandBuffer* cmd,
    VERenderTarget* renderTarget,
    VESwapchain* swapchain,
    VkFilter filter);  // VK_FILTER_LINEAR or VK_FILTER_NEAREST
```

Blits the render target to the swapchain. Internally acquires the next swapchain image and handles layout transitions.

**Returns**: `VE_SUCCESS` or `VE_ERROR_SWAPCHAIN_OUT_OF_DATE` (resize needed).

---

## Buffers

### veCreateBuffer

```c
VEBufferAddress veCreateBuffer(VEDevice* device, const VEBufferDesc* desc);
```

**VEBufferDesc**:
```c
typedef struct VEBufferDesc {
    uint64_t size;
    VkBufferUsageFlags usage;
    const void* initialData;
    uint64_t initialDataSize;
    bool persistentlyMapped;
    const char* debugName;
} VEBufferDesc;
```

**Returns**: 64-bit GPU address, or `VE_INVALID_ADDRESS` on failure.

---

### veDestroyBuffer

```c
void veDestroyBuffer(VEDevice* device, VEBufferAddress address);
```

---

### veUpdateBuffer

```c
VEResult veUpdateBuffer(
    VEDevice* device,
    VEBufferAddress address,
    const void* data,
    uint64_t size,
    uint64_t offset);
```

Updates buffer contents. Works with both mapped and unmapped buffers.

---

### veCreateVertexBuffer / veCreateIndexBuffer / veCreateUniformBuffer / veCreateStorageBuffer

```c
VEBufferAddress veCreateVertexBuffer(VEDevice* device, const void* vertices, uint64_t size, const char* debugName);
VEBufferAddress veCreateIndexBuffer(VEDevice* device, const void* indices, uint64_t size, const char* debugName);
VEBufferAddress veCreateUniformBuffer(VEDevice* device, uint64_t size, bool persistentlyMapped, const char* debugName);
VEBufferAddress veCreateStorageBuffer(VEDevice* device, uint64_t size, const char* debugName);
```

Convenience functions for common buffer types.

---

## Textures

### veCreateTexture

```c
VETextureIndex veCreateTexture(VEDevice* device, const VETextureDesc* desc);
```

**Returns**: 32-bit bindless index, or `VE_INVALID_TEXTURE_INDEX` on failure.

---

### veDestroyTexture

```c
void veDestroyTexture(VEDevice* device, VETextureIndex index);
```

---

### veCreateTexture2D

```c
VETextureIndex veCreateTexture2D(
    VEDevice* device,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    const char* debugName);
```

---

### veLoadTexture

```c
VETextureIndex veLoadTexture(
    VEDevice* device,
    const char* filename,
    VkImageUsageFlags usage,
    bool generateMips);
```

Loads a texture from file (PNG, JPG, etc.).

---

## Samplers

### veCreateSampler

```c
VESamplerIndex veCreateSampler(VEDevice* device, const VESamplerDesc* desc);
```

**Returns**: 32-bit bindless index.

---

### veDestroySampler

```c
void veDestroySampler(VEDevice* device, VESamplerIndex index);
```

---

### veCreateLinearSampler / veCreateNearestSampler / veCreateAnisotropicSampler

```c
VESamplerIndex veCreateLinearSampler(VEDevice* device);
VESamplerIndex veCreateNearestSampler(VEDevice* device);
VESamplerIndex veCreateAnisotropicSampler(VEDevice* device, float maxAnisotropy);
```

---

## Shaders

### veLoadShaderFromFile

```c
VEShader* veLoadShaderFromFile(
    VEDevice* device,
    const char* filename,
    VkShaderStageFlags stage,  // VK_SHADER_STAGE_VERTEX_BIT, etc.
    const char* entryPoint,    // Usually "main"
    const char* debugName);
```

Loads a shader from a SPIR-V file.

---

### veLoadShaderFromBuffer

```c
VEShader* veLoadShaderFromBuffer(
    VEDevice* device,
    VkShaderStageFlags stage,
    const void* code,
    size_t codeSize,
    const char* entryPoint,
    const char* debugName);
```

Creates a shader from SPIR-V binary data.

---

### veDestroyShader

```c
void veDestroyShader(VEShader* shader);
```

---

### veCreateShaderConfig

```c
VEShaderConfig* veCreateShaderConfig(VEDevice* device, const VEShaderConfigDesc* desc);
```

**VEShaderConfigDesc**:
```c
typedef struct VEShaderConfigDesc {
    VEShader* vertexShader;
    VEShader* fragmentShader;
    VEShader* geometryShader;      // Optional
    VEShader* tessControlShader;   // Optional
    VEShader* tessEvalShader;      // Optional
    VEShader* computeShader;       // For compute dispatches
    const char* debugName;
} VEShaderConfigDesc;
```

---

### veDestroyShaderConfig

```c
void veDestroyShaderConfig(VEShaderConfig* config);
```

---

## Render Configurations

### veCreateRenderConfig

```c
VERenderConfig* veCreateRenderConfig(VEDevice* device, const VERenderConfigDesc* desc);
```

---

### veDestroyRenderConfig

```c
void veDestroyRenderConfig(VERenderConfig* config);
```

---

### veCreateOpaqueRenderConfig / veCreateTransparentRenderConfig / veCreateWireframeRenderConfig

```c
VERenderConfig* veCreateOpaqueRenderConfig(VEDevice* device, const char* debugName);
VERenderConfig* veCreateTransparentRenderConfig(VEDevice* device, const char* debugName);
VERenderConfig* veCreateWireframeRenderConfig(VEDevice* device, const char* debugName);
```

Built-in configurations for common use cases.

---

## Command Buffers

### veBeginCommandBuffer

```c
VECommandBuffer* veBeginCommandBuffer(VEDevice* device);
```

Gets a command buffer and begins recording.

---

### veEndCommandBuffer

```c
VEResult veEndCommandBuffer(VECommandBuffer* cmd);
```

Ends recording. Called automatically by `veSubmitCommandBuffer`.

---

### veSubmitCommandBuffer

```c
VEResult veSubmitCommandBuffer(VECommandBuffer* cmd, bool waitForCompletion);
```

Submits the command buffer for execution.

---

### veReleaseCommandBuffer

```c
void veReleaseCommandBuffer(VECommandBuffer* cmd);
```

Returns the command buffer to the pool. Called automatically if you pass `true` to `vePresentImage`.

---

### veBeginSecondaryCommandBuffer

```c
VECommandBuffer* veBeginSecondaryCommandBuffer(
    VEDevice* device,
    const VESecondaryCommandBufferDesc* desc);
```

Creates a secondary command buffer for parallel recording.

**VESecondaryCommandBufferDesc**:
```c
typedef struct VESecondaryCommandBufferDesc {
    VkCommandBufferUsageFlags usageFlags;
    uint32_t colorAttachmentCount;
    VkFormat colorAttachmentFormats[8];
    VkFormat depthAttachmentFormat;
    VkFormat stencilAttachmentFormat;
    VkSampleCountFlags rasterizationSamples;
    uint32_t viewMask;
    bool occlusionQueryEnable;
    bool beginRecording;
    int32_t renderAreaX, renderAreaY;
    uint32_t renderAreaWidth, renderAreaHeight;
} VESecondaryCommandBufferDesc;
```

---

### vePopulateSecondaryDescFromRenderingInfo

```c
VEResult vePopulateSecondaryDescFromRenderingInfo(
    VEDevice* device,
    const VERenderingInfo* renderingInfo,
    VESecondaryCommandBufferDesc* desc);
```

Helper to fill in format/sample info from a `VERenderingInfo`.

---

### veExecuteSecondaryCommandBuffers

```c
VEResult veExecuteSecondaryCommandBuffers(
    VECommandBuffer* primaryCmd,
    uint32_t count,
    VECommandBuffer* const* secondaryCmds,
    bool releaseCommandBuffers);
```

Executes secondary command buffers from a primary. Must be called inside `veBeginRendering`/`veEndRendering`.

---

## Dynamic Rendering

### veCreateRenderingInfo

```c
VERenderingInfo veCreateRenderingInfo(uint32_t width, uint32_t height);
VERenderingInfo veCreateRenderingInfoWithOffset(int32_t x, int32_t y, uint32_t width, uint32_t height);
```

Creates a `VERenderingInfo` ready for adding attachments.

---

### veRenderingAddColorAttachment

```c
void veRenderingAddColorAttachment(
    VERenderingInfo* info,
    VETextureIndex texture,
    VkAttachmentLoadOp loadOp,  // VK_ATTACHMENT_LOAD_OP_CLEAR or VK_ATTACHMENT_LOAD_OP_LOAD
    VEColor clearValue);
```

---

### veRenderingAddColorAttachmentResolve

```c
void veRenderingAddColorAttachmentResolve(
    VERenderingInfo* info,
    VETextureIndex texture,        // MSAA texture
    VETextureIndex resolveTexture, // Resolve target
    VkAttachmentLoadOp loadOp,
    VEColor clearValue);
```

---

### veRenderingSetDepthAttachment

```c
void veRenderingSetDepthAttachment(
    VERenderingInfo* info,
    VETextureIndex texture,
    VkAttachmentLoadOp loadOp,
    float clearDepth);
```

---

### veRenderingSetStencilAttachment

```c
void veRenderingSetStencilAttachment(
    VERenderingInfo* info,
    VETextureIndex texture,
    VkAttachmentLoadOp loadOp,
    uint32_t clearStencil);
```

---

### veBeginRendering

```c
void veBeginRendering(VECommandBuffer* cmd, const VERenderingInfo* renderingInfo);
```

Begins dynamic rendering. Automatically handles texture layout transitions.

---

### veEndRendering

```c
void veEndRendering(VECommandBuffer* cmd);
```

---

## Render State

### veApplyRenderState

```c
void veApplyRenderState(VECommandBuffer* cmd, const VERenderState* state);
```

Applies shader config, render config, viewport, and scissor in one call. Must be called inside `veBeginRendering`/`veEndRendering`.

**VERenderState**:
```c
typedef struct VERenderState {
    VEShaderConfig* shaderConfig;  // NULL = don't bind
    VERenderConfig* renderConfig;  // NULL = don't apply
    const VEViewport* viewport;    // NULL = full render area
    const VERect2D* scissor;       // NULL = full render area
} VERenderState;
```

---

### veApplyRenderConfig

```c
void veApplyRenderConfig(VECommandBuffer* cmd, VERenderConfig* config);
```

Applies just the render configuration.

---

### veBindShader / veBindShaderConfig

```c
void veBindShader(VECommandBuffer* cmd, VEShader* shader);
void veBindShaderConfig(VECommandBuffer* cmd, VEShaderConfig* config);
```

---

### veSetViewport / veSetScissor

```c
void veSetViewport(VECommandBuffer* cmd, float x, float y, float width, float height, float minDepth, float maxDepth);
void veSetScissor(VECommandBuffer* cmd, int32_t x, int32_t y, uint32_t width, uint32_t height);
```

---

## Drawing

### vePushConstants

```c
void vePushConstants(VECommandBuffer* cmd, const void* data, uint64_t size, uint64_t offset);
```

Sets push constant data. Maximum 256 bytes.

---

### veBindIndexBuffer

```c
void veBindIndexBuffer(VECommandBuffer* cmd, VEBufferAddress indexBuffer, uint64_t offset, VkIndexType format);
```

---

### veDraw

```c
void veDraw(VECommandBuffer* cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
```

---

### veDrawIndexed

```c
void veDrawIndexed(VECommandBuffer* cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
```

---

### veDrawIndirect / veDrawIndexedIndirect

```c
void veDrawIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride);
void veDrawIndexedIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride);
```

---

## Compute

### veDispatch

```c
void veDispatch(VECommandBuffer* cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
```

---

### veDispatchIndirect

```c
void veDispatchIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer, uint64_t offset);
```

---

## Synchronization

### veBarrierVertexToFragment / veBarrierComputeToVertex / veBarrierComputeToCompute

```c
void veBarrierVertexToFragment(VECommandBuffer* cmd);
void veBarrierComputeToVertex(VECommandBuffer* cmd);
void veBarrierComputeToCompute(VECommandBuffer* cmd);
```

Simple memory barriers for common cases.

---

### veTransitionTexture

```c
void veTransitionTexture(VECommandBuffer* cmd, VETextureIndex texture, VkImageLayout oldLayout, VkImageLayout newLayout);
```

---

### veTransitionTextureForShaderRead / veTransitionTextureForColorAttachment / etc.

```c
void veTransitionTextureForShaderRead(VECommandBuffer* cmd, VETextureIndex texture);
void veTransitionTextureForColorAttachment(VECommandBuffer* cmd, VETextureIndex texture);
void veTransitionTextureForDepthAttachment(VECommandBuffer* cmd, VETextureIndex texture);
void veTransitionTextureForPresent(VECommandBuffer* cmd, VETextureIndex texture);
```

Convenience functions for common layout transitions.

---

## Presentation

### vePresentImage

```c
VEResult vePresentImage(VESwapchain* swapchain, VECommandBuffer* cmd, bool releaseCommandBuffer);
```

Presents the rendered frame. Call after `veBlitToSwapchain`.

- `releaseCommandBuffer = true`: Auto-release the command buffer
- `releaseCommandBuffer = false`: You manage command buffer lifetime

**Returns**: `VE_SUCCESS` or `VE_ERROR_SWAPCHAIN_OUT_OF_DATE`.

---

## Debug & Profiling

### veBeginDebugLabel / veEndDebugLabel / veInsertDebugLabel

```c
void veBeginDebugLabel(VECommandBuffer* cmd, const char* label, VEColor color);
void veEndDebugLabel(VECommandBuffer* cmd);
void veInsertDebugLabel(VECommandBuffer* cmd, const char* label, VEColor color);
```

Adds debug markers visible in GPU profilers (RenderDoc, etc.).

---

### veSetBufferDebugName / veSetTextureDebugName

```c
VEResult veSetBufferDebugName(VEDevice* device, VEBufferAddress address, const char* name);
VEResult veSetTextureDebugName(VEDevice* device, VETextureIndex texture, const char* name);
```

---

### veGetPerformanceStats / veGetMemoryStats

```c
VEResult veGetPerformanceStats(VEDevice* device, VEPerformanceStats* stats);
VEResult veGetMemoryStats(VEDevice* device, VEMemoryStats* stats);
```

---

### vePrintDebugInfo

```c
void vePrintDebugInfo(VEDevice* device);
```

Prints comprehensive debug information to stdout.

---

## Push Constants Structures

VulkEase provides standard push constant structures:

### VEGraphicsPushConstants (256 bytes)

```c
typedef struct VEGraphicsPushConstants {
    VEBufferAddress vertexBuffer;       // 8 bytes
    VEBufferAddress indexBuffer;        // 8 bytes
    VEBufferAddress uniformBuffers[8];  // 64 bytes
    VETextureIndex textures[16];        // 64 bytes
    VESamplerIndex samplers[16];        // 64 bytes
    float objectScale;                  // 4 bytes
    uint32_t activeTextureCount;        // 4 bytes
    uint32_t activeSamplerCount;        // 4 bytes
    uint32_t activeUniformCount;        // 4 bytes
    uint32_t reserved[7];               // 28 bytes
} VEGraphicsPushConstants;
```

Initialize with:
```c
VEGraphicsPushConstants pc = VE_INIT_GRAPHICS_PUSH_CONSTANTS();
```

### VEComputePushConstants (256 bytes)

```c
typedef struct VEComputePushConstants {
    VEBufferAddress buffers[16];   // 128 bytes
    VETextureIndex textures[16];   // 64 bytes
    VESamplerIndex samplers[8];    // 32 bytes
    uint32_t elementCount;         // 4 bytes
    uint32_t activeBufferCount;    // 4 bytes
    uint32_t activeTextureCount;   // 4 bytes
    uint32_t activeSamplerCount;   // 4 bytes
    uint32_t reserved[4];          // 16 bytes
} VEComputePushConstants;
```

Initialize with:
```c
VEComputePushConstants pc = VE_INIT_COMPUTE_PUSH_CONSTANTS();
```

---

## Color Constants

```c
#define VE_COLOR_BLACK   ((VEColor){0.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_WHITE   ((VEColor){1.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_RED     ((VEColor){1.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_GREEN   ((VEColor){0.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_BLUE    ((VEColor){0.0f, 0.0f, 1.0f, 1.0f})
```

---

## Invalid Constants

```c
#define VE_INVALID_ADDRESS        0ULL
#define VE_INVALID_TEXTURE_INDEX  0xFFFFFFFF
#define VE_INVALID_SAMPLER_INDEX  0xFFFFFFFF
```

Use these to check for creation failures:
```c
if (buffer == VE_INVALID_ADDRESS) {
    printf("Error: %s\n", veGetLastError());
}
```
