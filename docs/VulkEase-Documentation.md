# VulkEase 2.0 Documentation

## Table of Contents

1. [High-Level Overview](#1-high-level-overview)
2. [The Basics](#2-the-basics)
3. [Walkthrough: Textured Cube](#3-walkthrough-textured-cube)
4. [Multithreading and Secondary Command Buffers](#4-multithreading-and-secondary-command-buffers)

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
| **Graphics Pipelines** | Shader + material state in one object. Draw state for per-draw overrides. |
| **Mesh Shaders** | Optional support for `VK_EXT_mesh_shader` task and mesh shaders. |
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
- **Optional extensions** (for advanced features):
  - `VK_EXT_mesh_shader` - Mesh and task shaders (NVIDIA Turing+, AMD RDNA2+, Intel Arc)
- **Platforms**: Windows, Linux (X11/Wayland), macOS (via MoltenVK)

---

# Ownership, Threading, and Diagnostics

VulkEase is intentionally lightweight, which means you still need to be clear about **who owns what** and **what is safe to do concurrently**.

## Ownership / Lifetimes

- **`VEContext`**
  - Owns: `VkInstance`, debug messenger (if enabled), and message routing state.
  - Destroy with: `veDestroyContext(context)` *after* destroying all devices created from it.

- **`VEDevice`**
  - Owns: `VkDevice`, queues, and all resources created from it (buffers/textures/samplers/configs/shaders).
  - Destroy with: `veDestroyDevice(device)` after destroying swapchains/render targets and freeing other objects created from the device.

- **Escape hatch Vulkan handles**
  - APIs like `veGetVkDevice()`, `veGetVkImageFromTexture()`, etc return raw Vulkan handles.
  - **Returned handles are owned by VulkEase**. Do **not** destroy them directly.
  - Handle lifetime matches the owning VulkEase object (`VEContext`, `VEDevice`, etc).

## Threading Model (current)

- **Message callback**
  - May be invoked from **any thread**. Keep it thread-safe and non-blocking.

- **Command buffers**
  - A single `VECommandBuffer*` must only be recorded from **one thread at a time**.
  - Recording **different** command buffers on **different** threads is supported (this is how secondary command buffers are intended to be used).

## Diagnostics (return-by-value getters)

Many "simple getters" return values directly instead of `VEResult`.

- **On invalid input**, these getters:
  - **Emit an ERROR-level message** via the installed message callback (or stderr fallback)
  - Return a **sentinel** value (e.g. `VK_NULL_HANDLE`, `VK_FORMAT_UNDEFINED`, `{0,0}`, `0`)

# 2. The Basics

## Context and Device

Every VulkEase application starts by creating a **Context** (Vulkan instance) and a **Device** (GPU handle).

```c
// Create context - initializes Vulkan
VEContext* context = NULL;
VEResult r = veCreateContext("My Application", NULL, 0, &context);
if (r != VE_SUCCESS) { /* handle error */ }

// Create device - selects GPU (VK_NULL_HANDLE = auto-select best)
VEDevice* device = NULL;
r = veCreateDevice(context, VK_NULL_HANDLE, NULL, 0, &device);
if (r != VE_SUCCESS) { /* handle error */ }

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
    VEDevice* device = NULL;
    veCreateDevice(context, VK_NULL_HANDLE, extensions, 1, &device);
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

### Mesh Shaders (Optional)

VulkEase supports **mesh shaders** via `VK_EXT_mesh_shader` for GPU-driven geometry processing. Mesh shaders replace the traditional vertex/geometry pipeline with a more flexible compute-like approach.

```c
// Check if mesh shaders are supported
if (veIsMeshShaderSupported(device)) {
    // Load mesh shader (and optional task shader)
    VEShader* meshShader = veLoadShaderFromFile(device, "mesh.mesh.spv",
        VK_SHADER_STAGE_MESH_BIT_EXT, "main", "MyMeshShader");
    
    VEShader* taskShader = veLoadShaderFromFile(device, "mesh.task.spv",
        VK_SHADER_STAGE_TASK_BIT_EXT, "main", "MyTaskShader");  // Optional
    
    VEShader* fragmentShader = veLoadShaderFromFile(device, "mesh.frag.spv",
        VK_SHADER_STAGE_FRAGMENT_BIT, "main", "MyFragmentShader");
}
```

Create a mesh shader pipeline:

```c
VEGraphicsPipelineDesc desc = veDefaultGraphicsPipelineDesc();
desc.meshShader = meshShader;       // Required for mesh pipeline
desc.taskShader = taskShader;       // Optional (amplification shader)
desc.fragmentShader = fragmentShader;
desc.debugName = "MeshPipeline";

VEGraphicsPipeline* pipeline = NULL;
veCreateGraphicsPipeline(device, &desc, &pipeline);
```

Draw with mesh shaders:

```c
veBindGraphicsPipeline(cmd, pipeline);
veSetViewport(cmd, 0, 0, width, height, 0.0f, 1.0f);
veSetScissor(cmd, 0, 0, width, height);

// Issue mesh shader draw (workgroup counts, not vertex counts)
veDrawMeshTasks(cmd, groupCountX, groupCountY, groupCountZ);

// Or use indirect drawing for GPU-driven rendering
veDrawMeshTasksIndirect(cmd, indirectBuffer, offset, drawCount, stride);
veDrawMeshTasksIndirectCount(cmd, indirectBuffer, indirectOffset,
                             countBuffer, countOffset, maxDrawCount, stride);
```

**Note**: When a mesh shader is bound, the vertex input stage is bypassed. Mesh shaders generate primitives directly, so `vertexShader`, `geometryShader`, and tessellation shaders are ignored.

### Graphics Pipelines

Bundle shaders with material state into a single graphics pipeline:

```c
// Use helper for common configurations
VEGraphicsPipeline* pipeline = NULL;
veCreateOpaquePipeline(device, vertexShader, fragmentShader,
    NULL, 0, NULL, 0, "MyPipeline", &pipeline);

// Bind during rendering
veBindGraphicsPipeline(cmd, pipeline);
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

## Graphics Pipelines and Draw State

VulkEase uses a two-object system for graphics rendering:

- **`VEGraphicsPipeline`**: Combines shaders + vertex input + material state (depth, stencil, blend, rasterization defaults). Created once, rarely changes.
- **`VEDrawState`**: Per-draw overrides (primitive topology, polygon mode, cull mode, line width, depth bias). Can change every draw.

### Helper Pipelines

```c
// Use built-in pipelines (no vertex input for bindless rendering)
VEGraphicsPipeline* opaquePipeline = NULL;
VEGraphicsPipeline* transparentPipeline = NULL;
veCreateOpaquePipeline(device, vertexShader, fragmentShader, NULL, 0, NULL, 0, "Opaque", &opaquePipeline);
veCreateTransparentPipeline(device, vertexShader, fragmentShader, NULL, 0, NULL, 0, "Transparent", &transparentPipeline);

// Bind during rendering
veBindGraphicsPipeline(cmd, opaquePipeline);
```

### Custom Pipelines

```c
VEGraphicsPipelineDesc pipelineDesc = veDefaultGraphicsPipelineDesc();
pipelineDesc.vertexShader = vertexShader;
pipelineDesc.fragmentShader = fragmentShader;
pipelineDesc.depthTestEnable = true;
pipelineDesc.depthWriteEnable = true;
pipelineDesc.depthCompareOp = VK_COMPARE_OP_LESS;
pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
pipelineDesc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
pipelineDesc.debugName = "CustomPipeline";

VEGraphicsPipeline* pipeline = NULL;
veCreateGraphicsPipeline(device, &pipelineDesc, &pipeline);
```

### Draw State for Per-Draw Overrides

```c
// Create draw state with custom settings
VEDrawStateDesc drawStateDesc = veDefaultDrawStateDesc();
drawStateDesc.polygonMode = VK_POLYGON_MODE_LINE;  // Wireframe
drawStateDesc.lineWidth = 2.0f;

VEDrawState* wireframeState = NULL;
veCreateDrawState(device, &drawStateDesc, &wireframeState);

// Apply during rendering
veApplyDrawState(cmd, wireframeState);

// Or use individual setters
veSetPolygonMode(cmd, VK_POLYGON_MODE_LINE);
veSetLineWidth(cmd, 2.0f);
```

### Convenience Function

```c
// Bind pipeline + apply draw state + set viewport/scissor in one call
veApplyGraphicsState(cmd, pipeline, drawState, NULL, NULL);  // NULL = full render area
```

## The Render Loop

Every frame follows this pattern:

```c
while (running) {
    // 1. Begin command buffer
    VECommandBuffer* cmd = veBeginCommandBuffer(device);
    
    // 2. Begin rendering to render target
    veBeginRendering(cmd, &renderingInfo);
    
    // 3. Bind graphics pipeline (shaders + material state)
    veBindGraphicsPipeline(cmd, pipeline);
    
    // 4. Set viewport and scissor
    veSetViewport(cmd, 0, 0, width, height, 0.0f, 1.0f);
    veSetScissor(cmd, 0, 0, width, height);
    
    // 5. Push constants and draw
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    veBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    veDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    
    // 6. End rendering
    veEndRendering(cmd);
    
    // 7. Blit to swapchain
    veBlitToSwapchain(cmd, renderTarget, swapchain, VK_FILTER_LINEAR);
    
    // 8. Present
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
    VEGraphicsPipeline* pipeline;  // Shaders + material state
    
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

## Step 9: Create Graphics Pipeline

```c
// For bindless rendering (no vertex input needed)
veCreateOpaquePipeline(app->device, app->vertexShader, app->fragmentShader,
    NULL, 0, NULL, 0, "CubePipeline", &app->pipeline);

// Or with vertex input:
VEVertexBinding bindings[] = {
    { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }
};

VEVertexAttribute attributes[] = {
    { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
    { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 12 }
};

veCreateOpaquePipeline(app->device, app->vertexShader, app->fragmentShader,
    bindings, 1, attributes, 2, "CubePipeline", &app->pipeline);
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
    
    // Bind graphics pipeline (shaders + material state)
    veBindGraphicsPipeline(cmd, app->pipeline);
    
    // Set viewport and scissor
    veSetViewport(cmd, 0, 0, width, height, 0.0f, 1.0f);
    veSetScissor(cmd, 0, 0, width, height);
    
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

veDestroyGraphicsPipeline(app->pipeline);
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
    veBindGraphicsPipeline(cmd, pipeline);
    veSetViewport(cmd, 0, 0, width, height, 0.0f, 1.0f);
    veSetScissor(cmd, 0, 0, width, height);
    
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

- Graphics pipeline (via `veBindGraphicsPipeline`)
- Viewport and scissor (via `veSetViewport`/`veSetScissor`)
- Push constants

The `veApplyGraphicsState` function handles pipeline + draw state + viewport/scissor in one call.

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

# API Reference

The complete API reference is available directly in the header file with comprehensive Doxygen-style documentation:

- **C/C++**: See [`src/vulkease.h`](../src/vulkease.h) for all function signatures, parameter descriptions, return values, and usage notes.
- **.NET**: See [`dotnet/src/VulkEaseFunctions.cs`](../dotnet/src/VulkEaseFunctions.cs) for C# wrapper methods with XML documentation.

The header is organized into 41 sections covering all aspects of the API:

1. Version & API Constants
2. Core Types (Handles, Indices, Addresses)
3. Result Codes
4. Enumerations
5. Color & Geometry Types
6. Buffer Descriptors
7. Texture Descriptors
8. Sampler Descriptors
9. Shader & Pipeline Descriptors
10. Draw State Descriptors
11. Rendering Types
12. Swapchain & Surface Types
13. Render Target Types
14. Query Types
15. Statistics Types
16. Push Constants (Example Structures)
17. Context & Device Functions
18. Vulkan Handle Access (Escape Hatches)
19. Message Callback Functions
20. Fence Functions
21. Buffer Functions
22. Texture Functions
23. Sampler Functions
24. Shader Functions
25. Graphics Pipeline Functions
26. Draw State Functions
27. Command Buffer Functions
28. Rendering Functions
29. State Binding Functions
30. Viewport & Scissor Functions
31. Dynamic State Functions
32. Push Constants & Buffer Binding
33. Draw Commands (including Mesh Shader draws)
34. Clear Commands
35. Query Functions
36. Compute Functions
37. Synchronization Functions
38. Swapchain Functions
39. Render Target Functions
40. Debug & Profiling Functions
41. Frame Timing Functions

### Mesh Shader Functions

The following functions are available for mesh shader rendering (requires `VK_EXT_mesh_shader` support):

| Function | Description |
|----------|-------------|
| `veIsMeshShaderSupported` | Check if device supports mesh shaders |
| `veDrawMeshTasks` | Issue mesh shader draw with workgroup counts |
| `veDrawMeshTasksIndirect` | Indirect mesh shader draw |
| `veDrawMeshTasksIndirectCount` | Indirect mesh draw with GPU-driven count |

Use `taskShader` and `meshShader` fields in `VEGraphicsPipelineDesc` to create mesh shader pipelines.

To generate HTML documentation from the header, run Doxygen with the provided configuration (if available) or use a tool like `doxygen -g` to generate a config and point it at `src/vulkease.h`.
