/**
 * @file ve_internal.h
 * @brief VulkEase 2.0 Internal Headers and Definitions
 *
 * Internal structures and function declarations for VulkEase 2.0
 * implementation. This header is not exposed to users of the library.
 */

#ifndef VE_INTERNAL_H
#define VE_INTERNAL_H

#include "vulkease.h"

// VMA integration - declare interface only
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "../external/vk_mem_alloc.h"

// STB Image integration - declare interface only
#include "../external/stb_image.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdint>
#include <memory>
#include <mutex>

struct VEDeviceInternal;
struct VECommandPool;
struct VECommandBufferInternal;
struct VEDeviceQueueLocks;

// =============================================================================
// Internal Constants
// =============================================================================

#define VE_MAX_BUFFERS 65536
#define VE_MAX_TEXTURES 65536
#define VE_MAX_SAMPLERS 4096
#define VE_MAX_COMMAND_BUFFERS 128
#define VE_MAX_DEBUG_NAME_LENGTH 256
#define VE_MAX_RENDER_CONFIGS 1024
#define VE_MAX_VERTEX_CONFIGS 1024
#define VE_MAX_SHADER_CONFIGS 1024
#define VE_MAX_SHADERS 4096
#define VE_MAX_SWAPCHAIN_IMAGES 8
#define VE_MAX_FRAMES_IN_FLIGHT 3
#define VE_MAX_COLOR_ATTACHMENTS 4
#define VE_MAX_VERTEX_BINDINGS 16
#define VE_MAX_VERTEX_ATTRIBUTES 16
#define VE_MAX_PUSH_CONSTANT_BYTES 256

// =============================================================================
// Internal Structures
// =============================================================================

// =============================================================================
// Plain Data Structures (POD)
// =============================================================================

struct VEContextInternal
{
   VkInstance instance;
   VkDebugUtilsMessengerEXT debugMessenger;
   bool validationEnabled;
   char applicationName[256];
};

struct VEQueueFamilies
{
   uint32_t graphicsFamily;
   uint32_t computeFamily;
   uint32_t transferFamily;
};

struct VEDeviceFeatures
{
   // Core Vulkan 1.0 features
   bool samplerAnisotropy;
   bool fillModeNonSolid;
   bool wideLines;
   bool depthClamp;

   // Core Vulkan 1.2 features (mandatory in 1.4)
   bool bufferDeviceAddress;
   bool descriptorIndexing;
   bool scalarBlockLayout; // Mandatory in 1.4
   bool updateAfterBind;
   bool shaderInt8;  // Mandatory in 1.4
   bool shaderInt16; // Mandatory in 1.4

   // Core Vulkan 1.3 features (mandatory)
   bool dynamicRendering;

   // Core Vulkan 1.4 features (new mandatory features)
   bool pushDescriptor;            // Mandatory in 1.4 - replaces VK_KHR_push_descriptor
   bool dynamicRenderingLocalRead; // Optional but highly recommended

   // Optional Vulkan 1.4 features
   bool hostImageCopy; // Optional - VK_EXT_host_image_copy

   // Extension features (still required in 1.4)
   bool extendedDynamicState3;   // VK_EXT_extended_dynamic_state3
   bool vertexInputDynamicState; // VK_EXT_vertex_input_dynamic_state
   bool shaderObject;            // VK_EXT_shader_object
};

// =============================================================================
// RAII-enabled Helpers
// =============================================================================

struct VEDeviceQueueLocks
{
   std::mutex graphics;
   std::mutex compute;
   std::mutex transfer;

   [[nodiscard]] std::mutex &graphicsMutex() noexcept;
   [[nodiscard]] std::mutex &computeMutex() noexcept;
   [[nodiscard]] std::mutex &transferMutex() noexcept;

   void lockGraphics();
   void unlockGraphics();
   void lockCompute();
   void unlockCompute();
   void lockTransfer();
   void unlockTransfer();
};

struct VECommandBufferInternal
{
   VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
   VECommandPool *commandPool{nullptr};
   VEDeviceInternal *device{nullptr};
   bool isRecording{false};
   bool isOneTime{false};
   uint32_t index{0};
   VkShaderStageFlags boundShaders{0};
   VkFence inFlightFence{VK_NULL_HANDLE};
   VkFence activeFence{VK_NULL_HANDLE};
   bool fenceActive{false};
   VkPrimitiveTopology currentTopology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
   uint32_t currentPatchControlPoints{0};

   void clearFenceTracking();
   void markFenceActive(VkFence fence);
   [[nodiscard]] VkFence currentFence() const;
   void resetState();
};

struct VECommandPool
{
   VkCommandPool commandPool{VK_NULL_HANDLE};
   VECommandBufferInternal *commandBuffers{nullptr};
   bool *commandBufferInUse{nullptr};
   uint32_t nextFreeCommandBuffer{0};
   uint32_t commandBufferCount{0};
   uint32_t queueFamily{0};
   std::unique_ptr<std::mutex> allocationLock;
   VEDeviceInternal *ownerDevice{nullptr};

   VECommandPool() = default;
   VECommandPool(const VECommandPool &) = delete;
   VECommandPool &operator=(const VECommandPool &) = delete;
   ~VECommandPool();

   [[nodiscard]] bool initialize(VEDeviceInternal *device, uint32_t queueFamily);
   void destroy();
   [[nodiscard]] VEResult allocate(VECommandBufferInternal **outCmd);
   void release(VECommandBufferInternal &cmd);
   void notifyFenceSignaled(VkFence fence);

 private:
   void releaseCommandBufferResources();
};

struct VEBufferInternal
{
   VkBuffer buffer;
   VmaAllocation allocation;
   VmaAllocationInfo allocationInfo;
   VkDeviceAddress deviceAddress;
   uint64_t size;
   VkBufferUsageFlags usage;
   bool persistentlyMapped;
   void *mappedData;
   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   bool isValid;
};

struct VETextureInternal
{
   VkImage image;
   VkImageView imageView;
   VmaAllocation allocation;
   VmaAllocationInfo allocationInfo;
   uint32_t width;
   uint32_t height;
   uint32_t depth;
   uint32_t mipLevels;
   uint32_t arrayLayers;
   VkFormat format;
   VkImageUsageFlags usage;
   VkSampleCountFlags sampleCount;
   VkImageLayout currentLayout; // Track current layout for optimized transitions
   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   bool isValid;
   uint32_t index;
};

struct VESamplerInternal
{
   VkSampler sampler;
   VESamplerDesc desc;
   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   bool isValid;
   uint32_t index;
};

struct VEShaderInternal
{
   VkShaderEXT shaderObject;
   VkShaderStageFlags stage;
   char entryPoint[64];
   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   char sourceFile[512]; // For hot-reload
   bool hotReloadEnabled;
   bool isValid;
   VEDeviceInternal *device;
};

struct VERenderConfigInternal
{
   VEViewport viewport;
   VERect2D scissor;
   VERasterConfig rasterConfig;
   VEDepthConfig depthConfig;
   VEBlendConfig blendConfig;
   VEMultisampleConfig multisampleConfig;
   VEVertexInputConfig vertexInputConfig;

   VEConfigTypeFlags configTypes;
   uint32_t shaderCount;
   VEShader *shaders[6]; // Max 6 shader stages

   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   bool isValid;
   VEDeviceInternal *device;
};

struct VEVertexConfigInternal
{
   uint32_t bindingCount;
   VEVertexBinding bindings[16];
   uint32_t attributeCount;
   VEVertexAttribute attributes[32];
   VkPrimitiveTopology topology;
   bool primitiveRestartEnable;

   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   bool isValid;
   VEDeviceInternal *device;
};

struct VEShaderConfigInternal
{
   VEShader *vertexShader;
   VEShader *fragmentShader;
   VEShader *geometryShader;
   VEShader *tessControlShader;
   VEShader *tessEvalShader;
   VEShader *computeShader;

   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   bool isValid;
   VEDeviceInternal *device;
};

struct VESwapchainInternal
{
   VkSwapchainKHR swapchain{VK_NULL_HANDLE};
   VkSurfaceKHR surface{VK_NULL_HANDLE};
   VkFormat format{VK_FORMAT_UNDEFINED};
   uint32_t width{0};
   uint32_t height{0};
   uint32_t imageCount{0};
   VkImage images[VE_MAX_SWAPCHAIN_IMAGES]{};
   VkImageView imageViews[VE_MAX_SWAPCHAIN_IMAGES]{};
   VETextureIndex textureIndices[VE_MAX_SWAPCHAIN_IMAGES]{};
   uint32_t currentImageIndex{UINT32_MAX};

   uint32_t maxFramesInFlight{VE_MAX_FRAMES_IN_FLIGHT};
   uint32_t currentFrame{0};
   VkSemaphore imageAvailableSemaphores[VE_MAX_FRAMES_IN_FLIGHT]{};
   VkFence inFlightFences[VE_MAX_FRAMES_IN_FLIGHT]{};

   VkSemaphore renderFinishedSemaphores[VE_MAX_SWAPCHAIN_IMAGES]{};

   bool needsRecreation{false};
   VEDeviceInternal *device{nullptr};

   VESwapchainInternal() = default;
   VESwapchainInternal(const VESwapchainInternal &) = delete;
   VESwapchainInternal &operator=(const VESwapchainInternal &) = delete;
   ~VESwapchainInternal();

   void attachDevice(VEDeviceInternal *deviceInternal) noexcept;
   void markForResize(uint32_t newWidth, uint32_t newHeight) noexcept;
   [[nodiscard]] VETextureIndex acquireNextImage();
   [[nodiscard]] VEResult present(VECommandBufferInternal &cmd);
   void querySize(uint32_t *outWidth, uint32_t *outHeight) const noexcept;
   [[nodiscard]] VkFormat currentFormat() const noexcept;
   void requestRecreation(bool value = true) noexcept;
   bool hasDevice() const noexcept;
   void destroy();

 private:
   VEResult waitForCurrentFrameFence();
   void releaseTextureIndices();
   void destroySyncObjects();
   void destroyImageViews();
   void destroySurfaceAndSwapchain();
};

struct VEDeviceInternal
{
   VEContextInternal *context;

   VkPhysicalDevice physicalDevice;
   VkDevice device;

   VkPhysicalDeviceProperties deviceProperties;
   VkPhysicalDeviceMemoryProperties memoryProperties;
   VEDeviceFeatures features;
   VEQueueFamilies queueFamilies;

   VkQueue graphicsQueue;
   VkQueue computeQueue;
   VkQueue transferQueue;

   VmaAllocator allocator;

   // Resource management - simplified for address-only API
   void *bufferMap; // std::unordered_map<VEBufferAddress, VEBufferInternal*>*

   VETextureInternal *textures;
   uint32_t *freeTextureIndices;
   uint32_t freeTextureCount;
   uint32_t textureCount;
   uint32_t maxTextures;

   VESamplerInternal *samplers;
   uint32_t *freeSamplerIndices;
   uint32_t freeSamplerCount;
   uint32_t samplerCount;
   uint32_t maxSamplers;

   VEShaderInternal *shaders;
   uint32_t shaderCount;
   uint32_t maxShaders;

   VERenderConfigInternal *renderConfigs;
   uint32_t renderConfigCount;
   uint32_t maxRenderConfigs;

   VEVertexConfigInternal *vertexConfigs;
   uint32_t vertexConfigCount;
   uint32_t maxVertexConfigs;

   VEShaderConfigInternal *shaderConfigs;
   uint32_t shaderConfigCount;
   uint32_t maxShaderConfigs;

   VECommandPool *graphicsCommandPool;
   VECommandPool *computeCommandPool;
   VECommandPool *transferCommandPool;

   VkDescriptorPool descriptorPool;
   VkDescriptorSetLayout textureDescriptorSetLayout;
   VkDescriptorSetLayout samplerDescriptorSetLayout;
   VkDescriptorSet textureDescriptorSet;
   VkDescriptorSet samplerDescriptorSet;

   VkPipelineLayout globalGraphicsPipelineLayout;
   VkPipelineLayout globalComputePipelineLayout;

   VEPerformanceStats performanceStats;
   VEPerformanceStats frameStats;
   VEMemoryStats memoryStats;
   VERenderConfigStats renderConfigStats;
   double lastFrameTimestampSeconds{0.0};

   std::unique_ptr<VEDeviceQueueLocks> queueLocks;

   VEResult initializeVma();
   void cleanupVma();
   VEBufferInternal *getBufferFromAddress(VEBufferAddress address);
   bool validateBufferAddress(VEBufferAddress address) const;
   VkBuffer getVkBufferFromAddress(VEBufferAddress address) const;
   VEResult initializeBindlessDescriptors();
   void cleanupBindlessDescriptors();
   uint32_t allocateTextureIndex();
   void freeTextureIndex(uint32_t index);
   VETextureInternal *getTexture(VETextureIndex index);
   const VETextureInternal *getTexture(VETextureIndex index) const;
   VkImageView getImageViewFromTexture(VETextureIndex index) const;
   uint32_t allocateSamplerIndex();
   void freeSamplerIndex(uint32_t index);
   VESamplerInternal *getSampler(VESamplerIndex index);
   const VESamplerInternal *getSampler(VESamplerIndex index) const;
   VEResult updateTextureDescriptor(VETextureIndex index);
   VEResult updateSamplerDescriptor(VESamplerIndex index);
   VECommandBufferInternal *beginTransferCommandBuffer();
   VEResult submitTransferCommandBuffer(VECommandBufferInternal *cmd, bool waitForCompletion);

   [[nodiscard]] bool supportsBufferDeviceAddress() const noexcept { return features.bufferDeviceAddress; }
   [[nodiscard]] bool supportsDescriptorIndexing() const noexcept { return features.descriptorIndexing; }
   [[nodiscard]] bool supportsShaderObjects() const noexcept { return features.shaderObject; }
   [[nodiscard]] bool supportsExtendedDynamicState3() const noexcept { return features.extendedDynamicState3; }
   [[nodiscard]] bool supportsVertexInputDynamicState() const noexcept { return features.vertexInputDynamicState; }
};

// =============================================================================
// Internal Function Declarations
// =============================================================================

// Error handling
void veSetError(const char *format, ...);
const char *veResultToString(VkResult result);
void vePrintVkResult(const char *operation, VkResult result);

// Command pool managment
VECommandBufferInternal *veGetCommandBufferInternal(VECommandBuffer *cmd);
VEResult veAllocateCommandBuffer(VEDeviceInternal *device, VECommandPool *pool, VECommandBufferInternal **outCmd);
void veFreeCommandBuffer(VECommandBufferInternal *cmd);
void veInitializeQueueLocks(VEDeviceInternal *device);
void veDestroyQueueLocks(VEDeviceInternal *device);
void veNotifyCommandBufferFenceSignaled(VEDeviceInternal *device, VkFence fence);

// Resource index management functions
// Utility functions
void veSetObjectDebugName(VEDeviceInternal *device, uint64_t objectHandle, VkObjectType objectType, const char *name);

// Surface creation (platform-specific)
VkResult veCreateSurface(VEContextInternal *context, void *windowHandle, VkSurfaceKHR *surface);

struct VEFuncs
{
   // VK_EXT_shader_object
   PFN_vkCreateShadersEXT vkCreateShadersEXT;
   PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT;
   PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT;
   PFN_vkDestroyShaderEXT vkDestroyShaderEXT;

   // VK_EXT_extended_dynamic_state3
   PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT;
   PFN_vkCmdSetDepthClampEnableEXT vkCmdSetDepthClampEnableEXT;
   PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT;
   PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT;
   PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT;
   PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT;
   PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT;
   PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT;
   PFN_vkCmdSetAlphaToOneEnableEXT vkCmdSetAlphaToOneEnableEXT;
   PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
   PFN_vkCmdSetConservativeRasterizationModeEXT vkCmdSetConservativeRasterizationModeEXT;
   PFN_vkCmdSetLineRasterizationModeEXT vkCmdSetLineRasterizationModeEXT;
   PFN_vkCmdSetProvokingVertexModeEXT vkCmdSetProvokingVertexModeEXT;

   // extended dynamic state 2 logic op
   PFN_vkCmdSetLogicOpEnableEXT vkCmdSetLogicOpEnableEXT;
   PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;

   // VK_EXT_vertex_input_dynamic_state (still required)
   PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;

   // Push Descriptors (now core in 1.4 - use core function names)
   PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
   PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;

   // Host Image Copy (optional extension)
   PFN_vkCopyMemoryToImageEXT vkCopyMemoryToImageEXT;
   PFN_vkCopyImageToMemoryEXT vkCopyImageToMemoryEXT;
   PFN_vkCopyImageToImageEXT vkCopyImageToImageEXT;

   // Instance functions to initialize
   PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
   PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
   PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
   PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
   PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
   PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
   PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
};

extern VEFuncs veFuncs;

bool initializeInstanceFunctions(VkInstance instance);
bool initializeDeviceFunctions(VkDevice device);

#endif // VE_INTERNAL_H
