/**
 * Example 1: Basic Triangle Rendering
 * Demonstrates basic VulkEase setup with GLFW window, vertex buffers, and simple rendering
 */

#include "vulkease.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Vertex structure
typedef struct {
    float position[3];
    float color[4];
} Vertex;

// Global state
static VEContext* g_context = NULL;
static VEDevice* g_device = NULL;
static VESwapchain* g_swapchain = NULL;
static VEShader* g_vertexShader = NULL;
static VEShader* g_fragmentShader = NULL;
static VERenderConfig* g_renderConfig = NULL;
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;

// Vertex data for a triangle
static const Vertex triangleVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // Bottom left - Red
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // Bottom right - Green
    {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}  // Top - Blue
};

static void errorCallback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\\n", error, description);
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if (g_swapchain && width > 0 && height > 0) {
        veResizeSwapchain(g_swapchain, width, height);
    }
}

static bool initGLFW() {
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\\n");
        return false;
    }
    
    // Don't create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    return true;
}

static bool initVulkEase(GLFWwindow* window) {
    // Create context
    g_context = veCreateContext("VulkEase Triangle Example");
    if (!g_context) {
        fprintf(stderr, "Failed to create VulkEase context: %s\\n", veGetLastError());
        return false;
    }
    
    printf("VulkEase Version: %u.%u.%u\\n", 
           VULKEASE_VERSION_MAJOR, VULKEASE_VERSION_MINOR, VULKEASE_VERSION_PATCH);
    
    // Create device
    g_device = veCreateDevice(g_context);
    if (!g_device) {
        fprintf(stderr, "Failed to create VulkEase device: %s\\n", veGetLastError());
        return false;
    }
    
    printf("Device: %s\\n", veGetDeviceName(g_device));
    printf("Driver: %s\\n", veGetDriverVersion(g_device));
    
    // Check required features
    if (!veSupportsBufferDeviceAddress(g_device)) {
        fprintf(stderr, "Buffer device address not supported\\n");
        return false;
    }
    
    if (!veSupportsShaderObjects(g_device)) {
        fprintf(stderr, "Shader objects not supported\\n");
        return false;
    }
    
    // Get window size for swapchain
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    
    // Create swapchain
    g_swapchain = veCreateSwapchain(g_device, glfwGetWin32Window(window), width, height, VE_FORMAT_BGRA8_SRGB);
    if (!g_swapchain) {
        fprintf(stderr, "Failed to create swapchain: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createShaders() {
    // Vertex shader SPIR-V (simple pass-through with color)
    const uint32_t vertexShaderCode[] = {
        // This would be actual SPIR-V bytecode
        // For this example, assume we have pre-compiled shaders
        0x07230203, // SPIR-V magic number
        // ... rest of SPIR-V bytecode
    };
    
    // Fragment shader SPIR-V (uses vertex color)
    const uint32_t fragmentShaderCode[] = {
        0x07230203, // SPIR-V magic number
        // ... rest of SPIR-V bytecode
    };
    
    // Load shaders from files (recommended approach)
    g_vertexShader = veLoadShader(g_device, "shaders/triangle.vert.spv", 
                                 VE_SHADER_STAGE_VERTEX, "main", "TriangleVertexShader");
    if (!g_vertexShader) {
        fprintf(stderr, "Failed to load vertex shader: %s\\n", veGetLastError());
        return false;
    }
    
    g_fragmentShader = veLoadShader(g_device, "shaders/triangle.frag.spv", 
                                   VE_SHADER_STAGE_FRAGMENT, "main", "TriangleFragmentShader");
    if (!g_fragmentShader) {
        fprintf(stderr, "Failed to load fragment shader: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static bool createBuffers() {
    // Create vertex buffer
    VEBufferDesc bufferDesc = {
        .size = sizeof(triangleVertices),
        .usage = VE_BUFFER_USAGE_VERTEX,
        .initialData = triangleVertices,
        .initialDataSize = sizeof(triangleVertices),
        .persistentlyMapped = false,
        .debugName = "TriangleVertexBuffer"
    };
    
    g_vertexBuffer = veCreateBuffer(g_device, &bufferDesc);
    if (g_vertexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create vertex buffer: %s\\n", veGetLastError());
        return false;
    }
    
    printf("Created vertex buffer at address: 0x%llx\\n", g_vertexBuffer);
    return true;
}

static bool createRenderConfig() {
    // Use default opaque render configuration
    g_renderConfig = veCreateOpaqueRenderConfig(g_device, "TriangleRenderConfig");
    if (!g_renderConfig) {
        fprintf(stderr, "Failed to create render config: %s\\n", veGetLastError());
        return false;
    }
    
    return true;
}

static void render() {
    // Acquire next swapchain image
    VETextureIndex backbuffer = veAcquireNextImage(g_swapchain);
    if (backbuffer == VE_INVALID_TEXTURE_INDEX) {
        // Swapchain needs recreation or other error
        return;
    }
    
    // Begin command buffer
    VECommandBuffer* cmd = veBeginCommandBuffer(g_device);
    if (!cmd) {
        fprintf(stderr, "Failed to begin command buffer\\n");
        return;
    }
    
    // Set up rendering info
    VERenderingAttachment colorAttachment = {
        .texture = backbuffer,
        .loadOp = VE_LOAD_OP_CLEAR,
        .storeOp = VE_STORE_OP_STORE,
        .clearValue = {0.1f, 0.1f, 0.1f, 1.0f}, // Dark gray background
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
    float debugColor[] = {0.0f, 0.8f, 1.0f, 1.0f};
    veBeginDebugRegion(cmd, "Triangle Rendering", debugColor);
    
    // Begin rendering
    veBeginRendering(cmd, &renderingInfo);
    
    // Set viewport
    veSetViewport(cmd, 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
    
    // Apply render configuration
    veApplyRenderConfig(cmd, g_renderConfig);
    
    // Bind shaders
    veBindShader(cmd, g_vertexShader);
    veBindShader(cmd, g_fragmentShader);
    
    // Set push constants with vertex buffer address
    typedef struct {
        VEBufferAddress vertexBuffer;
        float padding[3];
    } PushConstants;
    
    PushConstants pushConstants = {
        .vertexBuffer = g_vertexBuffer,
        .padding = {0.0f, 0.0f, 0.0f}
    };
    
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    
    // Draw triangle (3 vertices, no indices)
    veDraw(cmd, 3, 1, 0, 0);
    
    // End rendering
    veEndRendering(cmd);
    
    // End debug region
    veEndDebugRegion(cmd);
    
    // Present
    VEResult result = vePresentImage(g_swapchain, cmd);
    if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
        // Handle swapchain recreation
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
    if (!initGLFW()) {
        return -1;
    }
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "VulkEase Triangle", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\\n");
        glfwTerminate();
        return -1;
    }
    
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    if (!initVulkEase(window) || !createShaders() || !createBuffers() || !createRenderConfig()) {
        cleanup();
        glfwTerminate();
        return -1;
    }
    
    printf("Starting render loop...\\n");
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        render();
    }
    
    printf("Shutting down...\\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}