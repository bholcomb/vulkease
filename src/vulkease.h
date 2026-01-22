/**
 * @file vulkease.h
 * @brief VulkEase 2.0 - Complete Modern Vulkan 1.4+ Bindless Graphics API
 *
 * Graphics programming API featuring:
 * - Buffer device address for all buffer access (no descriptor sets for buffers)
 * - Bindless textures and samplers via descriptor indexing
 * - Dynamic rendering (no render passes)
 * - Shader objects (no pipelines)
 * - Render configurations for lightweight state management
 * - Indirect draw and compute with GPU-driven rendering
 * - VMA integration for automatic memory management
 * - STB Image integration for texture loading
 *
 * Requirements:
 * - Vulkan 1.4+ runtime and drivers
 * - Required instance extensions:
 *   - VK_KHR_surface
 *   - VK_EXT_debug_utils
 *   - VK_KHR_get_physical_device_properties2
 *   - VK_KHR_get_surface_capabilities2
 *   - Platform surface extension (e.g. VK_KHR_win32_surface, VK_KHR_xlib_surface,
 *     VK_MVK_macos_surface)
 * - Required device extensions:
 *   - VK_KHR_swapchain
 *   - VK_EXT_shader_object
 *   - VK_EXT_extended_dynamic_state3
 *   - VK_EXT_vertex_input_dynamic_state
 *   - VK_EXT_host_image_copy
 */

#ifndef VULKEASE_H
#define VULKEASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C"
{
#endif

// =============================================================================
// VERSION & API MACROS
// =============================================================================

#define VULKEASE_VERSION_MAJOR 2
#define VULKEASE_VERSION_MINOR 0
#define VULKEASE_VERSION_PATCH 0

#define VE_MAKE_VERSION(major, minor, patch) \
   ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

#define VULKEASE_API_VERSION_2_0 VE_MAKE_VERSION(2, 0, 0)

// API decoration for shared libraries
#ifndef VULKEASE_API
#ifdef _WIN32
#ifdef VULKEASE_EXPORT
#define VULKEASE_API __declspec(dllexport)
#else
#define VULKEASE_API __declspec(dllimport)
#endif
#else
#define VULKEASE_API __attribute__((visibility("default")))
#endif
#endif

// =============================================================================
// FORWARD DECLARATIONS & HANDLES
// =============================================================================

// Opaque handle types
typedef struct VEContext VEContext;
typedef struct VEDevice VEDevice;
typedef struct VECommandBuffer VECommandBuffer;
typedef struct VESwapchain VESwapchain;
typedef struct VEShader VEShader;
typedef struct VEGraphicsPipeline VEGraphicsPipeline;
typedef struct VEDrawState VEDrawState;
typedef struct VEQueryPool VEQueryPool;

// GPU addresses for buffer device address (64-bit pointers)
typedef uint64_t VEBufferAddress;

// Bindless indices for textures and samplers (32-bit indices)
typedef uint32_t VETextureIndex;
typedef uint32_t VESamplerIndex;

// Invalid constants
#define VE_INVALID_ADDRESS 0ULL
#define VE_INVALID_TEXTURE_INDEX 0xFFFFFFFF
#define VE_INVALID_SAMPLER_INDEX 0xFFFFFFFF

// =============================================================================
// RESULT CODES & ERROR HANDLING
// =============================================================================

typedef enum VEResult
{
   VE_SUCCESS = 0,
   VE_ERROR_OUT_OF_MEMORY = 1,
   VE_ERROR_DEVICE_LOST = 2,
   VE_ERROR_INVALID_PARAMETER = 3,
   VE_ERROR_FEATURE_NOT_SUPPORTED = 4,
   VE_ERROR_UNSUPPORTED = 5,
   VE_ERROR_SHADER_COMPILATION_FAILED = 6,
   VE_ERROR_SWAPCHAIN_OUT_OF_DATE = 7,
   VE_ERROR_TRANSFER_FAILED = 8,
   VE_ERROR_NOT_FOUND = 9,
   VE_ERROR_NOT_INITIALIZED = 10,
   VE_ERROR_TIMEOUT = 11,
   VE_ERROR_UNKNOWN = 999
} VEResult;

/**
 * Convert a VulkEase VEResult to a stable string constant.
 */
VULKEASE_API const char *veResultToString(VEResult result);

// =============================================================================
// MESSAGE/DEBUG CALLBACK
// =============================================================================

typedef enum VEMessageSeverity
{
   VE_MESSAGE_SEVERITY_VERBOSE,
   VE_MESSAGE_SEVERITY_INFO,
   VE_MESSAGE_SEVERITY_WARNING,
   VE_MESSAGE_SEVERITY_ERROR
} VEMessageSeverity;

/**
 * Debug / diagnostic message callback.
 * Notes:
 * - May be invoked from any thread.
 * - Callback must be thread-safe and should avoid blocking.
 */
typedef void (*VEMessageCallback)(VEMessageSeverity severity, const char *message, void *userData);

typedef struct VEMessageCallbackDesc
{
   VEMessageCallback callback; // NULL disables callback
   void *userData;             // Passed through to callback
} VEMessageCallbackDesc;

// =============================================================================
// BASIC TYPES
// =============================================================================

// Color value for clear operations and debug labels
typedef struct VEColor
{
   float r, g, b, a;
} VEColor;

// 2D rectangle
typedef struct VERect2D
{
   int32_t x, y;
   uint32_t width, height;
} VERect2D;

// Viewport
typedef struct VEViewport
{
   float x, y;
   float width, height;
   float minDepth, maxDepth;
} VEViewport;

// =============================================================================
// RESOURCE DESCRIPTORS
// =============================================================================

// Buffer creation descriptor
typedef struct VEBufferDesc
{
   uint64_t size;
   VkBufferUsageFlags usage;
   const void *initialData;  // Optional initial data
   uint64_t initialDataSize;
   bool persistentlyMapped;  // Keep CPU-mapped for frequent updates
   const char *debugName;    // Debug name (optional)
} VEBufferDesc;

// Texture creation descriptor
typedef struct VETextureDesc
{
   uint32_t width;
   uint32_t height;
   uint32_t depth;                  // 1 for 2D textures
   uint32_t mipLevels;              // 0 = auto-generate all mips
   uint32_t arrayLayers;            // 1 for single texture
   VkFormat format;
   VkImageUsageFlags usage;
   VkSampleCountFlags sampleCount;  // For multisampled textures
   const void *initialData;         // Optional initial data
   uint64_t initialDataSize;
   const char *debugName;           // Debug name (optional)
} VETextureDesc;

// Sampler creation descriptor
typedef struct VESamplerDesc
{
   VkFilter minFilter;
   VkFilter magFilter;
   VkSamplerMipmapMode mipmapFilter;
   VkSamplerAddressMode addressModeU;
   VkSamplerAddressMode addressModeV;
   VkSamplerAddressMode addressModeW;
   float maxAnisotropy;     // 1.0 = no anisotropy
   bool compareEnable;      // For shadow mapping
   VkCompareOp compareOp;
   float minLod;
   float maxLod;
   const char *debugName;   // Debug name (optional)
} VESamplerDesc;

// =============================================================================
// COPY/TRANSFER STRUCTURES
// =============================================================================

// Region for texture-to-texture copy (1:1 copy, same format)
typedef struct VETextureCopyRegion
{
   uint32_t srcOffsetX, srcOffsetY, srcOffsetZ;
   uint32_t dstOffsetX, dstOffsetY, dstOffsetZ;
   uint32_t width, height, depth;
   uint32_t srcMipLevel, dstMipLevel;
   uint32_t srcArrayLayer, dstArrayLayer;
   uint32_t layerCount;
} VETextureCopyRegion;

// Region for texture blit (supports scaling and format conversion)
typedef struct VETextureBlitRegion
{
   int32_t srcOffsetX0, srcOffsetY0, srcOffsetZ0;  // Source min corner
   int32_t srcOffsetX1, srcOffsetY1, srcOffsetZ1;  // Source max corner
   int32_t dstOffsetX0, dstOffsetY0, dstOffsetZ0;  // Dest min corner
   int32_t dstOffsetX1, dstOffsetY1, dstOffsetZ1;  // Dest max corner
   uint32_t srcMipLevel, dstMipLevel;
   uint32_t srcArrayLayer, dstArrayLayer;
   uint32_t layerCount;
} VETextureBlitRegion;

// Region for buffer-to-texture or texture-to-buffer copy
typedef struct VEBufferTextureCopyRegion
{
   uint64_t bufferOffset;
   uint32_t bufferRowLength;    // 0 = tightly packed
   uint32_t bufferImageHeight;  // 0 = tightly packed
   uint32_t textureOffsetX, textureOffsetY, textureOffsetZ;
   uint32_t width, height, depth;
   uint32_t mipLevel;
   uint32_t arrayLayer;
   uint32_t layerCount;
} VEBufferTextureCopyRegion;

// =============================================================================
// QUERY TYPES
// =============================================================================

typedef enum VEQueryType
{
   VE_QUERY_TYPE_OCCLUSION,
   VE_QUERY_TYPE_TIMESTAMP,
   VE_QUERY_TYPE_PIPELINE_STATISTICS
} VEQueryType;

typedef struct VEQueryPoolDesc
{
   VEQueryType type;
   uint32_t queryCount;
   VkQueryPipelineStatisticFlags pipelineStatistics;  // For PIPELINE_STATISTICS type
   const char *debugName;
} VEQueryPoolDesc;

// =============================================================================
// FRAME TIMING
// =============================================================================

typedef struct VEFrameTimingInfo
{
   double frameTimeMs;     // Last frame time in milliseconds
   double avgFrameTimeMs;  // Rolling average (last N frames)
   double minFrameTimeMs;  // Min in rolling window
   double maxFrameTimeMs;  // Max in rolling window
   uint64_t frameNumber;   // Total frames rendered
   double fps;             // Instantaneous FPS
   double avgFps;          // Average FPS
} VEFrameTimingInfo;

// =============================================================================
// SURFACE & SWAPCHAIN TYPES
// =============================================================================

// Platform surface types
typedef enum VESurfaceType
{
   VE_SURFACE_TYPE_WIN32,    // Windows: HWND
   VE_SURFACE_TYPE_XLIB,     // Linux X11: Display* + Window
   VE_SURFACE_TYPE_WAYLAND,  // Linux Wayland: wl_display* + wl_surface*
   VE_SURFACE_TYPE_COCOA,    // macOS: NSWindow* (CAMetalLayer internally)
   VE_SURFACE_TYPE_ANDROID   // Android: ANativeWindow*
} VESurfaceType;

// Platform-specific surface descriptor
typedef struct VESurfaceDesc
{
   VESurfaceType type;
   union
   {
      struct
      {
         void *hwnd;  // HWND
      } win32;
      struct
      {
         void *display;         // Display*
         unsigned long window;  // Window (XID is unsigned long)
      } xlib;
      struct
      {
         void *display;  // wl_display*
         void *surface;  // wl_surface*
      } wayland;
      struct
      {
         void *window;  // NSWindow* or CAMetalLayer*
      } cocoa;
      struct
      {
         void *nativeWindow;  // ANativeWindow*
      } android;
   };
} VESurfaceDesc;

// Swapchain creation descriptor
typedef struct VESwapchainDesc
{
   VESurfaceDesc surface;
   uint32_t width;
   uint32_t height;
   VkFormat colorFormat;
   bool vsync;
   const char *debugName;  // Debug name (optional)
} VESwapchainDesc;

// Default sampler types
typedef enum VEDefaultSampler
{
   VE_DEFAULT_SAMPLER_NEAREST,
   VE_DEFAULT_SAMPLER_LINEAR,
   VE_DEFAULT_SAMPLER_ANISOTROPIC_4X,
   VE_DEFAULT_SAMPLER_ANISOTROPIC_16X,
   VE_DEFAULT_SAMPLER_SHADOW,
   VE_DEFAULT_SAMPLER_COUNT
} VEDefaultSampler;

// =============================================================================
// GRAPHICS PIPELINE TYPES
// =============================================================================

// Maximum color attachments supported
#define VE_MAX_COLOR_ATTACHMENTS 8

// Vertex input structures
typedef struct VEVertexBinding
{
   uint32_t binding;
   uint32_t stride;
   VkVertexInputRate inputRate;
   uint32_t divisor;  // For instanced rendering
} VEVertexBinding;

typedef struct VEVertexAttribute
{
   uint32_t location;
   uint32_t binding;
   VkFormat format;
   uint32_t offset;
} VEVertexAttribute;

// Color blending per-attachment
typedef struct VEBlendAttachment
{
   bool blendEnable;
   VkBlendFactor srcColorBlendFactor;
   VkBlendFactor dstColorBlendFactor;
   VkBlendOp colorBlendOp;
   VkBlendFactor srcAlphaBlendFactor;
   VkBlendFactor dstAlphaBlendFactor;
   VkBlendOp alphaBlendOp;
   VkColorComponentFlags colorWriteMask;
} VEBlendAttachment;

// Stencil operation state (front or back face)
typedef struct VEStencilOpState
{
   VkStencilOp failOp;
   VkStencilOp passOp;
   VkStencilOp depthFailOp;
   VkCompareOp compareOp;
   uint32_t compareMask;
   uint32_t writeMask;
   uint32_t reference;
} VEStencilOpState;

/**
 * Graphics Pipeline Descriptor
 *
 * Combines shaders, vertex input, and material-level state into a single object.
 * This is the "how should this class of objects be rendered" configuration.
 *
 * Changes per: material/technique (opaque PBR, transparent glass, shadow caster)
 */
typedef struct VEGraphicsPipelineDesc
{
   // === SHADERS ===
   VEShader *vertexShader;
   VEShader *fragmentShader;
   VEShader *geometryShader;     // Optional
   VEShader *tessControlShader;  // Optional
   VEShader *tessEvalShader;     // Optional

   // === VERTEX INPUT ===
   uint32_t vertexBindingCount;
   const VEVertexBinding *vertexBindings;
   uint32_t vertexAttributeCount;
   const VEVertexAttribute *vertexAttributes;

   // === DEPTH CONFIG ===
   bool depthTestEnable;
   bool depthWriteEnable;
   VkCompareOp depthCompareOp;
   bool depthBoundsTestEnable;
   float minDepthBounds;
   float maxDepthBounds;

   // === STENCIL CONFIG ===
   bool stencilTestEnable;
   VEStencilOpState frontStencil;
   VEStencilOpState backStencil;

   // === BLEND CONFIG ===
   bool logicOpEnable;
   VkLogicOp logicOp;
   uint32_t blendAttachmentCount;
   VEBlendAttachment blendAttachments[VE_MAX_COLOR_ATTACHMENTS];
   float blendConstants[4];

   // === RASTERIZATION (material-level defaults) ===
   VkCullModeFlags cullMode;
   VkFrontFace frontFace;

   // === MULTISAMPLING ===
   bool sampleShadingEnable;
   float minSampleShading;

   const char *debugName;
} VEGraphicsPipelineDesc;

/**
 * Draw State Descriptor
 *
 * Per-draw dynamic state for geometry interpretation and overrides.
 * All fields are dynamic state in Vulkan 1.3+ and can vary per draw call.
 *
 * Changes per: draw call or renderable type
 */
typedef struct VEDrawStateDesc
{
   // === GEOMETRY INTERPRETATION ===
   VkPrimitiveTopology topology;
   bool primitiveRestartEnable;
   uint32_t patchControlPoints;  // For tessellation

   // === RASTERIZATION OVERRIDES ===
   VkPolygonMode polygonMode;     // Fill, line, point
   float lineWidth;
   VkCullModeFlags cullMode;      // Override pipeline default (VK_CULL_MODE_FLAG_BITS_MAX_ENUM = use pipeline)
   VkFrontFace frontFace;         // Override pipeline default
   bool rasterizerDiscardEnable;

   // === DEPTH BIAS (polygon offset) ===
   bool depthBiasEnable;
   float depthBiasConstantFactor;
   float depthBiasClamp;
   float depthBiasSlopeFactor;
   bool depthClampEnable;

   // === COVERAGE ===
   bool alphaToCoverageEnable;
   bool alphaToOneEnable;

   const char *debugName;
} VEDrawStateDesc;

// Special value indicating "use pipeline default" for cull mode override
#define VE_CULL_MODE_USE_PIPELINE VK_CULL_MODE_FLAG_BITS_MAX_ENUM

// =============================================================================
// RENDERING TYPES
// =============================================================================

// Indices for depth and stencil in the attachments array
#define VE_DEPTH_ATTACHMENT_INDEX VE_MAX_COLOR_ATTACHMENTS
#define VE_STENCIL_ATTACHMENT_INDEX (VE_MAX_COLOR_ATTACHMENTS + 1)
#define VE_TOTAL_ATTACHMENT_SLOTS (VE_MAX_COLOR_ATTACHMENTS + 2)

// Render target attachment (for dynamic rendering)
typedef struct VERenderTargetAttachment
{
   VETextureIndex texture;         // Texture to render to
   VkAttachmentLoadOp loadOp;
   VkAttachmentStoreOp storeOp;
   VEColor clearValue;             // Used if loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR
   VETextureIndex resolveTexture;  // For MSAA resolve (optional)
} VERenderTargetAttachment;

// Render target configuration (replaces render passes)
// Trivially copyable struct with inline storage for all attachments.
// Layout of attachments array:
//   [0..colorAttachmentCount-1] = color attachments
//   [VE_DEPTH_ATTACHMENT_INDEX] = depth attachment (if hasDepthAttachment)
//   [VE_STENCIL_ATTACHMENT_INDEX] = stencil attachment (if hasStencilAttachment)
typedef struct VERenderTarget
{
   int32_t renderAreaX, renderAreaY;
   uint32_t renderAreaWidth, renderAreaHeight;

   uint32_t colorAttachmentCount;
   bool hasDepthAttachment;
   bool hasStencilAttachment;

   // All attachments stored inline - trivially copyable
   VERenderTargetAttachment attachments[VE_TOTAL_ATTACHMENT_SLOTS];
} VERenderTarget;

// =============================================================================
// SYNCHRONIZATION TYPES
// =============================================================================

typedef struct VEMemoryBarrier
{
   VkPipelineStageFlags2 srcStageMask;
   VkAccessFlags2 srcAccessMask;
   VkPipelineStageFlags2 dstStageMask;
   VkAccessFlags2 dstAccessMask;
} VEMemoryBarrier;

typedef struct VEBufferBarrier
{
   VkPipelineStageFlags2 srcStageMask;
   VkAccessFlags2 srcAccessMask;
   VkPipelineStageFlags2 dstStageMask;
   VkAccessFlags2 dstAccessMask;
   uint32_t srcQueueFamilyIndex;
   uint32_t dstQueueFamilyIndex;
   VEBufferAddress buffer;
   uint64_t offset;
   uint64_t size;
} VEBufferBarrier;

typedef struct VEImageBarrier
{
   VkPipelineStageFlags2 srcStageMask;
   VkAccessFlags2 srcAccessMask;
   VkPipelineStageFlags2 dstStageMask;
   VkAccessFlags2 dstAccessMask;
   uint32_t srcQueueFamilyIndex;
   uint32_t dstQueueFamilyIndex;
   VkImageLayout oldLayout;
   VkImageLayout newLayout;
   VETextureIndex image;
   VkImageSubresourceRange subresourceRange;
} VEImageBarrier;

typedef struct VEBarrierDesc
{
   uint32_t memoryBarrierCount;
   const VEMemoryBarrier *memoryBarriers;
   uint32_t bufferBarrierCount;
   const VEBufferBarrier *bufferBarriers;
   uint32_t imageBarrierCount;
   const VEImageBarrier *imageBarriers;
} VEBarrierDesc;

// =============================================================================
// SUBMISSION TYPES
// =============================================================================

// Submission info for advanced synchronization
typedef struct VESubmitInfo
{
   const VkSemaphore *waitSemaphores;            // Semaphores to wait on (optional)
   const uint64_t *waitSemaphoreValues;          // Timeline semaphore wait values (optional)
   const VkPipelineStageFlags2 *waitStageMasks;  // Pipeline stages for each wait semaphore
   uint32_t waitSemaphoreCount;

   const VkSemaphore *signalSemaphores;          // Semaphores to signal (optional)
   const uint64_t *signalSemaphoreValues;        // Timeline semaphore signal values (optional)
   uint32_t signalSemaphoreCount;

   VkFence fence;            // Fence to signal on completion (optional)
   bool waitForCompletion;   // True to block until GPU work completes
} VESubmitInfo;

// Secondary command buffer descriptor
typedef struct VESecondaryCommandBufferDesc
{
   VkCommandBufferUsageFlags usageFlags;     // VK_COMMAND_BUFFER_USAGE_*
   uint32_t colorAttachmentCount;            // Up to 8 color attachments
   VkFormat colorAttachmentFormats[8];       // Color formats for inheritance
   VkFormat depthAttachmentFormat;           // Depth format (VK_FORMAT_UNDEFINED if none)
   VkFormat stencilAttachmentFormat;         // Stencil format (VK_FORMAT_UNDEFINED if none)
   VkSampleCountFlags rasterizationSamples;  // Sample count (0 defaults to 1)
   uint32_t viewMask;                        // Multiview mask (0 disables multiview)
   bool occlusionQueryEnable;                // Enable occlusion queries for this secondary
   VkQueryControlFlags occlusionQueryFlags;  // VkQueryControlFlags
   bool beginRecording;                      // Begin recording automatically when true
   int32_t renderAreaX;                      // Render area X offset
   int32_t renderAreaY;                      // Render area Y offset
   uint32_t renderAreaWidth;                 // Render area width
   uint32_t renderAreaHeight;                // Render area height
} VESecondaryCommandBufferDesc;

// =============================================================================
// STATISTICS TYPES
// =============================================================================

// Performance statistics
typedef struct VEPerformanceStats
{
   uint64_t frameTime;          // GPU time in nanoseconds
   uint32_t drawCalls;
   uint32_t computeDispatches;
   uint64_t verticesRendered;
   uint64_t trianglesRendered;
   uint32_t pipelineBinds;      // For legacy compatibility
   uint32_t descriptorBinds;    // For legacy compatibility
} VEPerformanceStats;

// Memory statistics
typedef struct VEMemoryStats
{
   uint64_t totalAllocated;  // Total VMA allocations in bytes
   uint64_t totalUsed;       // Actually used memory in bytes
   uint32_t bufferCount;     // Number of active buffers
   uint32_t textureCount;    // Number of active textures
   uint32_t samplerCount;    // Number of active samplers

   // Per-heap statistics
   struct
   {
      uint64_t size;
      uint64_t used;
      uint64_t budget;
   } heaps[16];
} VEMemoryStats;

// Graphics pipeline statistics
typedef struct VEPipelineStats
{
   uint32_t totalPipelines;
   uint32_t activePipelines;
   uint32_t pipelineBinds;
   uint32_t drawStateChanges;
   uint32_t individualOverrides;
} VEPipelineStats;

// =============================================================================
// PUSH CONSTANTS & HELPER MACROS
// =============================================================================

/**
 * @brief Example push constants structure for graphics shaders - 256 bytes.
 *
 * This is an EXAMPLE layout used by VulkEase's built-in shaders. You are NOT
 * required to use this structure. You may define your own push constant layout
 * with any fields you need, as long as the total size does not exceed 256 bytes
 * (the minimum guaranteed by Vulkan spec via maxPushConstantsSize).
 *
 * Use vePushConstants() to upload your push constant data to shaders.
 */
typedef struct VEGraphicsPushConstants
{
   VEBufferAddress vertexBuffer;       // 8 bytes - vertex data address
   VEBufferAddress indexBuffer;        // 8 bytes - index data address (optional)
   VEBufferAddress uniformBuffers[8];  // 64 bytes - up to 8 uniform buffer addresses
   VETextureIndex textures[16];        // 64 bytes - up to 16 bindless texture indices
   VESamplerIndex samplers[16];        // 64 bytes - up to 16 bindless sampler indices
   float objectScale;                  // 4 bytes - per-object scale
   uint32_t activeTextureCount;        // 4 bytes - number of active textures (0-16)
   uint32_t activeSamplerCount;        // 4 bytes - number of active samplers (0-16)
   uint32_t activeUniformCount;        // 4 bytes - number of active uniform buffers (0-8)
   uint32_t reserved[7];               // 28 bytes - reserved for future use
} VEGraphicsPushConstants;             // Total: 256 bytes

/**
 * @brief Example push constants structure for compute shaders - 256 bytes.
 *
 * This is an EXAMPLE layout. You may define your own push constant structure
 * with any fields you need, as long as the total size does not exceed 256 bytes.
 */
typedef struct VEComputePushConstants
{
   VEBufferAddress buffers[16];   // 128 bytes - up to 16 buffer addresses
   VETextureIndex textures[16];   // 64 bytes - up to 16 texture indices
   VESamplerIndex samplers[8];    // 32 bytes - up to 8 sampler indices
   uint32_t elementCount;         // 4 bytes - number of elements to process
   uint32_t activeBufferCount;    // 4 bytes - number of active buffers (0-16)
   uint32_t activeTextureCount;   // 4 bytes - number of active textures (0-16)
   uint32_t activeSamplerCount;   // 4 bytes - number of active samplers (0-8)
   uint32_t reserved[4];          // 16 bytes - reserved for future use
} VEComputePushConstants;         // Total: 256 bytes

/**
 * @brief Initialize VEGraphicsPushConstants with safe defaults.
 *
 * Example macro for use with the example VEGraphicsPushConstants structure.
 */
#define VE_INIT_GRAPHICS_PUSH_CONSTANTS()                                                      \
   {                                                                                           \
      .vertexBuffer = VE_INVALID_ADDRESS, .indexBuffer = VE_INVALID_ADDRESS,                   \
      .uniformBuffers = {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,           \
                         VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,           \
                         VE_INVALID_ADDRESS, VE_INVALID_ADDRESS},                              \
      .textures = {VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX},                                                  \
      .samplers = {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX},                                                  \
      .objectScale = 1.0f, .activeTextureCount = 0, .activeSamplerCount = 0,                   \
      .activeUniformCount = 0, .reserved = {0, 0, 0, 0, 0, 0, 0}                               \
   }

/**
 * @brief Initialize VEComputePushConstants with safe defaults.
 *
 * Example macro for use with the example VEComputePushConstants structure.
 */
#define VE_INIT_COMPUTE_PUSH_CONSTANTS()                                                       \
   {                                                                                           \
      .buffers = {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, \
                  VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, \
                  VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, \
                  VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS}, \
      .textures = {VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX},                                                  \
      .samplers = {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX},                        \
      .elementCount = 0, .activeBufferCount = 0, .activeTextureCount = 0,                      \
      .activeSamplerCount = 0, .reserved = {0, 0, 0, 0}                                        \
   }

// =============================================================================
// Color Constants
// =============================================================================
// Predefined VEColor constants for common use cases such as clear colors,
// debug visualization, and UI elements. All colors use full opacity (alpha=1.0).

// --- Primary colors ---
#define VE_COLOR_BLACK   ((VEColor){0.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_WHITE   ((VEColor){1.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_RED     ((VEColor){1.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_GREEN   ((VEColor){0.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_BLUE    ((VEColor){0.0f, 0.0f, 1.0f, 1.0f})

// --- Secondary colors ---
#define VE_COLOR_YELLOW  ((VEColor){1.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_CYAN    ((VEColor){0.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_MAGENTA ((VEColor){1.0f, 0.0f, 1.0f, 1.0f})

// --- Extended colors ---
#define VE_COLOR_ORANGE  ((VEColor){1.0f, 0.5f, 0.0f, 1.0f})
#define VE_COLOR_PURPLE  ((VEColor){0.5f, 0.0f, 1.0f, 1.0f})
#define VE_COLOR_PINK    ((VEColor){1.0f, 0.4f, 0.7f, 1.0f})
#define VE_COLOR_BROWN   ((VEColor){0.6f, 0.3f, 0.1f, 1.0f})
#define VE_COLOR_LIME    ((VEColor){0.5f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_TEAL    ((VEColor){0.0f, 0.5f, 0.5f, 1.0f})
#define VE_COLOR_NAVY    ((VEColor){0.0f, 0.0f, 0.5f, 1.0f})
#define VE_COLOR_MAROON  ((VEColor){0.5f, 0.0f, 0.0f, 1.0f})

// --- Grayscale ---
#define VE_COLOR_GRAY_DARK  ((VEColor){0.25f, 0.25f, 0.25f, 1.0f})
#define VE_COLOR_GRAY       ((VEColor){0.5f, 0.5f, 0.5f, 1.0f})
#define VE_COLOR_GRAY_LIGHT ((VEColor){0.75f, 0.75f, 0.75f, 1.0f})

// --- Transparent ---
#define VE_COLOR_TRANSPARENT ((VEColor){0.0f, 0.0f, 0.0f, 0.0f})

// --- Common clear colors ---
#define VE_COLOR_CORNFLOWER_BLUE ((VEColor){0.392f, 0.584f, 0.929f, 1.0f})  // Classic XNA/DirectX clear color
#define VE_COLOR_DARK_GRAY       ((VEColor){0.1f, 0.1f, 0.1f, 1.0f})        // Good for dark themes

// =============================================================================
// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================
// =============================================================================

// =============================================================================
// CONTEXT & DEVICE FUNCTIONS
// =============================================================================

/**
 * @brief Get the VulkEase library version.
 *
 * Returns the version number encoded using VE_MAKE_VERSION().
 * Use VULKEASE_VERSION_MAJOR, VULKEASE_VERSION_MINOR, VULKEASE_VERSION_PATCH
 * to decode if needed.
 *
 * @return Version number encoded as (major << 22) | (minor << 12) | patch
 */
VULKEASE_API uint32_t veGetVersion(void);

/**
 * @brief Create a VulkEase context, initializing the Vulkan 1.4 instance.
 *
 * The context is the root object for VulkEase. It owns the VkInstance and
 * must outlive all devices created from it. Automatically enables all
 * required Vulkan extensions.
 *
 * @param[in] applicationName Application name shown in debug tools and driver info.
 *                            Can be NULL for a default name.
 * @param[in] additionalInstanceExtensions Optional array of additional instance extension
 *                                         names to enable. Can be NULL if count is 0.
 * @param[in] additionalInstanceExtensionCount Number of additional extensions in the array.
 * @param[out] outContext Receives the created context handle on success.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if outContext is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *         - VE_ERROR_FEATURE_NOT_SUPPORTED if required Vulkan version/extensions unavailable
 *
 * @note Call veDestroyContext() when done, after destroying all child devices.
 * @see veDestroyContext, veCreateDevice
 */
VULKEASE_API VEResult veCreateContext(const char *applicationName,
                                      const char *const *additionalInstanceExtensions,
                                      uint32_t additionalInstanceExtensionCount,
                                      VEContext **outContext);

/**
 * @brief Destroy a VulkEase context and release all associated resources.
 *
 * All devices created from this context must be destroyed before calling this.
 *
 * @param[in] context Context handle to destroy. Can be NULL (no-op).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if context is invalid
 *
 * @see veCreateContext
 */
VULKEASE_API VEResult veDestroyContext(VEContext *context);

/**
 * @brief Enumerate the number of available physical devices (GPUs).
 *
 * Call this before veGetPhysicalDeviceInfo() to determine how many devices
 * are available for selection.
 *
 * @param[in] context Valid VulkEase context.
 * @param[out] count Receives the number of available physical devices.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if context or count is NULL
 *
 * @see veGetPhysicalDeviceInfo, veCreateDevice
 */
VULKEASE_API VEResult veEnumeratePhysicalDevices(VEContext *context, uint32_t *count);

/**
 * @brief Get information about a specific physical device.
 *
 * Retrieves the Vulkan physical device handle, name, and type for display
 * or selection purposes.
 *
 * @param[in] context Valid VulkEase context.
 * @param[in] deviceIndex Index of the device (0 to count-1 from veEnumeratePhysicalDevices).
 * @param[out] physicalDevice Receives the VkPhysicalDevice handle. Can be NULL if not needed.
 * @param[out] deviceName Buffer to receive device name string. Should be at least 256 bytes.
 *                        Can be NULL if not needed.
 * @param[out] deviceType Receives the device type enum. Can be NULL if not needed.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if context is NULL or deviceIndex is out of range
 *
 * @see veEnumeratePhysicalDevices, veCreateDevice
 */
VULKEASE_API VEResult veGetPhysicalDeviceInfo(VEContext *context, uint32_t deviceIndex,
                                              VkPhysicalDevice *physicalDevice,
                                              char *deviceName,
                                              VkPhysicalDeviceType *deviceType);

/**
 * @brief Create a VulkEase device from the context.
 *
 * Creates a logical device for rendering operations. If preferredDevice is
 * VK_NULL_HANDLE, automatically selects the best available GPU (prefers
 * discrete GPUs over integrated).
 *
 * @param[in] context Valid VulkEase context.
 * @param[in] preferredDevice VkPhysicalDevice to use, or VK_NULL_HANDLE for auto-selection.
 * @param[in] additionalDeviceExtensions Optional array of additional device extension
 *                                       names to enable. Can be NULL if count is 0.
 * @param[in] additionalDeviceExtensionCount Number of additional extensions in the array.
 * @param[out] outDevice Receives the created device handle on success.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if context or outDevice is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *         - VE_ERROR_FEATURE_NOT_SUPPORTED if required features unavailable
 *
 * @note Call veDestroyDevice() when done, before destroying the context.
 * @see veDestroyDevice, veEnumeratePhysicalDevices
 */
VULKEASE_API VEResult veCreateDevice(VEContext *context,
                                     VkPhysicalDevice preferredDevice,
                                     const char *const *additionalDeviceExtensions,
                                     uint32_t additionalDeviceExtensionCount,
                                     VEDevice **outDevice);

/**
 * @brief Destroy a VulkEase device and release all associated resources.
 *
 * All resources created from this device (buffers, textures, shaders, etc.)
 * should be destroyed before calling this.
 *
 * @param[in] device Device handle to destroy. Can be NULL (no-op).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is invalid
 *
 * @see veCreateDevice
 */
VULKEASE_API VEResult veDestroyDevice(VEDevice *device);

/**
 * @brief Check if a Vulkan instance extension is available.
 *
 * Query availability before creating a context if you need specific extensions.
 *
 * @param[in] extensionName Name of the extension to check (e.g., "VK_KHR_surface").
 *
 * @return true if the extension is available, false otherwise.
 */
VULKEASE_API bool veIsInstanceExtensionAvailable(const char *extensionName);

/**
 * @brief Check if a Vulkan device extension is available.
 *
 * Query availability before creating a device if you need specific extensions.
 *
 * @param[in] context Valid VulkEase context.
 * @param[in] extensionName Name of the extension to check (e.g., "VK_KHR_swapchain").
 *
 * @return true if the extension is available, false otherwise or if context is NULL.
 */
VULKEASE_API bool veIsDeviceExtensionAvailable(VEContext *context, const char *extensionName);

/**
 * @brief Wait for the device to become idle.
 *
 * Blocks until all GPU operations on all queues have completed. Useful for
 * cleanup or synchronization points.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL
 *         - VE_ERROR_DEVICE_LOST if the device was lost
 *
 * @warning This is a heavy synchronization operation. Avoid in performance-critical paths.
 */
VULKEASE_API VEResult veDeviceWaitIdle(VEDevice *device);

/**
 * @brief Get the GPU device name as a human-readable string.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return Device name string, or NULL if device is invalid.
 *         The string is owned by VulkEase and valid until the device is destroyed.
 */
VULKEASE_API const char *veGetDeviceName(VEDevice *device);

/**
 * @brief Get the GPU driver version as a human-readable string.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return Driver version string, or NULL if device is invalid.
 *         The string is owned by VulkEase and valid until the device is destroyed.
 */
VULKEASE_API const char *veGetDriverVersion(VEDevice *device);

/**
 * @brief Get the Vulkan API version supported by the device.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return Vulkan version encoded as VK_MAKE_API_VERSION(), or 0 if device is invalid.
 */
VULKEASE_API uint32_t veGetVulkanVersion(VEDevice *device);

// =============================================================================
// VULKAN HANDLE ACCESS (Escape Hatches)
// =============================================================================

/**
 * @brief Get the underlying VkInstance handle.
 *
 * Provides access to the raw Vulkan instance for custom extension use or
 * interop with other Vulkan libraries.
 *
 * @param[in] context Valid VulkEase context.
 *
 * @return VkInstance handle, or VK_NULL_HANDLE if context is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Do not destroy it directly.
 *          Lifetime matches the owning VEContext.
 */
VULKEASE_API VkInstance veGetVkInstance(VEContext *context);

/**
 * @brief Get the underlying VkPhysicalDevice handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkPhysicalDevice handle, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkPhysicalDevice veGetVkPhysicalDevice(VEDevice *device);

/**
 * @brief Get the underlying VkDevice handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkDevice handle, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Do not destroy it directly.
 *          Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkDevice veGetVkDevice(VEDevice *device);

/**
 * @brief Get the graphics queue handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkQueue handle for graphics operations, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkQueue veGetVkGraphicsQueue(VEDevice *device);

/**
 * @brief Get the compute queue handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkQueue handle for compute operations, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkQueue veGetVkComputeQueue(VEDevice *device);

/**
 * @brief Get the transfer queue handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkQueue handle for transfer operations, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkQueue veGetVkTransferQueue(VEDevice *device);

/**
 * @brief Get the fence associated with a command buffer.
 *
 * Each command buffer has an associated fence for synchronization purposes.
 *
 * @param[in] cmd Valid VulkEase command buffer.
 *
 * @return VkFence handle, or VK_NULL_HANDLE if cmd is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VECommandBuffer.
 */
VULKEASE_API VkFence veGetVkCommandBufferFence(VECommandBuffer *cmd);

/**
 * @brief Get the VkBuffer handle from a buffer device address.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer device address obtained from veCreateBuffer().
 *
 * @return VkBuffer handle, or VK_NULL_HANDLE if device is NULL or address is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the buffer.
 */
VULKEASE_API VkBuffer veGetVkBufferFromAddress(VEDevice *device, VEBufferAddress address);

/**
 * @brief Get the VkImage handle from a texture index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture index obtained from veCreateTexture().
 *
 * @return VkImage handle, or VK_NULL_HANDLE if device is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the texture.
 */
VULKEASE_API VkImage veGetVkImageFromTexture(VEDevice *device, VETextureIndex texture);

/**
 * @brief Get the VkImageView handle from a texture index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture index obtained from veCreateTexture().
 *
 * @return VkImageView handle, or VK_NULL_HANDLE if device is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the texture.
 */
VULKEASE_API VkImageView veGetVkImageViewFromTexture(VEDevice *device, VETextureIndex texture);

/**
 * @brief Get the VkSampler handle from a sampler index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] sampler Sampler index obtained from veCreateSampler().
 *
 * @return VkSampler handle, or VK_NULL_HANDLE if device is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the sampler.
 */
VULKEASE_API VkSampler veGetVkSamplerFromIndex(VEDevice *device, VESamplerIndex sampler);

/**
 * @brief Get the VkSwapchainKHR handle from a swapchain.
 *
 * @param[in] swapchain Valid VulkEase swapchain.
 *
 * @return VkSwapchainKHR handle, or VK_NULL_HANDLE if swapchain is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the swapchain.
 */
VULKEASE_API VkSwapchainKHR veGetVkSwapchain(VESwapchain *swapchain);

/**
 * @brief Get a swapchain image by index.
 *
 * @param[in] swapchain Valid VulkEase swapchain.
 * @param[in] imageIndex Index of the swapchain image (0 to image count - 1).
 *
 * @return VkImage handle, or VK_NULL_HANDLE if swapchain is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the swapchain.
 */
VULKEASE_API VkImage veGetVkSwapchainImage(VESwapchain *swapchain, uint32_t imageIndex);

/**
 * @brief Get a swapchain image view by index.
 *
 * @param[in] swapchain Valid VulkEase swapchain.
 * @param[in] imageIndex Index of the swapchain image (0 to image count - 1).
 *
 * @return VkImageView handle, or VK_NULL_HANDLE if swapchain is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the swapchain.
 */
VULKEASE_API VkImageView veGetVkSwapchainImageView(VESwapchain *swapchain, uint32_t imageIndex);

// =============================================================================
// MESSAGE CALLBACK FUNCTIONS
// =============================================================================

/**
 * @brief Set the global debug/diagnostic message callback.
 *
 * VulkEase emits diagnostic messages for errors, warnings, and informational
 * events. Use this to capture messages for logging or debugging.
 *
 * @param[in] desc Callback descriptor containing the callback function and user data.
 *                 Set callback to NULL to disable the callback.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if desc is NULL
 *
 * @note The callback may be invoked from any thread. Ensure your callback
 *       implementation is thread-safe and non-blocking.
 * @see VEMessageCallbackDesc, veSetMinMessageSeverity
 */
VULKEASE_API VEResult veSetMessageCallback(const VEMessageCallbackDesc *desc);

/**
 * @brief Set the minimum message severity that will be emitted.
 *
 * Messages below this severity level will be filtered out and not delivered
 * to the callback. Default is VE_MESSAGE_SEVERITY_WARNING.
 *
 * @param[in] minSeverity Minimum severity level to emit.
 *
 * @return VE_SUCCESS on success.
 *
 * @see VEMessageSeverity, veSetMessageCallback
 */
VULKEASE_API VEResult veSetMinMessageSeverity(VEMessageSeverity minSeverity);

// =============================================================================
// FENCE FUNCTIONS
// =============================================================================

/**
 * @brief Create a fence for GPU-CPU synchronization.
 *
 * Fences are used to synchronize the CPU with GPU operations. They can be
 * passed to async transfer operations or used for custom synchronization.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] signaled If true, the fence starts in signaled state.
 *                     Set to true when you want to skip the first wait.
 * @param[out] outFence Receives the VkFence handle on success.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device or outFence is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @note Call veDestroyFence() when done.
 * @see veDestroyFence, veWaitFence, veResetFence
 */
VULKEASE_API VEResult veCreateFence(VEDevice *device, bool signaled, VkFence *outFence);

/**
 * @brief Destroy a fence.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] fence Fence handle to destroy. Can be VK_NULL_HANDLE (no-op).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL
 *
 * @see veCreateFence
 */
VULKEASE_API VEResult veDestroyFence(VEDevice *device, VkFence fence);

/**
 * @brief Wait for a fence to be signaled.
 *
 * Blocks the calling thread until the fence is signaled or the timeout expires.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] fence Fence to wait on.
 * @param[in] timeout Maximum time to wait in nanoseconds. Use UINT64_MAX to wait forever.
 *
 * @return VE_SUCCESS if the fence was signaled, or:
 *         - VE_ERROR_TIMEOUT if the timeout was reached before the fence signaled
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or fence is invalid
 *         - VE_ERROR_DEVICE_LOST if the device was lost
 *
 * @see veGetFenceStatus, veResetFence
 */
VULKEASE_API VEResult veWaitFence(VEDevice *device, VkFence fence, uint64_t timeout);

/**
 * @brief Check if a fence is signaled without blocking.
 *
 * Non-blocking query of fence status. Useful for polling-based synchronization.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] fence Fence to check.
 * @param[out] signaled Receives true if the fence is signaled, false otherwise.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device or signaled is NULL
 *         - VE_ERROR_DEVICE_LOST if the device was lost
 *
 * @see veWaitFence, veResetFence
 */
VULKEASE_API VEResult veGetFenceStatus(VEDevice *device, VkFence fence, bool *signaled);

/**
 * @brief Reset a fence to unsignaled state.
 *
 * Must be called before reusing a fence for another wait operation.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] fence Fence to reset.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or fence is invalid
 *
 * @see veCreateFence, veWaitFence
 */
VULKEASE_API VEResult veResetFence(VEDevice *device, VkFence fence);

// =============================================================================
// BUFFER FUNCTIONS
// =============================================================================

/**
 * @brief Create a buffer and return its GPU device address.
 *
 * Creates a new buffer with the specified properties. The buffer is accessible
 * via its device address for bindless rendering in shaders.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Buffer descriptor specifying size, usage, and optional initial data.
 * @param[out] outAddress Receives the GPU device address of the created buffer.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outAddress is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @note The returned address is valid until veDestroyBuffer() is called.
 * @see VEBufferDesc, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateBuffer(VEDevice *device, const VEBufferDesc *desc, VEBufferAddress *outAddress);

/**
 * @brief Destroy a buffer by its device address.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer address to destroy. VE_INVALID_ADDRESS is a no-op.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or address is invalid
 *
 * @see veCreateBuffer
 */
VULKEASE_API VEResult veDestroyBuffer(VEDevice *device, VEBufferAddress address);

/**
 * @brief Map a buffer for CPU access.
 *
 * Maps the buffer memory to a CPU-accessible pointer. Only valid for buffers
 * that were not created with persistentlyMapped=true.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer address to map.
 * @param[out] mappedData Receives a pointer to the mapped memory.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device or mappedData is NULL
 *         - VE_ERROR_INVALID_PARAMETER if the buffer is already mapped
 *
 * @note Call veUnmapBuffer() when done accessing the data.
 * @see veUnmapBuffer
 */
VULKEASE_API VEResult veMapBuffer(VEDevice *device, VEBufferAddress address, void **mappedData);

/**
 * @brief Unmap a previously mapped buffer.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer address to unmap.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or buffer is not mapped
 *
 * @see veMapBuffer
 */
VULKEASE_API VEResult veUnmapBuffer(VEDevice *device, VEBufferAddress address);

/**
 * @brief Update buffer data from the CPU.
 *
 * Copies data from CPU memory to the buffer. Works with both mapped and
 * unmapped buffers; handles staging internally for GPU-only memory.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer address to update.
 * @param[in] data Pointer to source data.
 * @param[in] size Number of bytes to copy.
 * @param[in] offset Byte offset into the buffer.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device or data is NULL
 *         - VE_ERROR_INVALID_PARAMETER if offset + size exceeds buffer size
 *
 * @see veMapBuffer
 */
VULKEASE_API VEResult veUpdateBuffer(VEDevice *device, VEBufferAddress address, const void *data,
                                     uint64_t size, uint64_t offset);

/**
 * @brief Get the size of a buffer in bytes.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer address to query.
 *
 * @return Buffer size in bytes, or 0 if device is NULL or address is invalid.
 */
VULKEASE_API uint64_t veGetBufferSize(VEDevice *device, VEBufferAddress address);

/**
 * @brief Get the usage flags of a buffer.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer address to query.
 *
 * @return VkBufferUsageFlags, or 0 if device is NULL or address is invalid.
 */
VULKEASE_API VkBufferUsageFlags veGetBufferUsage(VEDevice *device, VEBufferAddress address);

/**
 * @brief Create a vertex buffer with initial data.
 *
 * Convenience function that creates a buffer optimized for vertex data with
 * appropriate usage flags and uploads the initial data.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertices Pointer to vertex data to upload.
 * @param[in] size Size of vertex data in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateVertexBuffer(VEDevice *device, const void *vertices, uint64_t size,
                                           const char *debugName, VEBufferAddress *outAddress);

/**
 * @brief Create an index buffer with initial data.
 *
 * Convenience function that creates a buffer optimized for index data with
 * appropriate usage flags and uploads the initial data.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] indices Pointer to index data to upload.
 * @param[in] size Size of index data in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateIndexBuffer(VEDevice *device, const void *indices, uint64_t size,
                                          const char *debugName, VEBufferAddress *outAddress);

/**
 * @brief Create a uniform buffer.
 *
 * Convenience function that creates a buffer optimized for uniform data
 * (shader constants) with appropriate usage flags.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Size of buffer in bytes.
 * @param[in] persistentlyMapped If true, the buffer remains mapped for efficient updates.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateUniformBuffer(VEDevice *device, uint64_t size, bool persistentlyMapped,
                                            const char *debugName, VEBufferAddress *outAddress);

/**
 * @brief Create a storage buffer.
 *
 * Convenience function that creates a buffer for shader storage with
 * read/write access from shaders.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Size of buffer in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateStorageBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                            VEBufferAddress *outAddress);

/**
 * @brief Create an indirect draw/dispatch buffer.
 *
 * Convenience function that creates a buffer for indirect drawing or
 * compute dispatch commands populated by the GPU.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Size of buffer in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer, veDrawIndirect, veDispatchIndirect
 */
VULKEASE_API VEResult veCreateIndirectBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                             VEBufferAddress *outAddress);

/**
 * @brief Copy buffer data on the GPU (deferred/command buffer version).
 *
 * Records a buffer copy operation into a command buffer for later execution.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] src Source buffer address.
 * @param[in] dst Destination buffer address.
 * @param[in] srcOffset Byte offset into source buffer.
 * @param[in] dstOffset Byte offset into destination buffer.
 * @param[in] size Number of bytes to copy.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL or addresses are invalid
 *
 * @see veCopyBuffer
 */
VULKEASE_API VEResult veCmdCopyBuffer(VECommandBuffer *cmd, VEBufferAddress src, VEBufferAddress dst,
                                      uint64_t srcOffset, uint64_t dstOffset, uint64_t size);

/**
 * @brief Copy buffer data on the GPU (immediate version).
 *
 * Executes a buffer copy operation immediately or asynchronously.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source buffer address.
 * @param[in] dst Destination buffer address.
 * @param[in] srcOffset Byte offset into source buffer.
 * @param[in] dstOffset Byte offset into destination buffer.
 * @param[in] size Number of bytes to copy.
 * @param[in] fence Optional fence for async operation. NULL = blocking wait.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyBuffer
 */
VULKEASE_API VEResult veCopyBuffer(VEDevice *device, VEBufferAddress src, VEBufferAddress dst,
                                   uint64_t srcOffset, uint64_t dstOffset, uint64_t size, VkFence fence);

/**
 * @brief Fill a buffer with a 32-bit value.
 *
 * Records a buffer fill operation into a command buffer.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] dst Destination buffer address.
 * @param[in] offset Byte offset into buffer (must be a multiple of 4).
 * @param[in] size Number of bytes to fill (must be a multiple of 4, or VK_WHOLE_SIZE).
 * @param[in] data 32-bit value to fill with.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL or offset/size alignment is wrong
 */
VULKEASE_API VEResult veCmdFillBuffer(VECommandBuffer *cmd, VEBufferAddress dst, uint64_t offset,
                                      uint64_t size, uint32_t data);

/**
 * @brief Update buffer with inline data during command buffer recording.
 *
 * Copies small amounts of data directly into the command buffer stream.
 * Useful for small, per-draw updates without staging buffers.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] dst Destination buffer address.
 * @param[in] offset Byte offset into buffer.
 * @param[in] size Number of bytes to copy (max 65536 per Vulkan spec).
 * @param[in] data Pointer to source data.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or data is NULL
 *         - VE_ERROR_INVALID_PARAMETER if size exceeds 65536 bytes
 */
VULKEASE_API VEResult veCmdUpdateBuffer(VECommandBuffer *cmd, VEBufferAddress dst, uint64_t offset,
                                        uint64_t size, const void *data);

// =============================================================================
// TEXTURE FUNCTIONS
// =============================================================================

/**
 * @brief Create a texture and return its bindless index.
 *
 * Creates a new texture with the specified properties. The texture is accessible
 * via its bindless index in shaders.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Texture descriptor specifying dimensions, format, and usage.
 * @param[out] outIndex Receives the bindless texture index.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outIndex is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @note The returned index is valid until veDestroyTexture() is called.
 * @see VETextureDesc, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture(VEDevice *device, const VETextureDesc *desc, VETextureIndex *outIndex);

/**
 * @brief Destroy a texture by its bindless index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Texture index to destroy. VE_INVALID_TEXTURE_INDEX is a no-op.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or index is invalid
 *
 * @see veCreateTexture
 */
VULKEASE_API VEResult veDestroyTexture(VEDevice *device, VETextureIndex index);

/**
 * @brief Get the dimensions of a texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Texture index to query.
 *
 * @return VkExtent3D with width, height, and depth, or {0,0,0} if invalid.
 */
VULKEASE_API VkExtent3D veGetTextureSize(VEDevice *device, VETextureIndex index);

/**
 * @brief Get the format of a texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Texture index to query.
 *
 * @return VkFormat of the texture, or VK_FORMAT_UNDEFINED if invalid.
 */
VULKEASE_API VkFormat veGetTextureFormat(VEDevice *device, VETextureIndex index);

/**
 * @brief Create a 1D texture.
 *
 * Convenience function for creating a simple 1D texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] format Pixel format (e.g., VK_FORMAT_R8G8B8A8_UNORM).
 * @param[in] usage Usage flags (e.g., VK_IMAGE_USAGE_SAMPLED_BIT).
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture1D(VEDevice *device, uint32_t width, VkFormat format,
                                        VkImageUsageFlags usage, const char *debugName,
                                        VETextureIndex *outIndex);

/**
 * @brief Create a 2D texture.
 *
 * Convenience function for creating a simple 2D texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture2D(VEDevice *device, uint32_t width, uint32_t height,
                                        VkFormat format, VkImageUsageFlags usage,
                                        const char *debugName, VETextureIndex *outIndex);

/**
 * @brief Create a 3D texture.
 *
 * Convenience function for creating a 3D volume texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] depth Texture depth in pixels.
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture3D(VEDevice *device, uint32_t width, uint32_t height,
                                        uint32_t depth, VkFormat format, VkImageUsageFlags usage,
                                        const char *debugName, VETextureIndex *outIndex);

/**
 * @brief Create a 2D texture array.
 *
 * Convenience function for creating an array of 2D textures.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] layers Number of array layers.
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture2DArray(VEDevice *device, uint32_t width, uint32_t height,
                                             uint32_t layers, VkFormat format, VkImageUsageFlags usage,
                                             const char *debugName, VETextureIndex *outIndex);

/**
 * @brief Create a cube map texture.
 *
 * Convenience function for creating a cube map with 6 faces.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Width and height of each cube face (must be square).
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTextureCube(VEDevice *device, uint32_t size, VkFormat format,
                                          VkImageUsageFlags usage, const char *debugName,
                                          VETextureIndex *outIndex);

/**
 * @brief Create a multisampled 2D texture.
 *
 * Creates a 2D texture with multisampling for MSAA rendering.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] format Pixel format.
 * @param[in] sampleCount Number of samples (e.g., VK_SAMPLE_COUNT_4_BIT).
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture2DMultisample(VEDevice *device, uint32_t width, uint32_t height,
                                                   VkFormat format, VkSampleCountFlags sampleCount,
                                                   VkImageUsageFlags usage, const char *debugName,
                                                   VETextureIndex *outIndex);

/**
 * @brief Load a texture from an image file.
 *
 * Loads common image formats (PNG, JPG, BMP, TGA, etc.) using STB Image.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filename Path to the image file.
 * @param[in] usage Usage flags to apply to the texture.
 * @param[in] generateMips If true, automatically generate mipmaps.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_NOT_FOUND if the file doesn't exist
 *         - VE_ERROR_UNSUPPORTED if the format is not supported
 *
 * @see veDestroyTexture
 */
VULKEASE_API VEResult veLoadTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                    bool generateMips, VETextureIndex *outIndex);

/**
 * @brief Load an HDR texture from a file.
 *
 * Loads HDR image files (.hdr format) using STB Image.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filename Path to the HDR image file.
 * @param[in] usage Usage flags to apply to the texture.
 * @param[in] generateMips If true, automatically generate mipmaps.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veLoadTexture, veDestroyTexture
 */
VULKEASE_API VEResult veLoadHDRTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                       bool generateMips, VETextureIndex *outIndex);

/**
 * @brief Load a cube map texture from 6 image files.
 *
 * Loads 6 images for the cube map faces: +X, -X, +Y, -Y, +Z, -Z.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filenames Array of 6 file paths for each cube face.
 * @param[in] usage Usage flags to apply to the texture.
 * @param[in] generateMips If true, automatically generate mipmaps.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veDestroyTexture
 */
VULKEASE_API VEResult veLoadCubeTexture(VEDevice *device, const char *filenames[6],
                                        VkImageUsageFlags usage, bool generateMips,
                                        VETextureIndex *outIndex);

/**
 * @brief Write data to a region of a texture from the CPU.
 *
 * Uses VK_EXT_host_image_copy for efficient CPU-side texture updates
 * without GPU pipeline stalls.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] textureIndex Texture to write to.
 * @param[in] srcData Pointer to source pixel data.
 * @param[in] dataSize Size of source data in bytes.
 * @param[in] offsetX X offset in pixels.
 * @param[in] offsetY Y offset in pixels.
 * @param[in] offsetZ Z offset (for 3D textures, 0 for 2D).
 * @param[in] width Width of region in pixels.
 * @param[in] height Height of region in pixels.
 * @param[in] depth Depth of region (1 for 2D textures).
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veHostWriteTexture
 */
VULKEASE_API VEResult veHostWriteTextureRegion(VEDevice *device, VETextureIndex textureIndex,
                                               const void *srcData, size_t dataSize,
                                               uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
                                               uint32_t width, uint32_t height, uint32_t depth);

/**
 * @brief Read data from a region of a texture to the CPU.
 *
 * Uses VK_EXT_host_image_copy for efficient CPU-side texture reads.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] textureIndex Texture to read from.
 * @param[out] dstData Buffer to receive pixel data.
 * @param[in] dataSize Size of destination buffer in bytes.
 * @param[in] offsetX X offset in pixels.
 * @param[in] offsetY Y offset in pixels.
 * @param[in] offsetZ Z offset (for 3D textures, 0 for 2D).
 * @param[in] width Width of region in pixels.
 * @param[in] height Height of region in pixels.
 * @param[in] depth Depth of region (1 for 2D textures).
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veHostReadTexture
 */
VULKEASE_API VEResult veHostReadTextureRegion(VEDevice *device, VETextureIndex textureIndex,
                                              void *dstData, size_t dataSize,
                                              uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
                                              uint32_t width, uint32_t height, uint32_t depth);

/**
 * @brief Write data to an entire texture from the CPU.
 *
 * Convenience function that writes to the entire base mip level.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] textureIndex Texture to write to.
 * @param[in] srcData Pointer to source pixel data.
 * @param[in] dataSize Size of source data in bytes.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veHostWriteTextureRegion
 */
VULKEASE_API VEResult veHostWriteTexture(VEDevice *device, VETextureIndex textureIndex,
                                         const void *srcData, size_t dataSize);

/**
 * @brief Read data from an entire texture to the CPU.
 *
 * Convenience function that reads the entire base mip level.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] textureIndex Texture to read from.
 * @param[out] dstData Buffer to receive pixel data.
 * @param[in] dataSize Size of destination buffer in bytes.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veHostReadTextureRegion
 */
VULKEASE_API VEResult veHostReadTexture(VEDevice *device, VETextureIndex textureIndex,
                                        void *dstData, size_t dataSize);

/**
 * @brief Generate mipmaps for a texture (deferred/command buffer version).
 *
 * Records mipmap generation into a command buffer for later execution.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] texture Texture to generate mipmaps for.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL or texture is invalid
 *
 * @see veGenerateMipmaps
 */
VULKEASE_API VEResult veCmdGenerateMipmaps(VECommandBuffer *cmd, VETextureIndex texture);

/**
 * @brief Generate mipmaps for a texture (immediate/blocking version).
 *
 * Generates mipmaps immediately, blocking until complete.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture to generate mipmaps for.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdGenerateMipmaps
 */
VULKEASE_API VEResult veGenerateMipmaps(VEDevice *device, VETextureIndex texture);

/**
 * @brief Save a texture to an image file.
 *
 * Saves the texture to disk. Format is determined by file extension
 * (supports .png, .bmp, .tga, .jpg).
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture to save.
 * @param[in] filename Output file path with extension.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 */
VULKEASE_API VEResult veSaveTexture(VEDevice *device, VETextureIndex texture, const char *filename);

/**
 * @brief Copy texture to texture (deferred/command buffer version).
 *
 * Records a 1:1 texture copy. Source and destination must have compatible formats.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] src Source texture index.
 * @param[in] dst Destination texture index.
 * @param[in] region Copy region specification. Can be NULL for full texture copy.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see VETextureCopyRegion, veCopyTexture
 */
VULKEASE_API VEResult veCmdCopyTexture(VECommandBuffer *cmd, VETextureIndex src, VETextureIndex dst,
                                       const VETextureCopyRegion *region);

/**
 * @brief Copy texture to texture (immediate version).
 *
 * Executes a texture copy immediately or asynchronously.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source texture index.
 * @param[in] dst Destination texture index.
 * @param[in] region Copy region specification. Can be NULL for full texture copy.
 * @param[in] fence Optional fence for async operation. NULL = blocking wait.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyTexture
 */
VULKEASE_API VEResult veCopyTexture(VEDevice *device, VETextureIndex src, VETextureIndex dst,
                                    const VETextureCopyRegion *region, VkFence fence);

/**
 * @brief Blit (scaled copy) texture to texture (deferred/command buffer version).
 *
 * Records a texture blit that supports scaling and format conversion.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] src Source texture index.
 * @param[in] dst Destination texture index.
 * @param[in] region Blit region with source and destination bounds.
 * @param[in] filter Filtering mode (VK_FILTER_NEAREST or VK_FILTER_LINEAR).
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see VETextureBlitRegion, veBlitTexture
 */
VULKEASE_API VEResult veCmdBlitTexture(VECommandBuffer *cmd, VETextureIndex src, VETextureIndex dst,
                                       const VETextureBlitRegion *region, VkFilter filter);

/**
 * @brief Blit (scaled copy) texture to texture (immediate version).
 *
 * Executes a texture blit immediately or asynchronously.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source texture index.
 * @param[in] dst Destination texture index.
 * @param[in] region Blit region with source and destination bounds.
 * @param[in] filter Filtering mode.
 * @param[in] fence Optional fence for async operation. NULL = blocking wait.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdBlitTexture
 */
VULKEASE_API VEResult veBlitTexture(VEDevice *device, VETextureIndex src, VETextureIndex dst,
                                    const VETextureBlitRegion *region, VkFilter filter, VkFence fence);

/**
 * @brief Copy buffer to texture (deferred/command buffer version).
 *
 * Records a buffer-to-texture copy for uploading texture data.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] src Source buffer address.
 * @param[in] dst Destination texture index.
 * @param[in] region Copy region specification.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see VEBufferTextureCopyRegion, veCopyBufferToTexture
 */
VULKEASE_API VEResult veCmdCopyBufferToTexture(VECommandBuffer *cmd, VEBufferAddress src,
                                               VETextureIndex dst, const VEBufferTextureCopyRegion *region);

/**
 * @brief Copy buffer to texture (immediate version).
 *
 * Executes a buffer-to-texture copy immediately or asynchronously.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source buffer address.
 * @param[in] dst Destination texture index.
 * @param[in] region Copy region specification.
 * @param[in] fence Optional fence for async operation. NULL = blocking wait.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyBufferToTexture
 */
VULKEASE_API VEResult veCopyBufferToTexture(VEDevice *device, VEBufferAddress src, VETextureIndex dst,
                                            const VEBufferTextureCopyRegion *region, VkFence fence);

/**
 * @brief Copy texture to buffer (deferred/command buffer version).
 *
 * Records a texture-to-buffer copy for reading back texture data.
 *
 * @param[in] cmd Valid command buffer in recording state.
 * @param[in] src Source texture index.
 * @param[in] dst Destination buffer address.
 * @param[in] region Copy region specification.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see VEBufferTextureCopyRegion, veCopyTextureToBuffer
 */
VULKEASE_API VEResult veCmdCopyTextureToBuffer(VECommandBuffer *cmd, VETextureIndex src,
                                               VEBufferAddress dst, const VEBufferTextureCopyRegion *region);

/**
 * @brief Copy texture to buffer (immediate version).
 *
 * Executes a texture-to-buffer copy immediately or asynchronously.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source texture index.
 * @param[in] dst Destination buffer address.
 * @param[in] region Copy region specification.
 * @param[in] fence Optional fence for async operation. NULL = blocking wait.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyTextureToBuffer
 */
VULKEASE_API VEResult veCopyTextureToBuffer(VEDevice *device, VETextureIndex src, VEBufferAddress dst,
                                            const VEBufferTextureCopyRegion *region, VkFence fence);

// =============================================================================
// SAMPLER FUNCTIONS
// =============================================================================

/**
 * @brief Create a sampler and return its bindless index.
 *
 * Creates a texture sampler with the specified filtering, addressing, and
 * comparison properties. The sampler is accessible via its bindless index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Sampler descriptor specifying filtering and addressing modes.
 * @param[out] outIndex Receives the bindless sampler index.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outIndex is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @note The returned index is valid until veDestroySampler() is called.
 * @see VESamplerDesc, veDestroySampler
 */
VULKEASE_API VEResult veCreateSampler(VEDevice *device, const VESamplerDesc *desc, VESamplerIndex *outIndex);

/**
 * @brief Destroy a sampler by its bindless index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Sampler index to destroy. VE_INVALID_SAMPLER_INDEX is a no-op.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or index is invalid
 *
 * @see veCreateSampler
 */
VULKEASE_API VEResult veDestroySampler(VEDevice *device, VESamplerIndex index);

/**
 * @brief Create a sampler with linear (bilinear) filtering.
 *
 * Convenience function that creates a sampler using linear min/mag filtering
 * with repeat addressing and mipmap support.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateLinearSampler(VEDevice *device, VESamplerIndex *outIndex);

/**
 * @brief Create a sampler with nearest (point) filtering.
 *
 * Convenience function that creates a sampler using nearest-neighbor filtering
 * with repeat addressing. Ideal for pixel art or when exact texel values are needed.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateNearestSampler(VEDevice *device, VESamplerIndex *outIndex);

/**
 * @brief Create a sampler with anisotropic filtering.
 *
 * Convenience function that creates a sampler with anisotropic filtering for
 * high-quality texture sampling at oblique angles.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] maxAnisotropy Maximum anisotropy level (typically 2, 4, 8, or 16).
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateAnisotropicSampler(VEDevice *device, float maxAnisotropy, VESamplerIndex *outIndex);

/**
 * @brief Create a sampler optimized for shadow mapping.
 *
 * Convenience function that creates a sampler with comparison enabled for
 * hardware PCF shadow sampling.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateShadowSampler(VEDevice *device, VESamplerIndex *outIndex);

/**
 * @brief Get a pre-created default sampler.
 *
 * Returns one of the built-in samplers created at device initialization.
 * These samplers are managed by VulkEase and should not be destroyed.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] sampler Type of default sampler to retrieve.
 *
 * @return Sampler index, or VE_INVALID_SAMPLER_INDEX if device is NULL.
 *
 * @see VEDefaultSampler
 */
VULKEASE_API VESamplerIndex veGetDefaultSampler(VEDevice *device, VEDefaultSampler sampler);

// =============================================================================
// SHADER FUNCTIONS
// =============================================================================

/**
 * @brief Create a shader object from a SPIR-V binary buffer.
 *
 * Creates a shader object using VK_EXT_shader_object for the specified stage.
 * The shader can be bound dynamically without pipeline recreation.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] stage Shader stage (VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, etc.).
 * @param[in] code Pointer to SPIR-V bytecode.
 * @param[in] codeSize Size of SPIR-V bytecode in bytes.
 * @param[in] entryPoint Entry point function name (typically "main"). Can be NULL for "main".
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outShader Receives the shader object handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, code, or outShader is NULL
 *         - VE_ERROR_SHADER_COMPILATION_FAILED if shader creation failed
 *
 * @see veLoadShaderFromFile, veDestroyShader
 */
VULKEASE_API VEResult veLoadShaderFromBuffer(VEDevice *device, VkShaderStageFlags stage,
                                             const void *code, size_t codeSize,
                                             const char *entryPoint, const char *debugName,
                                             VEShader **outShader);

/**
 * @brief Load a shader from a SPIR-V file.
 *
 * Loads and creates a shader object from a .spv file.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filename Path to the .spv file.
 * @param[in] stage Shader stage.
 * @param[in] entryPoint Entry point function name. Can be NULL for "main".
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outShader Receives the shader object handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_NOT_FOUND if the file doesn't exist
 *         - VE_ERROR_SHADER_COMPILATION_FAILED if shader creation failed
 *
 * @see veLoadShaderFromBuffer, veDestroyShader
 */
VULKEASE_API VEResult veLoadShaderFromFile(VEDevice *device, const char *filename,
                                           VkShaderStageFlags stage, const char *entryPoint,
                                           const char *debugName, VEShader **outShader);

/**
 * @brief Destroy a shader object.
 *
 * @param[in] shader Shader object to destroy. Can be NULL (no-op).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if shader is invalid
 *
 * @see veLoadShaderFromBuffer, veLoadShaderFromFile
 */
VULKEASE_API VEResult veDestroyShader(VEShader *shader);

/**
 * @brief Enable or disable shader hot-reload for development.
 *
 * When enabled, shaders loaded from files will track their source files
 * for changes. Use veShaderNeedsReload() and veReloadShader() to update.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] enable True to enable hot-reload, false to disable.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @note Hot-reload is intended for development only. Disable in release builds.
 * @see veShaderNeedsReload, veReloadShader
 */
VULKEASE_API VEResult veSetShaderHotReloadEnabled(VEDevice *device, bool enable);

/**
 * @brief Check if a shader's source file has been modified.
 *
 * Only applicable to shaders loaded from files with hot-reload enabled.
 *
 * @param[in] shader Shader object to check.
 *
 * @return true if the shader needs to be reloaded, false otherwise.
 *
 * @see veSetShaderHotReloadEnabled, veReloadShader
 */
VULKEASE_API bool veShaderNeedsReload(VEShader *shader);

/**
 * @brief Reload a shader from its source file.
 *
 * Recompiles and updates the shader from its original file. Only works
 * for shaders loaded from files.
 *
 * @param[in] shader Shader object to reload.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if shader is NULL or not file-based
 *         - VE_ERROR_SHADER_COMPILATION_FAILED if reload failed
 *
 * @see veSetShaderHotReloadEnabled, veShaderNeedsReload
 */
VULKEASE_API VEResult veReloadShader(VEShader *shader);

// =============================================================================
// GRAPHICS PIPELINE FUNCTIONS
// =============================================================================

/**
 * @brief Create a graphics pipeline.
 *
 * Creates a graphics pipeline combining shaders, vertex input configuration,
 * and material-level state (depth, stencil, blend). The pipeline can be bound
 * to set all these states at once.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Pipeline descriptor with shaders, vertex input, and state.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outPipeline is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @see VEGraphicsPipelineDesc, veDestroyGraphicsPipeline, veBindGraphicsPipeline
 */
VULKEASE_API VEResult veCreateGraphicsPipeline(VEDevice *device, const VEGraphicsPipelineDesc *desc,
                                               VEGraphicsPipeline **outPipeline);

/**
 * @brief Destroy a graphics pipeline.
 *
 * @param[in] pipeline Pipeline to destroy. Can be NULL (no-op).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if pipeline is invalid
 *
 * @see veCreateGraphicsPipeline
 */
VULKEASE_API VEResult veDestroyGraphicsPipeline(VEGraphicsPipeline *pipeline);

/**
 * @brief Create an opaque rendering pipeline.
 *
 * Convenience function that creates a pipeline configured for standard opaque
 * geometry: depth test enabled, depth write enabled, no blending.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateOpaquePipeline(VEDevice *device, VEShader *vertexShader,
                                             VEShader *fragmentShader,
                                             const VEVertexBinding *bindings, uint32_t bindingCount,
                                             const VEVertexAttribute *attrs, uint32_t attrCount,
                                             const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Create a transparent rendering pipeline.
 *
 * Convenience function that creates a pipeline configured for alpha-blended
 * transparent geometry: depth test enabled, depth write disabled, alpha blending.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateTransparentPipeline(VEDevice *device, VEShader *vertexShader,
                                                  VEShader *fragmentShader,
                                                  const VEVertexBinding *bindings, uint32_t bindingCount,
                                                  const VEVertexAttribute *attrs, uint32_t attrCount,
                                                  const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Create an additive blending pipeline.
 *
 * Convenience function that creates a pipeline configured for additive blending,
 * commonly used for particles, glows, and light effects.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateAdditivePipeline(VEDevice *device, VEShader *vertexShader,
                                               VEShader *fragmentShader,
                                               const VEVertexBinding *bindings, uint32_t bindingCount,
                                               const VEVertexAttribute *attrs, uint32_t attrCount,
                                               const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Create a shadow map rendering pipeline.
 *
 * Convenience function that creates a pipeline optimized for shadow map generation:
 * depth-only rendering with optional depth bias.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object. Can be NULL for depth-only.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateShadowPipeline(VEDevice *device, VEShader *vertexShader,
                                             VEShader *fragmentShader,
                                             const VEVertexBinding *bindings, uint32_t bindingCount,
                                             const VEVertexAttribute *attrs, uint32_t attrCount,
                                             const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Create a UI overlay rendering pipeline.
 *
 * Convenience function that creates a pipeline optimized for UI rendering:
 * no depth testing, alpha blending enabled.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateUIOverlayPipeline(VEDevice *device, VEShader *vertexShader,
                                                VEShader *fragmentShader,
                                                const VEVertexBinding *bindings, uint32_t bindingCount,
                                                const VEVertexAttribute *attrs, uint32_t attrCount,
                                                const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Get a default-initialized graphics pipeline descriptor.
 *
 * Returns a VEGraphicsPipelineDesc with sensible defaults. Modify the returned
 * descriptor and pass to veCreateGraphicsPipeline().
 *
 * @return Default-initialized VEGraphicsPipelineDesc.
 *
 * @see VEGraphicsPipelineDesc, veCreateGraphicsPipeline
 */
VULKEASE_API VEGraphicsPipelineDesc veDefaultGraphicsPipelineDesc(void);

// =============================================================================
// DRAW STATE FUNCTIONS
// =============================================================================

/**
 * @brief Create a draw state preset.
 *
 * Draw states contain per-draw dynamic state settings like topology, polygon mode,
 * line width, cull mode, and depth bias. Apply them with veApplyDrawState().
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Draw state descriptor with dynamic state settings.
 * @param[out] outDrawState Receives the draw state handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outDrawState is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @see VEDrawStateDesc, veDestroyDrawState, veApplyDrawState
 */
VULKEASE_API VEResult veCreateDrawState(VEDevice *device, const VEDrawStateDesc *desc,
                                        VEDrawState **outDrawState);

/**
 * @brief Destroy a draw state.
 *
 * @param[in] drawState Draw state to destroy. Can be NULL (no-op).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if drawState is invalid
 *
 * @see veCreateDrawState
 */
VULKEASE_API VEResult veDestroyDrawState(VEDrawState *drawState);

/**
 * @brief Create a default draw state.
 *
 * Convenience function that creates a draw state with standard defaults:
 * triangle list topology, filled polygons, back-face culling, CCW front face.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outDrawState Receives the draw state handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateDrawState, veDestroyDrawState
 */
VULKEASE_API VEResult veCreateDefaultDrawState(VEDevice *device, VEDrawState **outDrawState);

/**
 * @brief Create a wireframe draw state.
 *
 * Convenience function that creates a draw state for wireframe rendering:
 * line polygon mode, no culling.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outDrawState Receives the draw state handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateDrawState, veDestroyDrawState
 */
VULKEASE_API VEResult veCreateWireframeDrawState(VEDevice *device, VEDrawState **outDrawState);

/**
 * @brief Create a draw state for shadow map rendering.
 *
 * Convenience function that creates a draw state optimized for shadow maps:
 * front-face culling and depth bias enabled to reduce shadow acne.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outDrawState Receives the draw state handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateDrawState, veDestroyDrawState
 */
VULKEASE_API VEResult veCreateShadowDrawState(VEDevice *device, VEDrawState **outDrawState);

/**
 * @brief Get a default-initialized draw state descriptor.
 *
 * Returns a VEDrawStateDesc with sensible defaults. Modify the returned
 * descriptor and pass to veCreateDrawState().
 *
 * @return Default-initialized VEDrawStateDesc.
 *
 * @see VEDrawStateDesc, veCreateDrawState
 */
VULKEASE_API VEDrawStateDesc veDefaultDrawStateDesc(void);

// =============================================================================
// COMMAND BUFFER FUNCTIONS
// =============================================================================

/**
 * @brief Begin a primary command buffer for recording.
 *
 * Allocates or retrieves a command buffer from the pool and begins recording.
 * The command buffer is ready to receive rendering commands.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outCmd Receives the command buffer handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device or outCmd is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @note Call veEndCommandBuffer() when done recording, then veSubmitCommandBuffer()
 *       or vePresentImage() to execute.
 * @see veEndCommandBuffer, veSubmitCommandBuffer, veReleaseCommandBuffer
 */
VULKEASE_API VEResult veBeginCommandBuffer(VEDevice *device, VECommandBuffer **outCmd);

/**
 * @brief Submit a command buffer for execution.
 *
 * Submits the command buffer to the appropriate queue with optional
 * synchronization via semaphores and fences.
 *
 * @param[in] cmd Command buffer to submit (must be ended).
 * @param[in] submitInfo Submission configuration including semaphores and fence.
 *                       Can be NULL for simple synchronous submission.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *         - VE_ERROR_DEVICE_LOST if the device was lost
 *
 * @see VESubmitInfo, veBeginCommandBuffer, veEndCommandBuffer
 */
VULKEASE_API VEResult veSubmitCommandBuffer(VECommandBuffer *cmd, const VESubmitInfo *submitInfo);

/**
 * @brief End command buffer recording.
 *
 * Finalizes the command buffer so it can be submitted for execution.
 *
 * @param[in] cmd Command buffer to end.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL or not recording
 *
 * @see veBeginCommandBuffer, veSubmitCommandBuffer
 */
VULKEASE_API VEResult veEndCommandBuffer(VECommandBuffer *cmd);

/**
 * @brief Reset a command buffer for reuse.
 *
 * Clears the command buffer contents so it can be recorded again.
 *
 * @param[in] cmd Command buffer to reset.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *
 * @see veBeginCommandBuffer
 */
VULKEASE_API VEResult veResetCommandBuffer(VECommandBuffer *cmd);

/**
 * @brief Release a command buffer back to the pool.
 *
 * Returns the command buffer to the device's pool for reuse. The handle
 * becomes invalid after this call.
 *
 * @param[in] cmd Command buffer to release.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *
 * @see veBeginCommandBuffer
 */
VULKEASE_API VEResult veReleaseCommandBuffer(VECommandBuffer *cmd);

/**
 * @brief Populate secondary command buffer descriptor from render target.
 *
 * Helper function that copies attachment format and sample information from
 * a VERenderTarget to a VESecondaryCommandBufferDesc, ensuring compatibility.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] renderTarget Render target to extract formats from.
 * @param[out] desc Secondary descriptor to populate.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if any parameter is NULL
 *
 * @see VESecondaryCommandBufferDesc, veBeginSecondaryCommandBuffer
 */
VULKEASE_API VEResult vePopulateSecondaryDescFromRenderTarget(VEDevice *device,
                                                              const VERenderTarget *renderTarget,
                                                              VESecondaryCommandBufferDesc *desc);

/**
 * @brief Begin a secondary command buffer for parallel recording.
 *
 * Creates a secondary command buffer for recording on a separate thread.
 * Secondary buffers are later executed within a primary command buffer.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Secondary buffer descriptor with inheritance info.
 * @param[out] outCmd Receives the command buffer handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outCmd is NULL
 *
 * @note Each secondary buffer should be recorded from a single thread only.
 * @see veBeginSecondaryRecording, veExecuteSecondaryCommandBuffers
 */
VULKEASE_API VEResult veBeginSecondaryCommandBuffer(VEDevice *device,
                                                    const VESecondaryCommandBufferDesc *desc,
                                                    VECommandBuffer **outCmd);

/**
 * @brief Begin recording into an existing secondary command buffer.
 *
 * Resets and begins recording into a previously allocated secondary buffer.
 *
 * @param[in] cmd Secondary command buffer to record into.
 * @param[in] desc Secondary buffer descriptor with inheritance info.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or desc is NULL
 *
 * @see veBeginSecondaryCommandBuffer
 */
VULKEASE_API VEResult veBeginSecondaryRecording(VECommandBuffer *cmd,
                                                const VESecondaryCommandBufferDesc *desc);

/**
 * @brief Execute secondary command buffers within a primary command buffer.
 *
 * Records execution of secondary command buffers into the primary buffer.
 * Must be called while inside a render pass started with veBeginRendering().
 *
 * @param[in] primaryCmd Primary command buffer (must be recording).
 * @param[in] count Number of secondary command buffers.
 * @param[in] secondaryCmds Array of secondary command buffer handles.
 * @param[in] releaseCommandBuffers If true, automatically release secondary buffers after execution.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if primaryCmd or secondaryCmds is NULL
 *
 * @see veBeginSecondaryCommandBuffer
 */
VULKEASE_API VEResult veExecuteSecondaryCommandBuffers(VECommandBuffer *primaryCmd, uint32_t count,
                                                       VECommandBuffer *const *secondaryCmds,
                                                       bool releaseCommandBuffers);

// =============================================================================
// RENDER TARGET FUNCTIONS
// =============================================================================

/**
 * @brief Create a VERenderTarget with the specified render area.
 *
 * Initializes a render target structure for use with veBeginRendering().
 * The render area starts at (0, 0) with the specified dimensions.
 *
 * @param[in] width Width of the render area in pixels.
 * @param[in] height Height of the render area in pixels.
 *
 * @return Initialized VERenderTarget structure.
 *
 * @see veCreateRenderTargetWithOffset, veBeginRendering
 */
VULKEASE_API VERenderTarget veCreateRenderTarget(uint32_t width, uint32_t height);

/**
 * @brief Create a VERenderTarget with offset and dimensions.
 *
 * Initializes a render target structure with a specific render area offset.
 * Useful for rendering to a sub-region of an attachment.
 *
 * @param[in] x X offset of the render area.
 * @param[in] y Y offset of the render area.
 * @param[in] width Width of the render area in pixels.
 * @param[in] height Height of the render area in pixels.
 *
 * @return Initialized VERenderTarget structure.
 *
 * @see veCreateRenderTarget, veBeginRendering
 */
VULKEASE_API VERenderTarget veCreateRenderTargetWithOffset(int32_t x, int32_t y,
                                                           uint32_t width, uint32_t height);

/**
 * @brief Add a color attachment to the render target.
 *
 * Configures a color attachment for rendering. Up to 8 color attachments
 * are supported.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] texture Texture index for the color attachment.
 * @param[in] loadOp Load operation (VK_ATTACHMENT_LOAD_OP_CLEAR, _LOAD, or _DONT_CARE).
 * @param[in] clearValue Clear color (used when loadOp is VK_ATTACHMENT_LOAD_OP_CLEAR).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL or attachment limit exceeded
 *
 * @see veRenderTargetAddColorAttachmentResolve, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetAddColorAttachment(VERenderTarget *target, VETextureIndex texture,
                                                       VkAttachmentLoadOp loadOp, VEColor clearValue);

/**
 * @brief Add a color attachment with MSAA resolve target.
 *
 * Configures a multisampled color attachment that resolves to a single-sampled
 * texture at the end of the render pass.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] texture Multisampled texture for rendering.
 * @param[in] resolveTexture Single-sampled texture to resolve into.
 * @param[in] loadOp Load operation.
 * @param[in] clearValue Clear color.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL or attachment limit exceeded
 *
 * @see veRenderTargetAddColorAttachment, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetAddColorAttachmentResolve(VERenderTarget *target, VETextureIndex texture,
                                                              VETextureIndex resolveTexture,
                                                              VkAttachmentLoadOp loadOp, VEColor clearValue);

/**
 * @brief Set the depth attachment for rendering.
 *
 * Configures the depth attachment for depth testing and writing.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] texture Texture index for the depth attachment (must have depth format).
 * @param[in] loadOp Load operation.
 * @param[in] clearDepth Clear depth value (typically 1.0 for reverse-Z, 0.0 otherwise).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL
 *
 * @see veRenderTargetSetStencilAttachment, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetSetDepthAttachment(VERenderTarget *target, VETextureIndex texture,
                                                       VkAttachmentLoadOp loadOp, float clearDepth);

/**
 * @brief Set the stencil attachment for rendering.
 *
 * Configures the stencil attachment for stencil testing and writing.
 * Can use the same texture as depth if it has a depth-stencil format.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] texture Texture index for the stencil attachment.
 * @param[in] loadOp Load operation.
 * @param[in] clearStencil Clear stencil value.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL
 *
 * @see veRenderTargetSetDepthAttachment, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetSetStencilAttachment(VERenderTarget *target, VETextureIndex texture,
                                                         VkAttachmentLoadOp loadOp, uint32_t clearStencil);

/**
 * @brief Resize all textures attached to a render target.
 *
 * Destroys the existing textures and creates new ones with the specified dimensions.
 * The render target configuration (attachments, load ops, clear values) is preserved.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in,out] target Render target to resize.
 * @param[in] width New width in pixels.
 * @param[in] height New height in pixels.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veResizeRenderTarget(VEDevice *device, VERenderTarget *target,
                                           uint32_t width, uint32_t height);

/**
 * @brief Create a simple render target with color and optional depth.
 *
 * Convenience function that creates textures and configures a render target
 * with a single color attachment and optional depth attachment.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Width in pixels.
 * @param[in] height Height in pixels.
 * @param[in] colorFormat Color texture format.
 * @param[in] depthFormat Depth texture format (VK_FORMAT_UNDEFINED for no depth).
 * @param[in] clearColor Clear color value.
 * @param[in] clearDepth Clear depth value.
 * @param[out] outColorTexture Receives the color texture index (optional, can be NULL).
 * @param[out] outDepthTexture Receives the depth texture index (optional, can be NULL).
 *
 * @return Configured VERenderTarget structure.
 */
VULKEASE_API VERenderTarget veCreateSimpleRenderTarget(VEDevice *device, uint32_t width, uint32_t height,
                                                       VkFormat colorFormat, VkFormat depthFormat,
                                                       VEColor clearColor, float clearDepth,
                                                       VETextureIndex *outColorTexture,
                                                       VETextureIndex *outDepthTexture);

/**
 * @brief Blit a texture to a swapchain for presentation.
 *
 * Copies/scales a texture to the current swapchain image. Acquires the next
 * swapchain image and transitions it for presentation.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] texture Source texture to blit.
 * @param[in] swapchain Destination swapchain.
 * @param[in] filter Filtering mode (VK_FILTER_NEAREST or VK_FILTER_LINEAR).
 *
 * @return VE_SUCCESS on success, or VE_ERROR_SWAPCHAIN_OUT_OF_DATE if resize needed.
 */
VULKEASE_API VEResult veBlitTextureToSwapchain(VECommandBuffer *cmd, VETextureIndex texture,
                                               VESwapchain *swapchain, VkFilter filter);

/**
 * @brief Begin dynamic rendering.
 *
 * Starts a rendering scope using dynamic rendering (VK_KHR_dynamic_rendering).
 * All draw commands must be issued between veBeginRendering() and veEndRendering().
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] renderTarget Render target configuration with attachments.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or renderTarget is NULL
 *
 * @note There are no render passes in VulkEase - just begin/end rendering blocks.
 * @see veEndRendering, VERenderTarget
 */
VULKEASE_API VEResult veBeginRendering(VECommandBuffer *cmd, const VERenderTarget *renderTarget);

/**
 * @brief End dynamic rendering.
 *
 * Ends the current rendering scope. Attachments are written/resolved as configured.
 *
 * @param[in] cmd Command buffer (must have an active rendering scope).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL or not in a render scope
 *
 * @see veBeginRendering
 */
VULKEASE_API VEResult veEndRendering(VECommandBuffer *cmd);

// =============================================================================
// STATE BINDING FUNCTIONS
// =============================================================================

/**
 * @brief Bind a graphics pipeline.
 *
 * Sets the shaders, vertex input layout, and material-level state (depth, stencil,
 * blend) from the pipeline. Call before drawing.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] pipeline Graphics pipeline to bind.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or pipeline is NULL
 *
 * @see veCreateGraphicsPipeline, veApplyDrawState
 */
VULKEASE_API VEResult veBindGraphicsPipeline(VECommandBuffer *cmd, VEGraphicsPipeline *pipeline);

/**
 * @brief Apply a draw state.
 *
 * Sets per-draw dynamic state from the draw state preset (topology, polygon mode,
 * cull mode, depth bias, etc.).
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] drawState Draw state to apply.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or drawState is NULL
 *
 * @see veCreateDrawState, veBindGraphicsPipeline
 */
VULKEASE_API VEResult veApplyDrawState(VECommandBuffer *cmd, VEDrawState *drawState);

/**
 * @brief Apply graphics state (convenience function).
 *
 * Combines pipeline binding, draw state application, and viewport/scissor
 * setting in a single call.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] pipeline Graphics pipeline to bind. NULL = don't change pipeline.
 * @param[in] drawState Draw state to apply. NULL = use defaults.
 * @param[in] viewport Viewport to set. NULL = use full render area.
 * @param[in] scissor Scissor rectangle. NULL = use full render area.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *
 * @see veBindGraphicsPipeline, veApplyDrawState, veSetViewport, veSetScissor
 */
VULKEASE_API VEResult veApplyGraphicsState(VECommandBuffer *cmd, VEGraphicsPipeline *pipeline,
                                           VEDrawState *drawState, const VEViewport *viewport,
                                           const VERect2D *scissor);

/**
 * @brief Bind a single shader.
 *
 * Binds an individual shader object. Primarily used for compute shaders or
 * advanced shader object usage.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] shader Shader to bind.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or shader is NULL
 *
 * @see veBindShaders, veUnbindShaderStage
 */
VULKEASE_API VEResult veBindShader(VECommandBuffer *cmd, VEShader *shader);

/**
 * @brief Bind multiple shaders at once.
 *
 * Binds an array of shader objects for the graphics pipeline stages.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] shaderCount Number of shaders to bind.
 * @param[in] shaders Array of shader handles.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or shaders is NULL
 *
 * @see veBindShader, veUnbindShaderStage
 */
VULKEASE_API VEResult veBindShaders(VECommandBuffer *cmd, uint32_t shaderCount, VEShader *const *shaders);

/**
 * @brief Unbind a shader stage.
 *
 * Removes the shader from the specified pipeline stage.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] stage Shader stage to unbind.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *
 * @see veBindShader, veBindShaders
 */
VULKEASE_API VEResult veUnbindShaderStage(VECommandBuffer *cmd, VkShaderStageFlags stage);

// =============================================================================
// VIEWPORT & SCISSOR FUNCTIONS
// =============================================================================

/**
 * @brief Set the viewport.
 *
 * Sets the viewport transformation for the rasterizer. The viewport maps
 * normalized device coordinates to framebuffer coordinates.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] x X coordinate of the viewport.
 * @param[in] y Y coordinate of the viewport.
 * @param[in] width Width of the viewport.
 * @param[in] height Height of the viewport.
 * @param[in] minDepth Minimum depth value (typically 0.0).
 * @param[in] maxDepth Maximum depth value (typically 1.0).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *
 * @see veSetScissor
 */
VULKEASE_API VEResult veSetViewport(VECommandBuffer *cmd, float x, float y, float width, float height,
                                    float minDepth, float maxDepth);

/**
 * @brief Set the scissor rectangle.
 *
 * Sets the scissor rectangle for clipping. Pixels outside the scissor are discarded.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] x X coordinate of the scissor rectangle.
 * @param[in] y Y coordinate of the scissor rectangle.
 * @param[in] width Width of the scissor rectangle.
 * @param[in] height Height of the scissor rectangle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *
 * @see veSetViewport
 */
VULKEASE_API VEResult veSetScissor(VECommandBuffer *cmd, int32_t x, int32_t y,
                                   uint32_t width, uint32_t height);

// =============================================================================
// DYNAMIC STATE FUNCTIONS
// =============================================================================

// --- Geometry interpretation ---

/** @brief Set the primitive topology. */
VULKEASE_API VEResult veSetTopology(VECommandBuffer *cmd, VkPrimitiveTopology topology);

/** @brief Enable or disable primitive restart for strip topologies. */
VULKEASE_API VEResult veSetPrimitiveRestart(VECommandBuffer *cmd, bool enable);

/** @brief Set the number of patch control points for tessellation. */
VULKEASE_API VEResult veSetPatchControlPoints(VECommandBuffer *cmd, uint32_t controlPoints);

// --- Rasterization ---

/** @brief Set the polygon rasterization mode (fill, line, or point). */
VULKEASE_API VEResult veSetPolygonMode(VECommandBuffer *cmd, VkPolygonMode mode);

/** @brief Set the line width for line primitives and polygon edges. */
VULKEASE_API VEResult veSetLineWidth(VECommandBuffer *cmd, float width);

/** @brief Set the face culling mode. */
VULKEASE_API VEResult veSetCullMode(VECommandBuffer *cmd, VkCullModeFlags cullMode);

/** @brief Set which winding order is considered front-facing. */
VULKEASE_API VEResult veSetFrontFace(VECommandBuffer *cmd, VkFrontFace frontFace);

/** @brief Enable or disable rasterizer discard (skips fragment processing). */
VULKEASE_API VEResult veSetRasterizerDiscard(VECommandBuffer *cmd, bool enable);

// --- Depth bias (polygon offset) ---

/**
 * @brief Set depth bias (polygon offset) parameters.
 *
 * Depth bias adds an offset to depth values to prevent z-fighting in shadow maps.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] enable Whether to enable depth bias.
 * @param[in] constantFactor Constant depth value added.
 * @param[in] clamp Maximum (or minimum if negative) depth bias.
 * @param[in] slopeFactor Slope-dependent depth bias factor.
 */
VULKEASE_API VEResult veSetDepthBias(VECommandBuffer *cmd, bool enable, float constantFactor,
                                     float clamp, float slopeFactor);

/** @brief Enable or disable depth value clamping. */
VULKEASE_API VEResult veSetDepthClamp(VECommandBuffer *cmd, bool enable);

// --- Coverage ---

/** @brief Enable or disable alpha-to-coverage multisampling. */
VULKEASE_API VEResult veSetAlphaToCoverage(VECommandBuffer *cmd, bool enable);

/** @brief Enable or disable alpha-to-one multisampling. */
VULKEASE_API VEResult veSetAlphaToOne(VECommandBuffer *cmd, bool enable);

// --- Depth/stencil ---

/** @brief Enable or disable depth testing. */
VULKEASE_API VEResult veSetDepthTest(VECommandBuffer *cmd, bool enable);

/** @brief Enable or disable writing to the depth buffer. */
VULKEASE_API VEResult veSetDepthWrite(VECommandBuffer *cmd, bool enable);

/** @brief Set the depth comparison operation. */
VULKEASE_API VEResult veSetDepthCompareOp(VECommandBuffer *cmd, VkCompareOp op);

/** @brief Enable or disable stencil testing. */
VULKEASE_API VEResult veSetStencilTest(VECommandBuffer *cmd, bool enable);

/**
 * @brief Set stencil operations.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] faceMask Which faces to affect (VK_STENCIL_FACE_FRONT_BIT, etc.).
 * @param[in] failOp Operation on stencil test fail.
 * @param[in] passOp Operation on stencil and depth test pass.
 * @param[in] depthFailOp Operation on stencil pass but depth test fail.
 * @param[in] compareOp Stencil comparison operation.
 */
VULKEASE_API VEResult veSetStencilOp(VECommandBuffer *cmd, VkStencilFaceFlags faceMask,
                                     VkStencilOp failOp, VkStencilOp passOp,
                                     VkStencilOp depthFailOp, VkCompareOp compareOp);

/**
 * @brief Set the stencil reference value.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] faceMask Which faces to affect.
 * @param[in] reference Reference value for stencil comparison.
 */
VULKEASE_API VEResult veSetStencilReference(VECommandBuffer *cmd, VkStencilFaceFlags faceMask,
                                            uint32_t reference);

// --- Convenience toggles ---

/** @brief Convenience function to enable or disable wireframe rendering. */
VULKEASE_API VEResult veSetWireframe(VECommandBuffer *cmd, bool enabled);

/**
 * @brief Convenience function to configure depth testing.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] testEnabled Whether depth testing is enabled.
 * @param[in] writeEnabled Whether depth writing is enabled.
 */
VULKEASE_API VEResult veSetDepthTesting(VECommandBuffer *cmd, bool testEnabled, bool writeEnabled);

// =============================================================================
// PUSH CONSTANTS & BUFFER BINDING
// =============================================================================

/**
 * @brief Push constants for bindless resource access.
 *
 * Updates push constant data visible to all shader stages. Use this to pass
 * buffer addresses, texture indices, and per-draw parameters to shaders.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] data Pointer to push constant data.
 * @param[in] size Size of data in bytes (max 256 bytes).
 * @param[in] offset Offset in push constant range (typically 0).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd or data is NULL
 *
 * @see VEGraphicsPushConstants, VEComputePushConstants
 */
VULKEASE_API VEResult vePushConstants(VECommandBuffer *cmd, const void *data, uint64_t size, uint64_t offset);

/**
 * @brief Bind a vertex buffer to a binding slot.
 *
 * Binds a vertex buffer for traditional vertex input (non-bindless).
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] binding Binding slot index.
 * @param[in] vertexBuffer Buffer device address.
 * @param[in] offset Byte offset into the buffer.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 */
VULKEASE_API VEResult veBindVertexBuffer(VECommandBuffer *cmd, uint32_t binding,
                                         VEBufferAddress vertexBuffer, uint64_t offset);

/**
 * @brief Bind an index buffer for indexed drawing.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indexBuffer Buffer device address containing indices.
 * @param[in] offset Byte offset into the buffer.
 * @param[in] format Index format (VK_INDEX_TYPE_UINT16 or VK_INDEX_TYPE_UINT32).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 */
VULKEASE_API VEResult veBindIndexBuffer(VECommandBuffer *cmd, VEBufferAddress indexBuffer,
                                        uint64_t offset, VkIndexType format);

// =============================================================================
// DRAW COMMANDS
// =============================================================================

/**
 * @brief Draw non-indexed primitives.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] vertexCount Number of vertices to draw.
 * @param[in] instanceCount Number of instances to draw.
 * @param[in] firstVertex Index of the first vertex.
 * @param[in] firstInstance Instance ID of the first instance.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDraw(VECommandBuffer *cmd, uint32_t vertexCount, uint32_t instanceCount,
                             uint32_t firstVertex, uint32_t firstInstance);

/**
 * @brief Draw indexed primitives.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indexCount Number of indices to draw.
 * @param[in] instanceCount Number of instances to draw.
 * @param[in] firstIndex Offset into the index buffer.
 * @param[in] vertexOffset Value added to vertex index before indexing into vertex buffer.
 * @param[in] firstInstance Instance ID of the first instance.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDrawIndexed(VECommandBuffer *cmd, uint32_t indexCount, uint32_t instanceCount,
                                    uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

/**
 * @brief Draw non-indexed primitives with parameters from a buffer (GPU-driven).
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indirectBuffer Buffer containing VkDrawIndirectCommand structures.
 * @param[in] offset Byte offset into the indirect buffer.
 * @param[in] drawCount Number of draws to execute.
 * @param[in] stride Byte stride between draw commands.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDrawIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer,
                                     uint64_t offset, uint32_t drawCount, uint32_t stride);

/**
 * @brief Draw indexed primitives with parameters from a buffer (GPU-driven).
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indirectBuffer Buffer containing VkDrawIndexedIndirectCommand structures.
 * @param[in] offset Byte offset into the indirect buffer.
 * @param[in] drawCount Number of draws to execute.
 * @param[in] stride Byte stride between draw commands.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDrawIndexedIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer,
                                            uint64_t offset, uint32_t drawCount, uint32_t stride);

/**
 * @brief Draw with GPU-determined draw count.
 *
 * The draw count is read from a buffer, allowing the GPU to control
 * how many draws are executed.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indirectBuffer Buffer containing draw commands.
 * @param[in] indirectOffset Byte offset into the indirect buffer.
 * @param[in] countBuffer Buffer containing the draw count (uint32_t).
 * @param[in] countOffset Byte offset into the count buffer.
 * @param[in] maxDrawCount Maximum number of draws (clamped by count buffer value).
 * @param[in] stride Byte stride between draw commands.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDrawIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer,
                                          uint64_t indirectOffset, VEBufferAddress countBuffer,
                                          uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride);

/**
 * @brief Draw indexed with GPU-determined draw count.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indirectBuffer Buffer containing indexed draw commands.
 * @param[in] indirectOffset Byte offset into the indirect buffer.
 * @param[in] countBuffer Buffer containing the draw count (uint32_t).
 * @param[in] countOffset Byte offset into the count buffer.
 * @param[in] maxDrawCount Maximum number of draws.
 * @param[in] stride Byte stride between draw commands.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDrawIndexedIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer,
                                                 uint64_t indirectOffset, VEBufferAddress countBuffer,
                                                 uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride);

// =============================================================================
// CLEAR COMMANDS
// =============================================================================

/**
 * @brief Clear a color attachment during rendering.
 *
 * Clears a portion of a color attachment. Must be called inside a
 * veBeginRendering/veEndRendering block.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] attachmentIndex Index of the color attachment to clear.
 * @param[in] clearValue Color to clear with.
 * @param[in] rect Region to clear. NULL = clear entire attachment.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 */
VULKEASE_API VEResult veClearColorAttachment(VECommandBuffer *cmd, uint32_t attachmentIndex,
                                             VEColor clearValue, const VERect2D *rect);

/**
 * @brief Clear depth and/or stencil attachment during rendering.
 *
 * Clears a portion of the depth/stencil attachment. Must be called inside a
 * veBeginRendering/veEndRendering block.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] depth Depth value to clear with (typically 1.0 or 0.0).
 * @param[in] stencil Stencil value to clear with.
 * @param[in] aspectMask Which aspects to clear (VK_IMAGE_ASPECT_DEPTH_BIT, _STENCIL_BIT, or both).
 * @param[in] rect Region to clear. NULL = clear entire attachment.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 */
VULKEASE_API VEResult veClearDepthStencilAttachment(VECommandBuffer *cmd, float depth, uint32_t stencil,
                                                    VkImageAspectFlags aspectMask, const VERect2D *rect);

// =============================================================================
// QUERY FUNCTIONS
// =============================================================================

/**
 * @brief Create a query pool for GPU queries.
 *
 * Query pools are used for occlusion queries, timestamp queries, and
 * pipeline statistics queries.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Query pool descriptor.
 * @param[out] outPool Receives the query pool handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outPool is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @see VEQueryPoolDesc, veDestroyQueryPool
 */
VULKEASE_API VEResult veCreateQueryPool(VEDevice *device, const VEQueryPoolDesc *desc, VEQueryPool **outPool);

/**
 * @brief Destroy a query pool.
 *
 * @param[in] pool Query pool to destroy. Can be NULL (no-op).
 *
 * @return VE_SUCCESS on success.
 *
 * @see veCreateQueryPool
 */
VULKEASE_API VEResult veDestroyQueryPool(VEQueryPool *pool);

/**
 * @brief Reset queries in a pool.
 *
 * Queries must be reset before first use or before reuse. This records a
 * reset command into the command buffer.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] pool Query pool to reset.
 * @param[in] firstQuery Index of first query to reset.
 * @param[in] queryCount Number of queries to reset.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veCmdResetQueryPool(VECommandBuffer *cmd, VEQueryPool *pool,
                                          uint32_t firstQuery, uint32_t queryCount);

/**
 * @brief Begin an occlusion or pipeline statistics query.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] pool Query pool (must be occlusion or pipeline statistics type).
 * @param[in] queryIndex Index of the query to begin.
 *
 * @return VE_SUCCESS on success.
 *
 * @see veCmdEndQuery
 */
VULKEASE_API VEResult veCmdBeginQuery(VECommandBuffer *cmd, VEQueryPool *pool, uint32_t queryIndex);

/**
 * @brief End an occlusion or pipeline statistics query.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] pool Query pool.
 * @param[in] queryIndex Index of the query to end.
 *
 * @return VE_SUCCESS on success.
 *
 * @see veCmdBeginQuery
 */
VULKEASE_API VEResult veCmdEndQuery(VECommandBuffer *cmd, VEQueryPool *pool, uint32_t queryIndex);

/**
 * @brief Write a GPU timestamp.
 *
 * Records a timestamp at the specified pipeline stage.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] pool Timestamp query pool.
 * @param[in] queryIndex Index of the query to write.
 * @param[in] stage Pipeline stage at which to record the timestamp.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veCmdWriteTimestamp(VECommandBuffer *cmd, VEQueryPool *pool, uint32_t queryIndex,
                                          VkPipelineStageFlags2 stage);

/**
 * @brief Get query results from a query pool.
 *
 * Reads query results from the GPU. Can optionally wait for results or
 * return partial results.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] pool Query pool to read from.
 * @param[in] firstQuery Index of first query to read.
 * @param[in] queryCount Number of queries to read.
 * @param[out] data Buffer to receive results.
 * @param[in] dataSize Size of data buffer in bytes.
 * @param[in] stride Byte stride between results.
 * @param[in] flags Query result flags (VK_QUERY_RESULT_64_BIT, _WAIT_BIT, etc.).
 *
 * @return VE_SUCCESS on success, VE_ERROR_NOT_READY if results not available.
 */
VULKEASE_API VEResult veGetQueryResults(VEDevice *device, VEQueryPool *pool, uint32_t firstQuery,
                                        uint32_t queryCount, void *data, uint64_t dataSize,
                                        uint64_t stride, VkQueryResultFlags flags);

// =============================================================================
// COMPUTE FUNCTIONS
// =============================================================================

/**
 * @brief Dispatch a compute shader.
 *
 * Launches compute work groups with the specified dimensions.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] groupCountX Number of work groups in X dimension.
 * @param[in] groupCountY Number of work groups in Y dimension.
 * @param[in] groupCountZ Number of work groups in Z dimension.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDispatch(VECommandBuffer *cmd, uint32_t groupCountX,
                                 uint32_t groupCountY, uint32_t groupCountZ);

/**
 * @brief Dispatch compute with parameters from a buffer (GPU-driven).
 *
 * The work group counts are read from the indirect buffer.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indirectBuffer Buffer containing VkDispatchIndirectCommand.
 * @param[in] offset Byte offset into the buffer.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veDispatchIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset);

// =============================================================================
// SYNCHRONIZATION FUNCTIONS
// =============================================================================

/** @brief Insert a barrier for vertex shader output to fragment shader read. */
VULKEASE_API VEResult veBarrierVertexToFragment(VECommandBuffer *cmd);

/** @brief Insert a barrier for compute shader output to vertex shader read. */
VULKEASE_API VEResult veBarrierComputeToVertex(VECommandBuffer *cmd);

/** @brief Insert a barrier between compute shader dispatches. */
VULKEASE_API VEResult veBarrierComputeToCompute(VECommandBuffer *cmd);

/** @brief Insert a barrier for graphics output to presentation. */
VULKEASE_API VEResult veBarrierGraphicsToPresent(VECommandBuffer *cmd);

/**
 * @brief Insert a custom pipeline barrier.
 *
 * Full-featured barrier for complex synchronization scenarios.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] desc Barrier descriptor with memory and image barriers.
 *
 * @return VE_SUCCESS on success.
 *
 * @see VEBarrierDesc
 */
VULKEASE_API VEResult veBarrier(VECommandBuffer *cmd, const VEBarrierDesc *desc);

/**
 * @brief Transition a texture between layouts.
 *
 * Explicit layout transition when you know both old and new layouts.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] texture Texture to transition.
 * @param[in] oldLayout Current layout of the texture.
 * @param[in] newLayout Target layout.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veTransitionTexture(VECommandBuffer *cmd, VETextureIndex texture,
                                          VkImageLayout oldLayout, VkImageLayout newLayout);

/** @brief Transition texture for shader read access. */
VULKEASE_API VEResult veTransitionTextureForShaderRead(VECommandBuffer *cmd, VETextureIndex texture);

/** @brief Transition texture for use as a color attachment. */
VULKEASE_API VEResult veTransitionTextureForColorAttachment(VECommandBuffer *cmd, VETextureIndex texture);

/** @brief Transition texture for use as a depth attachment. */
VULKEASE_API VEResult veTransitionTextureForDepthAttachment(VECommandBuffer *cmd, VETextureIndex texture);

/** @brief Transition texture for use as a transfer source. */
VULKEASE_API VEResult veTransitionTextureForTransferSrc(VECommandBuffer *cmd, VETextureIndex texture);

/** @brief Transition texture for use as a transfer destination. */
VULKEASE_API VEResult veTransitionTextureForTransferDst(VECommandBuffer *cmd, VETextureIndex texture);

/** @brief Transition texture for presentation to swapchain. */
VULKEASE_API VEResult veTransitionTextureForPresent(VECommandBuffer *cmd, VETextureIndex texture);

/**
 * @brief Transition texture to a specific layout (auto-detects current layout).
 *
 * VulkEase tracks the current layout internally, so you only need to specify
 * the target layout.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] texture Texture to transition.
 * @param[in] newLayout Target layout.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veTransitionTextureToLayout(VECommandBuffer *cmd, VETextureIndex texture,
                                                  VkImageLayout newLayout);

// =============================================================================
// SWAPCHAIN FUNCTIONS
// =============================================================================

/**
 * @brief Create a swapchain for a window surface.
 *
 * Creates a swapchain for presenting rendered images to the screen.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Swapchain descriptor with surface, size, and format settings.
 * @param[out] outSwapchain Receives the swapchain handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outSwapchain is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @see VESwapchainDesc, veDestroySwapchain, vePresentImage
 */
VULKEASE_API VEResult veCreateSwapchain(VEDevice *device, const VESwapchainDesc *desc,
                                        VESwapchain **outSwapchain);

/**
 * @brief Destroy a swapchain.
 *
 * @param[in] swapchain Swapchain to destroy. Can be NULL (no-op).
 *
 * @return VE_SUCCESS on success.
 *
 * @see veCreateSwapchain
 */
VULKEASE_API VEResult veDestroySwapchain(VESwapchain *swapchain);

/**
 * @brief Present a rendered image to the screen.
 *
 * Acquires the next swapchain image, blits the command buffer's rendering
 * result to it, and presents to the window.
 *
 * @param[in] swapchain Swapchain to present to.
 * @param[in] cmd Command buffer with recorded rendering commands.
 * @param[in] releaseCommandBuffer If true, automatically release the command buffer after submission.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_SWAPCHAIN_OUT_OF_DATE if the window was resized; call veResizeSwapchain()
 *
 * @see veResizeSwapchain
 */
VULKEASE_API VEResult vePresentImage(VESwapchain *swapchain, VECommandBuffer *cmd, bool releaseCommandBuffer);

/**
 * @brief Resize a swapchain after window resize.
 *
 * Recreates the swapchain images with the new dimensions.
 *
 * @param[in] swapchain Swapchain to resize.
 * @param[in] width New width in pixels.
 * @param[in] height New height in pixels.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veResizeSwapchain(VESwapchain *swapchain, uint32_t width, uint32_t height);

/**
 * @brief Get the current swapchain dimensions.
 *
 * @param[in] swapchain Valid swapchain.
 *
 * @return VkExtent2D with width and height, or {0,0} if swapchain is NULL.
 */
VULKEASE_API VkExtent2D veGetSwapchainSize(VESwapchain *swapchain);

/**
 * @brief Get the swapchain image format.
 *
 * @param[in] swapchain Valid swapchain.
 *
 * @return VkFormat of the swapchain images, or VK_FORMAT_UNDEFINED if swapchain is NULL.
 */
VULKEASE_API VkFormat veGetSwapchainFormat(VESwapchain *swapchain);

// =============================================================================
// DEBUG & PROFILING FUNCTIONS
// =============================================================================

/**
 * @brief Begin a debug label region in a command buffer.
 *
 * Creates a named region visible in GPU debugging tools (RenderDoc, Nsight, etc.).
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] label Label text.
 * @param[in] color Color for the region in the debugger.
 *
 * @return VE_SUCCESS on success.
 *
 * @see veEndDebugLabel, veInsertDebugLabel
 */
VULKEASE_API VEResult veBeginDebugLabel(VECommandBuffer *cmd, const char *label, VEColor color);

/**
 * @brief End a debug label region.
 *
 * @param[in] cmd Command buffer with an active debug label.
 *
 * @return VE_SUCCESS on success.
 *
 * @see veBeginDebugLabel
 */
VULKEASE_API VEResult veEndDebugLabel(VECommandBuffer *cmd);

/**
 * @brief Insert a debug label marker at the current position.
 *
 * Creates a point marker rather than a region.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] label Label text.
 * @param[in] color Color for the marker.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veInsertDebugLabel(VECommandBuffer *cmd, const char *label, VEColor color);

/**
 * @brief Set a debug name for a buffer.
 *
 * The name appears in GPU debugging tools.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer address.
 * @param[in] name Debug name string.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veSetBufferDebugName(VEDevice *device, VEBufferAddress address, const char *name);

/**
 * @brief Set a debug name for a texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture index.
 * @param[in] name Debug name string.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veSetTextureDebugName(VEDevice *device, VETextureIndex texture, const char *name);

/**
 * @brief Set a debug name for a sampler.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] sampler Sampler index.
 * @param[in] name Debug name string.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veSetSamplerDebugName(VEDevice *device, VESamplerIndex sampler, const char *name);

/**
 * @brief Get performance statistics.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] stats Receives performance statistics.
 *
 * @return VE_SUCCESS on success.
 *
 * @see VEPerformanceStats
 */
VULKEASE_API VEResult veGetPerformanceStats(VEDevice *device, VEPerformanceStats *stats);

/**
 * @brief Get memory usage statistics.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] stats Receives memory statistics.
 *
 * @return VE_SUCCESS on success.
 *
 * @see VEMemoryStats
 */
VULKEASE_API VEResult veGetMemoryStats(VEDevice *device, VEMemoryStats *stats);

/**
 * @brief Get graphics pipeline statistics.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] stats Receives pipeline statistics.
 *
 * @return VE_SUCCESS on success.
 *
 * @see VEPipelineStats
 */
VULKEASE_API VEResult veGetPipelineStats(VEDevice *device, VEPipelineStats *stats);

/** @brief Print debug information about the device to the message callback. */
VULKEASE_API VEResult vePrintDebugInfo(VEDevice *device);

/** @brief Print profiling information to the message callback. */
VULKEASE_API VEResult vePrintProfileInfo(VEDevice *device);

/** @brief Print graphics pipeline configuration to the message callback. */
VULKEASE_API VEResult vePrintGraphicsPipeline(VEGraphicsPipeline *pipeline);

/** @brief Validate a graphics pipeline configuration for common errors. */
VULKEASE_API VEResult veValidateGraphicsPipeline(VEGraphicsPipeline *pipeline);

// =============================================================================
// FRAME TIMING FUNCTIONS
// =============================================================================

/**
 * @brief Begin a frame for timing purposes.
 *
 * Call at the start of each frame to track frame timing statistics.
 * Must be paired with veEndFrame().
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VE_SUCCESS on success.
 *
 * @see veEndFrame, VEFrameTimingInfo
 */
VULKEASE_API VEResult veBeginFrame(VEDevice *device);

/**
 * @brief End a frame and get timing information.
 *
 * Call at the end of each frame after presenting. Calculates frame time,
 * FPS, and running averages.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outTiming Receives frame timing info. Can be NULL if not needed.
 *
 * @return VE_SUCCESS on success.
 *
 * @see veBeginFrame, VEFrameTimingInfo
 */
VULKEASE_API VEResult veEndFrame(VEDevice *device, VEFrameTimingInfo *outTiming);

#ifdef __cplusplus
}
#endif

#endif // VULKEASE_H
