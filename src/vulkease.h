/**
 * @file vulkease2.h
 * @brief VulkEase 2.0 - Complete Modern Vulkan 1.4+ Bindless Graphics API
 *
 * Graphics programming API featuring:
 * - Buffer device address for all buffer access (no descriptor sets for
 * buffers)
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

#ifndef VULKEASE2_H
#define VULKEASE2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C"
{
#endif

   // =============================================================================
   // Version and API Information
   // =============================================================================

#define VULKEASE_VERSION_MAJOR 2
#define VULKEASE_VERSION_MINOR 0
#define VULKEASE_VERSION_PATCH 0

#define VULKEASE_API_VERSION_2_0 VE_MAKE_VERSION(2, 0, 0)
#define VE_MAKE_VERSION(major, minor, patch)                                                                           \
   ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

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
   // Core Types and Handles
   // =============================================================================

   typedef struct VEContext VEContext;
   typedef struct VEDevice VEDevice;
   typedef struct VECommandBuffer VECommandBuffer;
   typedef struct VESwapchain VESwapchain;
   typedef struct VERenderTarget VERenderTarget;

   // Resource handles
   typedef struct VEShader VEShader;
   typedef struct VERenderConfig VERenderConfig;
   typedef struct VEVertexConfig VEVertexConfig;
   typedef struct VEShaderConfig VEShaderConfig;

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
   // Result and Error Handling
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
      VE_ERROR_UNKNOWN = 999
   } VEResult;

   // =============================================================================
   // Render Configuration Types (Modern State Management)
   // =============================================================================

   // Configuration type flags (inspired by Graphics Pipeline Libraries)
   typedef enum VEConfigType
   {
      VE_CONFIG_TYPE_VERTEX_INPUT = 0x1,  // Vertex layout and input assembly
      VE_CONFIG_TYPE_RASTERIZATION = 0x2, // Culling, polygon mode, depth bias
      VE_CONFIG_TYPE_DEPTH_STENCIL = 0x4, // Depth/stencil testing and operations
      VE_CONFIG_TYPE_COLOR_BLEND = 0x8,   // Color blending and write masks
      VE_CONFIG_TYPE_MULTISAMPLE = 0x10,  // MSAA and coverage operations
      VE_CONFIG_TYPE_SHADERS = 0x20,      // Shader object bindings
      VE_CONFIG_TYPE_VIEWPORT = 0x40,     // Viewport settings
      VE_CONFIG_TYPE_SCISSOR = 0x80,      // Scissor settings
      VE_CONFIG_TYPE_COMPLETE = 0xFF      // All categories combined
   } VEConfigType;
   typedef uint32_t VEConfigTypeFlags;

   typedef enum VEMessageSeverity
   {
      VE_MESSAGE_SEVERITY_VERBOSE,
      VE_MESSAGE_SEVERITY_INFO,
      VE_MESSAGE_SEVERITY_WARNING,
      VE_MESSAGE_SEVERITY_ERROR
   } VEMessageSeverity;

   // =============================================================================
   // Structure Types
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
      float maxAnisotropy; // 1.0 = no anisotropy
      bool compareEnable;  // For shadow mapping
      VkCompareOp compareOp;
      float minLod;
      float maxLod;
      const char *debugName; // Debug name (optional)
   } VESamplerDesc;

   // =============================================================================
   // Surface and Swapchain Types
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
            void *display;       // Display*
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

   // Render target descriptor
   typedef struct VERenderTargetDesc
   {
      uint32_t width;
      uint32_t height;
      VkFormat colorFormat;                // Color buffer format (required)
      VkFormat depthFormat;                // Depth buffer format (VK_FORMAT_UNDEFINED = no depth)
      VkSampleCountFlags sampleCount;      // MSAA sample count (0 or 1 = no MSAA)
      bool hasResolveTarget;               // If true with MSAA, creates a resolve target
      const char *debugName;               // Debug name (optional)
   } VERenderTargetDesc;

   // =============================================================================
   // Render Configuration Structures
   // =============================================================================

   // Rasterization configuration (VK_EXT_extended_dynamic_state3)
   typedef struct VERasterConfig
   {
      // Basic rasterization
      VkCullModeFlags cullMode;  // VK_DYNAMIC_STATE_CULL_MODE
      VkFrontFace frontFace;     // VK_DYNAMIC_STATE_FRONT_FACE
      VkPolygonMode polygonMode; // VK_DYNAMIC_STATE_POLYGON_MODE_EXT
      float lineWidth;           // VK_DYNAMIC_STATE_LINE_WIDTH

      // Depth bias
      bool depthBiasEnable;          // VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE
      float depthBiasConstantFactor; // VK_DYNAMIC_STATE_DEPTH_BIAS
      float depthBiasClamp;
      float depthBiasSlopeFactor;

      // Advanced rasterization
      bool depthClampEnable;        // VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT
      bool rasterizerDiscardEnable; // VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE
   } VERasterConfig;

   // Depth/stencil configuration
   typedef struct VEDepthConfig
   {
      // Depth testing
      bool depthTestEnable;       // VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
      bool depthWriteEnable;      // VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
      VkCompareOp depthCompareOp; // VK_DYNAMIC_STATE_DEPTH_COMPARE_OP

      // Depth bounds
      bool depthBoundsTestEnable; // VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE
      float minDepthBounds;       // VK_DYNAMIC_STATE_DEPTH_BOUNDS
      float maxDepthBounds;

      // Stencil testing
      bool stencilTestEnable; // VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE

      // Front face stencil
      VkStencilOp frontFailOp;
      VkStencilOp frontPassOp;
      VkStencilOp frontDepthFailOp;
      VkCompareOp frontCompareOp;
      uint32_t frontCompareMask; // VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
      uint32_t frontWriteMask;   // VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
      uint32_t frontReference;   // VK_DYNAMIC_STATE_STENCIL_REFERENCE

      // Back face stencil
      VkStencilOp backFailOp;
      VkStencilOp backPassOp;
      VkStencilOp backDepthFailOp;
      VkCompareOp backCompareOp;
      uint32_t backCompareMask;
      uint32_t backWriteMask;
      uint32_t backReference;
   } VEDepthConfig;

   // Color blending configuration (VK_EXT_extended_dynamic_state3)
   typedef struct VEBlendAttachment
   {
      bool blendEnable;                  // VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT
      VkBlendFactor srcColorBlendFactor; // VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT
      VkBlendFactor dstColorBlendFactor;
      VkBlendOp colorBlendOp;
      VkBlendFactor srcAlphaBlendFactor;
      VkBlendFactor dstAlphaBlendFactor;
      VkBlendOp alphaBlendOp;
      VkColorComponentFlags colorWriteMask; // VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT
   } VEBlendAttachment;

   typedef struct VEBlendConfig
   {
      bool logicOpEnable; // VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT
      VkLogicOp logicOp;  // VK_DYNAMIC_STATE_LOGIC_OP_EXT

      uint32_t attachmentCount; // Up to 8 attachments
      VEBlendAttachment attachments[8];

      float blendConstants[4]; // VK_DYNAMIC_STATE_BLEND_CONSTANTS
   } VEBlendConfig;

   // Multisample configuration
   typedef struct VEMultisampleConfig
   {
      VkSampleCountFlags rasterizationSamples;
      bool sampleShadingEnable;
      float minSampleShading;
      bool alphaToCoverageEnable;
      bool alphaToOneEnable;
   } VEMultisampleConfig;

   // Vertex input configuration (VK_EXT_vertex_input_dynamic_state)
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

   typedef struct VEVertexInputConfig
   {
      uint32_t bindingCount;
      VEVertexBinding *bindings; // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT

      uint32_t attributeCount;
      VEVertexAttribute *attributes; // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT

      VkPrimitiveTopology topology; // VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
      bool primitiveRestartEnable;  // VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE

      uint32_t patchControlPoints; // VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT
   } VEVertexInputConfig;

   // Complete render configuration descriptor
   typedef struct VERenderConfigDesc
   {
      VEConfigTypeFlags configTypes; // Which config categories to include

      // Configuration components (NULL = use defaults)
      const VEViewport *viewportConfig;
      const VERect2D *scissorConfig;
      const VERasterConfig *rasterConfig;
      const VEDepthConfig *depthConfig;
      const VEBlendConfig *blendConfig;
      const VEMultisampleConfig *multisampleConfig;
      const VEVertexInputConfig *vertexInputConfig;

      // Shader bindings (optional - can bind separately)
      uint32_t shaderCount;
      VEShader *const *shaders;

      const char *debugName;
   } VERenderConfigDesc;

   // Shader configuration descriptor
   typedef struct VEShaderConfigDesc
   {
      VEShader *vertexShader;
      VEShader *fragmentShader;
      VEShader *geometryShader;    // Optional
      VEShader *tessControlShader; // Optional
      VEShader *tessEvalShader;    // Optional
      VEShader *computeShader;     // For compute dispatches

      const char *debugName;
   } VEShaderConfigDesc;

   // =============================================================================
   // Dynamic Rendering Structures
   // =============================================================================

   // Rendering attachment (for dynamic rendering)
   typedef struct VERenderingAttachment
   {
      VETextureIndex texture; // Texture to render to
      VkAttachmentLoadOp loadOp;
      VkAttachmentStoreOp storeOp;
      VEColor clearValue;            // Used if loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR
      VETextureIndex resolveTexture; // For MSAA resolve (optional)
   } VERenderingAttachment;

// Maximum color attachments supported
#define VE_MAX_COLOR_ATTACHMENTS 8

// Indices for depth and stencil in the attachments array
#define VE_DEPTH_ATTACHMENT_INDEX VE_MAX_COLOR_ATTACHMENTS
#define VE_STENCIL_ATTACHMENT_INDEX (VE_MAX_COLOR_ATTACHMENTS + 1)
#define VE_TOTAL_ATTACHMENT_SLOTS (VE_MAX_COLOR_ATTACHMENTS + 2)

   // Dynamic rendering info (replaces render passes)
   // Trivially copyable struct with inline storage for all attachments.
   // Layout of attachments array:
   //   [0..colorAttachmentCount-1] = color attachments
   //   [VE_DEPTH_ATTACHMENT_INDEX] = depth attachment (if hasDepthAttachment)
   //   [VE_STENCIL_ATTACHMENT_INDEX] = stencil attachment (if hasStencilAttachment)
   typedef struct VERenderingInfo
   {
      int32_t renderAreaX, renderAreaY;
      uint32_t renderAreaWidth, renderAreaHeight;

      uint32_t colorAttachmentCount;
      bool hasDepthAttachment;
      bool hasStencilAttachment;

      // All attachments stored inline - trivially copyable
      VERenderingAttachment attachments[VE_TOTAL_ATTACHMENT_SLOTS];
   } VERenderingInfo;

   // =============================================================================
   // Statistics and Debug Structures
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

   // Render configuration statistics
   typedef struct VERenderConfigStats
   {
      uint32_t totalConfigs;
      uint32_t activeConfigs;
      uint32_t configSwitches;
      uint32_t stateSwitches;
      uint32_t overrides;
   } VERenderConfigStats;

   // =============================================================================
   // Context and Device Management
   // =============================================================================

   /**
    * Get VulkEase version
    */
   VULKEASE_API uint32_t veGetVersion(void);

   /**
    * Create VulkEase context - initializes Vulkan 1.4 instance
    * Automatically enables all required extensions:
    * - VK_KHR_buffer_device_address (core 1.2)
    * - VK_EXT_descriptor_indexing (core 1.2)
    * - VK_KHR_dynamic_rendering (core 1.3)
    * - VK_EXT_shader_object
    * - VK_EXT_extended_dynamic_state3
    * - VK_EXT_vertex_input_dynamic_state (if available)
    *
    * @param applicationName Name of the application (shown in debug tools)
    * @param additionalInstanceExtensions Array of additional instance extension names to enable (can be NULL)
    * @param additionalInstanceExtensionCount Number of additional extensions in the array
    * @return Context handle, or NULL on failure (call veGetLastError for details)
    */
   VULKEASE_API VEContext *veCreateContext(const char *applicationName,
                                            const char *const *additionalInstanceExtensions,
                                            uint32_t additionalInstanceExtensionCount);
   VULKEASE_API void veDestroyContext(VEContext *context);

   /**
    * Enumerate available physical devices (GPUs).
    *
    * @param context VulkEase context
    * @param count Pointer to receive the number of physical devices
    * @return VE_SUCCESS on success
    */
   VULKEASE_API VEResult veEnumeratePhysicalDevices(VEContext *context, uint32_t *count);

   /**
    * Get information about a physical device.
    *
    * @param context VulkEase context
    * @param deviceIndex Index of the device (0 to count-1)
    * @param physicalDevice Pointer to receive the VkPhysicalDevice handle
    * @param deviceName Optional buffer to receive device name (at least 256 chars, can be NULL)
    * @param deviceType Optional pointer to receive device type (can be NULL)
    * @return VE_SUCCESS on success, VE_ERROR_INVALID_PARAMETER if index out of range
    */
   VULKEASE_API VEResult veGetPhysicalDeviceInfo(VEContext *context, uint32_t deviceIndex,
                                                  VkPhysicalDevice *physicalDevice,
                                                  char *deviceName,
                                                  VkPhysicalDeviceType *deviceType);

   /**
    * Create device - selects specified GPU or auto-selects best if NULL
    *
    * @param context VulkEase context
    * @param preferredDevice Physical device to use, or VK_NULL_HANDLE to auto-select best
    * @param additionalDeviceExtensions Array of additional device extension names to enable (can be NULL)
    * @param additionalDeviceExtensionCount Number of additional extensions in the array
    * @return Device handle, or NULL on failure (call veGetLastError for details)
    */
   VULKEASE_API VEDevice *veCreateDevice(VEContext *context,
                                          VkPhysicalDevice preferredDevice,
                                          const char *const *additionalDeviceExtensions,
                                          uint32_t additionalDeviceExtensionCount);
   VULKEASE_API void veDestroyDevice(VEDevice *device);

   /**
    * Query extension availability.
    * Call these before veCreateContext/veCreateDevice to check if extensions are supported.
    *
    * @param extensionName Name of the extension to check (e.g., "VK_KHR_ray_tracing_pipeline")
    * @return true if the extension is available, false otherwise
    */
   VULKEASE_API bool veIsInstanceExtensionAvailable(const char *extensionName);
   VULKEASE_API bool veIsDeviceExtensionAvailable(VEContext *context, const char *extensionName);

   /**
    * Device wait operations
    */
   VULKEASE_API VEResult veDeviceWaitIdle(VEDevice *device);

   /**
    * Get device information
    */
   VULKEASE_API const char *veGetDeviceName(VEDevice *device);
   VULKEASE_API const char *veGetDriverVersion(VEDevice *device);
   VULKEASE_API uint32_t veGetVulkanVersion(VEDevice *device);

   /**
    * Access underlying Vulkan handles.
    * Returned handles are owned by VulkEase; do not destroy them.
    * Lifetime matches the owning VulkEase object.
    */
   VULKEASE_API VkInstance veGetVkInstance(VEContext *context);
   VULKEASE_API VkPhysicalDevice veGetVkPhysicalDevice(VEDevice *device);
   VULKEASE_API VkDevice veGetVkDevice(VEDevice *device);
   VULKEASE_API VkQueue veGetVkGraphicsQueue(VEDevice *device);
   VULKEASE_API VkQueue veGetVkComputeQueue(VEDevice *device);
   VULKEASE_API VkQueue veGetVkTransferQueue(VEDevice *device);
   VULKEASE_API VkFence veGetVkCommandBufferFence(VECommandBuffer *cmd);

   /**
    * Get last error message
    */
   VULKEASE_API const char *veGetLastError(void);

   // =============================================================================
   // Buffer Management (Buffer Device Address)
   // =============================================================================

   /**
    * Create buffer and return its GPU address
    * All buffers are created with SHADER_DEVICE_ADDRESS usage when supported
    */
   VULKEASE_API VEBufferAddress veCreateBuffer(VEDevice *device, const VEBufferDesc *desc);

   /**
    * Destroy buffer by address
    */
   VULKEASE_API void veDestroyBuffer(VEDevice *device, VEBufferAddress address);

   /**
    * Map buffer for CPU access (if created with persistentlyMapped=false)
    */
   VULKEASE_API VEResult veMapBuffer(VEDevice *device, VEBufferAddress address, void **mappedData);
   VULKEASE_API void veUnmapBuffer(VEDevice *device, VEBufferAddress address);

   /**
    * Update buffer data (works with both mapped and unmapped buffers)
    */
   VULKEASE_API VEResult veUpdateBuffer(VEDevice *device, VEBufferAddress address, const void *data, uint64_t size,
                                        uint64_t offset);

   /**
    * Get buffer size and properties
    */
   VULKEASE_API uint64_t veGetBufferSize(VEDevice *device, VEBufferAddress address);
   VULKEASE_API VkBufferUsageFlags veGetBufferUsage(VEDevice *device, VEBufferAddress address);

   /**
    * Convenience functions for common buffer types
    */
   VULKEASE_API VEBufferAddress veCreateVertexBuffer(VEDevice *device, const void *vertices, uint64_t size,
                                                     const char *debugName);
   VULKEASE_API VEBufferAddress veCreateIndexBuffer(VEDevice *device, const void *indices, uint64_t size,
                                                    const char *debugName);
   VULKEASE_API VEBufferAddress veCreateUniformBuffer(VEDevice *device, uint64_t size, bool persistentlyMapped,
                                                      const char *debugName);
   VULKEASE_API VEBufferAddress veCreateStorageBuffer(VEDevice *device, uint64_t size, const char *debugName);
   VULKEASE_API VEBufferAddress veCreateIndirectBuffer(VEDevice *device, uint64_t size, const char *debugName);

   // =============================================================================
   // Texture Management (Bindless)
   // =============================================================================

   /**
    * Create texture and return bindless index
    */
   VULKEASE_API VETextureIndex veCreateTexture(VEDevice *device, const VETextureDesc *desc);

   /**
    * Destroy texture by index
    */
   VULKEASE_API void veDestroyTexture(VEDevice *device, VETextureIndex index);

   /**
    * Get texture properties
    */
   VULKEASE_API VEResult veGetTextureSize(VEDevice *device, VETextureIndex index, uint32_t *width, uint32_t *height,
                                          uint32_t *depth);
   VULKEASE_API VkFormat veGetTextureFormat(VEDevice *device, VETextureIndex index);

   /**
    * Convenience functions for common texture types
    */
   VULKEASE_API VETextureIndex veCreateTexture1D(VEDevice *device, uint32_t width, VkFormat format,
                                                 VkImageUsageFlags usage, const char *debugName);
   VULKEASE_API VETextureIndex veCreateTexture2D(VEDevice *device, uint32_t width, uint32_t height, VkFormat format,
                                                 VkImageUsageFlags usage, const char *debugName);
   VULKEASE_API VETextureIndex veCreateTexture3D(VEDevice *device, uint32_t width, uint32_t height, uint32_t depth,
                                                 VkFormat format, VkImageUsageFlags usage, const char *debugName);
   VULKEASE_API VETextureIndex veCreateTexture2DArray(VEDevice *device, uint32_t width, uint32_t height,
                                                      uint32_t layers, VkFormat format, VkImageUsageFlags usage,
                                                      const char *debugName);
   VULKEASE_API VETextureIndex veCreateTextureCube(VEDevice *device, uint32_t size, VkFormat format,
                                                   VkImageUsageFlags usage, const char *debugName);
   VULKEASE_API VETextureIndex veCreateTexture2DMultisample(VEDevice *device, uint32_t width, uint32_t height,
                                                            VkFormat format, VkSampleCountFlags sampleCount,
                                                            VkImageUsageFlags usage, const char *debugName);

   /**
    * Load texture from file using STB Image
    */
   VULKEASE_API VETextureIndex veLoadTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                             bool generateMips);
   VULKEASE_API VETextureIndex veLoadHDRTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                                bool generateMips);
   VULKEASE_API VETextureIndex veLoadCubeTexture(VEDevice *device, const char *filenames[6], VkImageUsageFlags usage,
                                                 bool generateMips);

   // =============================================================================
   // Host Image Copy Functions (VK_EXT_host_image_copy)
   // =============================================================================

   /**
    * Copy data from host memory to texture using VK_EXT_host_image_copy
    *
    * Provides CPU-side texture updates without GPU pipeline stalls.
    * Requires texture to be created with VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT.
    *
    * @param device         Device handle
    * @param textureIndex   Texture to copy data to
    * @param srcData        Source data in host memory
    * @param dataSize       Size of source data in bytes
    * @param offsetX        X offset in texture
    * @param offsetY        Y offset in texture
    * @param offsetZ        Z offset in texture (for 3D textures)
    * @param width          Width of region to copy
    * @param height         Height of region to copy
    * @param depth          Depth of region to copy (1 for 2D textures)
    * @return VE_SUCCESS on success, error code on failure
    */
   VULKEASE_API VEResult veHostCopyToTexture(VEDevice *device, VETextureIndex textureIndex, const void *srcData,
                                             size_t dataSize, uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
                                             uint32_t width, uint32_t height, uint32_t depth);

   /**
    * Copy data from texture to host memory using VK_EXT_host_image_copy
    *
    * Provides CPU-side texture readback without GPU pipeline stalls.
    * Requires texture to be created with VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT.
    *
    * @param device         Device handle
    * @param textureIndex   Texture to copy data from
    * @param dstData        Destination buffer in host memory
    * @param dataSize       Size of destination buffer in bytes
    * @param offsetX        X offset in texture
    * @param offsetY        Y offset in texture
    * @param offsetZ        Z offset in texture (for 3D textures)
    * @param width          Width of region to copy
    * @param height         Height of region to copy
    * @param depth          Depth of region to copy (1 for 2D textures)
    * @return VE_SUCCESS on success, error code on failure
    */
   VULKEASE_API VEResult veHostCopyFromTexture(VEDevice *device, VETextureIndex textureIndex, void *dstData,
                                               size_t dataSize, uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
                                               uint32_t width, uint32_t height, uint32_t depth);

   VULKEASE_API VEResult veHostUpdateEntireTexture(VEDevice *device, VETextureIndex textureIndex, const void *srcData,
                                                   size_t dataSize);
   VULKEASE_API VEResult veHostCopyEntireTexture(VEDevice *device, VETextureIndex textureIndex, void *dstData,
                                                 size_t dataSize);
   /**
    * Generate mipmaps for texture
    */
   VULKEASE_API VEResult veGenerateMipmaps(VEDevice *device, VECommandBuffer *cmd, VETextureIndex texture);

   /**
    * Generate mipmaps immediately (convenience function)
    * Creates a temporary command buffer, generates mipmaps, and submits
    * immediately
    */
   VULKEASE_API VEResult veGenerateMipmapsImmediate(VEDevice *device, VETextureIndex texture);

   /**
    * Save texture to file
    */
   VULKEASE_API VEResult veSaveTexture(VEDevice *device, VETextureIndex texture, const char *filename);

   // =============================================================================
   // Sampler Management (Bindless)
   // =============================================================================

   /**
    * Create sampler and return bindless index
    */
   VULKEASE_API VESamplerIndex veCreateSampler(VEDevice *device, const VESamplerDesc *desc);

   /**
    * Destroy sampler by index
    */
   VULKEASE_API void veDestroySampler(VEDevice *device, VESamplerIndex index);

   /**
    * Convenience functions for common samplers
    */
   VULKEASE_API VESamplerIndex veCreateLinearSampler(VEDevice *device);
   VULKEASE_API VESamplerIndex veCreateNearestSampler(VEDevice *device);
   VULKEASE_API VESamplerIndex veCreateAnisotropicSampler(VEDevice *device, float maxAnisotropy);
   VULKEASE_API VESamplerIndex veCreateShadowSampler(VEDevice *device);

   // =============================================================================
   // Shader Objects (VK_EXT_shader_object)
   // =============================================================================

   /**
    * Create shader object from SPIR-V binary buffer
    */
   VULKEASE_API VEShader *veLoadShaderFromBuffer(VEDevice *device, VkShaderStageFlags stage, const void *code,
                                                 size_t codeSize, const char *entryPoint, const char *debugName);

                                                 /**
    * Load shader from file (.spv for SPIR-V, .glsl/.vert/.frag/.comp etc. for
    * GLSL)
    */
   VULKEASE_API VEShader *veLoadShaderFromFile(VEDevice *device, const char *filename, VkShaderStageFlags stage,
                                                const char *entryPoint, const char *debugName);

   /**
    * Destroy shader object
    */
   VULKEASE_API void veDestroyShader(VEShader *shader);

   /**
    * Shader hot-reload support (for development)
    */
   VULKEASE_API void veSetShaderHotReloadEnabled(VEDevice *device, bool enable);
   VULKEASE_API bool veShaderNeedsReload(VEShader *shader);
   VULKEASE_API VEResult veReloadShader(VEShader *shader);

   /**
    * Shader configuration management
    */
   VULKEASE_API VEShaderConfig *veCreateShaderConfig(VEDevice *device, const VEShaderConfigDesc *desc);
   VULKEASE_API void veDestroyShaderConfig(VEShaderConfig *config);

   // =============================================================================
   // Render Configuration Management
   // =============================================================================

   /**
    * Create render configuration - lightweight state template
    */
   VULKEASE_API VERenderConfig *veCreateRenderConfig(VEDevice *device, const VERenderConfigDesc *desc);

   /**
    * Destroy render configuration
    */
   VULKEASE_API void veDestroyRenderConfig(VERenderConfig *config);

   /**
    * Create vertex configuration for dynamic vertex input
    */
   VULKEASE_API VEVertexConfig *veCreateVertexConfig(VEDevice *device, uint32_t bindingCount,
                                                     const VEVertexBinding *bindings, uint32_t attributeCount,
                                                     const VEVertexAttribute *attributes);
   VULKEASE_API void veDestroyVertexConfig(VEVertexConfig *config);

   /**
    * Get default configurations for common scenarios
    */
   VULKEASE_API VERasterConfig veDefaultRasterConfig(void);
   VULKEASE_API VEDepthConfig veDefaultDepthConfig(void);
   VULKEASE_API VEBlendConfig veDefaultOpaqueBlendConfig(void);
   VULKEASE_API VEBlendConfig veDefaultAlphaBlendConfig(void);
   VULKEASE_API VEBlendConfig veDefaultAdditiveBlendConfig(void);
   VULKEASE_API VEMultisampleConfig veDefaultMultisampleConfig(void);
   VULKEASE_API VEVertexInputConfig veDefaultVertexInputConfig(void);

   /**
    * Create common render configurations
    */
   VULKEASE_API VERenderConfig *veCreateOpaqueRenderConfig(VEDevice *device, const char *debugName);
   VULKEASE_API VERenderConfig *veCreateTransparentRenderConfig(VEDevice *device, const char *debugName);
   VULKEASE_API VERenderConfig *veCreateWireframeRenderConfig(VEDevice *device, const char *debugName);
   VULKEASE_API VERenderConfig *veCreateShadowRenderConfig(VEDevice *device, const char *debugName);
   VULKEASE_API VERenderConfig *veCreateUIRenderConfig(VEDevice *device, const char *debugName);

   /**
    * Configuration composition and variants
    */
   VULKEASE_API VERenderConfig *veCreateConfigVariant(VERenderConfig *baseConfig, const VERenderConfigDesc *overrides);
   VULKEASE_API VERenderConfig *veMergeRenderConfigs(VEDevice *device, uint32_t configCount,
                                                     VERenderConfig *const *configs, const char *debugName);
   VULKEASE_API VERenderConfig *veCloneRenderConfig(VERenderConfig *config, const char *debugName);

   // =============================================================================
   // Command Buffer and Rendering
   // =============================================================================

   /**
    * Get command buffer for recording
    */
   VULKEASE_API VECommandBuffer *veBeginCommandBuffer(VEDevice *device);

   /**
    * Submission info for advanced synchronization
    */
   typedef struct VESubmitInfo
   {
      const VkSemaphore *waitSemaphores;             // Semaphores to wait on (optional)
      const uint64_t *waitSemaphoreValues;           // Timeline semaphore wait values (optional)
      const VkPipelineStageFlags2 *waitStageMasks;   // Pipeline stages for each wait semaphore
      uint32_t waitSemaphoreCount;

      const VkSemaphore *signalSemaphores;           // Semaphores to signal (optional)
      const uint64_t *signalSemaphoreValues;         // Timeline semaphore signal values (optional)
      uint32_t signalSemaphoreCount;

      VkFence fence;                                 // Fence to signal on completion (optional)
      bool waitForCompletion;                        // True to block until GPU work completes
   } VESubmitInfo;

   /**
    * Submit command buffer
    */
   VULKEASE_API VEResult veSubmitCommandBuffer(VECommandBuffer *cmd, bool waitForCompletion);
   VULKEASE_API VEResult veSubmitCommandBufferEx(VECommandBuffer *cmd, const VESubmitInfo *submitInfo);

   /**
    * Command buffer lifecycle management
    * Use these to explicitly finish, reuse, and compose command buffers.
    * Convenience submit will still end one-time primary buffers automatically.
    */
   VULKEASE_API VEResult veEndCommandBuffer(VECommandBuffer *cmd);
   VULKEASE_API VEResult veResetCommandBuffer(VECommandBuffer *cmd);
   VULKEASE_API void veReleaseCommandBuffer(VECommandBuffer *cmd);

   typedef struct VESecondaryCommandBufferDesc
   {
      VkCommandBufferUsageFlags usageFlags;        // VK_COMMAND_BUFFER_USAGE_* (render pass continue not supported)
      uint32_t colorAttachmentCount;               // Up to 8 color attachments
      VkFormat colorAttachmentFormats[8];          // Color formats for inheritance
      VkFormat depthAttachmentFormat;              // Depth format (VK_FORMAT_UNDEFINED if none)
      VkFormat stencilAttachmentFormat;            // Stencil format (VK_FORMAT_UNDEFINED if none)
      VkSampleCountFlags rasterizationSamples;     // Sample count (0 defaults to 1)
      uint32_t viewMask;                           // Multiview mask (0 disables multiview)
      bool occlusionQueryEnable;                   // Enable occlusion queries for this secondary
      VkQueryControlFlags occlusionQueryFlags;     // VkQueryControlFlags (e.g., VK_QUERY_CONTROL_PRECISE_BIT)
      bool beginRecording;                         // Begin recording automatically when true
      int32_t renderAreaX;                         // Render area X offset (for veApplyRenderState defaults)
      int32_t renderAreaY;                         // Render area Y offset (for veApplyRenderState defaults)
      uint32_t renderAreaWidth;                    // Render area width (for veApplyRenderState defaults)
      uint32_t renderAreaHeight;                   // Render area height (for veApplyRenderState defaults)
   } VESecondaryCommandBufferDesc;

   /**
    * Helper: populate secondary descriptor formats/samples from a rendering info
    */
   VULKEASE_API VEResult vePopulateSecondaryDescFromRenderingInfo(VEDevice *device,
                                                                  const VERenderingInfo *renderingInfo,
                                                                  VESecondaryCommandBufferDesc *desc);

   VULKEASE_API VECommandBuffer *veBeginSecondaryCommandBuffer(VEDevice *device,
                                                               const VESecondaryCommandBufferDesc *desc);
   VULKEASE_API VEResult veBeginSecondaryRecording(VECommandBuffer *cmd,
                                                   const VESecondaryCommandBufferDesc *desc);
   /**
    * Execute secondary command buffers within a primary command buffer
    * @param primaryCmd Primary command buffer (must be recording and inside a render pass)
    * @param count Number of secondary command buffers to execute
    * @param secondaryCmds Array of secondary command buffers to execute
    * @param releaseCommandBuffers If true, automatically release the secondary command buffers after execution
    */
   VULKEASE_API VEResult veExecuteSecondaryCommandBuffers(VECommandBuffer *primaryCmd, uint32_t count,
                                                          VECommandBuffer *const *secondaryCmds,
                                                          bool releaseCommandBuffers);

   // =============================================================================
   // Rendering Info Builder Functions
   // =============================================================================

   /**
    * Create a VERenderingInfo with the specified render area
    * @param width Render area width
    * @param height Render area height
    * @return Initialized VERenderingInfo ready for adding attachments
    */
   VULKEASE_API VERenderingInfo veCreateRenderingInfo(uint32_t width, uint32_t height);

   /**
    * Create a VERenderingInfo with offset and dimensions
    * @param x Render area X offset
    * @param y Render area Y offset
    * @param width Render area width
    * @param height Render area height
    * @return Initialized VERenderingInfo ready for adding attachments
    */
   VULKEASE_API VERenderingInfo veCreateRenderingInfoWithOffset(int32_t x, int32_t y, uint32_t width, uint32_t height);

   /**
    * Add a color attachment to the rendering info
    * @param info Rendering info to modify
    * @param texture Texture to render to
    * @param loadOp Load operation (VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_DONT_CARE)
    * @param clearValue Clear color (used only when loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
    */
   VULKEASE_API void veRenderingAddColorAttachment(VERenderingInfo *info, VETextureIndex texture,
                                                    VkAttachmentLoadOp loadOp, VEColor clearValue);

   /**
    * Add a color attachment with MSAA resolve target
    * @param info Rendering info to modify
    * @param texture MSAA texture to render to
    * @param resolveTexture Non-MSAA texture to resolve to
    * @param loadOp Load operation
    * @param clearValue Clear color (used only when loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
    */
   VULKEASE_API void veRenderingAddColorAttachmentResolve(VERenderingInfo *info, VETextureIndex texture,
                                                           VETextureIndex resolveTexture, VkAttachmentLoadOp loadOp,
                                                           VEColor clearValue);

   /**
    * Set the depth attachment for rendering
    * @param info Rendering info to modify
    * @param texture Depth texture
    * @param loadOp Load operation
    * @param clearDepth Clear depth value (used only when loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
    */
   VULKEASE_API void veRenderingSetDepthAttachment(VERenderingInfo *info, VETextureIndex texture,
                                                    VkAttachmentLoadOp loadOp, float clearDepth);

   /**
    * Set the stencil attachment for rendering
    * @param info Rendering info to modify
    * @param texture Stencil texture
    * @param loadOp Load operation
    * @param clearStencil Clear stencil value (used only when loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
    */
   VULKEASE_API void veRenderingSetStencilAttachment(VERenderingInfo *info, VETextureIndex texture,
                                                      VkAttachmentLoadOp loadOp, uint32_t clearStencil);

   /**
    * Begin/end dynamic rendering (replaces render passes)
    */
   VULKEASE_API void veBeginRendering(VECommandBuffer *cmd, const VERenderingInfo *renderingInfo);
   VULKEASE_API void veEndRendering(VECommandBuffer *cmd);

   /**
    * Apply render configuration - sets all associated rendering state
    */
   VULKEASE_API void veApplyRenderConfig(VECommandBuffer *cmd, VERenderConfig *config);

   /**
    * Bind shaders (can mix and match any combination)
    */
   VULKEASE_API void veBindShader(VECommandBuffer *cmd, VEShader *shader);
   VULKEASE_API void veBindShaders(VECommandBuffer *cmd, uint32_t shaderCount, VEShader *const *shaders);
   VULKEASE_API void veBindShaderConfig(VECommandBuffer *cmd, VEShaderConfig *config);
   VULKEASE_API void veUnbindShaderStage(VECommandBuffer *cmd, VkShaderStageFlags stage);

   /**
    * Set dynamic viewport and scissor (always dynamic)
    */
   VULKEASE_API void veSetViewport(VECommandBuffer *cmd, float x, float y, float width, float height, float minDepth,
                                   float maxDepth);
   VULKEASE_API void veSetScissor(VECommandBuffer *cmd, int32_t x, int32_t y, uint32_t width, uint32_t height);

   // =============================================================================
   // Combined Render State
   // =============================================================================

   /**
    * Combined render state for convenience.
    * Bundles shader config, render config, viewport, and scissor into a single call.
    * NULL viewport/scissor will default to the current render area (must be called inside veBeginRendering).
    */
   typedef struct VERenderState
   {
      VEShaderConfig *shaderConfig; // Shader configuration (NULL = don't bind)
      VERenderConfig *renderConfig; // Render state configuration (NULL = don't apply)
      const VEViewport *viewport;   // Viewport (NULL = use full render area)
      const VERect2D *scissor;      // Scissor rectangle (NULL = use full render area)
   } VERenderState;

   /**
    * Apply combined render state in a single call.
    * Must be called inside a rendering pass (after veBeginRendering).
    * If viewport or scissor are NULL, they default to the full render area.
    *
    * @param cmd Command buffer (must be inside a rendering pass)
    * @param state Render state to apply
    */
   VULKEASE_API void veApplyRenderState(VECommandBuffer *cmd, const VERenderState *state);

   /**
    * Runtime state overrides (maximum flexibility)
    */
   VULKEASE_API void veOverrideRasterState(VECommandBuffer *cmd, const VERasterConfig *raster);
   VULKEASE_API void veOverrideDepthState(VECommandBuffer *cmd, const VEDepthConfig *depth);
   VULKEASE_API void veOverrideBlendState(VECommandBuffer *cmd, const VEBlendConfig *blend);

   /**
    * Quick state toggles for common cases
    */
   VULKEASE_API void veSetWireframe(VECommandBuffer *cmd, bool enabled);
   VULKEASE_API void veSetAlphaBlending(VECommandBuffer *cmd, bool enabled);
   VULKEASE_API void veSetDepthTesting(VECommandBuffer *cmd, bool testEnabled, bool writeEnabled);
   VULKEASE_API void veSetCulling(VECommandBuffer *cmd, VkCullModeFlags cullMode);

   /**
    * Push constants for bindless resource access
    * Contains buffer addresses and texture/sampler indices
    */
   VULKEASE_API void vePushConstants(VECommandBuffer *cmd, const void *data, uint64_t size, uint64_t offset);

   /**
    * Bind an index buffer fro indexed calls
    */
   VULKEASE_API void veBindIndexBuffer(VECommandBuffer *cmd, VEBufferAddress indexBuffer, uint64_t offset,
                                       VkIndexType format);

   /**
    * Direct drawing (buffer addresses in push constants - no binding operations!)
    */
   VULKEASE_API void veDraw(VECommandBuffer *cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
                            uint32_t firstInstance);
   VULKEASE_API void veDrawIndexed(VECommandBuffer *cmd, uint32_t indexCount, uint32_t instanceCount,
                                   uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

   /**
    * Indirect drawing (GPU-driven)
    */
   VULKEASE_API void veDrawIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                                    uint32_t drawCount, uint32_t stride);
   VULKEASE_API void veDrawIndexedIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                                           uint32_t drawCount, uint32_t stride);

   /**
    * Multi-draw indirect with count buffer (GPU determines draw count)
    */
   VULKEASE_API void veDrawIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                                         VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount,
                                         uint32_t stride);
   VULKEASE_API void veDrawIndexedIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer,
                                                uint64_t indirectOffset, VEBufferAddress countBuffer,
                                                uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride);

   // =============================================================================
   // Compute Shaders
   // =============================================================================

   /**
    * Dispatch compute shader
    */
   VULKEASE_API void veDispatch(VECommandBuffer *cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

   /**
    * Indirect compute dispatch
    */
   VULKEASE_API void veDispatchIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset);

   // =============================================================================
   // Synchronization and Memory Barriers
   // =============================================================================

   /**
    * Simple memory barriers for common cases
    */
   VULKEASE_API void veBarrierVertexToFragment(VECommandBuffer *cmd);
   VULKEASE_API void veBarrierComputeToVertex(VECommandBuffer *cmd);
   VULKEASE_API void veBarrierComputeToCompute(VECommandBuffer *cmd);
   VULKEASE_API void veBarrierGraphicsToPresent(VECommandBuffer *cmd);

   /**
    * Texture layout transitions
    */
   VULKEASE_API void veTransitionTexture(VECommandBuffer *cmd, VETextureIndex texture, VkImageLayout oldLayout,
                                         VkImageLayout newLayout);

   /**
    * Common texture layout transition helpers
    */
   VULKEASE_API void veTransitionTextureForShaderRead(VECommandBuffer *cmd, VETextureIndex texture);
   VULKEASE_API void veTransitionTextureForColorAttachment(VECommandBuffer *cmd, VETextureIndex texture);
   VULKEASE_API void veTransitionTextureForDepthAttachment(VECommandBuffer *cmd, VETextureIndex texture);
   VULKEASE_API void veTransitionTextureForTransferSrc(VECommandBuffer *cmd, VETextureIndex texture);
   VULKEASE_API void veTransitionTextureForTransferDst(VECommandBuffer *cmd, VETextureIndex texture);
   VULKEASE_API void veTransitionTextureForPresent(VECommandBuffer *cmd, VETextureIndex texture);

   /**
    * Smart layout transition - automatically detects current layout
    */
   VULKEASE_API void veTransitionTextureToLayout(VECommandBuffer *cmd, VETextureIndex texture, VkImageLayout newLayout);

   // =============================================================================
   // Swapchain and Presentation
   // =============================================================================

   /**
    * Create swapchain for window
    * @param device Device handle
    * @param desc Swapchain descriptor containing surface info, dimensions, format, and vsync setting
    * @return Swapchain handle, or NULL on failure (call veGetLastError for details)
    */
   VULKEASE_API VESwapchain *veCreateSwapchain(VEDevice *device, const VESwapchainDesc *desc);
   VULKEASE_API void veDestroySwapchain(VESwapchain *swapchain);

   /**
    * Present rendered image to screen.
    * Call this after veBlitToSwapchain to present the frame.
    *
    * @param swapchain Swapchain to present to
    * @param cmd Command buffer that rendered the frame
    * @param releaseCommandBuffer If true, automatically release the command buffer after submission
    * @return VE_SUCCESS on success, VE_ERROR_SWAPCHAIN_OUT_OF_DATE if resize needed
    */
   VULKEASE_API VEResult vePresentImage(VESwapchain *swapchain, VECommandBuffer *cmd, bool releaseCommandBuffer);

   /**
    * Handle window resize
    */
   VULKEASE_API VEResult veResizeSwapchain(VESwapchain *swapchain, uint32_t width, uint32_t height);

   /**
    * Get swapchain information
    */
   VULKEASE_API VEResult veGetSwapchainSize(VESwapchain *swapchain, uint32_t *width, uint32_t *height);
   VULKEASE_API VkFormat veGetSwapchainFormat(VESwapchain *swapchain);

   // =============================================================================
   // Render Targets (Offscreen Rendering)
   // =============================================================================

   /**
    * Create an offscreen render target for rendering.
    * All rendering should target a VERenderTarget, then blit to swapchain for presentation.
    *
    * @param device Device handle
    * @param desc Render target descriptor
    * @return Render target handle, or NULL on failure
    */
   VULKEASE_API VERenderTarget *veCreateRenderTarget(VEDevice *device, const VERenderTargetDesc *desc);

   /**
    * Destroy a render target and its associated resources
    */
   VULKEASE_API void veDestroyRenderTarget(VERenderTarget *renderTarget);

   /**
    * Resize a render target (recreates internal textures)
    */
   VULKEASE_API VEResult veResizeRenderTarget(VERenderTarget *renderTarget, uint32_t width, uint32_t height);

   /**
    * Get render target textures for use in VERenderingInfo
    */
   VULKEASE_API VETextureIndex veGetRenderTargetColorTexture(VERenderTarget *renderTarget);
   VULKEASE_API VETextureIndex veGetRenderTargetDepthTexture(VERenderTarget *renderTarget);
   VULKEASE_API VETextureIndex veGetRenderTargetResolveTexture(VERenderTarget *renderTarget);

   /**
    * Get render target dimensions
    */
   VULKEASE_API VEResult veGetRenderTargetSize(VERenderTarget *renderTarget, uint32_t *width, uint32_t *height);

   /**
    * Get render target format
    */
   VULKEASE_API VkFormat veGetRenderTargetColorFormat(VERenderTarget *renderTarget);
   VULKEASE_API VkFormat veGetRenderTargetDepthFormat(VERenderTarget *renderTarget);

   /**
    * Blit render target to swapchain for presentation.
    * Acquires the next swapchain image, handles layout transitions, and blits.
    * Call this after veEndRendering, then call vePresentImage.
    *
    * @param cmd Command buffer to record blit commands
    * @param renderTarget Source render target
    * @param swapchain Destination swapchain
    * @param filter Filtering mode for scaling (VK_FILTER_NEAREST or VK_FILTER_LINEAR)
    * @return VE_SUCCESS on success, VE_ERROR_SWAPCHAIN_OUT_OF_DATE if resize needed
    */
   VULKEASE_API VEResult veBlitToSwapchain(VECommandBuffer *cmd, VERenderTarget *renderTarget,
                                            VESwapchain *swapchain, VkFilter filter);

   // =============================================================================
   // Debug and Profiling
   // =============================================================================

   /**
    * Debug labels for command buffers
    */
   VULKEASE_API void veBeginDebugLabel(VECommandBuffer *cmd, const char *label, VEColor color);
   VULKEASE_API void veEndDebugLabel(VECommandBuffer *cmd);
   VULKEASE_API void veInsertDebugLabel(VECommandBuffer *cmd, const char *label, VEColor color);

   /**
    * Set debug names for resources
    */
   VULKEASE_API VEResult veSetBufferDebugName(VEDevice *device, VEBufferAddress address, const char *name);
   VULKEASE_API VEResult veSetTextureDebugName(VEDevice *device, VETextureIndex texture, const char *name);
   VULKEASE_API VEResult veSetSamplerDebugName(VEDevice *device, VESamplerIndex sampler, const char *name);

   /**
    * Performance and memory statistics
    */
   VULKEASE_API VEResult veGetPerformanceStats(VEDevice *device, VEPerformanceStats *stats);
   VULKEASE_API VEResult veGetMemoryStats(VEDevice *device, VEMemoryStats *stats);
   VULKEASE_API VEResult veGetRenderConfigStats(VEDevice *device, VERenderConfigStats *stats);

   /**
    * Debug information
    */
   VULKEASE_API void vePrintDebugInfo(VEDevice *device);
   VULKEASE_API void vePrintProfileInfo(VEDevice *device);
   VULKEASE_API void vePrintRenderConfig(VERenderConfig *config);
   VULKEASE_API VEResult veValidateRenderConfig(VERenderConfig *config);

   // =============================================================================
   // Utility Macros and Helper Functions
   // =============================================================================

   // =============================================================================
   // Push Constants Structures
   // =============================================================================

   // Standard push constants structure for graphics shaders - Now 256 bytes
   typedef struct VEGraphicsPushConstants
   {
      VEBufferAddress vertexBuffer;      // 8 bytes - vertex data address
      VEBufferAddress indexBuffer;       // 8 bytes - index data address (optional)
      VEBufferAddress uniformBuffers[8]; // 64 bytes - up to 8 uniform buffer addresses (increased from 4)
      VETextureIndex textures[16];       // 64 bytes - up to 16 bindless texture indices (increased from 8)
      VESamplerIndex samplers[16];       // 64 bytes - up to 16 bindless sampler indices (increased from 8)
      float objectScale;                 // 4 bytes - per-object scale
      uint32_t activeTextureCount;       // 4 bytes - number of active textures (0-16)
      uint32_t activeSamplerCount;       // 4 bytes - number of active samplers (0-16)
      uint32_t activeUniformCount;       // 4 bytes - number of active uniform buffers (0-8)
      uint32_t reserved[7];              // 28 bytes - reserved for future use
   } VEGraphicsPushConstants;            // Total: 256 bytes

   // Standard push constants for compute shaders - Now 256 bytes
   typedef struct VEComputePushConstants
   {
      VEBufferAddress buffers[16]; // 128 bytes - up to 16 buffer addresses (increased from 8)
      VETextureIndex textures[16]; // 64 bytes - up to 16 texture indices (increased from 8)
      VESamplerIndex samplers[8];  // 32 bytes - up to 8 sampler indices (compute typically needs fewer)
      uint32_t elementCount;       // 4 bytes - number of elements to process
      uint32_t activeBufferCount;  // 4 bytes - number of active buffers (0-16)
      uint32_t activeTextureCount; // 4 bytes - number of active textures (0-16)
      uint32_t activeSamplerCount; // 4 bytes - number of active samplers (0-8)
      uint32_t reserved[4];        // 16 bytes - reserved for future use
   } VEComputePushConstants;       // Total: 256 bytes

// =============================================================================
// Helper Macros - Updated for new push constant sizes
// =============================================================================

// Initialize push constant structures with safe defaults
#define VE_INIT_GRAPHICS_PUSH_CONSTANTS()                                                                              \
   {                                                                                                                   \
      .vertexBuffer = VE_INVALID_ADDRESS, .indexBuffer = VE_INVALID_ADDRESS,                                           \
      .uniformBuffers = {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,               \
                         VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS},              \
      .textures = {VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX},                                                                          \
      .samplers = {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,                       \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,                       \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,                       \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,                       \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,                       \
                   VE_INVALID_SAMPLER_INDEX},                                                                          \
      .objectScale = 1.0f, .activeTextureCount = 0, .activeSamplerCount = 0, .activeUniformCount = 0, .reserved = {    \
         0,                                                                                                            \
         0,                                                                                                            \
         0,                                                                                                            \
         0,                                                                                                            \
         0,                                                                                                            \
         0,                                                                                                            \
         0                                                                                                             \
      }                                                                                                                \
   }

#define VE_INIT_COMPUTE_PUSH_CONSTANTS()                                                                               \
   {                                                                                                                   \
      .buffers = {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                      \
                  VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                      \
                  VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS,                      \
                  VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS},                     \
      .textures = {VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX,                       \
                   VE_INVALID_TEXTURE_INDEX},                                                                          \
      .samplers = {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,                       \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX,                       \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX},                                                \
      .elementCount = 0, .activeBufferCount = 0, .activeTextureCount = 0, .activeSamplerCount = 0, .reserved = {       \
         0,                                                                                                            \
         0,                                                                                                            \
         0,                                                                                                            \
         0                                                                                                             \
      }                                                                                                                \
   }

// Color constants
#define VE_COLOR_BLACK ((VEColor){0.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_WHITE ((VEColor){1.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_RED ((VEColor){1.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_GREEN ((VEColor){0.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_BLUE ((VEColor){0.0f, 0.0f, 1.0f, 1.0f})
#define VE_COLOR_YELLOW ((VEColor){1.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_CYAN ((VEColor){0.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_MAGENTA ((VEColor){1.0f, 0.0f, 1.0f, 1.0f})

#ifdef __cplusplus
}
#endif

#endif // VULKEASE2_H
