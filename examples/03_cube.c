/**
 * 03_cube.c - VulkEase Spinning Cube Demo
 * 
 * A classic spinning textured cube using VulkEase's modern Vulkan 1.3 API
 * with shader objects, dynamic rendering, and bindless resources.
 * 
 * Build with: gcc -o cube 03_cube.c -lvulkease -lglfw -lm
 */

#include <vulkease.h>
#include <GLFW/glfw3.h>

// Platform-specific includes for GLFW window handle
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#elif defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
    #include <GLFW/glfw3native.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

// Window settings
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1280
#define WINDOW_TITLE "VulkEase - Spinning Cube"

#if DEBUG
bool vsync = true;
#else
bool vsync = false;
#endif

// Math constants
#define PI 3.14159265359f

// Vertex data structure
typedef struct Vertex {
    float pos[3];      // Position
    float texCoord[2]; // Texture coordinates
} Vertex;

// Cube vertices with proper texture coordinates for each face
static const Vertex cubeVertices[] = {
    // Front face
    {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}},
    
    // Back face  
    {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}},
    {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}},
    {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f}, {1.0f, 1.0f}},
};

// Cube indices (36 indices for 12 triangles)
static const uint16_t cubeIndices[] = {
    // Front face
    0, 1, 2,  2, 3, 0,
    // Back face
    4, 6, 5,  6, 4, 7,
    // Left face
    4, 0, 3,  3, 7, 4,
    // Right face
    1, 5, 6,  6, 2, 1,
    // Top face
    3, 2, 6,  6, 7, 3,
    // Bottom face
    4, 5, 1,  1, 0, 4,
};

// Matrix 4x4 for transformations
typedef struct Mat4 {
    float m[16];
} Mat4;

// Uniform buffer data
typedef struct UniformData {
    Mat4 mvpMatrix;
} UniformData;

// Application state
typedef struct CubeApp {
    GLFWwindow* window;
    VEContext* context;
    VEDevice* device;
    VESwapchain* swapchain;
    
    // Resources
    VEBufferAddress vertexBuffer;
    VEBufferAddress indexBuffer;
    VEBufferAddress uniformBuffer;
    VETextureIndex texture;
    VESamplerIndex sampler;
    
    // Shaders
    VEShader* vertexShader;
    VEShader* fragmentShader;
    
    // Graphics pipeline (includes shaders and render state)
    VEGraphicsPipeline* pipeline;
    
    // Render target and textures
    VERenderTarget renderTarget;
    VETextureIndex colorTexture;
    VETextureIndex depthTexture;
    
    // Animation
    float rotationAngle;
    double lastTime;
} CubeApp;

// Matrix math functions
static Mat4 mat4Identity() {
    Mat4 m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

static Mat4 mat4Perspective(float fov, float aspect, float near, float far) {
    Mat4 m = {0};
    float tanHalfFov = tanf(fov * 0.5f);

    //flipped Y for vulkan screen
    m.m[0] = 1.0f / (aspect * tanHalfFov);
    m.m[5] = -1.0f / tanHalfFov;                   // ✅ Flip Y for Vulkan
    m.m[10] = far / (near - far);                  // ✅ Z [0,1] mapping  
    m.m[11] = -1.0f;
    m.m[14] = -(far * near) / (far - near);       // ✅ Z [0,1] mapping
    return m;
}

static Mat4 mat4RotateY(float angle) {
    Mat4 m = mat4Identity();
    float c = cosf(angle);
    float s = sinf(angle);
    
    m.m[0] = c;   m.m[2] = s;
    m.m[8] = -s;  m.m[10] = c;
    
    return m;
}

static Mat4 mat4Translate(float x, float y, float z) {
    Mat4 m = mat4Identity();
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    return m;
}

static Mat4 mat4Multiply(const Mat4* a, const Mat4* b) {
    Mat4 result = {0};
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += a->m[i * 4 + k] * b->m[k * 4 + j];
            }
        }
    }
    
    return result;
}

// GLFW callbacks
static void errorCallback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(window);
    if (app && width > 0 && height > 0) {
        if (app->swapchain) {
            veResizeSwapchain(app->swapchain, (uint32_t)width, (uint32_t)height);
        }
        if (app->colorTexture != VE_INVALID_TEXTURE_INDEX) {
            veResizeRenderTarget(app->device, &app->renderTarget, (uint32_t)width, (uint32_t)height);
            app->colorTexture = app->renderTarget.attachments[0].texture;
            app->depthTexture = app->renderTarget.attachments[VE_DEPTH_ATTACHMENT_INDEX].texture;
        }
    }
}

// Initialize GLFW and create window
static bool initWindow(CubeApp* app) {
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    
    // Don't create OpenGL context - we're using Vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    app->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (!app->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }
    
    glfwSetWindowUserPointer(app->window, app);
    glfwSetWindowSizeCallback(app->window, framebufferSizeCallback);
    
    return true;
}

// Initialize VulkEase
static bool initVulkEase(CubeApp* app) {
    // Create context
    if (veCreateContext("VulkEase Cube Demo", NULL, 0, &app->context) != VE_SUCCESS || !app->context) {
        fprintf(stderr, "Failed to create VulkEase context\n");
        return false;
    }
    
    // Create device (VK_NULL_HANDLE = auto-select best GPU)
    if (veCreateDevice(app->context, VK_NULL_HANDLE, NULL, 0, &app->device) != VE_SUCCESS || !app->device) {
        fprintf(stderr, "Failed to create VulkEase device\n");
        return false;
    }

    // Set up swapchain descriptor with platform-specific surface
    VESwapchainDesc swapchainDesc = {0};
    swapchainDesc.width = WINDOW_WIDTH;
    swapchainDesc.height = WINDOW_HEIGHT;
    swapchainDesc.colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainDesc.vsync = vsync;
    swapchainDesc.debugName = "CubeSwapchain";

#if defined(_WIN32)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_WIN32;
    swapchainDesc.surface.win32.hwnd = glfwGetWin32Window(app->window);
    if (!swapchainDesc.surface.win32.hwnd) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }
#elif defined(__linux__)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_XLIB;
    swapchainDesc.surface.xlib.display = (void*)glfwGetX11Display();
    swapchainDesc.surface.xlib.window = glfwGetX11Window(app->window);
    if (!swapchainDesc.surface.xlib.display || !swapchainDesc.surface.xlib.window) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }
#elif defined(__APPLE__)
    swapchainDesc.surface.type = VE_SURFACE_TYPE_COCOA;
    swapchainDesc.surface.cocoa.window = glfwGetCocoaWindow(app->window);
    if (!swapchainDesc.surface.cocoa.window) {
        fprintf(stderr, "Failed to get native window data\n");
        return false;
    }
#endif

    if (veCreateSwapchain(app->device, &swapchainDesc, &app->swapchain) != VE_SUCCESS || !app->swapchain) {
        fprintf(stderr, "Failed to create swapchain\n");
        return false;
    }

    // Create render target for offscreen rendering (with depth buffer for cube)
    int width, height;
    glfwGetFramebufferSize(app->window, &width, &height);
    
    VEColor clearColor = {0.1f, 0.2f, 0.3f, 1.0f};
    app->renderTarget = veCreateSimpleRenderTarget(app->device,
                                                    (uint32_t)width, (uint32_t)height,
                                                    veGetSwapchainFormat(app->swapchain),
                                                    VK_FORMAT_D32_SFLOAT,  // Depth buffer
                                                    clearColor, 1.0f,
                                                    &app->colorTexture, &app->depthTexture);

    if (app->colorTexture == VE_INVALID_TEXTURE_INDEX) {
        fprintf(stderr, "Failed to create render target\n");
        return false;
    }
    
    printf("VulkEase initialized successfully\n");
    printf("Device: %s\n", veGetDeviceName(app->device));
    printf("Driver: %s\n", veGetDriverVersion(app->device));
    
    return true;
}

// Load shaders
static bool loadShaders(CubeApp* app) {
    // Load vertex shader
    if (veLoadShaderFromFile(app->device, "examples/shaders/cube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, "main",
                             "CubeVertexShader", &app->vertexShader) != VE_SUCCESS ||
        !app->vertexShader) {
        fprintf(stderr, "Failed to load vertex shader\n");
        return false;
    }
    
    // Load fragment shader
    if (veLoadShaderFromFile(app->device, "examples/shaders/cube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main",
                             "CubeFragmentShader", &app->fragmentShader) != VE_SUCCESS ||
        !app->fragmentShader) {
        fprintf(stderr, "Failed to load fragment shader\n");
        return false;
    }
    
    return true;
}

// Create buffers and geometry
static bool createGeometry(CubeApp* app) {
    // Create vertex buffer
    if (veCreateVertexBuffer(app->device, cubeVertices, sizeof(cubeVertices), "CubeVertices", &app->vertexBuffer) != VE_SUCCESS ||
        app->vertexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create vertex buffer\n");
        return false;
    }
    
    // Create index buffer  
    if (veCreateIndexBuffer(app->device, cubeIndices, sizeof(cubeIndices), "CubeIndices", &app->indexBuffer) != VE_SUCCESS ||
        app->indexBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create index buffer\n");
        return false;
    }
    
    // Create uniform buffer (persistently mapped for easy updates)
    if (veCreateUniformBuffer(app->device, sizeof(UniformData), true, "CubeUniforms", &app->uniformBuffer) != VE_SUCCESS ||
        app->uniformBuffer == VE_INVALID_ADDRESS) {
        fprintf(stderr, "Failed to create uniform buffer\n");
        return false;
    }
    
    return true;
}

// Load texture
static bool loadTexture(CubeApp* app) {
    // Load texture from file
    if (veLoadTexture(app->device, "examples/data/testCard.png", VK_IMAGE_USAGE_SAMPLED_BIT, true, &app->texture) != VE_SUCCESS ||
        app->texture == VE_INVALID_TEXTURE_INDEX) {
        fprintf(stderr, "Failed to load texture\n");
        // Create a simple fallback texture if file doesn't exist
        if (veCreateTexture2D(app->device, 2, 2, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, "FallbackTexture",
                              &app->texture) != VE_SUCCESS ||
            app->texture == VE_INVALID_TEXTURE_INDEX) {
            fprintf(stderr, "Failed to create fallback texture\n");
            return false;
        }
        
        // Fill with simple checkerboard pattern
        uint32_t pixels[4] = {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF};
        VEResult result = veHostWriteTexture(app->device, app->texture, pixels, sizeof(pixels));
        if (result != VE_SUCCESS) {
            printf("Warning: Could not update fallback texture\n");
        }
    }
    
    // Create linear sampler
    if (veCreateLinearSampler(app->device, &app->sampler) != VE_SUCCESS || app->sampler == VE_INVALID_SAMPLER_INDEX) {
        fprintf(stderr, "Failed to create sampler\n");
        return false;
    }
    
    return true;
}

// Create graphics pipeline
static bool createPipeline(CubeApp* app) {
    // Define vertex input layout
    VEVertexBinding bindings[] = {
        {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            .divisor = 1
        }
    };
    
    VEVertexAttribute attributes[] = {
        {
            .location = 0,  // Position
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, pos)
        },
        {
            .location = 1,  // Texture coordinates
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, texCoord)
        }
    };
    
    // Create graphics pipeline with default state
    VEGraphicsPipelineDesc pipelineDesc = veDefaultGraphicsPipelineDesc();
    pipelineDesc.vertexShader = app->vertexShader;
    pipelineDesc.fragmentShader = app->fragmentShader;
    
    // Set vertex input
    pipelineDesc.vertexBindingCount = 1;
    pipelineDesc.vertexBindings = bindings;
    pipelineDesc.vertexAttributeCount = 2;
    pipelineDesc.vertexAttributes = attributes;
    
    pipelineDesc.debugName = "CubePipeline";
    
    if (veCreateGraphicsPipeline(app->device, &pipelineDesc, &app->pipeline) != VE_SUCCESS || !app->pipeline) {
        fprintf(stderr, "Failed to create graphics pipeline\n");
        return false;
    }
    
    return true;
}

// Update uniform buffer with current transformation matrix
static void updateUniforms(CubeApp* app) {
    // Update rotation
    double currentTime = glfwGetTime();
    float deltaTime = (float)(currentTime - app->lastTime);
    app->lastTime = currentTime;
    
    app->rotationAngle += deltaTime * 0.8f; // Rotate at 0.8 radians per second
    if (app->rotationAngle > 2.0f * PI) {
        app->rotationAngle -= 2.0f * PI;
    }
    
    // Build transformation matrices
    VkExtent2D extent = veGetSwapchainSize(app->swapchain);
    uint32_t width = extent.width;
    uint32_t height = extent.height;
    
    float aspect = (float)width / (float)height;
    Mat4 projection = mat4Perspective(PI * 0.25f, aspect, 0.1f, 100.0f);
    Mat4 view = mat4Translate(0.0f, 0.0f, -5.0f);
    Mat4 model = mat4RotateY(app->rotationAngle);
    
    Mat4 vm = mat4Multiply(&model, &view);
    Mat4 mvp = mat4Multiply(&vm, &projection);
    
    // Update uniform buffer
    UniformData uniformData = { .mvpMatrix = mvp };
    
    VEResult result = veUpdateBuffer(app->device, app->uniformBuffer, 
                                   &uniformData, sizeof(uniformData), 0);
    if (result != VE_SUCCESS) {
        fprintf(stderr, "Failed to update uniform buffer\n");
    }
}

// Render one frame
static void renderFrame(CubeApp* app) {
    // Begin command buffer
    VECommandBuffer* cmd = NULL;
    if (veBeginCommandBuffer(app->device, &cmd) != VE_SUCCESS || !cmd) {
        fprintf(stderr, "Failed to begin command buffer\n");
        return;
    }
    
    // Begin rendering (automatically handles texture transitions)
    veBeginRendering(cmd, &app->renderTarget);
    
    // Bind graphics pipeline and apply state
    veApplyGraphicsState(cmd, app->pipeline, NULL, NULL, NULL);
    
    // Set up push constants with bindless resource indices
    VEGraphicsPushConstants pushConstants = VE_INIT_GRAPHICS_PUSH_CONSTANTS();
    pushConstants.vertexBuffer = app->vertexBuffer;
    pushConstants.indexBuffer = app->indexBuffer;
    pushConstants.uniformBuffers[0] = app->uniformBuffer;
    pushConstants.textures[0] = app->texture;
    pushConstants.samplers[0] = app->sampler;
    pushConstants.activeUniformCount = 1;
    pushConstants.activeTextureCount = 1;
    pushConstants.activeSamplerCount = 1;
    pushConstants.objectScale = 1.0f;
    
    vePushConstants(cmd, &pushConstants, sizeof(pushConstants), 0);
    
    // Draw the cube
    const uint32_t indexCount = sizeof(cubeIndices) / sizeof(cubeIndices[0]);
    veBindIndexBuffer(cmd, app->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    veDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    
    // End rendering
    veEndRendering(cmd);
    
    // Blit color texture to swapchain (acquires swapchain image automatically)
    VEResult result = veBlitTextureToSwapchain(cmd, app->colorTexture, app->swapchain, VK_FILTER_LINEAR);
    if (result == VE_ERROR_SWAPCHAIN_OUT_OF_DATE) {
        // Resize callback handles this
        return;
    }
    
    // Present
    vePresentImage(app->swapchain, cmd, true);
}

// Main loop
static void mainLoop(CubeApp* app) {
    app->lastTime = glfwGetTime();
    
    bool shouldQuit = false;
    VEFrameTimingInfo timing = {0};
    
    while (!glfwWindowShouldClose(app->window) && !shouldQuit) {
        // Begin frame timing
        veBeginFrame(app->device);
        
        glfwPollEvents();
        
        // Handle window minimize
        int width, height;
        glfwGetFramebufferSize(app->window, &width, &height);
        if (width == 0 || height == 0) {
            glfwWaitEvents();
            continue;
        }
        
        updateUniforms(app);
        renderFrame(app);

        // End frame and get timing info
        veEndFrame(app->device, &timing);

        // Print debug info on first frame
        if(timing.frameNumber == 1) vePrintDebugInfo(app->device);
        
        // Save texture on frame 10
        if(timing.frameNumber == 10) veSaveTexture(app->device, app->texture, "output-00000.png");
        
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
        
        // Print profile info every 10000 frames
        if(timing.frameNumber % 10000 == 0) vePrintProfileInfo(app->device);

        if(glfwGetKey(app->window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
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
    
    veDeviceWaitIdle(app->device);
}

// Cleanup resources
static void cleanup(CubeApp* app) {
    if (app->device) {
        veDeviceWaitIdle(app->device);
    }
    
    // Destroy pipeline
    if (app->pipeline) {
        (void)veDestroyGraphicsPipeline(app->pipeline);
    }
    
    // Destroy shaders
    if (app->vertexShader) {
        (void)veDestroyShader(app->vertexShader);
    }
    if (app->fragmentShader) {
        (void)veDestroyShader(app->fragmentShader);
    }
    
    // Destroy resources
    if (app->sampler != VE_INVALID_SAMPLER_INDEX) {
        (void)veDestroySampler(app->device, app->sampler);
    }
    if (app->texture != VE_INVALID_TEXTURE_INDEX) {
        (void)veDestroyTexture(app->device, app->texture);
    }
    if (app->uniformBuffer != VE_INVALID_ADDRESS) {
        (void)veDestroyBuffer(app->device, app->uniformBuffer);
    }
    if (app->indexBuffer != VE_INVALID_ADDRESS) {
        (void)veDestroyBuffer(app->device, app->indexBuffer);
    }
    if (app->vertexBuffer != VE_INVALID_ADDRESS) {
        (void)veDestroyBuffer(app->device, app->vertexBuffer);
    }
    
    // Destroy render target textures
    if (app->colorTexture != VE_INVALID_TEXTURE_INDEX) {
        veDestroyTexture(app->device, app->colorTexture);
    }
    if (app->depthTexture != VE_INVALID_TEXTURE_INDEX) {
        veDestroyTexture(app->device, app->depthTexture);
    }
    if (app->swapchain) {
        (void)veDestroySwapchain(app->swapchain);
    }
    if (app->device) {
        (void)veDestroyDevice(app->device);
    }
    if (app->context) {
        (void)veDestroyContext(app->context);
    }
    
    // Cleanup GLFW
    if (app->window) {
        glfwDestroyWindow(app->window);
    }
    glfwTerminate();
}

// Main function
int main() {
    CubeApp app = {0};
    
    printf("VulkEase Spinning Cube Demo\n");
    printf("===========================\n");
    printf("Modern Vulkan 1.4 with shader objects and bindless resources\n\n");
    
    // Initialize everything step by step
    if (!initWindow(&app)) {
        fprintf(stderr, "Failed to initialize window\n");
        cleanup(&app);
        return EXIT_FAILURE;
    }
    
    if (!initVulkEase(&app)) {
        fprintf(stderr, "Failed to initialize VulkEase\n");
        cleanup(&app);
        return EXIT_FAILURE;
    }
    
    if (!loadShaders(&app)) {
        fprintf(stderr, "Failed to load shaders\n");
        cleanup(&app);
        return EXIT_FAILURE;
    }
    
    if (!createGeometry(&app)) {
        fprintf(stderr, "Failed to create geometry\n");
        cleanup(&app);
        return EXIT_FAILURE;
    }
    
    if (!loadTexture(&app)) {
        fprintf(stderr, "Failed to load texture\n");
        cleanup(&app);
        return EXIT_FAILURE;
    }
    
    if (!createPipeline(&app)) {
        fprintf(stderr, "Failed to create graphics pipeline\n");
        cleanup(&app);
        return EXIT_FAILURE;
    }
    
    printf("Initialization complete. Starting main loop...\n");
    printf("Controls: Close window to exit\n\n");
    
    // Run main loop
    mainLoop(&app);
    
    printf("Shutting down gracefully...\n");
    cleanup(&app);
    
    printf("VulkEase Cube Demo completed successfully!\n");
    return EXIT_SUCCESS;
}