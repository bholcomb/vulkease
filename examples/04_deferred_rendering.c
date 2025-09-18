/**
 * Example 4: Multi-Pass Deferred Rendering
 * Demonstrates multiple render targets, depth buffers, and multi-pass rendering
 */

#include "vulkease.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_LIGHTS 32

// Vertex structure for geometry
typedef struct {
    float position[3];
    float normal[3];
    float texCoord[2];
} Vertex;

// Light structure
typedef struct {
    float position[3];
    float padding1;
    float color[3];
    float intensity;
    float attenuation[3]; // constant, linear, quadratic
    float padding2;
} Light;

// Uniform buffer structures
typedef struct {
    float model[16];
    float view[16];
    float projection[16];
    float normalMatrix[16];
} SceneUniforms;

typedef struct {
    Light lights[MAX_LIGHTS];
    uint32_t lightCount;
    float viewPos[3];
} LightUniforms;

// Global rendering state
static VEContext* g_context = NULL;
static VEDevice* g_device = NULL;
static VESwapchain* g_swapchain = NULL;

// G-Buffer textures (for deferred rendering)
static VETextureIndex g_gBufferAlbedo = VE_INVALID_TEXTURE_INDEX;    // RGB: albedo, A: metallic
static VETextureIndex g_gBufferNormal = VE_INVALID_TEXTURE_INDEX;    // RGB: world normal, A: roughness
static VETextureIndex g_gBufferDepth = VE_INVALID_TEXTURE_INDEX;     // Depth buffer

// Shaders for different passes
static VEShader* g_geometryVertShader = NULL;   // G-buffer fill vertex
static VEShader* g_geometryFragShader = NULL;   // G-buffer fill fragment
static VEShader* g_lightingVertShader = NULL;   // Lighting pass vertex
static VEShader* g_lightingFragShader = NULL;   // Lighting pass fragment

// Render configurations
static VERenderConfig* g_geometryConfig = NULL;  // G-buffer fill pass
static VERenderConfig* g_lightingConfig = NULL;  // Lighting pass

// Buffers
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;
static VEBufferAddress g_indexBuffer = VE_INVALID_ADDRESS;
static VEBufferAddress g_sceneUniformBuffer = VE_INVALID_ADDRESS;
static VEBufferAddress g_lightUniformBuffer = VE_INVALID_ADDRESS;

// Samplers
static VESamplerIndex g_linearSampler = VE_INVALID_SAMPLER_INDEX;

// Geometry data for a cube
static const Vertex cubeVertices[] = {
    // Front face
    {{-1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
    
    // Back face
    {{-1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
    {{-1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
    
    // Top face
    {{-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
    
    // Bottom face
    {{-1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
    {{-1.0f, -1.0f,  1.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
    
    // Right face
    {{ 1.0f, -1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    
    // Left face
    {{-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    {{-1.0f, -1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}
};

static const uint32_t cubeIndices[] = {
    0,  1,  2,    0,  2,  3,    // front
    4,  5,  6,    4,  6,  7,    // back
    8,  9, 10,    8, 10, 11,    // top
   12, 13, 14,   12, 14, 15,    // bottom
   16, 17, 18,   16, 18, 19,    // right
   20, 21, 22,   20, 22, 23     // left
};

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if (g_swapchain && width > 0 && height > 0) {
        veResizeSwapchain(g_swapchain, width, height);
        
        // Recreate G-buffer textures with new size
        if (g_gBufferAlbedo != VE_INVALID_TEXTURE_INDEX) {
            veDestroyTexture(g_device, g_gBufferAlbedo);
            veDestroyTexture(g_device, g_gBufferNormal);
            veDestroyTexture(g_device, g_gBufferDepth);
            
            // Recreate with new size
            g_gBufferAlbedo = veCreateTexture2D(g_device, width, height, VE_FORMAT_RGBA8_UNORM, 
                                               VE_TEXTURE_USAGE_COLOR_ATTACHMENT | VE_TEXTURE_USAGE_SAMPLED, 
                                               "GBufferAlbedo");
            g_gBufferNormal = veCreateTexture2D(g_device, width, height, VE_FORMAT_RGBA16_SFLOAT, 
                                               VE_TEXTURE_USAGE_COLOR_ATTACHMENT | VE_TEXTURE_USAGE_SAMPLED, 
                                               "GBufferNormal");
            g_gBufferDepth = veCreateTexture2D(g_device, width, height, VE_FORMAT_D32_SFLOAT, 
                                              VE_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 
                                              "GBufferDepth");
        }
    }
}

static bool initVulkEase(GLFWwindow* window) {
    g_context = veCreateContext("VulkEase Deferred Rendering");
    if (!g_context) {
        fprintf(stderr, "Failed to create context: %s\\n", veGetLastError());
        return false;
    }
    
    g_device = veCreateDevice(g_context);
    if (!g_device) {
        fprintf(stderr, "Failed to create device: %s\\n", veGetLastError());
        return false;
    }
    
    printf("Device: %s\\n", veGetDeviceName(g_device));
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    g_swapchain = veCreateSwapchain(g_device, glfwGetWin32Window(window), width, height, VE_FORMAT_BGRA8_SRGB);
    if (!g_swapchain) {
        fprintf(stderr, "Failed to create swapchain: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createGBuffer(uint32_t width, uint32_t height) {
    // Create G-buffer textures
    g_gBufferAlbedo = veCreateTexture2D(g_device, width, height, VE_FORMAT_RGBA8_UNORM, 
                                       VE_TEXTURE_USAGE_COLOR_ATTACHMENT | VE_TEXTURE_USAGE_SAMPLED, 
                                       "GBufferAlbedo");
    if (g_gBufferAlbedo == VE_INVALID_TEXTURE_INDEX) {
        fprintf(stderr, "Failed to create G-buffer albedo texture: %s\\n", veGetLastError());
        return false;
    }
    
    g_gBufferNormal = veCreateTexture2D(g_device, width, height, VE_FORMAT_RGBA16_SFLOAT, 
                                       VE_TEXTURE_USAGE_COLOR_ATTACHMENT | VE_TEXTURE_USAGE_SAMPLED, 
                                       "GBufferNormal");
    if (g_gBufferNormal == VE_INVALID_TEXTURE_INDEX) {
        fprintf(stderr, "Failed to create G-buffer normal texture: %s\\n", veGetLastError());
        return false;
    }
    
    g_gBufferDepth = veCreateTexture2D(g_device, width, height, VE_FORMAT_D32_SFLOAT, 
                                      VE_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT, 
                                      "GBufferDepth");
    if (g_gBufferDepth == VE_INVALID_TEXTURE_INDEX) {
        fprintf(stderr, "Failed to create G-buffer depth texture: %s\\n", veGetLastError());
        return false;
    }
    
    printf("Created G-buffer textures: Albedo=%u, Normal=%u, Depth=%u\\n", 
           g_gBufferAlbedo, g_gBufferNormal, g_gBufferDepth);
    
    return true;
}

static bool createShaders() {
    // Load geometry pass shaders
    g_geometryVertShader = veLoadShader(g_device, "shaders/deferred_geometry.vert.spv", 
                                       VE_SHADER_STAGE_VERTEX, "main", "GeometryVertexShader");
    if (!g_geometryVertShader) {
        fprintf(stderr, "Failed to load geometry vertex shader: %s\\n", veGetLastError());
        return false;
    }
    
    g_geometryFragShader = veLoadShader(g_device, "shaders/deferred_geometry.frag.spv", 
                                       VE_SHADER_STAGE_FRAGMENT, "main", "GeometryFragmentShader");
    if (!g_geometryFragShader) {
        fprintf(stderr, "Failed to load geometry fragment shader: %s\\n", veGetLastError());
        return false;
    }
    
    // Load lighting pass shaders  
    g_lightingVertShader = veLoadShader(g_device, "shaders/deferred_lighting.vert.spv", 
                                       VE_SHADER_STAGE_VERTEX, "main", "LightingVertexShader");
    if (!g_lightingVertShader) {
        fprintf(stderr, "Failed to load lighting vertex shader: %s\\n", veGetLastError());
        return false;
    }
    
    g_lightingFragShader = veLoadShader(g_device, "shaders/deferred_lighting.frag.spv", 
                                       VE_SHADER_STAGE_FRAGMENT, "main", "LightingFragmentShader");
    if (!g_lightingFragShader) {
        fprintf(stderr, "Failed to load lighting fragment shader: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createBuffers() {
    // Create vertex buffer
    VEBufferDesc vertexBufferDesc = {
        .size = sizeof(cubeVertices),
        .usage = VE_BUFFER_USAGE_VERTEX,
        .initialData = cubeVertices,
        .initialDataSize = sizeof(cubeVertices),
        .persistentlyMapped = false,
        .debugName = "CubeVertexBuffer"
    };
    
    g_vertexBuffer = veCreateBuffer(g_device, &vertexBufferDesc);
    if (g_vertexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create vertex buffer: %s\\n", veGetLastError());
        return false;
    }
    
    // Create index buffer
    VEBufferDesc indexBufferDesc = {
        .size = sizeof(cubeIndices),
        .usage = VE_BUFFER_USAGE_INDEX,
        .initialData = cubeIndices,
        .initialDataSize = sizeof(cubeIndices),
        .persistentlyMapped = false,
        .debugName = "CubeIndexBuffer"
    };
    
    g_indexBuffer = veCreateBuffer(g_device, &indexBufferDesc);
    if (g_indexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create index buffer: %s\\n", veGetLastError());
        return false;
    }
    
    // Create uniform buffers
    VEBufferDesc sceneUniformDesc = {
        .size = sizeof(SceneUniforms),
        .usage = VE_BUFFER_USAGE_UNIFORM,
        .initialData = NULL,
        .initialDataSize = 0,
        .persistentlyMapped = true,
        .debugName = "SceneUniformBuffer"
    };
    
    g_sceneUniformBuffer = veCreateBuffer(g_device, &sceneUniformDesc);
    if (g_sceneUniformBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create scene uniform buffer: %s\\n", veGetLastError());
        return false;
    }
    
    VEBufferDesc lightUniformDesc = {
        .size = sizeof(LightUniforms),
        .usage = VE_BUFFER_USAGE_UNIFORM,
        .initialData = NULL,
        .initialDataSize = 0,
        .persistentlyMapped = true,
        .debugName = "LightUniformBuffer"
    };
    
    g_lightUniformBuffer = veCreateBuffer(g_device, &lightUniformDesc);
    if (g_lightUniformBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create light uniform buffer: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createRenderConfigs() {
    // Geometry pass configuration (writes to G-buffer)
    g_geometryConfig = veCreateOpaqueRenderConfig(g_device, "GeometryPassConfig");
    if (!g_geometryConfig) {
        fprintf(stderr, "Failed to create geometry render config: %s\\n", veGetLastError());
        return false;
    }
    
    // Lighting pass configuration (no depth test, additive blending)
    g_lightingConfig = veCreateTransparentRenderConfig(g_device, "LightingPassConfig");
    if (!g_lightingConfig) {
        fprintf(stderr, "Failed to create lighting render config: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createSamplers() {
    g_linearSampler = veCreateLinearSampler(g_device);
    if (g_linearSampler == VE_INVALID_SAMPLER_INDEX) {
        fprintf(stderr, "Failed to create linear sampler: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static void identityMatrix(float matrix[16]) {
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
}

static void updateSceneUniforms(float time) {
    void* mappedData;
    VEResult result = veMapBuffer(g_device, g_sceneUniformBuffer, &mappedData);
    if (result != VE_SUCCESS) return;
    
    SceneUniforms* uniforms = (SceneUniforms*)mappedData;
    
    // Set up matrices (simplified for example)
    identityMatrix(uniforms->model);
    identityMatrix(uniforms->view);
    identityMatrix(uniforms->projection);
    identityMatrix(uniforms->normalMatrix);
    
    // Rotate model
    uniforms->model[0] = cosf(time);
    uniforms->model[2] = sinf(time);
    uniforms->model[8] = -sinf(time);
    uniforms->model[10] = cosf(time);
}

static void updateLightUniforms() {
    void* mappedData;
    VEResult result = veMapBuffer(g_device, g_lightUniformBuffer, &mappedData);
    if (result != VE_SUCCESS) return;
    
    LightUniforms* uniforms = (LightUniforms*)mappedData;
    
    // Set up a simple light
    uniforms->lightCount = 1;
    uniforms->lights[0].position[0] = 2.0f;
    uniforms->lights[0].position[1] = 2.0f;
    uniforms->lights[0].position[2] = 2.0f;
    uniforms->lights[0].color[0] = 1.0f;
    uniforms->lights[0].color[1] = 1.0f;
    uniforms->lights[0].color[2] = 1.0f;
    uniforms->lights[0].intensity = 1.0f;
    uniforms->lights[0].attenuation[0] = 1.0f;
    uniforms->lights[0].attenuation[1] = 0.1f;
    uniforms->lights[0].attenuation[2] = 0.01f;
    
    uniforms->viewPos[0] = 0.0f;
    uniforms->viewPos[1] = 0.0f;
    uniforms->viewPos[2] = 3.0f;
}

static void geometryPass(VECommandBuffer* cmd) {
    // Set up G-buffer rendering
    VERenderingAttachment colorAttachments[2] = {
        {
            .texture = g_gBufferAlbedo,
            .loadOp = VE_LOAD_OP_CLEAR,
            .storeOp = VE_STORE_OP_STORE,
            .clearValue = {0.0f, 0.0f, 0.0f, 0.0f},
            .resolveTexture = VE_INVALID_TEXTURE_INDEX
        },
        {
            .texture = g_gBufferNormal,
            .loadOp = VE_LOAD_OP_CLEAR,
            .storeOp = VE_STORE_OP_STORE,
            .clearValue = {0.0f, 0.0f, 0.0f, 0.0f},
            .resolveTexture = VE_INVALID_TEXTURE_INDEX
        }
    };
    
    VERenderingAttachment depthAttachment = {
        .texture = g_gBufferDepth,
        .loadOp = VE_LOAD_OP_CLEAR,
        .storeOp = VE_STORE_OP_STORE,
        .clearValue = {1.0f, 0.0f, 0.0f, 0.0f},
        .resolveTexture = VE_INVALID_TEXTURE_INDEX
    };
    
    uint32_t width, height;
    veGetSwapchainSize(g_swapchain, &width, &height);
    
    VERenderingInfo renderingInfo = {
        .renderAreaX = 0,
        .renderAreaY = 0,
        .renderAreaWidth = width,
        .renderAreaHeight = height,
        .colorAttachmentCount = 2,
        .colorAttachments = colorAttachments,
        .depthAttachment = &depthAttachment,
        .stencilAttachment = NULL
    };
    
    float debugColor[] = {0.8f, 0.2f, 0.2f, 1.0f};
    veBeginDebugRegion(cmd, "Geometry Pass", debugColor);
    
    veBeginRendering(cmd, &renderingInfo);
    
    veSetViewport(cmd, 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
    veApplyRenderConfig(cmd, g_geometryConfig);
    
    veBindShader(cmd, g_geometryVertShader);
    veBindShader(cmd, g_geometryFragShader);
    
    // Push constants for geometry pass
    typedef struct {
        VEBufferAddress vertexBuffer;
        VEBufferAddress indexBuffer;
        VEBufferAddress sceneUniforms;
        uint32_t padding;
    } GeometryPushConstants;
    
    GeometryPushConstants pushConstants = {
        .vertexBuffer = g_vertexBuffer,
        .indexBuffer = g_indexBuffer,
        .sceneUniforms = g_sceneUniformBuffer,
        .padding = 0
    };
    
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    
    // Draw cube
    veDrawIndexed(cmd, sizeof(cubeIndices) / sizeof(uint32_t), 1, 0, 0, 0);
    
    veEndRendering(cmd);
    veEndDebugRegion(cmd);
}

static void lightingPass(VECommandBuffer* cmd, VETextureIndex backbuffer) {
    // Set up lighting pass rendering to backbuffer
    VERenderingAttachment colorAttachment = {
        .texture = backbuffer,
        .loadOp = VE_LOAD_OP_CLEAR,
        .storeOp = VE_STORE_OP_STORE,
        .clearValue = {0.0f, 0.0f, 0.0f, 1.0f},
        .resolveTexture = VE_INVALID_TEXTURE_INDEX
    };
    
    uint32_t width, height;
    veGetSwapchainSize(g_swapchain, &width, &height);
    
    VERenderingInfo renderingInfo = {
        .renderAreaX = 0,
        .renderAreaY = 0,
        .renderAreaWidth = width,
        .renderAreaHeight = height,
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
        .depthAttachment = NULL,
        .stencilAttachment = NULL
    };
    
    float debugColor[] = {0.2f, 0.8f, 0.2f, 1.0f};
    veBeginDebugRegion(cmd, "Lighting Pass", debugColor);
    
    veBeginRendering(cmd, &renderingInfo);
    
    veSetViewport(cmd, 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
    veApplyRenderConfig(cmd, g_lightingConfig);
    
    veBindShader(cmd, g_lightingVertShader);
    veBindShader(cmd, g_lightingFragShader);
    
    // Push constants for lighting pass
    typedef struct {
        VEBufferAddress lightUniforms;
        VETextureIndex gBufferAlbedo;
        VETextureIndex gBufferNormal;
        VETextureIndex gBufferDepth;
        VESamplerIndex sampler;
        float screenSize[2];
        uint32_t padding;
    } LightingPushConstants;
    
    LightingPushConstants pushConstants = {
        .lightUniforms = g_lightUniformBuffer,
        .gBufferAlbedo = g_gBufferAlbedo,
        .gBufferNormal = g_gBufferNormal,
        .gBufferDepth = g_gBufferDepth,
        .sampler = g_linearSampler,
        .screenSize = {(float)width, (float)height},
        .padding = 0
    };
    
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    
    // Draw fullscreen quad (3 vertices for triangle trick)
    veDraw(cmd, 3, 1, 0, 0);
    
    veEndRendering(cmd);
    veEndDebugRegion(cmd);
}

static void render(float time) {
    updateSceneUniforms(time);
    updateLightUniforms();
    
    VETextureIndex backbuffer = veAcquireNextImage(g_swapchain);
    if (backbuffer == VE_INVALID_TEXTURE_INDEX) {
        return;
    }
    
    VECommandBuffer* cmd = veBeginCommandBuffer(g_device);
    if (!cmd) {
        return;
    }
    
    // Execute deferred rendering passes
    geometryPass(cmd);
    lightingPass(cmd, backbuffer);
    
    VEResult result = vePresentImage(g_swapchain, cmd);
    if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
        uint32_t width, height;
        veGetSwapchainSize(g_swapchain, &width, &height);
        veResizeSwapchain(g_swapchain, width, height);
    }
}

static void cleanup() {
    if (g_device) {
        veDeviceWaitIdle(g_device);
    }
    
    // Clean up resources
    if (g_vertexBuffer != VE_INVALID_ADDRESS) veDestroyBuffer(g_device, g_vertexBuffer);
    if (g_indexBuffer != VE_INVALID_ADDRESS) veDestroyBuffer(g_device, g_indexBuffer);
    if (g_sceneUniformBuffer != VE_INVALID_ADDRESS) veDestroyBuffer(g_device, g_sceneUniformBuffer);
    if (g_lightUniformBuffer != VE_INVALID_ADDRESS) veDestroyBuffer(g_device, g_lightUniformBuffer);
    
    if (g_gBufferAlbedo != VE_INVALID_TEXTURE_INDEX) veDestroyTexture(g_device, g_gBufferAlbedo);
    if (g_gBufferNormal != VE_INVALID_TEXTURE_INDEX) veDestroyTexture(g_device, g_gBufferNormal);
    if (g_gBufferDepth != VE_INVALID_TEXTURE_INDEX) veDestroyTexture(g_device, g_gBufferDepth);
    
    if (g_linearSampler != VE_INVALID_SAMPLER_INDEX) veDestroySampler(g_device, g_linearSampler);
    
    if (g_geometryConfig) veDestroyRenderConfig(g_geometryConfig);
    if (g_lightingConfig) veDestroyRenderConfig(g_lightingConfig);
    
    if (g_geometryVertShader) veDestroyShader(g_geometryVertShader);
    if (g_geometryFragShader) veDestroyShader(g_geometryFragShader);
    if (g_lightingVertShader) veDestroyShader(g_lightingVertShader);
    if (g_lightingFragShader) veDestroyShader(g_lightingFragShader);
    
    if (g_swapchain) veDestroySwapchain(g_swapchain);
    if (g_device) veDestroyDevice(g_device);
    if (g_context) veDestroyContext(g_context);
}

int main() {
    if (!glfwInit()) {
        return -1;
    }
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "VulkEase Deferred Rendering", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    if (!initVulkEase(window)) {
        cleanup();
        glfwTerminate();
        return -1;
    }
    
    uint32_t width, height;
    veGetSwapchainSize(g_swapchain, &width, &height);
    
    if (!createGBuffer(width, height) || !createShaders() || !createBuffers() || 
        !createRenderConfigs() || !createSamplers()) {
        cleanup();
        glfwTerminate();
        return -1;
    }
    
    printf("Starting deferred rendering demo...\\n");
    printf("G-buffer: %ux%u\\n", width, height);
    
    double startTime = glfwGetTime();
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        double currentTime = glfwGetTime();
        float time = (float)(currentTime - startTime);
        
        render(time);
        veUpdateFrameStats(g_device);
    }
    
    printf("Shutting down...\\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}