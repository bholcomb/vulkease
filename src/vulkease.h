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

#define VE_MAKE_VERSION(major, minor, patch)                                                                           \
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

// Texture resources and shader-visible texture views use independent handles.
typedef uint32_t VETexture;
typedef uint32_t VETextureView;
typedef uint32_t VESamplerIndex;

// Invalid constants
#define VE_INVALID_ADDRESS 0ULL
#define VE_INVALID_TEXTURE 0xFFFFFFFF
#define VE_INVALID_TEXTURE_VIEW 0xFFFFFFFF
#define VE_INVALID_SAMPLER_INDEX 0xFFFFFFFF

// Use the texture's native view type in VETextureViewDesc.
#define VE_TEXTURE_VIEW_TYPE_DEFAULT VK_IMAGE_VIEW_TYPE_MAX_ENUM

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
   const void *initialData; // Optional initial data
   uint64_t initialDataSize;
   bool persistentlyMapped; // Keep CPU-mapped for frequent updates
   const char *debugName;   // Debug name (optional)
} VEBufferDesc;

// Texture creation descriptor
typedef struct VETextureDesc
{
   uint32_t width;
   uint32_t height;
   uint32_t depth;       // 1 for 2D textures
   uint32_t mipLevels;   // 0 = auto-generate all mips
   uint32_t arrayLayers; // 1 for single texture
   VkFormat format;
   VkImageUsageFlags usage;
   VkSampleCountFlags sampleCount; // For multisampled textures
   const void *initialData;        // Optional initial data
   uint64_t initialDataSize;
   const char *debugName; // Debug name (optional)
   bool sparse;           // If true, create as sparse texture (no memory initially bound)
} VETextureDesc;

// A view selects how a texture is interpreted by rendering and shaders.
// VE_TEXTURE_VIEW_TYPE_DEFAULT and VK_FORMAT_UNDEFINED inherit from the texture.
// Zero counts select the remaining mip levels or array layers.
typedef struct VETextureViewDesc
{
   VkImageViewType viewType;
   VkFormat format;
   VkComponentMapping components;
   VkImageAspectFlags aspectMask;
   uint32_t baseMipLevel;
   uint32_t mipLevelCount;
   uint32_t baseArrayLayer;
   uint32_t arrayLayerCount;
   const char *debugName;
} VETextureViewDesc;

// Sampler creation descriptor
typedef struct VESamplerDesc
{
   VkFilter minFilter;
   VkFilter magFilter;
   VkSamplerMipmapMode mipmapFilter;
   VkSamplerAddressMode addressModeU;
   VkSamplerAddressMode addressModeV;
   VkSamplerAddressMode addressModeW;
   float maxAnisotropy; // 1.0 = no anisotropy
   bool compareEnable;  // For shadow mapping
   VkCompareOp compareOp;
   float minLod;
   float maxLod;
   const char *debugName; // Debug name (optional)
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
   int32_t srcOffsetX0, srcOffsetY0, srcOffsetZ0; // Source min corner
   int32_t srcOffsetX1, srcOffsetY1, srcOffsetZ1; // Source max corner
   int32_t dstOffsetX0, dstOffsetY0, dstOffsetZ0; // Dest min corner
   int32_t dstOffsetX1, dstOffsetY1, dstOffsetZ1; // Dest max corner
   uint32_t srcMipLevel, dstMipLevel;
   uint32_t srcArrayLayer, dstArrayLayer;
   uint32_t layerCount;
} VETextureBlitRegion;

// Region for buffer-to-texture or texture-to-buffer copy
typedef struct VEBufferTextureCopyRegion
{
   uint64_t bufferOffset;
   uint32_t bufferRowLength;   // 0 = tightly packed
   uint32_t bufferImageHeight; // 0 = tightly packed
   uint32_t textureOffsetX, textureOffsetY, textureOffsetZ;
   uint32_t width, height, depth;
   uint32_t mipLevel;
   uint32_t arrayLayer;
   uint32_t layerCount;
} VEBufferTextureCopyRegion;

// =============================================================================
// SPARSE TEXTURE STRUCTURES
// =============================================================================

// Information about sparse texture page layout
typedef struct VESparseTextureInfo
{
   uint32_t pageWidth;       // Page dimensions in texels (e.g., 256)
   uint32_t pageHeight;
   uint32_t pageDepth;       // 1 for 2D textures
   uint32_t pagesX;          // Number of pages in X at mip 0
   uint32_t pagesY;          // Number of pages in Y at mip 0
   uint32_t pagesZ;          // Number of pages in Z at mip 0
   uint32_t mipTailFirstLod; // First mip level in the mip tail (auto-committed)
   uint64_t pageSize;        // Size of one page in bytes (typically 64KB)
} VESparseTextureInfo;

// Region for sparse page commit/uncommit operations
typedef struct VESparsePageRegion
{
   uint32_t mipLevel;
   uint32_t arrayLayer;
   uint32_t pageX;      // Page index (not texel offset)
   uint32_t pageY;
   uint32_t pageZ;
   uint32_t pageCountX; // Number of pages to commit (0 = 1)
   uint32_t pageCountY;
   uint32_t pageCountZ;
} VESparsePageRegion;

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
   VkQueryPipelineStatisticFlags pipelineStatistics; // For PIPELINE_STATISTICS type
   const char *debugName;
} VEQueryPoolDesc;

// =============================================================================
// FRAME TIMING
// =============================================================================

typedef struct VEFrameTimingInfo
{
   double frameTimeMs;    // Last frame time in milliseconds
   double avgFrameTimeMs; // Rolling average (last N frames)
   double minFrameTimeMs; // Min in rolling window
   double maxFrameTimeMs; // Max in rolling window
   uint64_t frameNumber;  // Total frames rendered
   double fps;            // Instantaneous FPS
   double avgFps;         // Average FPS
} VEFrameTimingInfo;

// =============================================================================
// SURFACE & SWAPCHAIN TYPES
// =============================================================================

// Platform surface types
typedef enum VESurfaceType
{
   VE_SURFACE_TYPE_WIN32,   // Windows: HWND
   VE_SURFACE_TYPE_XLIB,    // Linux X11: Display* + Window
   VE_SURFACE_TYPE_WAYLAND, // Linux Wayland: wl_display* + wl_surface*
   VE_SURFACE_TYPE_COCOA,   // macOS: NSWindow* (CAMetalLayer internally)
   VE_SURFACE_TYPE_ANDROID  // Android: ANativeWindow*
} VESurfaceType;

// Platform-specific surface descriptor
typedef struct VESurfaceDesc
{
   VESurfaceType type;
   union
   {
      struct
      {
         void *hwnd; // HWND
      } win32;
      struct
      {
         void *display;        // Display*
         unsigned long window; // Window (XID is unsigned long)
      } xlib;
      struct
      {
         void *display; // wl_display*
         void *surface; // wl_surface*
      } wayland;
      struct
      {
         void *window; // NSWindow* or CAMetalLayer*
      } cocoa;
      struct
      {
         void *nativeWindow; // ANativeWindow*
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
   const char *debugName; // Debug name (optional)
} VESwapchainDesc;

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
   uint32_t divisor; // For instanced rendering
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
   VEShader *geometryShader;    // Optional
   VEShader *tessControlShader; // Optional
   VEShader *tessEvalShader;    // Optional
   VEShader *taskShader;        // Optional - task shader (VK_SHADER_STAGE_TASK_BIT_EXT)
   VEShader *meshShader;        // Optional - mesh shader (VK_SHADER_STAGE_MESH_BIT_EXT)

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
   uint32_t patchControlPoints; // For tessellation

   // === RASTERIZATION OVERRIDES ===
   VkPolygonMode polygonMode; // Fill, line, point
   float lineWidth;
   VkCullModeFlags cullMode; // Override pipeline default (VK_CULL_MODE_FLAG_BITS_MAX_ENUM = use pipeline)
   VkFrontFace frontFace;    // Override pipeline default
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
   VETextureView view;
   VkAttachmentLoadOp loadOp;
   VkAttachmentStoreOp storeOp;
   VEColor clearValue;
   VETextureView resolveView;
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
   VkRenderingFlags flags; // 0 for inline draws; use CONTENTS_SECONDARY for secondary command buffers

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
   VETexture image;
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
   const VkSemaphore *waitSemaphores;           // Semaphores to wait on (optional)
   const uint64_t *waitSemaphoreValues;         // Timeline semaphore wait values (optional)
   const VkPipelineStageFlags2 *waitStageMasks; // Pipeline stages for each wait semaphore
   uint32_t waitSemaphoreCount;

   const VkSemaphore *signalSemaphores;   // Semaphores to signal (optional)
   const uint64_t *signalSemaphoreValues; // Timeline semaphore signal values (optional)
   uint32_t signalSemaphoreCount;

   VkFence fence;          // Fence to signal on completion (optional)
   bool waitForCompletion; // True to block until GPU work completes
} VESubmitInfo;

// Secondary command buffer descriptor
typedef struct VESecondaryCommandBufferDesc
{
   VkCommandBufferUsageFlags usageFlags;    // VK_COMMAND_BUFFER_USAGE_*
   VkRenderingFlags renderingFlags;         // Must match the dynamic rendering scope
   uint32_t colorAttachmentCount;           // Up to 8 color attachments
   VkFormat colorAttachmentFormats[8];      // Color formats for inheritance
   VkFormat depthAttachmentFormat;          // Depth format (VK_FORMAT_UNDEFINED if none)
   VkFormat stencilAttachmentFormat;        // Stencil format (VK_FORMAT_UNDEFINED if none)
   VkSampleCountFlags rasterizationSamples; // Sample count (0 defaults to 1)
   uint32_t viewMask;                       // Multiview mask (0 disables multiview)
   bool occlusionQueryEnable;               // Enable occlusion queries for this secondary
   VkQueryControlFlags occlusionQueryFlags; // VkQueryControlFlags
   bool beginRecording;                     // Begin recording automatically when true
   int32_t renderAreaX;                     // Render area X offset
   int32_t renderAreaY;                     // Render area Y offset
   uint32_t renderAreaWidth;                // Render area width
   uint32_t renderAreaHeight;               // Render area height
} VESecondaryCommandBufferDesc;

// =============================================================================
// STATISTICS TYPES
// =============================================================================

// Performance statistics
typedef struct VEPerformanceStats
{
   uint64_t frameTime; // GPU time in nanoseconds
   uint32_t drawCalls;
   uint32_t computeDispatches;
   uint64_t verticesRendered;
   uint64_t trianglesRendered;
   uint32_t pipelineBinds;   // For legacy compatibility
   uint32_t descriptorBinds; // For legacy compatibility
} VEPerformanceStats;

// Memory statistics
typedef struct VEMemoryStats
{
   uint64_t totalAllocated; // Total VMA allocations in bytes
   uint64_t totalUsed;      // Actually used memory in bytes
   uint32_t bufferCount;    // Number of active buffers
   uint32_t textureCount;   // Number of active textures
   uint32_t samplerCount;   // Number of active samplers

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
   VEBufferAddress vertexBuffer;      // 8 bytes - vertex data address
   VEBufferAddress indexBuffer;       // 8 bytes - index data address (optional)
   VEBufferAddress uniformBuffers[8]; // 64 bytes - up to 8 uniform buffer addresses
   VETextureView textures[16];        // Shader-visible bindless texture views
   VESamplerIndex samplers[16];       // 64 bytes - up to 16 bindless sampler indices
   float objectScale;                 // 4 bytes - per-object scale
   uint32_t activeTextureCount;       // 4 bytes - number of active textures (0-16)
   uint32_t activeSamplerCount;       // 4 bytes - number of active samplers (0-16)
   uint32_t activeUniformCount;       // 4 bytes - number of active uniform buffers (0-8)
   uint32_t reserved[7];              // 28 bytes - reserved for future use
} VEGraphicsPushConstants;            // Total: 256 bytes

/**
 * @brief Example push constants structure for compute shaders - 256 bytes.
 *
 * This is an EXAMPLE layout. You may define your own push constant structure
 * with any fields you need, as long as the total size does not exceed 256 bytes.
 */
typedef struct VEComputePushConstants
{
   VEBufferAddress buffers[16]; // 128 bytes - up to 16 buffer addresses
   VETextureView textures[16];  // Shader-visible bindless texture views
   VESamplerIndex samplers[8];  // 32 bytes - up to 8 sampler indices
   uint32_t elementCount;       // 4 bytes - number of elements to process
   uint32_t activeBufferCount;  // 4 bytes - number of active buffers (0-16)
   uint32_t activeTextureCount; // 4 bytes - number of active textures (0-16)
   uint32_t activeSamplerCount; // 4 bytes - number of active samplers (0-8)
   uint32_t reserved[4];        // 16 bytes - reserved for future use
} VEComputePushConstants;       // Total: 256 bytes

/**
 * @brief Initialize VEGraphicsPushConstants with safe defaults.
 *
 * Example macro for use with the example VEGraphicsPushConstants structure.
 * Uses positional initialization for C++17 compatibility (MSVC).
 */
#define VE_INIT_GRAPHICS_PUSH_CONSTANTS()                                                                              \
   {                                                                                                                   \
      VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                                                                          \
          {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                             \
           VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS},                            \
          {VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW,         \
           VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW,         \
           VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW,         \
           VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW},        \
          {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,     \
           VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,     \
           VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,     \
           VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX},    \
          1.0f, 0, 0, 0,                                                                                               \
      {                                                                                                                \
         0, 0, 0, 0, 0, 0, 0                                                                                           \
      }                                                                                                                \
   }

/**
 * @brief Initialize VEComputePushConstants with safe defaults.
 *
 * Example macro for use with the example VEComputePushConstants structure.
 * Uses positional initialization for C++17 compatibility (MSVC).
 */
#define VE_INIT_COMPUTE_PUSH_CONSTANTS()                                                                               \
   {                                                                                                                   \
      {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                                 \
       VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                                 \
       VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                                 \
       VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS},                                \
          {VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW,         \
           VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW,         \
           VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW,         \
           VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW, VE_INVALID_TEXTURE_VIEW},        \
          {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,     \
           VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX},    \
          0, 0, 0, 0,                                                                                                  \
      {                                                                                                                \
         0, 0, 0, 0                                                                                                    \
      }                                                                                                                \
   }

// =============================================================================
// Color Constants
// =============================================================================
// Predefined VEColor constants for common use cases such as clear colors,
// debug visualization, and UI elements. All colors use full opacity (alpha=1.0).
// Uses brace initialization for C++17 compatibility (MSVC).

// --- Primary colors ---
#define VE_COLOR_BLACK (VEColor{0.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_WHITE (VEColor{1.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_RED (VEColor{1.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_GREEN (VEColor{0.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_BLUE (VEColor{0.0f, 0.0f, 1.0f, 1.0f})

// --- Secondary colors ---
#define VE_COLOR_YELLOW (VEColor{1.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_CYAN (VEColor{0.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_MAGENTA (VEColor{1.0f, 0.0f, 1.0f, 1.0f})

// --- Extended colors ---
#define VE_COLOR_ORANGE (VEColor{1.0f, 0.5f, 0.0f, 1.0f})
#define VE_COLOR_PURPLE (VEColor{0.5f, 0.0f, 1.0f, 1.0f})
#define VE_COLOR_PINK (VEColor{1.0f, 0.4f, 0.7f, 1.0f})
#define VE_COLOR_BROWN (VEColor{0.6f, 0.3f, 0.1f, 1.0f})
#define VE_COLOR_LIME (VEColor{0.5f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_TEAL (VEColor{0.0f, 0.5f, 0.5f, 1.0f})
#define VE_COLOR_NAVY (VEColor{0.0f, 0.0f, 0.5f, 1.0f})
#define VE_COLOR_MAROON (VEColor{0.5f, 0.0f, 0.0f, 1.0f})

// --- Grayscale ---
#define VE_COLOR_GRAY_DARK (VEColor{0.25f, 0.25f, 0.25f, 1.0f})
#define VE_COLOR_GRAY (VEColor{0.5f, 0.5f, 0.5f, 1.0f})
#define VE_COLOR_GRAY_LIGHT (VEColor{0.75f, 0.75f, 0.75f, 1.0f})

// --- Transparent ---
#define VE_COLOR_TRANSPARENT (VEColor{0.0f, 0.0f, 0.0f, 0.0f})

// --- Common clear colors ---
#define VE_COLOR_CORNFLOWER_BLUE (VEColor{0.392f, 0.584f, 0.929f, 1.0f}) // Classic XNA/DirectX clear color
#define VE_COLOR_DARK_GRAY (VEColor{0.1f, 0.1f, 0.1f, 1.0f})             // Good for dark themes

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
VULKEASE_API VEResult veCreateContext(const char *applicationName, const char *const *additionalInstanceExtensions,
                                      uint32_t additionalInstanceExtensionCount, VEContext **outContext);

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
                                              VkPhysicalDevice *physicalDevice, char *deviceName,
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
VULKEASE_API VEResult veCreateDevice(VEContext *context, VkPhysicalDevice preferredDevice,
                                     const char *const *additionalDeviceExtensions,
                                     uint32_t additionalDeviceExtensionCount, VEDevice **outDevice);

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

/**
 * @brief Check if mesh shaders are supported.
 *
 * Queries whether VK_EXT_mesh_shader is available and enabled on the device.
 * Mesh shaders replace the traditional vertex/tessellation/geometry pipeline
 * with a more flexible compute-like model.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return true if mesh shaders are available, false otherwise or if device is NULL.
 *
 * @see veDrawMeshTasks, veDrawMeshTasksIndirect, veDrawMeshTasksIndirectCount
 */
VULKEASE_API bool veIsMeshShaderSupported(VEDevice *device);

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
VULKEASE_API VEResult veUpdateBuffer(VEDevice *device, VEBufferAddress address, const void *data, uint64_t size,
                                     uint64_t offset);

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
VULKEASE_API VEResult veCmdFillBuffer(VECommandBuffer *cmd, VEBufferAddress dst, uint64_t offset, uint64_t size,
                                      uint32_t data);

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
VULKEASE_API VEResult veCmdUpdateBuffer(VECommandBuffer *cmd, VEBufferAddress dst, uint64_t offset, uint64_t size,
                                        const void *data);

// =============================================================================
// TEXTURE FUNCTIONS
// =============================================================================

/**
 * @brief Create a texture resource.
 *
 * Creates an image and its default full-resource texture view. Pass the result of
 * veGetDefaultTextureView() to shaders and render targets.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc Texture descriptor specifying dimensions, format, and usage.
 * @param[out] outIndex Receives the texture resource handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outIndex is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if allocation failed
 *
 * @note The returned texture and all of its views are valid until veDestroyTexture() is called.
 * @see VETextureDesc, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture(VEDevice *device, const VETextureDesc *desc, VETexture *outIndex);

/**
 * @brief Destroy a texture resource and all views created from it.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Texture index to destroy. VE_INVALID_TEXTURE is a no-op.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or index is invalid
 *
 * @see veCreateTexture
 */
VULKEASE_API VEResult veDestroyTexture(VEDevice *device, VETexture index);

/**
 * @brief Get the dimensions of a texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Texture index to query.
 *
 * @return VkExtent3D with width, height, and depth, or {0,0,0} if invalid.
 */
VULKEASE_API VkExtent3D veGetTextureSize(VEDevice *device, VETexture index);

/**
 * @brief Get the format of a texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Texture index to query.
 *
 * @return VkFormat of the texture, or VK_FORMAT_UNDEFINED if invalid.
 */
VULKEASE_API VkFormat veGetTextureFormat(VEDevice *device, VETexture index);

/** Return the automatically-created full-resource view for a texture. */
VULKEASE_API VETextureView veGetDefaultTextureView(VEDevice *device, VETexture texture);

/** Return the texture resource that owns a view. */
VULKEASE_API VETexture veGetTextureFromView(VEDevice *device, VETextureView view);

/** Create an additional shader-visible/renderable view of a texture. */
VULKEASE_API VEResult veCreateTextureView(VEDevice *device, VETexture texture, const VETextureViewDesc *desc,
                                          VETextureView *outView);

/** Destroy an additional texture view. A texture's default view is owned by the texture. */
VULKEASE_API VEResult veDestroyTextureView(VEDevice *device, VETextureView view);

/**
 * @brief Write data to a region of a texture from the CPU.
 *
 * Uses VK_EXT_host_image_copy for efficient CPU-side texture updates
 * without GPU pipeline stalls. Supports texture arrays and mip levels.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] textureIndex Texture to write to.
 * @param[in] srcData Pointer to source pixel data.
 * @param[in] dataSize Size of source data in bytes.
 * @param[in] region Region specifying the texture area to write to. If NULL, writes to the
 *                   entire base mip level (layer 0). The bufferOffset, bufferRowLength, and
 *                   bufferImageHeight fields are ignored (data is assumed tightly packed).
 *                   Use layerCount=0 for single layer.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veHostReadTextureRegion
 */
VULKEASE_API VEResult veHostWriteTextureRegion(VEDevice *device, VETexture textureIndex, const void *srcData,
                                               size_t dataSize, const VEBufferTextureCopyRegion *region);

/**
 * @brief Read data from a region of a texture to the CPU.
 *
 * Uses VK_EXT_host_image_copy for efficient CPU-side texture reads.
 * Supports texture arrays and mip levels.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] textureIndex Texture to read from.
 * @param[out] dstData Buffer to receive pixel data.
 * @param[in] dataSize Size of destination buffer in bytes.
 * @param[in] region Region specifying the texture area to read from. If NULL, reads the
 *                   entire base mip level (layer 0). The bufferOffset, bufferRowLength, and
 *                   bufferImageHeight fields are ignored (data is assumed tightly packed).
 *                   Use layerCount=0 for single layer.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veHostWriteTextureRegion
 */
VULKEASE_API VEResult veHostReadTextureRegion(VEDevice *device, VETexture textureIndex, void *dstData, size_t dataSize,
                                              const VEBufferTextureCopyRegion *region);

// =============================================================================
// SPARSE TEXTURE FUNCTIONS
// =============================================================================

/**
 * @brief Get information about a sparse texture's page layout.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Sparse texture index.
 * @param[out] outInfo Receives the sparse texture information.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, outInfo is NULL, or texture is not sparse
 */
VULKEASE_API VEResult veGetSparseTextureInfo(VEDevice *device, VETexture texture, VESparseTextureInfo *outInfo);

/**
 * @brief Commit pages for a sparse texture.
 *
 * Allocates memory and binds it to the specified page regions. This operation
 * is non-blocking; the bindings are queued and will be flushed either explicitly
 * via veFlushSparseBindings() or automatically before command buffer submission.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Sparse texture index.
 * @param[in] regions Array of page regions to commit.
 * @param[in] regionCount Number of regions in the array.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL, texture is not sparse, or regions is NULL
 *         - VE_ERROR_OUT_OF_MEMORY if memory allocation failed
 */
VULKEASE_API VEResult veCommitSparsePages(VEDevice *device, VETexture texture, const VESparsePageRegion *regions,
                                          uint32_t regionCount);

/**
 * @brief Uncommit pages for a sparse texture.
 *
 * Unbinds memory from the specified page regions. Memory is retained in a pool
 * for potential reuse and freed after a timeout period.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Sparse texture index.
 * @param[in] regions Array of page regions to uncommit.
 * @param[in] regionCount Number of regions in the array.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL, texture is not sparse, or regions is NULL
 */
VULKEASE_API VEResult veUncommitSparsePages(VEDevice *device, VETexture texture, const VESparsePageRegion *regions,
                                            uint32_t regionCount);

/**
 * @brief Flush all pending sparse bindings and wait for completion.
 *
 * This function is BLOCKING - it waits for all pending sparse bind operations
 * to complete on the GPU. Required before veHostWriteTextureRegion() on sparse
 * textures. Not required before veSubmitCommandBuffer() (auto-flushed with semaphores).
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL
 */
VULKEASE_API VEResult veFlushSparseBindings(VEDevice *device);

/**
 * @brief Check if a specific sparse page is committed.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Sparse texture index.
 * @param[in] mipLevel Mip level of the page.
 * @param[in] arrayLayer Array layer of the page.
 * @param[in] pageX X index of the page.
 * @param[in] pageY Y index of the page.
 * @param[in] pageZ Z index of the page.
 *
 * @return true if committed, false if not committed or if texture is invalid/not sparse.
 */
VULKEASE_API bool veIsSparsePagesCommitted(VEDevice *device, VETexture texture, uint32_t mipLevel, uint32_t arrayLayer,
                                           uint32_t pageX, uint32_t pageY, uint32_t pageZ);

/**
 * @brief Get total number of committed pages for a sparse texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Sparse texture index.
 *
 * @return Number of committed pages, or 0 if texture is invalid/not sparse.
 */
VULKEASE_API uint32_t veGetSparseCommittedPageCount(VEDevice *device, VETexture texture);

/**
 * @brief Get total number of addressable pages for a sparse texture.
 *
 * Returns the total page count across all mip levels and array layers,
 * excluding the mip tail (which is auto-committed).
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Sparse texture index.
 *
 * @return Total page count, or 0 if texture is invalid/not sparse.
 */
VULKEASE_API uint32_t veGetSparseTotalPageCount(VEDevice *device, VETexture texture);

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
VULKEASE_API VEResult veCmdGenerateMipmaps(VECommandBuffer *cmd, VETexture texture);

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
VULKEASE_API VEResult veCmdCopyTexture(VECommandBuffer *cmd, VETexture src, VETexture dst,
                                       const VETextureCopyRegion *region);

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
VULKEASE_API VEResult veCmdBlitTexture(VECommandBuffer *cmd, VETexture src, VETexture dst,
                                       const VETextureBlitRegion *region, VkFilter filter);

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
VULKEASE_API VEResult veCmdCopyBufferToTexture(VECommandBuffer *cmd, VEBufferAddress src, VETexture dst,
                                               const VEBufferTextureCopyRegion *region);

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
VULKEASE_API VEResult veCmdCopyTextureToBuffer(VECommandBuffer *cmd, VETexture src, VEBufferAddress dst,
                                               const VEBufferTextureCopyRegion *region);

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
VULKEASE_API VEResult veLoadShaderFromBuffer(VEDevice *device, VkShaderStageFlags stage, const void *code,
                                             size_t codeSize, const char *entryPoint, const char *debugName,
                                             VEShader **outShader);


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
VULKEASE_API VEResult veCreateOpaquePipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                             const VEVertexBinding *bindings, uint32_t bindingCount,
                                             const VEVertexAttribute *attrs, uint32_t attrCount, const char *debugName,
                                             VEGraphicsPipeline **outPipeline);

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
VULKEASE_API VEResult veCreateTransparentPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
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
VULKEASE_API VEResult veCreateAdditivePipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
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
VULKEASE_API VEResult veCreateShadowPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                             const VEVertexBinding *bindings, uint32_t bindingCount,
                                             const VEVertexAttribute *attrs, uint32_t attrCount, const char *debugName,
                                             VEGraphicsPipeline **outPipeline);

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
VULKEASE_API VEResult veCreateUIOverlayPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                                const VEVertexBinding *bindings, uint32_t bindingCount,
                                                const VEVertexAttribute *attrs, uint32_t attrCount,
                                                const char *debugName, VEGraphicsPipeline **outPipeline);


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
VULKEASE_API VEResult veCreateDrawState(VEDevice *device, const VEDrawStateDesc *desc, VEDrawState **outDrawState);

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
VULKEASE_API VEResult vePopulateSecondaryDescFromRenderTarget(VEDevice *device, const VERenderTarget *renderTarget,
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
VULKEASE_API VEResult veBeginSecondaryCommandBuffer(VEDevice *device, const VESecondaryCommandBufferDesc *desc,
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
VULKEASE_API VEResult veBeginSecondaryRecording(VECommandBuffer *cmd, const VESecondaryCommandBufferDesc *desc);

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
 * @brief Add a color attachment to the render target.
 *
 * Configures a color attachment for rendering. Up to 8 color attachments
 * are supported.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] view Texture view for the color attachment.
 * @param[in] loadOp Load operation (VK_ATTACHMENT_LOAD_OP_CLEAR, _LOAD, or _DONT_CARE).
 * @param[in] clearValue Clear color (used when loadOp is VK_ATTACHMENT_LOAD_OP_CLEAR).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL or attachment limit exceeded
 *
 * @see veRenderTargetAddColorAttachmentResolve, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetAddColorAttachment(VERenderTarget *target, VETextureView view,
                                                       VkAttachmentLoadOp loadOp, VEColor clearValue);

/**
 * @brief Add a color attachment with MSAA resolve target.
 *
 * Configures a multisampled color attachment that resolves to a single-sampled
 * texture at the end of the render pass.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] view Multisampled texture view for rendering.
 * @param[in] resolveView Single-sampled texture view to resolve into.
 * @param[in] loadOp Load operation.
 * @param[in] clearValue Clear color.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL or attachment limit exceeded
 *
 * @see veRenderTargetAddColorAttachment, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetAddColorAttachmentResolve(VERenderTarget *target, VETextureView view,
                                                              VETextureView resolveView, VkAttachmentLoadOp loadOp,
                                                              VEColor clearValue);

/**
 * @brief Set the depth attachment for rendering.
 *
 * Configures the depth attachment for depth testing and writing.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] view Texture view for the depth attachment (must have depth format).
 * @param[in] loadOp Load operation.
 * @param[in] clearDepth Clear depth value (typically 1.0 for reverse-Z, 0.0 otherwise).
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL
 *
 * @see veRenderTargetSetStencilAttachment, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetSetDepthAttachment(VERenderTarget *target, VETextureView view,
                                                       VkAttachmentLoadOp loadOp, float clearDepth);

/**
 * @brief Set the stencil attachment for rendering.
 *
 * Configures the stencil attachment for stencil testing and writing.
 * Can use the same texture as depth if it has a depth-stencil format.
 *
 * @param[in,out] target Render target to modify.
 * @param[in] view Texture view for the stencil attachment.
 * @param[in] loadOp Load operation.
 * @param[in] clearStencil Clear stencil value.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if target is NULL
 *
 * @see veRenderTargetSetDepthAttachment, veBeginRendering
 */
VULKEASE_API VEResult veRenderTargetSetStencilAttachment(VERenderTarget *target, VETextureView view,
                                                         VkAttachmentLoadOp loadOp, uint32_t clearStencil);

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
VULKEASE_API VEResult veBlitTextureToSwapchain(VECommandBuffer *cmd, VETexture texture, VESwapchain *swapchain,
                                               VkFilter filter);

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
VULKEASE_API VEResult veApplyGraphicsState(VECommandBuffer *cmd, VEGraphicsPipeline *pipeline, VEDrawState *drawState,
                                           const VEViewport *viewport, const VERect2D *scissor);

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
VULKEASE_API VEResult veSetViewport(VECommandBuffer *cmd, float x, float y, float width, float height, float minDepth,
                                    float maxDepth);

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
VULKEASE_API VEResult veSetScissor(VECommandBuffer *cmd, int32_t x, int32_t y, uint32_t width, uint32_t height);

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
VULKEASE_API VEResult veSetDepthBias(VECommandBuffer *cmd, bool enable, float constantFactor, float clamp,
                                     float slopeFactor);

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
VULKEASE_API VEResult veSetStencilOp(VECommandBuffer *cmd, VkStencilFaceFlags faceMask, VkStencilOp failOp,
                                     VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp);

/**
 * @brief Set the stencil reference value.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] faceMask Which faces to affect.
 * @param[in] reference Reference value for stencil comparison.
 */
VULKEASE_API VEResult veSetStencilReference(VECommandBuffer *cmd, VkStencilFaceFlags faceMask, uint32_t reference);

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
VULKEASE_API VEResult veBindVertexBuffer(VECommandBuffer *cmd, uint32_t binding, VEBufferAddress vertexBuffer,
                                         uint64_t offset);

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
VULKEASE_API VEResult veBindIndexBuffer(VECommandBuffer *cmd, VEBufferAddress indexBuffer, uint64_t offset,
                                        VkIndexType format);

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
VULKEASE_API VEResult veDraw(VECommandBuffer *cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
                             uint32_t firstInstance);

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
VULKEASE_API VEResult veDrawIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                                     uint32_t drawCount, uint32_t stride);

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
VULKEASE_API VEResult veDrawIndexedIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                                            uint32_t drawCount, uint32_t stride);

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
VULKEASE_API VEResult veDrawIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                                          VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount,
                                          uint32_t stride);

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
// MESH SHADER DRAW COMMANDS
// =============================================================================

/**
 * @brief Draw using mesh shaders.
 *
 * Dispatches mesh shader workgroups. Used instead of veDraw/veDrawIndexed
 * when a mesh shader pipeline is bound. Requires VK_EXT_mesh_shader.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] groupCountX Number of local workgroups in X dimension.
 * @param[in] groupCountY Number of local workgroups in Y dimension.
 * @param[in] groupCountZ Number of local workgroups in Z dimension.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *         - VE_ERROR_FEATURE_NOT_SUPPORTED if mesh shaders are not available
 */
VULKEASE_API VEResult veDrawMeshTasks(VECommandBuffer *cmd, uint32_t groupCountX, uint32_t groupCountY,
                                      uint32_t groupCountZ);

/**
 * @brief Draw mesh tasks with parameters from a buffer (GPU-driven).
 *
 * The workgroup counts are read from the indirect buffer.
 * Requires VK_EXT_mesh_shader.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indirectBuffer Buffer containing VkDrawMeshTasksIndirectCommandEXT structures.
 * @param[in] offset Byte offset into the indirect buffer.
 * @param[in] drawCount Number of draws to execute.
 * @param[in] stride Byte stride between draw commands.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *         - VE_ERROR_FEATURE_NOT_SUPPORTED if mesh shaders are not available
 */
VULKEASE_API VEResult veDrawMeshTasksIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                                              uint32_t drawCount, uint32_t stride);

/**
 * @brief Draw mesh tasks with GPU-determined draw count.
 *
 * The draw count is read from a buffer, allowing the GPU to control
 * how many mesh shader dispatches are executed. Requires VK_EXT_mesh_shader.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] indirectBuffer Buffer containing draw commands.
 * @param[in] indirectOffset Byte offset into the indirect buffer.
 * @param[in] countBuffer Buffer containing the draw count (uint32_t).
 * @param[in] countOffset Byte offset into the count buffer.
 * @param[in] maxDrawCount Maximum number of draws (clamped by count buffer value).
 * @param[in] stride Byte stride between draw commands.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if cmd is NULL
 *         - VE_ERROR_FEATURE_NOT_SUPPORTED if mesh shaders are not available
 */
VULKEASE_API VEResult veDrawMeshTasksIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer,
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
VULKEASE_API VEResult veClearColorAttachment(VECommandBuffer *cmd, uint32_t attachmentIndex, VEColor clearValue,
                                             const VERect2D *rect);

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
VULKEASE_API VEResult veCmdResetQueryPool(VECommandBuffer *cmd, VEQueryPool *pool, uint32_t firstQuery,
                                          uint32_t queryCount);

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
VULKEASE_API VEResult veGetQueryResults(VEDevice *device, VEQueryPool *pool, uint32_t firstQuery, uint32_t queryCount,
                                        void *data, uint64_t dataSize, uint64_t stride, VkQueryResultFlags flags);

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
VULKEASE_API VEResult veDispatch(VECommandBuffer *cmd, uint32_t groupCountX, uint32_t groupCountY,
                                 uint32_t groupCountZ);

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
VULKEASE_API VEResult veTransitionTexture(VECommandBuffer *cmd, VETexture texture, VkImageLayout oldLayout,
                                          VkImageLayout newLayout);

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
VULKEASE_API VEResult veTransitionTextureToLayout(VECommandBuffer *cmd, VETexture texture, VkImageLayout newLayout);

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
VULKEASE_API VEResult veCreateSwapchain(VEDevice *device, const VESwapchainDesc *desc, VESwapchain **outSwapchain);

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
VULKEASE_API VEResult veSetTextureDebugName(VEDevice *device, VETexture texture, const char *name);

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
