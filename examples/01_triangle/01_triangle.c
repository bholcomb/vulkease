/**
 * Example 1: Spinning Triangle
 * Demonstrates basic VulkEase setup with GLFW window, vertex buffers, and animated rendering
 * Features embedded GLSL shaders and a spinning colorful triangle
 */

#include "vulkease.h"
#include <GLFW/glfw3.h>
#if defined(__linux__) || defined(_WIN32)
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
#define VERTEX_SHADER_PATH "data/01_triangle/shaders/triangle.vert.spv"
#define FRAGMENT_SHADER_PATH "data/01_triangle/shaders/triangle.frag.spv"

// Global state
static VEContext* g_context = NULL;
static VEDevice* g_device = NULL;
static VESwapchain* g_swapchain = NULL;
static VEShader* g_vertexShader = NULL;
static VEShader* g_fragmentShader = NULL;
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;
static VEGraphicsPipeline* g_pipeline = NULL;
static VERenderTarget g_renderTarget;
static VETextureIndex g_colorTexture = VE_INVALID_TEXTURE_INDEX;

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
        if (g_colorTexture != VE_INVALID_TEXTURE_INDEX) {
            // Resize the render target (resizes all attached textures)
            veResizeRenderTarget(g_device, &g_renderTarget, (uint32_t)width, (uint32_t)height);
            
            // Update the color texture reference (texture index may have changed)
            g_colorTexture = g_renderTarget.attachments[0].texture;
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

    // Create render target for offscreen rendering using convenience function
    VEColor clearColor = {0.1f, 0.1f, 0.1f, 1.0f};
    g_renderTarget = veCreateSimpleRenderTarget(g_device, 
                                                 (uint32_t)win_width, (uint32_t)win_height,
                                                 veGetSwapchainFormat(g_swapchain),
                                                 VK_FORMAT_UNDEFINED,  // No depth buffer
                                                 clearColor, 1.0f,
                                                 &g_colorTexture, NULL);

    if (g_colorTexture == VE_INVALID_TEXTURE_INDEX) {
        fprintf(stderr, "Failed to create render target\n");
        return false;
    }

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
    veBeginRendering(cmd, &g_renderTarget);
  
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
    
    // Blit color texture to swapchain (acquires swapchain image automatically)
    VEResult result = veBlitTextureToSwapchain(cmd, g_colorTexture, g_swapchain, VK_FILTER_LINEAR);
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
    
    if (g_colorTexture != VE_INVALID_TEXTURE_INDEX) {
        veDestroyTexture(g_device, g_colorTexture);
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

    bool shouldQuit = false;
    VEFrameTimingInfo timing = {0};
    
    while (!glfwWindowShouldClose(window) && !shouldQuit) {
        // Begin frame timing
        veBeginFrame(g_device);
        
        glfwPollEvents();
        render();

        // End frame and get timing info
        veEndFrame(g_device, &timing);

        // Print frame timing every 1000 frames
        if(timing.frameNumber % 1000 == 0) {
            printf("Frame %llu: %.2f ms (%.1f FPS) | Avg: %.2f ms (%.1f FPS) | Min: %.2f ms | Max: %.2f ms\n",
                   (unsigned long long)timing.frameNumber,
                   timing.frameTimeMs,
                   timing.fps,
                   timing.avgFrameTimeMs,
                   timing.avgFps,
                   timing.minFrameTimeMs,
                   timing.maxFrameTimeMs);
        }
        
        // Print debug info every 10000 frames
        if(timing.frameNumber % 10000 == 0) vePrintDebugInfo(g_device);

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            shouldQuit = true;
        }
    }
    
    // Print final timing summary
    printf("\n=== Final Timing Summary ===\n");
    printf("Total frames: %llu\n", (unsigned long long)timing.frameNumber);
    printf("Average frame time: %.2f ms (%.1f FPS)\n", timing.avgFrameTimeMs, timing.avgFps);
    printf("Min frame time: %.2f ms\n", timing.minFrameTimeMs);
    printf("Max frame time: %.2f ms\n", timing.maxFrameTimeMs);
    
    printf("Shutting down...\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}