# VulkEase Best Practices Guide

**Expert Guidelines for High-Quality VulkEase Applications**

---

## Table of Contents

1. [Architecture Best Practices](#architecture-best-practices)
2. [Resource Management](#resource-management)
3. [Shader Development](#shader-development)
4. [Error Handling](#error-handling)
5. [Debugging and Development](#debugging-and-development)
6. [Production Considerations](#production-considerations)
7. [Cross-Platform Development](#cross-platform-development)
8. [Performance Patterns](#performance-patterns)

---

## Architecture Best Practices

### Application Structure

Design your VulkEase application with clear separation of concerns:

```c
// Recommended application structure
typedef struct {
    VEContext* context;
    VEDevice* device;
    VESwapchain* swapchain;
    
    // Resource managers
    ResourceManager* resourceManager;
    ShaderManager* shaderManager;
    RenderGraph* renderGraph;
    
    // Performance monitoring
    PerformanceTracker* perfTracker;
    DebugRenderer* debugRenderer;
} VulkEaseApp;

// Initialize in proper order
VEResult initializeApp(VulkEaseApp* app, const char* appName, void* windowHandle) {
    // 1. Core VulkEase initialization
    app->context = veCreateContext(appName);
    if (!app->context) return VE_ERROR_INITIALIZATION_FAILED;
    
    app->device = veCreateDevice(app->context, VE_DEVICE_TYPE_AUTO);
    if (!app->device) return VE_ERROR_INITIALIZATION_FAILED;
    
    app->swapchain = veCreateSwapchain(app->device, windowHandle, 1920, 1080);
    if (!app->swapchain) return VE_ERROR_INITIALIZATION_FAILED;
    
    // 2. Initialize subsystems
    app->resourceManager = createResourceManager(app->device);
    app->shaderManager = createShaderManager(app->device);
    app->renderGraph = createRenderGraph(app->device);
    
    // 3. Development tools (debug builds only)
    #ifdef DEBUG
    app->perfTracker = createPerformanceTracker(app->device);
    app->debugRenderer = createDebugRenderer(app->device);
    veSetValidationEnabled(app->device, VE_TRUE);
    #endif
    
    return VE_SUCCESS;
}
```

### Resource Manager Pattern

Implement centralized resource management:

```c
typedef struct {
    VEDevice* device;
    
    // Resource pools
    VETextureIndex textures[MAX_TEXTURES];
    VEBufferIndex buffers[MAX_BUFFERS];  
    VESamplerIndex samplers[MAX_SAMPLERS];
    
    // Metadata
    ResourceMetadata textureMetadata[MAX_TEXTURES];
    ResourceMetadata bufferMetadata[MAX_BUFFERS];
    ResourceMetadata samplerMetadata[MAX_SAMPLERS];
    
    // Free lists
    uint32_t freeTextures[MAX_TEXTURES];
    uint32_t freeBuffers[MAX_BUFFERS];
    uint32_t freeSamplers[MAX_SAMPLERS];
    
    uint32_t freeTextureCount;
    uint32_t freeBufferCount;
    uint32_t freeSamplerCount;
    
    // Debug information
    const char* textureNames[MAX_TEXTURES];
    const char* bufferNames[MAX_BUFFERS];
    const char* samplerNames[MAX_SAMPLERS];
} ResourceManager;

VETextureIndex rmCreateTexture2D(ResourceManager* rm, const char* name,
                                uint32_t width, uint32_t height, VEFormat format, 
                                VETextureUsage usage, const void* data, size_t dataSize) {
    if (rm->freeTextureCount == 0) {
        printf("ERROR: No free texture slots available\n");
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    // Get next free slot
    uint32_t slot = rm->freeTextures[--rm->freeTextureCount];
    
    // Create VulkEase texture
    VETextureIndex texture = veCreateTexture2D(rm->device, width, height, format, usage, data, dataSize);
    if (texture == VE_INVALID_TEXTURE_INDEX) {
        // Return slot to free list
        rm->freeTextures[rm->freeTextureCount++] = slot;
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    // Store in managed slot
    rm->textures[slot] = texture;
    rm->textureNames[slot] = strdup(name);
    rm->textureMetadata[slot] = (ResourceMetadata){
        .width = width,
        .height = height,
        .format = format,
        .usage = usage,
        .memorySize = estimateTextureMemory(width, height, format),
        .creationTime = getCurrentTime(),
        .lastAccessTime = getCurrentTime()
    };
    
    // Set debug name
    veSetResourceDebugName(rm->device, "texture", texture, name);
    
    printf("Created texture '%s' at index %u (slot %u)\n", name, texture, slot);
    return texture;
}
```

### Render Graph Architecture

Use a render graph for complex rendering pipelines:

```c
typedef struct RenderPass RenderPass;
typedef struct RenderGraph RenderGraph;

typedef struct {
    const char* name;
    VETextureIndex* inputs;
    uint32_t inputCount;
    VETextureIndex* outputs; 
    uint32_t outputCount;
    void (*execute)(RenderPass* pass, VECommandBuffer* cmd);
    void* userData;
} RenderPassDesc;

struct RenderPass {
    RenderPassDesc desc;
    uint32_t passIndex;
    VEPipeline* pipeline;
    VETextureIndex* resolvedInputs;
    VETextureIndex* resolvedOutputs;
};

struct RenderGraph {
    VEDevice* device;
    RenderPass passes[MAX_RENDER_PASSES];
    uint32_t passCount;
    
    // Resource tracking
    VETextureIndex transientTextures[MAX_TRANSIENT_TEXTURES];
    uint32_t transientCount;
};

void rgAddPass(RenderGraph* rg, const RenderPassDesc* desc) {
    RenderPass* pass = &rg->passes[rg->passCount++];
    pass->desc = *desc;
    pass->passIndex = rg->passCount - 1;
    
    // Resolve resource dependencies
    rgResolveResources(rg, pass);
    
    printf("Added render pass '%s' (index %u)\n", desc->name, pass->passIndex);
}

void rgExecute(RenderGraph* rg, VECommandBuffer* cmd) {
    for (uint32_t i = 0; i < rg->passCount; i++) {
        RenderPass* pass = &rg->passes[i];
        
        vePushDebugGroup(cmd, pass->desc.name);
        
        uint32_t timer = veBeginGpuTimer(cmd, pass->desc.name);
        
        // Execute pass
        pass->desc.execute(pass, cmd);
        
        veEndGpuTimer(cmd, timer);
        vePopDebugGroup(cmd);
    }
}
```

---

## Resource Management

### Texture Management Best Practices

```c
// Texture loading and management
typedef enum {
    TEXTURE_TYPE_DIFFUSE,
    TEXTURE_TYPE_NORMAL,
    TEXTURE_TYPE_ROUGHNESS,
    TEXTURE_TYPE_METALLIC,
    TEXTURE_TYPE_EMISSION,
    TEXTURE_TYPE_HEIGHT,
    TEXTURE_TYPE_AO,
    TEXTURE_TYPE_COUNT
} TextureType;

typedef struct {
    const char* filename;
    TextureType type;
    VEFormat preferredFormat;
    VETextureUsage usage;
    VEBool generateMipmaps;
    VEBool compress;
} TextureLoadDesc;

VETextureIndex loadTexture(ResourceManager* rm, const TextureLoadDesc* desc) {
    // Load image data
    int width, height, channels;
    uint8_t* data = loadImageFile(desc->filename, &width, &height, &channels);
    if (!data) {
        printf("ERROR: Failed to load texture '%s'\n", desc->filename);
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    // Determine optimal format
    VEFormat format = desc->preferredFormat;
    if (format == VE_FORMAT_UNDEFINED) {
        format = determineOptimalFormat(desc->type, channels);
    }
    
    // Compress if requested
    uint8_t* finalData = data;
    size_t finalSize = width * height * channels;
    
    if (desc->compress) {
        finalData = compressTexture(data, width, height, channels, format, &finalSize);
        if (finalData) {
            free(data);
        } else {
            finalData = data;  // Fall back to uncompressed
        }
    }
    
    // Create texture
    VETextureIndex texture = rmCreateTexture2D(rm, desc->filename, width, height, format, 
                                              desc->usage, finalData, finalSize);
    
    // Generate mipmaps if requested
    if (desc->generateMipmaps && texture != VE_INVALID_TEXTURE_INDEX) {
        VECommandBuffer* cmd = veGetSecondaryCommandBuffer(rm->device);
        veGenerateMipmaps(cmd, texture);
        veSubmitCommandBuffer(cmd, NULL);
        veDeviceWaitIdle(rm->device);
    }
    
    free(finalData);
    return texture;
}

VEFormat determineOptimalFormat(TextureType type, int channels) {
    switch (type) {
        case TEXTURE_TYPE_DIFFUSE:
            return channels == 4 ? VE_FORMAT_RGBA8_SRGB : VE_FORMAT_RGB8_SRGB;
        case TEXTURE_TYPE_NORMAL:
            return VE_FORMAT_RG8_UNORM;  // Reconstruct Z in shader
        case TEXTURE_TYPE_ROUGHNESS:
        case TEXTURE_TYPE_METALLIC:
        case TEXTURE_TYPE_AO:
            return VE_FORMAT_R8_UNORM;
        case TEXTURE_TYPE_HEIGHT:
            return VE_FORMAT_R16_UNORM;  // Higher precision for height
        case TEXTURE_TYPE_EMISSION:
            return VE_FORMAT_RGBA16_FLOAT;  // HDR emission
        default:
            return VE_FORMAT_RGBA8_UNORM;
    }
}
```

### Buffer Management Patterns

```c
// Streaming buffer for dynamic data
typedef struct {
    VEBufferIndex buffer;
    void* mappedMemory;
    size_t size;
    size_t currentOffset;
    uint32_t frameIndex;
} StreamingBuffer;

StreamingBuffer* createStreamingBuffer(VEDevice* device, size_t size) {
    StreamingBuffer* sb = malloc(sizeof(StreamingBuffer));
    
    // Create buffer with CPU-visible memory
    sb->buffer = veCreateUniformBuffer(device, size, 0, NULL);
    sb->size = size;
    sb->currentOffset = 0;
    sb->frameIndex = 0;
    
    // Map memory persistently
    // In real implementation, would use veMapBuffer
    sb->mappedMemory = NULL;  // Would be mapped memory
    
    return sb;
}

uint32_t streamingBufferAlloc(StreamingBuffer* sb, const void* data, size_t dataSize) {
    // Align to 256 bytes for uniform buffer requirements
    size_t alignedSize = (dataSize + 255) & ~255;
    
    // Check if we need to wrap around
    if (sb->currentOffset + alignedSize > sb->size) {
        sb->currentOffset = 0;
        sb->frameIndex++;
    }
    
    uint32_t offset = sb->currentOffset;
    
    // Copy data
    if (sb->mappedMemory) {
        memcpy((uint8_t*)sb->mappedMemory + offset, data, dataSize);
    }
    
    sb->currentOffset += alignedSize;
    return offset;
}
```

### Memory Management Strategy

```c
// Memory allocation strategy
typedef enum {
    MEMORY_CATEGORY_STATIC_GEOMETRY,    // Long-lived vertex/index data
    MEMORY_CATEGORY_DYNAMIC_UNIFORMS,   // Per-frame uniform data
    MEMORY_CATEGORY_TEXTURES,           // Texture data
    MEMORY_CATEGORY_STAGING,            // Temporary upload buffers
    MEMORY_CATEGORY_COUNT
} MemoryCategory;

typedef struct {
    VEMemoryPool* pools[MEMORY_CATEGORY_COUNT];
    size_t poolSizes[MEMORY_CATEGORY_COUNT];
    VEMemoryUsage poolUsages[MEMORY_CATEGORY_COUNT];
    uint64_t totalAllocated[MEMORY_CATEGORY_COUNT];
    uint32_t allocationCounts[MEMORY_CATEGORY_COUNT];
} MemoryManager;

void initializeMemoryManager(MemoryManager* mm, VEDevice* device) {
    // Configure pool sizes based on application needs
    mm->poolSizes[MEMORY_CATEGORY_STATIC_GEOMETRY] = 512 * 1024 * 1024;  // 512MB
    mm->poolSizes[MEMORY_CATEGORY_DYNAMIC_UNIFORMS] = 64 * 1024 * 1024;   // 64MB
    mm->poolSizes[MEMORY_CATEGORY_TEXTURES] = 2048 * 1024 * 1024;         // 2GB
    mm->poolSizes[MEMORY_CATEGORY_STAGING] = 256 * 1024 * 1024;           // 256MB
    
    mm->poolUsages[MEMORY_CATEGORY_STATIC_GEOMETRY] = VE_MEMORY_USAGE_GPU_ONLY;
    mm->poolUsages[MEMORY_CATEGORY_DYNAMIC_UNIFORMS] = VE_MEMORY_USAGE_CPU_TO_GPU;
    mm->poolUsages[MEMORY_CATEGORY_TEXTURES] = VE_MEMORY_USAGE_GPU_ONLY;
    mm->poolUsages[MEMORY_CATEGORY_STAGING] = VE_MEMORY_USAGE_CPU_TO_GPU;
    
    // Create pools
    for (int i = 0; i < MEMORY_CATEGORY_COUNT; i++) {
        mm->pools[i] = veCreateMemoryPool(device, mm->poolSizes[i], mm->poolUsages[i]);
        mm->totalAllocated[i] = 0;
        mm->allocationCounts[i] = 0;
    }
}

uint64_t mmAlloc(MemoryManager* mm, MemoryCategory category, size_t size, size_t alignment) {
    uint64_t offset = veAllocateFromPool(mm->pools[category], size, alignment);
    if (offset != UINT64_MAX) {
        mm->totalAllocated[category] += size;
        mm->allocationCounts[category]++;
    }
    return offset;
}

void mmFree(MemoryManager* mm, MemoryCategory category, uint64_t offset, size_t size) {
    veFreeFromPool(mm->pools[category], offset);
    mm->totalAllocated[category] -= size;
    mm->allocationCounts[category]--;
}
```

---

## Shader Development

### Shader Organization

Structure shaders for maintainability and reuse:

```glsl
// common_defines.glsl - Shared constants and macros
#ifndef COMMON_DEFINES_GLSL
#define COMMON_DEFINES_GLSL

#define MAX_LIGHTS 32
#define PI 3.14159265359
#define EPSILON 0.000001

// VulkEase bindless access macros
#define SAMPLE_TEXTURE(tex, samp, uv) texture(sampler2D(globalTextures[tex], globalSamplers[samp]), uv)
#define LOAD_UNIFORM(buf, offset) globalUniformBuffers[buf].data[offset]
#define LOAD_STORAGE(buf, index) globalStorageBuffers[buf].data[index]

#endif
```

```glsl
// lighting_common.glsl - Shared lighting functions
#ifndef LIGHTING_COMMON_GLSL
#define LIGHTING_COMMON_GLSL

#include "common_defines.glsl"

struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

vec3 calculateDirectionalLight(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor) {
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // Blinn-Phong specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfwayDir), 0.0);
    float spec = pow(NdotH, 64.0);
    
    return lightColor * (NdotL + spec * 0.3);
}

vec3 calculatePointLight(Light light, vec3 fragPos, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
    
    vec3 radiance = light.color * light.intensity * attenuation;
    return calculateDirectionalLight(normal, viewDir, lightDir, radiance);
}

#endif
```

```glsl
// pbr_vertex.glsl - PBR vertex shader
#version 450 core

#include "common_defines.glsl"

layout(push_constant) uniform PushConstants {
    uint vertexBuffer;
    uint uniformBuffer;
    uint instanceBuffer;
    uint materialIndex;
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out flat uint materialIndex;

void main() {
    // Load vertex data (conceptual - actual implementation varies)
    uint vertexIndex = gl_VertexIndex;
    vec3 position = vec3(LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 0),
                        LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 1),
                        LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 2));
    vec3 normal = vec3(LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 3),
                      LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 4), 
                      LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 5));
    vec2 texCoord = vec2(LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 6),
                        LOAD_STORAGE(pc.vertexBuffer, vertexIndex * 8 + 7));
    
    // Load instance transform
    mat4 modelMatrix = mat4(1.0);  // Would load from instance buffer
    
    // Load view-projection matrix
    mat4 viewProjMatrix = mat4(1.0);  // Would load from uniform buffer
    
    // Transform vertex
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    gl_Position = viewProjMatrix * worldPos;
    
    // Output data
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(modelMatrix) * normal;
    fragTexCoord = texCoord;
    materialIndex = pc.materialIndex;
}
```

### Shader Hot Reload System

```c
// Hot reload implementation
typedef struct {
    const char* filename;
    time_t lastModified;
    VEShader* shader;
    VEShaderStage stage;
    const char* entryPoint;
} ShaderFile;

typedef struct {
    VEDevice* device;
    ShaderFile files[MAX_SHADER_FILES];
    uint32_t fileCount;
    
    // Shader to pipeline mapping
    VEPipeline* dependentPipelines[MAX_SHADER_FILES][MAX_PIPELINES_PER_SHADER];
    uint32_t dependentCounts[MAX_SHADER_FILES];
} ShaderManager;

void smAddShader(ShaderManager* sm, const char* filename, VEShaderStage stage, const char* entryPoint) {
    ShaderFile* file = &sm->files[sm->fileCount++];
    file->filename = strdup(filename);
    file->stage = stage;
    file->entryPoint = strdup(entryPoint);
    file->lastModified = getFileModificationTime(filename);
    
    // Load initial shader
    file->shader = veCreateShaderFromFile(sm->device, filename, stage, entryPoint);
    if (file->shader) {
        veEnableShaderHotReload(file->shader);
    }
}

void smCheckForUpdates(ShaderManager* sm) {
    for (uint32_t i = 0; i < sm->fileCount; i++) {
        ShaderFile* file = &sm->files[i];
        time_t currentTime = getFileModificationTime(file->filename);
        
        if (currentTime > file->lastModified) {
            printf("Reloading shader: %s\n", file->filename);
            
            // Create new shader
            VEShader* newShader = veCreateShaderFromFile(sm->device, file->filename, file->stage, file->entryPoint);
            if (newShader) {
                // Update dependent pipelines
                for (uint32_t j = 0; j < sm->dependentCounts[i]; j++) {
                    recreatePipeline(sm->dependentPipelines[i][j], file->shader, newShader);
                }
                
                veDestroyShader(file->shader);
                file->shader = newShader;
                file->lastModified = currentTime;
                
                veEnableShaderHotReload(newShader);
            } else {
                printf("ERROR: Failed to reload shader %s\n", file->filename);
            }
        }
    }
}
```

---

## Error Handling

### Comprehensive Error Management

```c
// Error context for debugging
typedef struct {
    VEResult lastResult;
    char lastError[512];
    const char* lastFunction;
    const char* lastFile;
    uint32_t lastLine;
    uint64_t errorCount;
} ErrorContext;

static ErrorContext g_errorContext = {0};

#define VE_CHECK(call, message) \
    do { \
        VEResult result = (call); \
        if (result != VE_SUCCESS) { \
            recordError(result, message, __FUNCTION__, __FILE__, __LINE__); \
            return result; \
        } \
    } while(0)

#define VE_CHECK_NULL(ptr, message) \
    do { \
        if (!(ptr)) { \
            recordError(VE_ERROR_INVALID_PARAMETER, message, __FUNCTION__, __FILE__, __LINE__); \
            return VE_ERROR_INVALID_PARAMETER; \
        } \
    } while(0)

void recordError(VEResult result, const char* message, const char* function, const char* file, uint32_t line) {
    g_errorContext.lastResult = result;
    snprintf(g_errorContext.lastError, sizeof(g_errorContext.lastError), "%s", message);
    g_errorContext.lastFunction = function;
    g_errorContext.lastFile = file;
    g_errorContext.lastLine = line;
    g_errorContext.errorCount++;
    
    // Log error
    printf("ERROR [%s:%u in %s]: %s (result: %d)\n", file, line, function, message, result);
    
    // Break in debugger in debug builds
    #ifdef DEBUG
    #ifdef _WIN32
    __debugbreak();
    #else
    __builtin_trap();
    #endif
    #endif
}

// Safe resource creation with error recovery
VETextureIndex safeCreateTexture2D(VEDevice* device, uint32_t width, uint32_t height, 
                                  VEFormat format, VETextureUsage usage, 
                                  const void* data, size_t dataSize) {
    // Validate parameters
    if (!device || width == 0 || height == 0) {
        recordError(VE_ERROR_INVALID_PARAMETER, "Invalid texture parameters", __FUNCTION__, __FILE__, __LINE__);
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    // Check device limits
    uint32_t maxTextureSize;
    if (veGetDeviceInfo(device, NULL, &maxTextureSize, NULL, NULL) == VE_SUCCESS) {
        if (width > maxTextureSize || height > maxTextureSize) {
            recordError(VE_ERROR_INVALID_PARAMETER, "Texture size exceeds device limits", __FUNCTION__, __FILE__, __LINE__);
            return VE_INVALID_TEXTURE_INDEX;
        }
    }
    
    // Attempt creation
    VETextureIndex texture = veCreateTexture2D(device, width, height, format, usage, data, dataSize);
    if (texture == VE_INVALID_TEXTURE_INDEX) {
        // Try fallback strategies
        
        // 1. Try without initial data
        if (data) {
            texture = veCreateTexture2D(device, width, height, format, usage, NULL, 0);
            if (texture != VE_INVALID_TEXTURE_INDEX) {
                // Upload data separately
                if (veUpdateTexture(device, texture, data, dataSize, 0, 0) != VE_SUCCESS) {
                    veDestroyTexture(device, texture);
                    texture = VE_INVALID_TEXTURE_INDEX;
                }
            }
        }
        
        // 2. Try different format
        if (texture == VE_INVALID_TEXTURE_INDEX && format != VE_FORMAT_RGBA8_UNORM) {
            texture = veCreateTexture2D(device, width, height, VE_FORMAT_RGBA8_UNORM, usage, NULL, 0);
        }
        
        // 3. Try smaller size
        if (texture == VE_INVALID_TEXTURE_INDEX && (width > 512 || height > 512)) {
            uint32_t fallbackWidth = width > 512 ? 512 : width;
            uint32_t fallbackHeight = height > 512 ? 512 : height;
            texture = veCreateTexture2D(device, fallbackWidth, fallbackHeight, format, usage, NULL, 0);
        }
    }
    
    return texture;
}
```

### Validation and Recovery

```c
// Device validation and recovery
VEResult validateDevice(VEDevice* device) {
    if (!device) {
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Check device status
    VEResult deviceStatus = veCheckDeviceStatus(device);  // Hypothetical function
    if (deviceStatus != VE_SUCCESS) {
        return deviceStatus;
    }
    
    // Validate critical resources still exist
    VEPerformanceStats stats;
    if (veGetPerformanceStats(device, &stats) != VE_SUCCESS) {
        return VE_ERROR_DEVICE_LOST;
    }
    
    return VE_SUCCESS;
}

VEResult recoverFromDeviceLoss(VEDevice* device) {
    printf("Attempting device recovery...\n");
    
    // Wait for device to become idle
    VEResult result = veDeviceWaitIdle(device);
    if (result != VE_SUCCESS) {
        printf("Device wait idle failed during recovery\n");
        return result;
    }
    
    // Recreate critical resources
    // This would involve recreating pipelines, swapchains, etc.
    
    printf("Device recovery completed\n");
    return VE_SUCCESS;
}
```

---

## Debugging and Development

### Debug Renderer

```c
// Simple debug rendering system
typedef struct {
    VEDevice* device;
    VEPipeline* linePipeline;
    VEPipeline* textPipeline;
    VEBufferIndex lineVertexBuffer;
    VEBufferIndex textVertexBuffer;
    
    // Line data
    DebugLine lines[MAX_DEBUG_LINES];
    uint32_t lineCount;
    
    // Text data
    DebugText texts[MAX_DEBUG_TEXTS];
    uint32_t textCount;
} DebugRenderer;

typedef struct {
    vec3 start;
    vec3 end;
    vec4 color;
} DebugLine;

typedef struct {
    vec3 position;
    vec4 color;
    char text[64];
} DebugText;

void debugDrawLine(DebugRenderer* dr, vec3 start, vec3 end, vec4 color) {
    if (dr->lineCount < MAX_DEBUG_LINES) {
        dr->lines[dr->lineCount++] = (DebugLine){start, end, color};
    }
}

void debugDrawText(DebugRenderer* dr, vec3 position, vec4 color, const char* format, ...) {
    if (dr->textCount < MAX_DEBUG_TEXTS) {
        DebugText* text = &dr->texts[dr->textCount++];
        text->position = position;
        text->color = color;
        
        va_list args;
        va_start(args, format);
        vsnprintf(text->text, sizeof(text->text), format, args);
        va_end(args);
    }
}

void debugRender(DebugRenderer* dr, VECommandBuffer* cmd, const mat4 viewProjMatrix) {
    if (dr->lineCount == 0 && dr->textCount == 0) return;
    
    vePushDebugGroup(cmd, "Debug Rendering");
    
    // Render lines
    if (dr->lineCount > 0) {
        // Update line vertex buffer
        veUpdateBuffer(dr->device, dr->lineVertexBuffer, dr->lines, 
                      dr->lineCount * sizeof(DebugLine), 0);
        
        // Draw lines
        veBindPipeline(cmd, dr->linePipeline);
        
        struct { mat4 viewProj; } linePush = {viewProjMatrix};
        vePushConstants(cmd, &linePush, sizeof(linePush), 0);
        
        veDraw(cmd, dr->lineVertexBuffer, dr->lineCount * 2, 1, 0, 0);
    }
    
    // Render text (would need text rendering implementation)
    if (dr->textCount > 0) {
        // ... text rendering code
    }
    
    vePopDebugGroup(cmd);
    
    // Clear for next frame
    dr->lineCount = 0;
    dr->textCount = 0;
}
```

### Performance Monitoring

```c
// Performance monitoring system
typedef struct {
    uint64_t frameStartTime;
    uint64_t frameEndTime;
    
    float frameTimes[60];  // Last 60 frames
    uint32_t frameIndex;
    
    uint64_t totalFrames;
    uint64_t totalDrawCalls;
    uint64_t totalVertices;
    
    float avgFrameTime;
    float minFrameTime;
    float maxFrameTime;
} PerformanceMonitor;

void perfBeginFrame(PerformanceMonitor* pm) {
    pm->frameStartTime = getCurrentTimeNs();
}

void perfEndFrame(PerformanceMonitor* pm) {
    pm->frameEndTime = getCurrentTimeNs();
    
    float frameTime = (pm->frameEndTime - pm->frameStartTime) / 1000000.0f;  // Convert to ms
    pm->frameTimes[pm->frameIndex] = frameTime;
    pm->frameIndex = (pm->frameIndex + 1) % 60;
    pm->totalFrames++;
    
    // Update statistics
    pm->avgFrameTime = 0.0f;
    pm->minFrameTime = FLT_MAX;
    pm->maxFrameTime = 0.0f;
    
    for (int i = 0; i < 60; i++) {
        pm->avgFrameTime += pm->frameTimes[i];
        pm->minFrameTime = fminf(pm->minFrameTime, pm->frameTimes[i]);
        pm->maxFrameTime = fmaxf(pm->maxFrameTime, pm->frameTimes[i]);
    }
    pm->avgFrameTime /= 60.0f;
}

void perfPrintStats(PerformanceMonitor* pm) {
    printf("Performance Stats:\n");
    printf("  Current: %.2f ms (%.1f FPS)\n", 
           pm->frameTimes[(pm->frameIndex + 59) % 60], 
           1000.0f / pm->frameTimes[(pm->frameIndex + 59) % 60]);
    printf("  Average: %.2f ms (%.1f FPS)\n", pm->avgFrameTime, 1000.0f / pm->avgFrameTime);
    printf("  Min: %.2f ms (%.1f FPS)\n", pm->minFrameTime, 1000.0f / pm->minFrameTime);
    printf("  Max: %.2f ms (%.1f FPS)\n", pm->maxFrameTime, 1000.0f / pm->maxFrameTime);
    printf("  Total Frames: %llu\n", (unsigned long long)pm->totalFrames);
}
```

This covers a comprehensive set of best practices for VulkEase development, focusing on architecture, resource management, shader development, error handling, and debugging techniques.