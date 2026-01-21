/**
 * Example 3: Compute Shader Particle System
 * Demonstrates compute shaders, storage buffers, and GPU-driven rendering
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
#include <time.h>

#define PARTICLE_COUNT 10000
#define WORKGROUP_SIZE 64

// Shader file paths (compiled SPIR-V)
#define COMPUTE_SHADER_PATH "examples/shaders/particles.comp.spv"
#define VERTEX_SHADER_PATH "examples/shaders/particles.vert.spv"
#define FRAGMENT_SHADER_PATH "examples/shaders/particles.frag.spv"

#if DEBUG
bool vsync = true;
#else
bool vsync = false;
#endif

// Particle structure (used in both CPU and GPU)
typedef struct {
    float position[2];
    float velocity[2];
    float life;
    float size;
    float color[4];
    float padding; // Align to 16 bytes
} Particle;

// Compute push constants
typedef struct {
    uint64_t particleBufferAddress;
    float deltaTime;
    float time;
    float attractorPos[2];
    float attractorStrength;
    uint32_t particleCount;
} ComputePushConstants;

// Graphics push constants
typedef struct {
    uint64_t vertexBufferAddress;
    uint64_t particleBufferAddress;
    float screenSize[2];
    float padding[2];
} GraphicsPushConstants;

// Global state
static VEContext* g_context = NULL;
static VEDevice* g_device = NULL;
static VESwapchain* g_swapchain = NULL;
static VERenderTarget* g_renderTarget = NULL;
static VEShader* g_computeShader = NULL;
static VEShader* g_vertexShader = NULL;
static VEShader* g_fragmentShader = NULL;
static VEGraphicsPipeline* g_pipeline = NULL;
static VEBufferAddress g_particleBuffer = VE_INVALID_ADDRESS;
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;
static VERenderingInfo g_renderingInfo;

int win_width = 800;
int win_height = 600;

// Simple quad vertices for instanced particle rendering
static const float quadVertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f
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
                                           (VEColor){0.05f, 0.05f, 0.1f, 1.0f});
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
    if (veCreateContext("VulkEase Compute Particle System", NULL, 0, &g_context) != VE_SUCCESS || !g_context) {
        fprintf(stderr, "Failed to create context\n");
        return false;
    }

    printf("VulkEase initialized successfully\n");
    
    // Create device (VK_NULL_HANDLE = auto-select best GPU)
    if (veCreateDevice(g_context, VK_NULL_HANDLE, NULL, 0, &g_device) != VE_SUCCESS || !g_device) {
        fprintf(stderr, "Failed to create device\n");
        return false;
    }
    
    printf("VulkEase device created successfully\n");
    printf("Device: %s\n", veGetDeviceName(g_device));
    
    // Note: Compute capabilities check not implemented yet
    printf("Compute workgroup size: %d (assumed optimal)\n", WORKGROUP_SIZE);
    
    // Set up swapchain descriptor with platform-specific surface
    VESwapchainDesc swapchainDesc = {0};
    swapchainDesc.width = (uint32_t)win_width;
    swapchainDesc.height = (uint32_t)win_height;
    swapchainDesc.colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainDesc.vsync = vsync;
    swapchainDesc.debugName = "ParticlesSwapchain";

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
    rtDesc.depthFormat = VK_FORMAT_UNDEFINED;  // No depth buffer for particles
    rtDesc.sampleCount = 1;
    rtDesc.hasResolveTarget = false;
    rtDesc.debugName = "ParticleRenderTarget";

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
                                   (VEColor){0.05f, 0.05f, 0.1f, 1.0f}); // Dark blue background

    printf("Render target created: %dx%d\n", win_width, win_height);

    return true;
}

static bool createShaders() {
    // Load compute shader for particle simulation
    if (veLoadShaderFromFile(g_device, COMPUTE_SHADER_PATH, VK_SHADER_STAGE_COMPUTE_BIT, "main",
                             "ParticleComputeShader", &g_computeShader) != VE_SUCCESS ||
        !g_computeShader) {
        fprintf(stderr, "Failed to load compute shader\n");
        return false;
    }
    
    // Load graphics shaders for particle rendering
    if (veLoadShaderFromFile(g_device, VERTEX_SHADER_PATH, VK_SHADER_STAGE_VERTEX_BIT, "main",
                             "ParticleVertexShader", &g_vertexShader) != VE_SUCCESS ||
        !g_vertexShader) {
        fprintf(stderr, "Failed to load vertex shader\n");
        return false;
    }
    
    if (veLoadShaderFromFile(g_device, FRAGMENT_SHADER_PATH, VK_SHADER_STAGE_FRAGMENT_BIT, "main",
                             "ParticleFragmentShader", &g_fragmentShader) != VE_SUCCESS ||
        !g_fragmentShader) {
        fprintf(stderr, "Failed to load fragment shader\n");
        return false;
    }
    
    printf("Shaders loaded successfully\n");
    return true;
}

static void initializeParticles(Particle* particles, uint32_t count) {
    srand((unsigned int)time(NULL));
    
    for (uint32_t i = 0; i < count; i++) {
        // Random position in circle
        float angle = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265359f;
        float radius = ((float)rand() / (float)RAND_MAX) * 0.5f;
        
        particles[i].position[0] = cosf(angle) * radius;
        particles[i].position[1] = sinf(angle) * radius;
        
        // Random velocity
        particles[i].velocity[0] = (((float)rand() /(float)RAND_MAX) - 0.5f) * 0.1f;
        particles[i].velocity[1] = (((float)rand() / (float)RAND_MAX) - 0.5f) * 0.1f;
        
        // Random properties
        particles[i].life = ((float)rand() / (float)RAND_MAX) * 5.0f + 1.0f;
        particles[i].size = ((float)rand() / (float)RAND_MAX) * 0.02f + 0.005f;
        
        // Random color
        particles[i].color[0] = ((float)rand() / (float)RAND_MAX);
        particles[i].color[1] = ((float)rand() / (float)RAND_MAX);
        particles[i].color[2] = ((float)rand() / (float)RAND_MAX);
        particles[i].color[3] = 1.0f;
        
        particles[i].padding = 0.0f;
    }
}

static bool createBuffers() {
    // Allocate and initialize particle data
    Particle* particles = malloc(sizeof(Particle) * PARTICLE_COUNT);
    if (!particles) {
        fprintf(stderr, "Failed to allocate particle data\n");
        return false;
    }
    
    initializeParticles(particles, PARTICLE_COUNT);
    
    // Create particle storage buffer (read/write from compute shader)
    VEBufferDesc particleBufferDesc = {
        .size = sizeof(Particle) * PARTICLE_COUNT,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .initialData = particles,
        .initialDataSize = sizeof(Particle) * PARTICLE_COUNT,
        .persistentlyMapped = false,
        .debugName = "ParticleStorageBuffer"
    };
    
    if (veCreateBuffer(g_device, &particleBufferDesc, &g_particleBuffer) != VE_SUCCESS ||
        g_particleBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create particle buffer\n");
        free(particles);
        return false;
    }
    
    free(particles);
    
    // Create vertex buffer for quad geometry
    VEBufferDesc vertexBufferDesc = {
        .size = sizeof(quadVertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .initialData = quadVertices,
        .initialDataSize = sizeof(quadVertices),
        .persistentlyMapped = false,
        .debugName = "QuadVertexBuffer"
    };
    
    if (veCreateBuffer(g_device, &vertexBufferDesc, &g_vertexBuffer) != VE_SUCCESS || g_vertexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create vertex buffer\n");
        return false;
    }
    
    printf("Created particle buffer (0x%lx) for %d particles\n", g_particleBuffer, PARTICLE_COUNT);
    printf("Created vertex buffer (0x%lx) for quad geometry\n", g_vertexBuffer);
    
    return true;
}

static bool createPipeline() {
    // Create a transparent graphics pipeline for particles with alpha blending
    VEGraphicsPipelineDesc pipelineDesc = veDefaultGraphicsPipelineDesc();
    pipelineDesc.vertexShader = g_vertexShader;
    pipelineDesc.fragmentShader = g_fragmentShader;
    pipelineDesc.debugName = "ParticlePipeline";
    
    // Enable alpha blending for particles
    pipelineDesc.blendAttachments[0].blendEnable = true;
    pipelineDesc.blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipelineDesc.blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    pipelineDesc.blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pipelineDesc.blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    
    // Disable depth write for particles
    pipelineDesc.depthWriteEnable = false;
    
    if (veCreateGraphicsPipeline(g_device, &pipelineDesc, &g_pipeline) != VE_SUCCESS || !g_pipeline) {
        fprintf(stderr, "Failed to create graphics pipeline\n");
        return false;
    }
    
    return true;
}

static void runComputeShader(VECommandBuffer* cmd, float deltaTime, float time, float mouseX, float mouseY) {
    // Begin compute debug region
    VEColor computeColor = {1.0f, 0.0f, 1.0f, 1.0f};
    veBeginDebugLabel(cmd, "Particle Simulation", computeColor);
    
    // Bind compute shader
    veBindShader(cmd, g_computeShader);
    
    // Set compute push constants
    ComputePushConstants computeConstants = {
        .particleBufferAddress = g_particleBuffer,
        .deltaTime = deltaTime,
        .time = time,
        .attractorPos = {mouseX, mouseY},
        .attractorStrength = 0.1f,
        .particleCount = PARTICLE_COUNT
    };
    
    vePushConstants(cmd, &computeConstants, sizeof(computeConstants), 0);
    
    // Calculate dispatch size (simple calculation)
    uint32_t numGroups = (PARTICLE_COUNT + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    
    // Dispatch compute work
    veDispatch(cmd, numGroups, 1, 1);
    
    // Add barrier between compute and graphics
    veBarrierComputeToVertex(cmd);
    
    veEndDebugLabel(cmd);
}

static void renderParticles(VECommandBuffer* cmd) {
    // Get render target size for push constants
    VkExtent2D extent = veGetRenderTargetSize(g_renderTarget);
    uint32_t width = extent.width;
    uint32_t height = extent.height;
    
    // Begin graphics debug region
    VEColor graphicsColor = {0.0f, 1.0f, 0.5f, 1.0f};
    veBeginDebugLabel(cmd, "Particle Rendering", graphicsColor);
    
    // Begin rendering (automatically handles texture transitions)
    veBeginRendering(cmd, &g_renderingInfo);
    
    // Bind graphics pipeline and apply state (includes shaders and alpha blending)
    veApplyGraphicsState(cmd, g_pipeline, NULL, NULL, NULL);
    
    // Set graphics push constants
    GraphicsPushConstants graphicsConstants = {
        .vertexBufferAddress = g_vertexBuffer,
        .particleBufferAddress = g_particleBuffer,
        .screenSize = {(float)width, (float)height},
        .padding = {0.0f, 0.0f}
    };
    
    vePushConstants(cmd, &graphicsConstants, sizeof(graphicsConstants), 0);
    
    // Draw instanced particles (4 vertices per quad, PARTICLE_COUNT instances)
    veDraw(cmd, 4, PARTICLE_COUNT, 0, 0);
    
    // End rendering
    veEndRendering(cmd);
    
    veEndDebugLabel(cmd);
}

static void render(float deltaTime, float time) {
    // Convert to normalized coordinates [-1, 1]
    float normalizedMouseX = (float)sin(time);
    float normalizedMouseY = (float)cos(time);
    
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
    
    // Run compute shader to simulate particles
    runComputeShader(cmd, deltaTime, time, normalizedMouseX, normalizedMouseY);
    
    // Render particles to offscreen render target
    renderParticles(cmd);
    
    // Blit render target to swapchain (acquires swapchain image automatically)
    VEResult result = veBlitToSwapchain(cmd, g_renderTarget, g_swapchain, VK_FILTER_LINEAR);
    if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
        // Resize callback handles this
        return;
    }
    
    // Present
    vePresentImage(g_swapchain, cmd, true);
}

static void cleanup() {
    if (g_device) {
        veDeviceWaitIdle(g_device);
    }
    
    if (g_particleBuffer != VE_INVALID_ADDRESS) {
        (void)veDestroyBuffer(g_device, g_particleBuffer);
    }
    
    if (g_vertexBuffer != VE_INVALID_ADDRESS) {
        (void)veDestroyBuffer(g_device, g_vertexBuffer);
    }
    
    if (g_pipeline) {
        (void)veDestroyGraphicsPipeline(g_pipeline);
    }
    
    if (g_computeShader) {
        (void)veDestroyShader(g_computeShader);
    }
    
    if (g_vertexShader) {
        (void)veDestroyShader(g_vertexShader);
    }
    
    if (g_fragmentShader) {
        (void)veDestroyShader(g_fragmentShader);
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
    
    GLFWwindow* window = glfwCreateWindow(1024, 768, "VulkEase Compute Particles", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    if (!initVulkEase(window) || !createShaders() || !createBuffers() || !createPipeline()) {
        cleanup();
        glfwTerminate();
        return -1;
    }
    
    printf("Starting particle simulation with %d particles...\n", PARTICLE_COUNT);
    printf("Move mouse to attract particles\n");
    
    double lastTime = glfwGetTime();
    
    uint64_t frameCount = 0;
    bool shouldQuit = false;
    double frameTime = 0.0;
    while (!glfwWindowShouldClose(window) && !shouldQuit) {
        double start = glfwGetTime();
        glfwPollEvents();
        
        float deltaTime = (float)(start - lastTime);
        float time = (float)start;
        lastTime = start;
    
        render(deltaTime, time);
        
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
    
    printf("\nShutting down...\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}