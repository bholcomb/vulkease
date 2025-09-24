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
static VEShader* g_computeShader = NULL;
static VEShader* g_vertexShader = NULL;
static VEShader* g_fragmentShader = NULL;
static VERenderConfig* g_renderConfig = NULL;
static VEBufferAddress g_particleBuffer = VE_INVALID_ADDRESS;
static VEBufferAddress g_vertexBuffer = VE_INVALID_ADDRESS;

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
    g_context = veCreateContext("VulkEase Compute Particle System");
    if (!g_context) {
        fprintf(stderr, "Failed to create context: %s\n", veGetLastError());
        return false;
    }

    printf("VulkEase initialized successfully\n");
    
    g_device = veCreateDevice(g_context);
    if (!g_device) {
        fprintf(stderr, "Failed to create device: %s\n", veGetLastError());
        return false;
    }
    
    printf("VulkEase device created successfully\n");
    printf("Device: %s\n", veGetDeviceName(g_device));
    
    // Note: Compute capabilities check not implemented yet
    printf("Compute workgroup size: %d (assumed optimal)\n", WORKGROUP_SIZE);
           
#if defined(_WIN32)
    void* windowHandle = NULL;    
    
    windowHandle = glfwGetWin32Window(window);
    if (!windowHandle) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }

    g_swapchain = veCreateSwapchain(g_device, windowHandle, width, height, VE_FORMAT_BGRA8_SRGB);
#elif defined(__linux__)
    // Get native window handle for VulkEase's simple approach
    void* displayHandle = NULL;
    void* windowHandle = NULL;
    
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
    g_swapchain = veCreateSwapchain(g_device, windowData, win_width, win_height, VK_FORMAT_B8G8R8A8_SRGB, true);

#elif defined(__APPLE__)
    void* windowHandle = NULL;    
    
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

    printf("Swapchain created: %dx%d\n", win_width, win_height);

    return true;
}

static bool createShaders() {
    // Load compute shader for particle simulation
    g_computeShader = veLoadShader(g_device, COMPUTE_SHADER_PATH, 
                                  VE_SHADER_STAGE_COMPUTE, "main", "ParticleComputeShader");
    if (!g_computeShader) {
        fprintf(stderr, "Failed to load compute shader: %s\n", veGetLastError());
        return false;
    }
    
    // Load graphics shaders for particle rendering
    g_vertexShader = veLoadShader(g_device, VERTEX_SHADER_PATH, 
                                 VE_SHADER_STAGE_VERTEX, "main", "ParticleVertexShader");
    if (!g_vertexShader) {
        fprintf(stderr, "Failed to load vertex shader: %s\n", veGetLastError());
        return false;
    }
    
    g_fragmentShader = veLoadShader(g_device, FRAGMENT_SHADER_PATH, 
                                   VE_SHADER_STAGE_FRAGMENT, "main", "ParticleFragmentShader");
    if (!g_fragmentShader) {
        fprintf(stderr, "Failed to load fragment shader: %s\n", veGetLastError());
        return false;
    }
    
    printf("Shaders loaded successfully\n");
    return true;
}

static void initializeParticles(Particle* particles, uint32_t count) {
    srand((unsigned int)time(NULL));
    
    for (uint32_t i = 0; i < count; i++) {
        // Random position in circle
        float angle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159265359f;
        float radius = ((float)rand() / RAND_MAX) * 0.5f;
        
        particles[i].position[0] = cosf(angle) * radius;
        particles[i].position[1] = sinf(angle) * radius;
        
        // Random velocity
        particles[i].velocity[0] = (((float)rand() / RAND_MAX) - 0.5f) * 0.1f;
        particles[i].velocity[1] = (((float)rand() / RAND_MAX) - 0.5f) * 0.1f;
        
        // Random properties
        particles[i].life = ((float)rand() / RAND_MAX) * 5.0f + 1.0f;
        particles[i].size = ((float)rand() / RAND_MAX) * 0.02f + 0.005f;
        
        // Random color
        particles[i].color[0] = ((float)rand() / RAND_MAX);
        particles[i].color[1] = ((float)rand() / RAND_MAX);
        particles[i].color[2] = ((float)rand() / RAND_MAX);
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
    
    g_particleBuffer = veCreateBuffer(g_device, &particleBufferDesc);
    if (g_particleBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create particle buffer: %s\n", veGetLastError());
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
    
    g_vertexBuffer = veCreateBuffer(g_device, &vertexBufferDesc);
    if (g_vertexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create vertex buffer: %s\n", veGetLastError());
        return false;
    }
    
    printf("Created particle buffer (0x%lx) for %d particles\n", g_particleBuffer, PARTICLE_COUNT);
    printf("Created vertex buffer (0x%lx) for quad geometry\n", g_vertexBuffer);
    
    return true;
}

static bool createRenderConfig() {
    // Use transparent render config for particles with alpha blending
    g_renderConfig = veCreateTransparentRenderConfig(g_device, "ParticleRenderConfig");
    if (!g_renderConfig) {
        fprintf(stderr, "Failed to create render config: %s\n", veGetLastError());
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

static void renderParticles(VECommandBuffer* cmd, VETextureIndex backbuffer) {
    uint32_t width, height;
    veGetSwapchainSize(g_swapchain, &width, &height);
    
    // Set up rendering info
    VERenderingAttachment colorAttachment = {
        .texture = backbuffer,
        .loadOp = VE_LOAD_OP_CLEAR,
        .storeOp = VE_STORE_OP_STORE,
        .clearValue = {0.05f, 0.05f, 0.1f, 1.0f}, // Dark blue background
        .resolveTexture = VE_INVALID_TEXTURE_INDEX
    };
    
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
    
    // Begin graphics debug region
    VEColor graphicsColor = {0.0f, 1.0f, 0.5f, 1.0f};
    veBeginDebugLabel(cmd, "Particle Rendering", graphicsColor);
    
    // Begin rendering
    veBeginRendering(cmd, &renderingInfo);
    
    //setup viewport and scissoring state
    veSetViewport(cmd, 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
    veSetScissor(cmd, 0, 0, width, height);

    // Apply render configuration (with alpha blending)
    veApplyRenderConfig(cmd, g_renderConfig);
    
    // Bind graphics shaders
    veBindShader(cmd, g_vertexShader);
    veBindShader(cmd, g_fragmentShader);
    
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
    float normalizedMouseX = sin(time); //(float)(mouseX / win_width) * 2.0f - 1.0f;
    float normalizedMouseY = cos(time); //1.0f - (float)(mouseY / win_height) * 2.0f;
    
    // Acquire next swapchain image
    VETextureIndex backbuffer = veAcquireNextImage(g_swapchain);
    if (backbuffer == VE_INVALID_TEXTURE_INDEX) {
        return;
    }
    
    // Begin command buffer
    VECommandBuffer* cmd = veBeginCommandBuffer(g_device);
    if (!cmd) {
        fprintf(stderr, "Failed to begin command buffer\n");
        return;
    }

    veTransitionTextureForColorAttachment(cmd, backbuffer); //TODO:  Find a way to remove this from the user's workload
    
    // Run compute shader to simulate particles
    runComputeShader(cmd, deltaTime, time, normalizedMouseX, normalizedMouseY);
    
    // Render particles
    renderParticles(cmd, backbuffer);
    
    // Present
    veTransitionTextureForPresent(cmd, backbuffer);  //TODO:  Find a way to remove this from the user's work load

    VEResult result = vePresentImage(g_swapchain, cmd);
    if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
        uint32_t swWidth, swHeight;
        veGetSwapchainSize(g_swapchain, &swWidth, &swHeight);
        veResizeSwapchain(g_swapchain, swWidth, swHeight);
    }
}

static void printPerformanceStats() {
    VEPerformanceStats stats = {0};
    VEMemoryStats memStats = {0};
    
    veGetPerformanceStats(g_device, &stats);
    veGetMemoryStats(g_device, &memStats);
    
    printf("\rFrame: %lu ns | Draws: %u | Memory: %lu bytes", 
           (unsigned long)stats.frameTime, stats.drawCalls, (unsigned long)memStats.totalAllocated);
    fflush(stdout);
}

static void cleanup() {
    if (g_device) {
        veDeviceWaitIdle(g_device);
    }
    
    if (g_particleBuffer != VE_INVALID_ADDRESS) {
        veDestroyBuffer(g_device, g_particleBuffer);
    }
    
    if (g_vertexBuffer != VE_INVALID_ADDRESS) {
        veDestroyBuffer(g_device, g_vertexBuffer);
    }
    
    if (g_renderConfig) {
        veDestroyRenderConfig(g_renderConfig);
    }
    
    if (g_computeShader) {
        veDestroyShader(g_computeShader);
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
    
    GLFWwindow* window = glfwCreateWindow(1024, 768, "VulkEase Compute Particles", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    if (!initVulkEase(window) || !createShaders() || !createBuffers() || !createRenderConfig()) {
        cleanup();
        glfwTerminate();
        return -1;
    }
    
    printf("Starting particle simulation with %d particles...\n", PARTICLE_COUNT);
    printf("Move mouse to attract particles\n");
    
    double lastTime = glfwGetTime();
    double statsTime = lastTime;
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        float time = (float)currentTime;
        lastTime = currentTime;
        
        render(deltaTime, time);
        
        // Note: Frame stats update not implemented yet
        
        // Print performance stats every second
        if (currentTime - statsTime >= 1.0) {
            printPerformanceStats();
            statsTime = currentTime;
        }
        
        // Cap frame rate to prevent excessive GPU usage
        if (deltaTime < 1.0f / 120.0f) {
            double sleepTime = (1.0f / 120.0f) - deltaTime;
            if (sleepTime > 0) {
                glfwWaitEventsTimeout(sleepTime);
            }
        }
    }
    
    printf("\nShutting down...\n");
    
    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}