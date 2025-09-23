/**
 * Example 1: Spinning Triangle
 * Demonstrates basic VulkEase setup with GLFW window, vertex buffers, and animated rendering
 * Features embedded GLSL shaders and a spinning colorful triangle
 */

#include "vulkease.h"
#include <GLFW/glfw3.h>
#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Vertex structure
typedef struct {
    float position[3];
    float color[4];
} Vertex;

// Shader file paths (compiled SPIR-V)
#define VERTEX_SHADER_PATH "examples/shaders/triangle.vert.spv"
#define FRAGMENT_SHADER_PATH "examples/shaders/triangle.frag.spv"

// Global state
static VEContext* g_context = NULL;
static VEDevice* g_device = NULL;
static VESwapchain* g_swapchain = NULL;
static VEShader* g_vertexShader = NULL;
static VEShader* g_fragmentShader = NULL;
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;
static VERenderConfig* g_renderConfig = NULL;

// Animation state
static double g_startTime = 0.0;

// Vertex data for a triangle
static const Vertex triangleVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // Bottom left - Red
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // Bottom right - Green
    {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}  // Top - Blue
};

static void errorCallback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if(!window)
    {
        return;
    }
    
    if (g_swapchain && width > 0 && height > 0) {
        veResizeSwapchain(g_swapchain, width, height);
    }
}

static bool initGLFW() {
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    
    // Don't create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    return true;
}

static bool initVulkEase(GLFWwindow* window) {
    // Create context
    g_context = veCreateContext("VulkEase Spinning Triangle");
    if (!g_context) {
        fprintf(stderr, "Failed to create VulkEase context: %s\n", veGetLastError());
        return false;
    }
    
    printf("VulkEase initialized successfully\n");
    
    // Create device
    g_device = veCreateDevice(g_context);
    if (!g_device) {
        fprintf(stderr, "Failed to create VulkEase device: %s\n", veGetLastError());
        return false;
    }
    
    printf("VulkEase device created successfully\n");
    
    // Get window size for swapchain
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    
    // Get native window handle for VulkEase's simple approach
    void* displayHandle = NULL;
    void* windowHandle = NULL;
    
#if defined(_WIN32)
    windowHandle = glfwGetWin32Window(window);
    if (!windowHandle) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }

    g_swapchain = veCreateSwapchain(g_device, windowHandle, width, height, VE_FORMAT_BGRA8_SRGB);
#elif defined(__linux__)
    // For simplicity, assume X11 for now
    // In production, you'd detect the platform properly
    displayHandle = (void*)glfwGetX11Display();
    windowHandle = (void*)glfwGetX11Window(window);
    
    if (!displayHandle || !windowHandle) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }

    void* windowData[] = {displayHandle, windowHandle};

    // Create swapchain using VulkEase's simple window handle approach
    g_swapchain = veCreateSwapchain(g_device, windowData, width, height, VE_FORMAT_BGRA8_SRGB);

#elif defined(__APPLE__)
    windowHandle = glfwGetCocoaWindow(window);
    if (!windowHandle) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }
    g_swapchain = veCreateSwapchain(g_device, windowHandle, width, height, VE_FORMAT_BGRA8_SRGB);
#endif    
    
    if (!g_swapchain) {
        fprintf(stderr, "Failed to create swapchain: %s\n", veGetLastError());
        return false;
    }
    
    printf("Swapchain created: %dx%d\n", width, height);
    
    // Initialize animation timer
    g_startTime = glfwGetTime();
    
    return true;
}

static bool createShaders() {
    // Load vertex shader from compiled SPIR-V file
    g_vertexShader = veLoadShader(g_device, VERTEX_SHADER_PATH, VE_SHADER_STAGE_VERTEX,
                                 "main", "SpinningTriangleVertex");
    if (!g_vertexShader) {
        fprintf(stderr, "Failed to load vertex shader: %s\n", veGetLastError());
        return false;
    }
    
    // Load fragment shader from compiled SPIR-V file
    g_fragmentShader = veLoadShader(g_device, FRAGMENT_SHADER_PATH, VE_SHADER_STAGE_FRAGMENT,
                                   "main", "SpinningTriangleFragment");
    if (!g_fragmentShader) {
        fprintf(stderr, "Failed to load fragment shader: %s\n", veGetLastError());
        return false;
    }

    //create a default render config
    g_renderConfig = veCreateOpaqueRenderConfig(g_device, "opaque render config");
    
    printf("Shaders loaded successfully\n");
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
        fprintf(stderr, "Failed to create vertex buffer: %s\n", veGetLastError());
        return false;
    }
    
    printf("Created vertex buffer at address: 0x%llx\n", (unsigned long long)g_vertexBuffer);
    return true;
}

// Matrix utility functions
static void createIdentityMatrix(float matrix[16]) {
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0] = 1.0f;   // m[0][0]
    matrix[5] = 1.0f;   // m[1][1]
    matrix[10] = 1.0f;  // m[2][2]
    matrix[15] = 1.0f;  // m[3][3]
}

static void createRotationMatrix(float matrix[16], float angleRadians) {
    createIdentityMatrix(matrix);
    float cosTheta = cosf(angleRadians);
    float sinTheta = sinf(angleRadians);
    
    matrix[0] = cosTheta;   // m[0][0]
    matrix[1] = sinTheta;   // m[0][1]
    matrix[4] = -sinTheta;  // m[1][0]
    matrix[5] = cosTheta;   // m[1][1]
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
        fprintf(stderr, "Failed to begin command buffer\n");
        return;
    }

    veTransitionTextureForColorAttachment(cmd, backbuffer); //TODO:  Find a way to remove this from the user's workload
    
    // Calculate animation
    double currentTime = glfwGetTime();
    float elapsedTime = (float)(currentTime - g_startTime);
    float rotationAngle = elapsedTime * 2.0f; // 2 radians per second
    
    // Create rotation matrix for spinning animation
    float mvpMatrix[16];
    createRotationMatrix(mvpMatrix, rotationAngle);
    
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
        .stencilAttachment = NULL, 
    };
    
    // Begin rendering
    veBeginRendering(cmd, &renderingInfo);
    
    // Bind shaders
    veBindShader(cmd, g_vertexShader);
    veBindShader(cmd, g_fragmentShader);
    
    //setup viewport and scissoring state
    veSetViewport(cmd, 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
    veSetScissor(cmd, 0, 0, width, height);

    //default render state
    veApplyRenderConfig(cmd, g_renderConfig);
    
    // Set vertex input (using VulkEase's flexible vertex input)
    // VulkEase uses bindless buffers - vertex data is accessed via push constants!
    // Set up push constants with the vertex buffer address
    typedef struct {
        float mvpMatrix[16];
        uint64_t vertexBufferAddress;
    } TrianglePushConstants;
    
    TrianglePushConstants pushConstants = {0};
    memcpy(pushConstants.mvpMatrix, mvpMatrix, sizeof(mvpMatrix));
    pushConstants.vertexBufferAddress = g_vertexBuffer;
    
    // Set push constants with transformation matrix and vertex buffer address
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    
    // Draw triangle (3 vertices, no indices)
    veDraw(cmd, 3, 1, 0, 0);
    
    // End rendering
    veEndRendering(cmd);
    
    // Present
    veTransitionTextureForPresent(cmd, backbuffer);  //TODO:  Find a way to remove this from the user's work load

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
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "VulkEase Spinning Triangle", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    if (!initVulkEase(window) || !createShaders() || !createBuffers()) {
        cleanup();
        glfwTerminate();
        return -1;
    }
    
    printf("Starting render loop...\n");
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        render();
    }
    
    printf("Shutting down...\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}