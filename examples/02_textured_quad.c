/**
 * Example 2: Textured Quad with Index Buffer
 * Demonstrates indexed rendering, texture loading, and uniform buffers
 */

#include "vulkease.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Vertex structure with texture coordinates
typedef struct {
    float position[3];
    float texCoord[2];
} Vertex;

// Matrix structure for uniforms
typedef struct {
    float model[16];
    float view[16];
    float projection[16];
} Matrices;

// Global state
static VEContext* g_context = NULL;
static VEDevice* g_device = NULL;
static VESwapchain* g_swapchain = NULL;
static VEShader* g_vertexShader = NULL;
static VEShader* g_fragmentShader = NULL;
static VERenderConfig* g_renderConfig = NULL;
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;
static VEBufferAddress g_indexBuffer = VE_INVALID_ADDRESS;
static VEBufferAddress g_uniformBuffer = VE_INVALID_ADDRESS;
static VETextureIndex g_texture = VE_INVALID_TEXTURE_INDEX;
static VESamplerIndex g_sampler = VE_INVALID_SAMPLER_INDEX;

// Quad vertex data (with texture coordinates)
static const Vertex quadVertices[] = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // Bottom left
    {{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // Bottom right
    {{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}}, // Top right
    {{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}}  // Top left
};

// Index data for quad (two triangles)
static const uint32_t quadIndices[] = {
    0, 1, 2,  // First triangle
    2, 3, 0   // Second triangle
};

// Matrix helper functions
static void identityMatrix(float matrix[16]) {
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
}

static void perspectiveMatrix(float matrix[16], float fov, float aspect, float near, float far) {
    memset(matrix, 0, sizeof(float) * 16);
    
    float f = 1.0f / tanf(fov * 0.5f);
    matrix[0] = f / aspect;
    matrix[5] = f;
    matrix[10] = (far + near) / (near - far);
    matrix[11] = -1.0f;
    matrix[14] = (2.0f * far * near) / (near - far);
}

static void rotationMatrix(float matrix[16], float angle, float x, float y, float z) {
    identityMatrix(matrix);
    
    float c = cosf(angle);
    float s = sinf(angle);
    float t = 1.0f - c;
    
    matrix[0] = c + x * x * t;
    matrix[1] = x * y * t + z * s;
    matrix[2] = x * z * t - y * s;
    
    matrix[4] = x * y * t - z * s;
    matrix[5] = c + y * y * t;
    matrix[6] = y * z * t + x * s;
    
    matrix[8] = x * z * t + y * s;
    matrix[9] = y * z * t - x * s;
    matrix[10] = c + z * z * t;
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if (g_swapchain && width > 0 && height > 0) {
        veResizeSwapchain(g_swapchain, width, height);
    }
}

static bool initVulkEase(GLFWwindow* window) {
    // Create context
    g_context = veCreateContext("VulkEase Textured Quad Example");
    if (!g_context) {
        fprintf(stderr, "Failed to create VulkEase context: %s\\n", veGetLastError());
        return false;
    }
    
    // Create device
    g_device = veCreateDevice(g_context);
    if (!g_device) {
        fprintf(stderr, "Failed to create VulkEase device: %s\\n", veGetLastError());
        return false;
    }
    
    printf("Device: %s\\n", veGetDeviceName(g_device));
    
    // Create swapchain
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    g_swapchain = veCreateSwapchain(g_device, glfwGetWin32Window(window), width, height, VE_FORMAT_BGRA8_SRGB);
    if (!g_swapchain) {
        fprintf(stderr, "Failed to create swapchain: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createShaders() {
    g_vertexShader = veLoadShader(g_device, "shaders/textured_quad.vert.spv", 
                                 VE_SHADER_STAGE_VERTEX, "main", "TexturedQuadVertexShader");
    if (!g_vertexShader) {
        fprintf(stderr, "Failed to load vertex shader: %s\\n", veGetLastError());
        return false;
    }
    
    g_fragmentShader = veLoadShader(g_device, "shaders/textured_quad.frag.spv", 
                                   VE_SHADER_STAGE_FRAGMENT, "main", "TexturedQuadFragmentShader");
    if (!g_fragmentShader) {
        fprintf(stderr, "Failed to load fragment shader: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createBuffers() {
    // Create vertex buffer
    VEBufferDesc vertexBufferDesc = {
        .size = sizeof(quadVertices),
        .usage = VE_BUFFER_USAGE_VERTEX,
        .initialData = quadVertices,
        .initialDataSize = sizeof(quadVertices),
        .persistentlyMapped = false,
        .debugName = "QuadVertexBuffer"
    };
    
    g_vertexBuffer = veCreateBuffer(g_device, &vertexBufferDesc);
    if (g_vertexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create vertex buffer: %s\\n", veGetLastError());
        return false;
    }
    
    // Create index buffer
    VEBufferDesc indexBufferDesc = {
        .size = sizeof(quadIndices),
        .usage = VE_BUFFER_USAGE_INDEX,
        .initialData = quadIndices,
        .initialDataSize = sizeof(quadIndices),
        .persistentlyMapped = false,
        .debugName = "QuadIndexBuffer"
    };
    
    g_indexBuffer = veCreateBuffer(g_device, &indexBufferDesc);
    if (g_indexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create index buffer: %s\\n", veGetLastError());
        return false;
    }
    
    // Create uniform buffer
    VEBufferDesc uniformBufferDesc = {
        .size = sizeof(Matrices),
        .usage = VE_BUFFER_USAGE_UNIFORM,
        .initialData = NULL,
        .initialDataSize = 0,
        .persistentlyMapped = true, // Map persistently for frequent updates
        .debugName = "MatricesUniformBuffer"
    };
    
    g_uniformBuffer = veCreateBuffer(g_device, &uniformBufferDesc);
    if (g_uniformBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create uniform buffer: %s\\n", veGetLastError());
        return false;
    }
    
    printf("Created buffers: VB=0x%llx, IB=0x%llx, UB=0x%llx\\n", 
           g_vertexBuffer, g_indexBuffer, g_uniformBuffer);
    
    return true;
}

static bool createTexture() {
    // Load texture from file
    g_texture = veLoadTexture(g_device, "textures/sample.jpg", VE_TEXTURE_USAGE_SAMPLED, true);
    if (g_texture == VE_INVALID_TEXTURE_INDEX) {
        fprintf(stderr, "Failed to load texture: %s\\n", veGetLastError());
        
        // Create a simple procedural texture as fallback
        const uint32_t textureWidth = 256;
        const uint32_t textureHeight = 256;
        uint32_t* textureData = malloc(textureWidth * textureHeight * sizeof(uint32_t));
        
        // Generate a checkerboard pattern
        for (uint32_t y = 0; y < textureHeight; y++) {
            for (uint32_t x = 0; x < textureWidth; x++) {
                bool checker = ((x / 32) % 2) ^ ((y / 32) % 2);
                uint32_t color = checker ? 0xFFFFFFFF : 0xFF808080;
                textureData[y * textureWidth + x] = color;
            }
        }
        
        VETextureDesc textureDesc = {
            .width = textureWidth,
            .height = textureHeight,
            .depth = 1,
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = VE_FORMAT_RGBA8_UNORM,
            .usage = VE_TEXTURE_USAGE_SAMPLED,
            .sampleCount = VE_SAMPLE_COUNT_1,
            .initialData = textureData,
            .initialDataSize = textureWidth * textureHeight * sizeof(uint32_t),
            .debugName = "ProceduralTexture"
        };
        
        g_texture = veCreateTexture(g_device, &textureDesc);
        free(textureData);
        
        if (g_texture == VE_INVALID_TEXTURE_INDEX) {
            fprintf(stderr, "Failed to create fallback texture: %s\\n", veGetLastError());
            return false;
        }
        
        printf("Created procedural checkerboard texture\\n");
    } else {
        printf("Loaded texture from file\\n");
    }
    
    // Create sampler
    VESamplerDesc samplerDesc = {
        .minFilter = VE_FILTER_LINEAR,
        .magFilter = VE_FILTER_LINEAR,
        .mipmapFilter = VE_FILTER_LINEAR,
        .addressModeU = VE_ADDRESS_MODE_REPEAT,
        .addressModeV = VE_ADDRESS_MODE_REPEAT,
        .addressModeW = VE_ADDRESS_MODE_REPEAT,
        .maxAnisotropy = 4.0f,
        .compareEnable = false,
        .compareOp = VE_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 1000.0f,
        .debugName = "LinearSampler"
    };
    
    g_sampler = veCreateSampler(g_device, &samplerDesc);
    if (g_sampler == VE_INVALID_SAMPLER_INDEX) {
        fprintf(stderr, "Failed to create sampler: %s\\n", veGetLastError());
        return false;
    }
    
    printf("Created texture (index %u) and sampler (index %u)\\n", g_texture, g_sampler);
    return true;
}

static bool createRenderConfig() {
    g_renderConfig = veCreateOpaqueRenderConfig(g_device, "QuadRenderConfig");
    if (!g_renderConfig) {
        fprintf(stderr, "Failed to create render config: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static void updateUniforms(float time) {
    // Map the uniform buffer
    void* mappedData;
    VEResult result = veMapBuffer(g_device, g_uniformBuffer, &mappedData);
    if (result != VE_SUCCESS) {
        fprintf(stderr, "Failed to map uniform buffer\\n");
        return;
    }
    
    Matrices* matrices = (Matrices*)mappedData;
    
    // Create transformation matrices
    rotationMatrix(matrices->model, time, 0.0f, 0.0f, 1.0f); // Rotate around Z-axis
    identityMatrix(matrices->view); // No view transformation
    
    uint32_t width, height;
    veGetSwapchainSize(g_swapchain, &width, &height);
    float aspect = (float)width / (float)height;
    perspectiveMatrix(matrices->projection, 45.0f * M_PI / 180.0f, aspect, 0.1f, 100.0f);
    
    // No need to unmap if persistently mapped
}

static void render(float time) {
    // Update uniform buffer
    updateUniforms(time);
    
    // Acquire next swapchain image
    VETextureIndex backbuffer = veAcquireNextImage(g_swapchain);
    if (backbuffer == VE_INVALID_TEXTURE_INDEX) {
        return;
    }
    
    // Begin command buffer
    VECommandBuffer* cmd = veBeginCommandBuffer(g_device);
    if (!cmd) {
        fprintf(stderr, "Failed to begin command buffer\\n");
        return;
    }
    
    // Set up rendering
    VERenderingAttachment colorAttachment = {
        .texture = backbuffer,
        .loadOp = VE_LOAD_OP_CLEAR,
        .storeOp = VE_STORE_OP_STORE,
        .clearValue = {0.2f, 0.3f, 0.4f, 1.0f}, // Blue-gray background
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
    
    // Begin debug region
    float debugColor[] = {1.0f, 0.5f, 0.0f, 1.0f};
    veBeginDebugRegion(cmd, "Textured Quad Rendering", debugColor);
    
    // Begin rendering
    veBeginRendering(cmd, &renderingInfo);
    
    // Set viewport
    veSetViewport(cmd, 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
    
    // Apply render configuration
    veApplyRenderConfig(cmd, g_renderConfig);
    
    // Bind shaders
    veBindShader(cmd, g_vertexShader);
    veBindShader(cmd, g_fragmentShader);
    
    // Set push constants with buffer addresses and texture indices
    typedef struct {
        VEBufferAddress vertexBuffer;
        VEBufferAddress indexBuffer;
        VEBufferAddress uniformBuffer;
        VETextureIndex diffuseTexture;
        VESamplerIndex sampler;
        float padding[3];
    } PushConstants;
    
    PushConstants pushConstants = {
        .vertexBuffer = g_vertexBuffer,
        .indexBuffer = g_indexBuffer,
        .uniformBuffer = g_uniformBuffer,
        .diffuseTexture = g_texture,
        .sampler = g_sampler,
        .padding = {0.0f, 0.0f, 0.0f}
    };
    
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    
    // Draw indexed quad (6 indices)
    veDrawIndexed(cmd, 6, 1, 0, 0, 0);
    
    // End rendering
    veEndRendering(cmd);
    
    // End debug region
    veEndDebugRegion(cmd);
    
    // Present
    VEResult result = vePresentImage(g_swapchain, cmd);
    if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
        veResizeSwapchain(g_swapchain, width, height);
    }
}

static void cleanup() {
    if (g_device) {
        veDeviceWaitIdle(g_device);
    }
    
    if (g_vertexBuffer != VE_INVALID_ADDRESS) {
        veDestroyBuffer(g_device, g_vertexBuffer);
    }
    
    if (g_indexBuffer != VE_INVALID_ADDRESS) {
        veDestroyBuffer(g_device, g_indexBuffer);
    }
    
    if (g_uniformBuffer != VE_INVALID_ADDRESS) {
        veDestroyBuffer(g_device, g_uniformBuffer);
    }
    
    if (g_texture != VE_INVALID_TEXTURE_INDEX) {
        veDestroyTexture(g_device, g_texture);
    }
    
    if (g_sampler != VE_INVALID_SAMPLER_INDEX) {
        veDestroySampler(g_device, g_sampler);
    }
    
    if (g_renderConfig) {
        veDestroyRenderConfig(g_renderConfig);
    }
    
    if (g_vertexShader) {
        veDestroyShader(g_vertexShader);
    }
    
    if (g_fragmentShader) {
        veDestroyShader(g_fragmentShader);
    }
    
    if (g_swapchain) {
        veDestroySwapchain(g_swapchain);
    }
    
    if (g_device) {
        veDestroyDevice(g_device);
    }
    
    if (g_context) {
        veDestroyContext(g_context);
    }
}

int main() {
    if (!glfwInit()) {
        return -1;
    }
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "VulkEase Textured Quad", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    if (!initVulkEase(window) || !createShaders() || !createBuffers() || 
        !createTexture() || !createRenderConfig()) {
        cleanup();
        glfwTerminate();
        return -1;
    }
    
    printf("Starting render loop...\\n");
    
    double startTime = glfwGetTime();
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        double currentTime = glfwGetTime();
        float time = (float)(currentTime - startTime);
        
        render(time);
    }
    
    printf("Shutting down...\\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}