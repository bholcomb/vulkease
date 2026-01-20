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
static VERenderTarget* g_renderTarget = NULL;
static VEShader* g_vertexShader = NULL;
static VEShader* g_fragmentShader = NULL;
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;
static VEGraphicsPipeline* g_pipeline = NULL;
static VERenderingInfo g_renderingInfo;

int win_width = 800;
int win_height = 600;

#if DEBUG
bool vsync = true;
#else
bool vsync = false;
#endif

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
    
    if (width > 0 && height > 0) {
        if (g_swapchain) {
            veResizeSwapchain(g_swapchain, (uint32_t)width, (uint32_t)height);
        }
        if (g_renderTarget) {
            veResizeRenderTarget(g_renderTarget, (uint32_t)width, (uint32_t)height);
            
            // Rebuild rendering info with new size and texture handle
            g_renderingInfo = veCreateRenderingInfo((uint32_t)width, (uint32_t)height);
            VETextureIndex rtColor = veGetRenderTargetColorTexture(g_renderTarget);
            (void)veRenderingAddColorAttachment(&g_renderingInfo, 
                                           rtColor,
                                           VK_ATTACHMENT_LOAD_OP_CLEAR,
                                           (VEColor){0.1f, 0.1f, 0.1f, 1.0f});
        }
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
    if (veCreateContext("VulkEase Spinning Triangle", NULL, 0, &g_context) != VE_SUCCESS || !g_context) {
        fprintf(stderr, "Failed to create VulkEase context\n");
        return false;
    }
    
    printf("VulkEase initialized successfully\n");
    
    // Create device (VK_NULL_HANDLE = auto-select best GPU)
    if (veCreateDevice(g_context, VK_NULL_HANDLE, NULL, 0, &g_device) != VE_SUCCESS || !g_device) {
        fprintf(stderr, "Failed to create VulkEase device\n");
        return false;
    }
    
    printf("VulkEase device created successfully\n");
    printf("Device: %s\n", veGetDeviceName(g_device));
    
    // Set up swapchain descriptor with platform-specific surface
    VESwapchainDesc swapchainDesc = {0};
    swapchainDesc.width = (uint32_t)win_width;
    swapchainDesc.height = (uint32_t)win_height;
    swapchainDesc.colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainDesc.vsync = vsync;
    swapchainDesc.debugName = "TriangleSwapchain";

#if defined(_WIN32)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_WIN32;
    swapchainDesc.surface.win32.hwnd = glfwGetWin32Window(window);
    if (!swapchainDesc.surface.win32.hwnd) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }
#elif defined(__linux__)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_XLIB;
    swapchainDesc.surface.xlib.display = (void*)glfwGetX11Display();
    swapchainDesc.surface.xlib.window = glfwGetX11Window(window);
    if (!swapchainDesc.surface.xlib.display || !swapchainDesc.surface.xlib.window) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }
#elif defined(__APPLE__)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_COCOA;
    swapchainDesc.surface.cocoa.window = glfwGetCocoaWindow(window);
    if (!swapchainDesc.surface.cocoa.window) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }
#endif

    if (veCreateSwapchain(g_device, &swapchainDesc, &g_swapchain) != VE_SUCCESS || !g_swapchain) {
        fprintf(stderr, "Failed to create swapchain\n");
        return false;
    }
    
    printf("Swapchain created: %dx%d\n", win_width, win_height);

    // Create render target for offscreen rendering
    VERenderTargetDesc rtDesc = {0};
    rtDesc.width = (uint32_t)win_width;
    rtDesc.height = (uint32_t)win_height;
    rtDesc.colorFormat = veGetSwapchainFormat(g_swapchain);
    rtDesc.depthFormat = VK_FORMAT_UNDEFINED;  // No depth buffer for this example
    rtDesc.sampleCount = 1;
    rtDesc.hasResolveTarget = false;
    rtDesc.debugName = "TriangleRenderTarget";

    if (veCreateRenderTarget(g_device, &rtDesc, &g_renderTarget) != VE_SUCCESS || !g_renderTarget) {
        fprintf(stderr, "Failed to create render target\n");
        return false;
    }
    
    // Build rendering info once (reused every frame)
    g_renderingInfo = veCreateRenderingInfo((uint32_t)win_width, (uint32_t)win_height);
    VETextureIndex rtColor = veGetRenderTargetColorTexture(g_renderTarget);
    (void)veRenderingAddColorAttachment(&g_renderingInfo, 
                                   rtColor,
                                   VK_ATTACHMENT_LOAD_OP_CLEAR,
                                   (VEColor){0.1f, 0.1f, 0.1f, 1.0f}); // Dark gray background

    printf("Render target created: %dx%d\n", win_width, win_height);
    
    // Initialize animation timer
    g_startTime = glfwGetTime();
    
    return true;
}

static bool createShaders() {
    // Load vertex shader from compiled SPIR-V file
    if (veLoadShaderFromFile(g_device, VERTEX_SHADER_PATH, VK_SHADER_STAGE_VERTEX_BIT, "main",
                             "SpinningTriangleVertex", &g_vertexShader) != VE_SUCCESS ||
        !g_vertexShader) {
        fprintf(stderr, "Failed to load vertex shader\n");
        return false;
    }
    
    // Load fragment shader from compiled SPIR-V file
    if (veLoadShaderFromFile(g_device, FRAGMENT_SHADER_PATH, VK_SHADER_STAGE_FRAGMENT_BIT, "main",
                             "SpinningTriangleFragment", &g_fragmentShader) != VE_SUCCESS ||
        !g_fragmentShader) {
        fprintf(stderr, "Failed to load fragment shader\n");
        return false;
    }

    // Create a graphics pipeline with the shaders
    VEGraphicsPipelineDesc pipelineDesc = veDefaultGraphicsPipelineDesc();
    pipelineDesc.vertexShader = g_vertexShader;
    pipelineDesc.fragmentShader = g_fragmentShader;
    pipelineDesc.debugName = "TrianglePipeline";
    
    if (veCreateGraphicsPipeline(g_device, &pipelineDesc, &g_pipeline) != VE_SUCCESS || !g_pipeline) {
        fprintf(stderr, "Failed to create graphics pipeline\n");
        return false;
    }
    
    printf("Shaders loaded successfully\n");
    return true;
}

static bool createBuffers() {
    // Create vertex buffer
    VEBufferDesc bufferDesc = {
        .size = sizeof(triangleVertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .initialData = triangleVertices,
        .initialDataSize = sizeof(triangleVertices),
        .persistentlyMapped = false,
        .debugName = "TriangleVertexBuffer"
    };
    
    if (veCreateBuffer(g_device, &bufferDesc, &g_vertexBuffer) != VE_SUCCESS || g_vertexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create vertex buffer\n");
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
    // Begin command buffer
    VECommandBuffer* cmd = NULL;
    if (veBeginCommandBuffer(g_device, &cmd) != VE_SUCCESS || !cmd) {
        fprintf(stderr, "Failed to begin command buffer\n");
        return;
    }
    if (!cmd) {
        fprintf(stderr, "Failed to begin command buffer\n");
        return;
    }
    
    // Calculate animation
    double currentTime = glfwGetTime();
    float elapsedTime = (float)(currentTime - g_startTime);
    float rotationAngle = elapsedTime * 2.0f; // 2 radians per second
    
    // Create rotation matrix for spinning animation
    float mvpMatrix[16];
    createRotationMatrix(mvpMatrix, rotationAngle);
    
    // Begin rendering (automatically handles texture transitions)
    veBeginRendering(cmd, &g_renderingInfo);
    
    // Bind graphics pipeline (includes shaders and state)
    veBindGraphicsPipeline(cmd, g_pipeline);
    
    // Apply graphics state (NULL viewport/scissor uses full render area)
    veApplyGraphicsState(cmd, g_pipeline, NULL, NULL, NULL);
    
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
    
    // Blit render target to swapchain (acquires swapchain image automatically)
    VEResult result = veBlitToSwapchain(cmd, g_renderTarget, g_swapchain, VK_FILTER_LINEAR);
    if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
        // Handle swapchain recreation - resize callback handles this
        return;
    }
    
    // Present
    vePresentImage(g_swapchain, cmd, true);
}

static void cleanup() {
    if (g_device) {
        veDeviceWaitIdle(g_device);
    }
    
    if (g_vertexBuffer != VE_INVALID_ADDRESS) {
        (void)veDestroyBuffer(g_device, g_vertexBuffer);
    }
    
    if (g_vertexShader) {
        (void)veDestroyShader(g_vertexShader);
    }
    
    if (g_fragmentShader) {
        (void)veDestroyShader(g_fragmentShader);
    }
    
    if (g_pipeline) {
        (void)veDestroyGraphicsPipeline(g_pipeline);
    }
    
    if (g_renderTarget) {
        (void)veDestroyRenderTarget(g_renderTarget);
    }

    if (g_swapchain) {
        (void)veDestroySwapchain(g_swapchain);
    }
    
    if (g_device) {
        (void)veDestroyDevice(g_device);
    }
    
    if (g_context) {
        (void)veDestroyContext(g_context);
    }
}

int main() {
    if (!initGLFW()) {
        return -1;
    }
    
    GLFWwindow* window = glfwCreateWindow(win_width, win_height, "VulkEase Spinning Triangle", NULL, NULL);
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

    uint64_t frameCount = 0;
    bool shouldQuit = false;
    double frameTime = 0.0;
    while (!glfwWindowShouldClose(window) && !shouldQuit) {
        double start = glfwGetTime();
        
        glfwPollEvents();
        render();

        frameCount++;
        if(frameCount % 1000 == 0) printf("Average Framerate: %f:4.2ms\n", (frameTime / (double)frameCount) * 1000.0);
        if(frameCount % 10000 == 0) vePrintDebugInfo(g_device);

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            shouldQuit = true;
        }

        double end = glfwGetTime();
        frameTime += (end - start);
    }
    
    printf("Shutting down...\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}