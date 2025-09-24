/**
 * @file ve_internal.h
 * @brief VulkEase 2.0 Internal Headers and Definitions
 * 
 * Internal structures and function declarations for VulkEase 2.0 implementation.
 * This header is not exposed to users of the library.
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Internal Constants
// =============================================================================

#define VE_MAX_BUFFERS 65536
#define VE_MAX_TEXTURES 65536
#define VE_MAX_SAMPLERS 4096
#define VE_MAX_COMMAND_BUFFERS 64
#define VE_MAX_DEBUG_NAME_LENGTH 256
#define VE_MAX_RENDER_CONFIGS 1024
#define VE_MAX_VERTEX_CONFIGS 1024
#define VE_MAX_SHADER_CONFIGS 1024
#define VE_MAX_SHADERS 4096
#define VE_MAX_SWAPCHAIN_IMAGES 8
#define VE_MAX_COLOR_ATTACHMENTS 4
#define VE_MAX_VERTEX_BINDINGS 16
#define VE_MAX_VERTEX_ATTRIBUTES 16

// =============================================================================
// Forward Declarations
// =============================================================================

typedef struct VEDeviceInternal VEDeviceInternal;
typedef struct VECommandBufferInternal VECommandBufferInternal;

// =============================================================================
// Internal Structures
// =============================================================================

// Context internal structure
typedef struct VEContextInternal {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    bool validationEnabled;
    char applicationName[256];
} VEContextInternal;

// Queue families
typedef struct VEQueueFamilies {
    uint32_t graphicsFamily;
    uint32_t computeFamily;
    uint32_t transferFamily;
} VEQueueFamilies;

// Device features
typedef struct VEDeviceFeatures {
    // Core features
    bool samplerAnisotropy;
    bool fillModeNonSolid;
    bool wideLines;
    bool depthClamp;
    
    // Modern features (Vulkan 1.2+)
    bool bufferDeviceAddress;
    bool descriptorIndexing;
    bool dynamicRendering;
    bool updateAfterBind;
    
    // Extension features
    bool extendedDynamicState3;
    bool vertexInputDynamicState;
    bool shaderObject;
} VEDeviceFeatures;

// Command pool management
typedef struct VECommandPool
{
    VkCommandPool commandPool;
    VECommandBufferInternal* commandBuffers;
    bool* commandBufferInUse;
    uint32_t commandBufferCount;
    uint32_t queueFamily;
} VECommandPool;

// Buffer internal structure
typedef struct VEBufferInternal {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    VkDeviceAddress deviceAddress;
    size_t size;
    VkBufferUsageFlags usage;
    bool persistentlyMapped;
    void* mappedData;
    char debugName[VE_MAX_DEBUG_NAME_LENGTH];
    bool isValid;
} VEBufferInternal;

// Texture internal structure
typedef struct VETextureInternal {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    uint32_t width, height, depth;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkFormat format;
    VkImageUsageFlags usage;
    VkSampleCountFlags sampleCount;
    VkImageLayout currentLayout;  // Track current layout for optimized transitions
    char debugName[VE_MAX_DEBUG_NAME_LENGTH];
    bool isValid;
    uint32_t index;
} VETextureInternal;

// Sampler internal structure
typedef struct VESamplerInternal {
    VkSampler sampler;
    VESamplerDesc desc;
    char debugName[VE_MAX_DEBUG_NAME_LENGTH];
    bool isValid;
    uint32_t index;
} VESamplerInternal;

// Shader internal structure
typedef struct VEShaderInternal {
    VkShaderEXT shaderObject;
    VkShaderStageFlags stage;
    char entryPoint[64];
    char debugName[VE_MAX_DEBUG_NAME_LENGTH];
    char sourceFile[512]; // For hot-reload
    bool hotReloadEnabled;
    bool isValid;
    VEDeviceInternal* device;
} VEShaderInternal;

// Render configuration internal structure
typedef struct VERenderConfigInternal {
    VEViewport viewport;
    VERect2D scissor;
    VERasterConfig rasterConfig;
    VEDepthConfig depthConfig;
    VEBlendConfig blendConfig;
    VEMultisampleConfig multisampleConfig;
    VEVertexInputConfig vertexInputConfig;
    
    VEConfigTypeFlags configTypes;
    uint32_t shaderCount;
    VEShader* shaders[6]; // Max 6 shader stages
    
    char debugName[VE_MAX_DEBUG_NAME_LENGTH];
    bool isValid;
    VEDeviceInternal* device;
} VERenderConfigInternal;

// Vertex configuration internal structure
typedef struct VEVertexConfigInternal {
    uint32_t bindingCount;
    VEVertexBinding bindings[16];
    uint32_t attributeCount;
    VEVertexAttribute attributes[32];
    VkPrimitiveTopology topology;
    bool primitiveRestartEnable;
    
    char debugName[VE_MAX_DEBUG_NAME_LENGTH];
    bool isValid;
    VEDeviceInternal* device;
} VEVertexConfigInternal;

// Shader configuration internal structure  
typedef struct VEShaderConfigInternal {
    VEShader* vertexShader;
    VEShader* fragmentShader;
    VEShader* geometryShader;
    VEShader* tessControlShader;
    VEShader* tessEvalShader;
    VEShader* computeShader;
    
    char debugName[VE_MAX_DEBUG_NAME_LENGTH];
    bool isValid;
    VEDeviceInternal* device;
} VEShaderConfigInternal;

// Command buffer internal structure
typedef struct VECommandBufferInternal {
    VkCommandBuffer commandBuffer;
    VECommandPool* commandPool; // Reference to pool this came from
    VEDeviceInternal* device;  // Reference to device for submission
    bool isRecording;
    bool isOneTime;
    uint32_t index;
    VkShaderStageFlags boundShaders;
} VECommandBufferInternal;

// Swapchain internal structure
typedef struct VESwapchainInternal {
    VkSwapchainKHR swapchain;
    VkSurfaceKHR surface;
    VkFormat format;
    uint32_t width, height;
    uint32_t imageCount;
    VkImage images[VE_MAX_SWAPCHAIN_IMAGES];
    VkImageView imageViews[VE_MAX_SWAPCHAIN_IMAGES];
    VETextureIndex textureIndices[VE_MAX_SWAPCHAIN_IMAGES];
    uint32_t currentImageIndex;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    bool needsRecreation;
    VEDeviceInternal* device;
} VESwapchainInternal;


// Device internal structure
typedef struct VEDeviceInternal {
    VEContextInternal* context;
    
    // Vulkan objects
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    
    // Device properties
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VEDeviceFeatures features;
    VEQueueFamilies queueFamilies;
    
    // Queues
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;
    
    // VMA allocator
    VmaAllocator allocator;
    
    // Resource management - simplified for address-only API
    void* bufferMap; // std::unordered_map<VEBufferAddress, VEBufferInternal*>* (C++ container)
    
    VETextureInternal* textures;
    uint32_t* freeTextureIndices;
    uint32_t freeTextureCount;
    uint32_t textureCount;
    uint32_t maxTextures;
    
    VESamplerInternal* samplers;
    uint32_t* freeSamplerIndices;
    uint32_t freeSamplerCount;
    uint32_t samplerCount;
    uint32_t maxSamplers;
    
    VEShaderInternal* shaders;
    uint32_t shaderCount;
    uint32_t maxShaders;
    
    VERenderConfigInternal* renderConfigs;
    uint32_t renderConfigCount;
    uint32_t maxRenderConfigs;
    
    VEVertexConfigInternal* vertexConfigs;
    uint32_t vertexConfigCount;
    uint32_t maxVertexConfigs;
    
    VEShaderConfigInternal* shaderConfigs;
    uint32_t shaderConfigCount;
    uint32_t maxShaderConfigs;
    
    VECommandPool* graphicsCommandPool;
    VECommandPool* computeCommandPool;
    VECommandPool* transferCommandPool;
    
    // Bindless descriptor sets
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout textureDescriptorSetLayout;
    VkDescriptorSetLayout samplerDescriptorSetLayout;
    VkDescriptorSet textureDescriptorSet;
    VkDescriptorSet samplerDescriptorSet;

    VkPipelineLayout globalGraphicsPipelineLayout;
    VkPipelineLayout globalComputePipelineLayout;
    
    // Performance statistics
    VEPerformanceStats performanceStats;
    VEMemoryStats memoryStats;
    VERenderConfigStats renderConfigStats;
} VEDeviceInternal;

// =============================================================================
// Internal Function Declarations
// =============================================================================

// Error handling
void veSetError(const char* format, ...);
const char* veResultToString(VkResult result);
void vePrintVkResult(const char* operation, VkResult result);

// VMA management functions
VEResult veInitializeVMA(VEDeviceInternal* device);
void veCleanupVMA(VEDeviceInternal* device);

// Command pool managment
bool veInitCommandPool(VEDeviceInternal* device, uint32_t queueFamily, VECommandPool* pool);
bool veDestroyCommandPool(VEDeviceInternal* device, VECommandPool* pool);

VECommandBufferInternal* veGetCommandBufferInternal(VECommandBuffer* cmd);
VEResult veAllocateCommandBuffer(VEDeviceInternal* device, VECommandPool* pool, VECommandBufferInternal** outCmd);
void veFreeCommandBuffer(VECommandBufferInternal* cmd);

// Resource index management functions
uint32_t veAllocateBufferIndex(VEDeviceInternal* device);
void veFreeBufferIndex(VEDeviceInternal* device, uint32_t index);
VEBufferInternal* veGetBufferFromAddress(VEDeviceInternal* device, VEBufferAddress address);
bool veValidateBufferAddress(VEDeviceInternal* device, VEBufferAddress address);
VkBuffer veGetVkBufferFromAddress(VEDeviceInternal* device, VEBufferAddress address);

uint32_t veAllocateTextureIndex(VEDeviceInternal* device);
void veFreeTextureIndex(VEDeviceInternal* device, uint32_t index);
VETextureInternal* veGetTexture(VEDeviceInternal* device, VETextureIndex index);
VkImageView veGetImageViewFromTexture(VEDeviceInternal* device, VETextureIndex index);

uint32_t veAllocateSamplerIndex(VEDeviceInternal* device);
void veFreeSamplerIndex(VEDeviceInternal* device, uint32_t index);
VESamplerInternal* veGetSampler(VEDeviceInternal* device, VESamplerIndex index);

// Bindless descriptor management
VEResult veInitializeBindlessDescriptors(VEDeviceInternal* device);
void veCleanupBindlessDescriptors(VEDeviceInternal* device);
VEResult veUpdateTextureDescriptor(VEDeviceInternal* device, VETextureIndex index);
VEResult veUpdateSamplerDescriptor(VEDeviceInternal* device, VESamplerIndex index);

// Utility functions
void veSetObjectDebugName(VEDeviceInternal* device, uint64_t objectHandle, 
                         VkObjectType objectType, const char* name);

// Surface creation (platform-specific)
VkResult veCreateSurface(VEContextInternal* context, void* windowHandle, VkSurfaceKHR* surface);


typedef struct VEFuncs
{
    // Device Functions to initialize
    //VK_EXT_shader_object
    PFN_vkCreateShadersEXT vkCreateShadersEXT;
    PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT;
    PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT;
    PFN_vkDestroyShaderEXT vkDestroyShaderEXT;

    //VK_EXT_extended_dynamic_state3
    PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT;
    PFN_vkCmdSetDepthClampEnableEXT vkCmdSetDepthClampEnableEXT;
    PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT;
    PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT;
    PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT;
    PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
    PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT;
    PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT;
    PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT;
    PFN_vkCmdSetAlphaToOneEnableEXT vkCmdSetAlphaToOneEnableEXT;
    PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;

    PFN_vkCmdSetConservativeRasterizationModeEXT vkCmdSetConservativeRasterizationModeEXT;
    PFN_vkCmdSetLineRasterizationModeEXT vkCmdSetLineRasterizationModeEXT;
    PFN_vkCmdSetProvokingVertexModeEXT vkCmdSetProvokingVertexModeEXT;

    //extended dynamic state 2 logic op
    PFN_vkCmdSetLogicOpEnableEXT vkCmdSetLogicOpEnableEXT;
    PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;

    // Instance functions to initialize
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;

} VEFuncs;

extern VEFuncs veFuncs;

bool initializeInstanceFunctions(VkInstance instance);
bool initializeDeviceFunctions(VkDevice device);


#ifdef __cplusplus
}
#endif

#endif // VE_INTERNAL_H