/**
 * @file vulkease2.h
 * @brief VulkEase 2.0 - Complete Modern Vulkan 1.3+ Bindless Graphics API
 * 
 * The ultimate graphics programming API featuring:
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
 * - Vulkan 1.3+ (core dynamic rendering)
 * - VK_EXT_shader_object 
 * - VK_EXT_extended_dynamic_state3
 * - VK_EXT_vertex_input_dynamic_state (optional but recommended)
 */

#ifndef VULKEASE2_H
#define VULKEASE2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Version and API Information
// =============================================================================

#define VULKEASE_VERSION_MAJOR 2
#define VULKEASE_VERSION_MINOR 0
#define VULKEASE_VERSION_PATCH 0

#define VULKEASE_API_VERSION_2_0 VE_MAKE_VERSION(2, 0, 0)
#define VE_MAKE_VERSION(major, minor, patch) \
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

typedef enum VEResult {
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
// Enums and Constants
// =============================================================================

// Formats (subset of VkFormat)
typedef enum VEFormat {
    // 8-bit formats
    VE_FORMAT_R8_UNORM = 9,
    VE_FORMAT_RG8_UNORM = 16,
    VE_FORMAT_RGBA8_UNORM = 37,
    VE_FORMAT_RGBA8_SRGB = 43,
    VE_FORMAT_BGRA8_UNORM = 44,
    VE_FORMAT_BGRA8_SRGB = 50,
    
    // 16-bit formats
    VE_FORMAT_R16_UNORM = 70,
    VE_FORMAT_R16_SFLOAT = 76,
    VE_FORMAT_RG16_UNORM = 77,
    VE_FORMAT_RG16_SFLOAT = 83,
    VE_FORMAT_RGBA16_UNORM = 91,
    VE_FORMAT_RGBA16_SFLOAT = 97,
    
    // 32-bit formats
    VE_FORMAT_R32_UINT = 98,
    VE_FORMAT_R32_SINT = 99,
    VE_FORMAT_R32_SFLOAT = 100,
    VE_FORMAT_RG32_UINT = 101,
    VE_FORMAT_RG32_SINT = 102,
    VE_FORMAT_RG32_SFLOAT = 103,
    VE_FORMAT_RGB32_UINT = 104,
    VE_FORMAT_RGB32_SINT = 105,
    VE_FORMAT_RGB32_SFLOAT = 106,
    VE_FORMAT_RGBA32_UINT = 107,
    VE_FORMAT_RGBA32_SINT = 108,
    VE_FORMAT_RGBA32_SFLOAT = 109,
    
    // Depth/stencil formats
    VE_FORMAT_D16_UNORM = 124,
    VE_FORMAT_X8_D24_UNORM_PACK32 = 125,
    VE_FORMAT_D32_SFLOAT = 126,
    VE_FORMAT_S8_UINT = 127,
    VE_FORMAT_D16_UNORM_S8_UINT = 128,
    VE_FORMAT_D24_UNORM_S8_UINT = 129,
    VE_FORMAT_D32_SFLOAT_S8_UINT = 130,
    
    // Compressed formats
    VE_FORMAT_BC1_RGB_UNORM = 131,
    VE_FORMAT_BC1_RGB_SRGB = 132,
    VE_FORMAT_BC1_RGBA_UNORM = 133,
    VE_FORMAT_BC1_RGBA_SRGB = 134,
    VE_FORMAT_BC2_UNORM = 135,
    VE_FORMAT_BC2_SRGB = 136,
    VE_FORMAT_BC3_UNORM = 137,
    VE_FORMAT_BC3_SRGB = 138,
    VE_FORMAT_BC4_UNORM = 139,
    VE_FORMAT_BC4_SNORM = 140,
    VE_FORMAT_BC5_UNORM = 141,
    VE_FORMAT_BC5_SNORM = 142,
    VE_FORMAT_BC6H_UFLOAT = 143,
    VE_FORMAT_BC6H_SFLOAT = 144,
    VE_FORMAT_BC7_UNORM = 145,
    VE_FORMAT_BC7_SRGB = 146
} VEFormat;

// Buffer usage flags
typedef enum VEBufferUsage {
    VE_BUFFER_USAGE_VERTEX = 0x1,
    VE_BUFFER_USAGE_INDEX = 0x2,
    VE_BUFFER_USAGE_UNIFORM = 0x4,
    VE_BUFFER_USAGE_STORAGE = 0x8,
    VE_BUFFER_USAGE_INDIRECT = 0x10,
    VE_BUFFER_USAGE_TRANSFER_SRC = 0x20,
    VE_BUFFER_USAGE_TRANSFER_DST = 0x40,
    VE_BUFFER_USAGE_CONDITIONAL_RENDERING = 0x200
} VEBufferUsage;

// Texture usage flags
typedef enum VETextureUsage {
    VE_TEXTURE_USAGE_SAMPLED = 0x1,
    VE_TEXTURE_USAGE_STORAGE = 0x2,
    VE_TEXTURE_USAGE_COLOR_ATTACHMENT = 0x4,
    VE_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT = 0x8,
    VE_TEXTURE_USAGE_TRANSFER_SRC = 0x10,
    VE_TEXTURE_USAGE_TRANSFER_DST = 0x20,
    VE_TEXTURE_USAGE_INPUT_ATTACHMENT = 0x80
} VETextureUsage;

// Shader stages
typedef enum VEShaderStage {
    VE_SHADER_STAGE_VERTEX = 0x1,
    VE_SHADER_STAGE_GEOMETRY = 0x2,
    VE_SHADER_STAGE_TESSELLATION_CONTROL = 0x4,
    VE_SHADER_STAGE_TESSELLATION_EVALUATION = 0x8,
    VE_SHADER_STAGE_FRAGMENT = 0x10,
    VE_SHADER_STAGE_GRAPHICS = 0x1f,
    VE_SHADER_STAGE_COMPUTE = 0x20
} VEShaderStage;

// Filtering modes
typedef enum VEFilter {
    VE_FILTER_NEAREST = 0,
    VE_FILTER_LINEAR = 1
} VEFilter;

// Address modes for samplers
typedef enum VEAddressMode {
    VE_ADDRESS_MODE_REPEAT = 0,
    VE_ADDRESS_MODE_MIRRORED_REPEAT = 1,
    VE_ADDRESS_MODE_CLAMP_TO_EDGE = 2,
    VE_ADDRESS_MODE_CLAMP_TO_BORDER = 3,
    VE_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE = 4
} VEAddressMode;

// Dynamic rendering load/store operations
typedef enum VELoadOp {
    VE_LOAD_OP_LOAD = 0,
    VE_LOAD_OP_CLEAR = 1,
    VE_LOAD_OP_DONT_CARE = 2
} VELoadOp;

typedef enum VEStoreOp {
    VE_STORE_OP_STORE = 0,
    VE_STORE_OP_DONT_CARE = 1
} VEStoreOp;

typedef enum VEIndexFormat {
    VE_INDEX_UINT16 = 0,
    VE_INDEX_UINT32 = 1
} VEIndexFormat;

// =============================================================================
// Render Configuration Types (Modern State Management)
// =============================================================================

// Configuration type flags (inspired by Graphics Pipeline Libraries)
typedef enum VEConfigType {
    VE_CONFIG_TYPE_VERTEX_INPUT = 0x1,      // Vertex layout and input assembly
    VE_CONFIG_TYPE_RASTERIZATION = 0x2,     // Culling, polygon mode, depth bias  
    VE_CONFIG_TYPE_DEPTH_STENCIL = 0x4,     // Depth/stencil testing and operations
    VE_CONFIG_TYPE_COLOR_BLEND = 0x8,       // Color blending and write masks
    VE_CONFIG_TYPE_MULTISAMPLE = 0x10,      // MSAA and coverage operations
    VE_CONFIG_TYPE_SHADERS = 0x20,          // Shader object bindings
    VE_CONFIG_TYPE_VIEWPORT = 0x40,         // Viewport settings 
    VE_CONFIG_TYPE_SCISSOR = 0x80,          // Scissor settings
    VE_CONFIG_TYPE_COMPLETE = 0xFF          // All categories combined
} VEConfigType;

typedef uint32_t VEConfigTypeFlags;

// Rasterization state enums (VK_EXT_extended_dynamic_state3)
typedef enum VECullMode {
    VE_CULL_MODE_NONE = 0,
    VE_CULL_MODE_FRONT = 1,
    VE_CULL_MODE_BACK = 2,
    VE_CULL_MODE_FRONT_AND_BACK = 3
} VECullMode;

typedef enum VEFrontFace {
    VE_FRONT_FACE_COUNTER_CLOCKWISE = 0,
    VE_FRONT_FACE_CLOCKWISE = 1
} VEFrontFace;

typedef enum VEPolygonMode {
    VE_POLYGON_MODE_FILL = 0,
    VE_POLYGON_MODE_LINE = 1,
    VE_POLYGON_MODE_POINT = 2
} VEPolygonMode;

// Depth/stencil state enums
typedef enum VECompareOp {
    VE_COMPARE_OP_NEVER = 0,
    VE_COMPARE_OP_LESS = 1,
    VE_COMPARE_OP_EQUAL = 2,
    VE_COMPARE_OP_LESS_OR_EQUAL = 3,
    VE_COMPARE_OP_GREATER = 4,
    VE_COMPARE_OP_NOT_EQUAL = 5,
    VE_COMPARE_OP_GREATER_OR_EQUAL = 6,
    VE_COMPARE_OP_ALWAYS = 7
} VECompareOp;

typedef enum VEStencilOp {
    VE_STENCIL_OP_KEEP = 0,
    VE_STENCIL_OP_ZERO = 1,
    VE_STENCIL_OP_REPLACE = 2,
    VE_STENCIL_OP_INCREMENT_AND_CLAMP = 3,
    VE_STENCIL_OP_DECREMENT_AND_CLAMP = 4,
    VE_STENCIL_OP_INVERT = 5,
    VE_STENCIL_OP_INCREMENT_AND_WRAP = 6,
    VE_STENCIL_OP_DECREMENT_AND_WRAP = 7
} VEStencilOp;

// Debug and profiling enums
typedef enum VEObjectType {
    VE_OBJECT_TYPE_BUFFER,
    VE_OBJECT_TYPE_IMAGE,
    VE_OBJECT_TYPE_IMAGE_VIEW,
    VE_OBJECT_TYPE_SAMPLER,
    VE_OBJECT_TYPE_SHADER,
    VE_OBJECT_TYPE_COMMAND_BUFFER,
    VE_OBJECT_TYPE_QUEUE,
    VE_OBJECT_TYPE_DEVICE,
    VE_OBJECT_TYPE_INSTANCE,
    VE_OBJECT_TYPE_PHYSICAL_DEVICE
} VEObjectType;

typedef enum VEMessageSeverity {
    VE_MESSAGE_SEVERITY_VERBOSE,
    VE_MESSAGE_SEVERITY_INFO,
    VE_MESSAGE_SEVERITY_WARNING,
    VE_MESSAGE_SEVERITY_ERROR
} VEMessageSeverity;

// Color blending enums
typedef enum VEBlendFactor {
    VE_BLEND_FACTOR_ZERO = 0,
    VE_BLEND_FACTOR_ONE = 1,
    VE_BLEND_FACTOR_SRC_COLOR = 2,
    VE_BLEND_FACTOR_ONE_MINUS_SRC_COLOR = 3,
    VE_BLEND_FACTOR_DST_COLOR = 4,
    VE_BLEND_FACTOR_ONE_MINUS_DST_COLOR = 5,
    VE_BLEND_FACTOR_SRC_ALPHA = 6,
    VE_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 7,
    VE_BLEND_FACTOR_DST_ALPHA = 8,
    VE_BLEND_FACTOR_ONE_MINUS_DST_ALPHA = 9,
    VE_BLEND_FACTOR_CONSTANT_COLOR = 10,
    VE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = 11,
    VE_BLEND_FACTOR_CONSTANT_ALPHA = 12,
    VE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = 13
} VEBlendFactor;

typedef enum VEBlendOp {
    VE_BLEND_OP_ADD = 0,
    VE_BLEND_OP_SUBTRACT = 1,
    VE_BLEND_OP_REVERSE_SUBTRACT = 2,
    VE_BLEND_OP_MIN = 3,
    VE_BLEND_OP_MAX = 4
} VEBlendOp;

typedef enum VELogicOp {
    VE_LOGIC_OP_CLEAR = 0,
    VE_LOGIC_OP_AND = 1,
    VE_LOGIC_OP_AND_REVERSE = 2,
    VE_LOGIC_OP_COPY = 3,
    VE_LOGIC_OP_AND_INVERTED = 4,
    VE_LOGIC_OP_NO_OP = 5,
    VE_LOGIC_OP_XOR = 6,
    VE_LOGIC_OP_OR = 7,
    VE_LOGIC_OP_NOR = 8,
    VE_LOGIC_OP_EQUIVALENT = 9,
    VE_LOGIC_OP_INVERT = 10,
    VE_LOGIC_OP_OR_REVERSE = 11,
    VE_LOGIC_OP_COPY_INVERTED = 12,
    VE_LOGIC_OP_OR_INVERTED = 13,
    VE_LOGIC_OP_NAND = 14,
    VE_LOGIC_OP_SET = 15
} VELogicOp;

typedef enum VEColorComponentFlags {
    VE_COLOR_COMPONENT_R_BIT = 0x1,
    VE_COLOR_COMPONENT_G_BIT = 0x2,
    VE_COLOR_COMPONENT_B_BIT = 0x4,
    VE_COLOR_COMPONENT_A_BIT = 0x8,
    VE_COLOR_COMPONENT_ALL = 0xF
} VEColorComponentFlags;

// Vertex input enums (VK_EXT_vertex_input_dynamic_state)
typedef enum VEVertexInputRate {
    VE_VERTEX_INPUT_RATE_VERTEX = 0,
    VE_VERTEX_INPUT_RATE_INSTANCE = 1
} VEVertexInputRate;

typedef enum VEPrimitiveTopology {
    VE_PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
    VE_PRIMITIVE_TOPOLOGY_LINE_LIST = 1,
    VE_PRIMITIVE_TOPOLOGY_LINE_STRIP = 2,
    VE_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3,
    VE_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4,
    VE_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN = 5,
    VE_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY = 6,
    VE_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY = 7,
    VE_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY = 8,
    VE_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 9,
    VE_PRIMITIVE_TOPOLOGY_PATCH_LIST = 10
} VEPrimitiveTopology;

// Sample count for multisampling
typedef enum VESampleCount {
    VE_SAMPLE_COUNT_1 = 1,
    VE_SAMPLE_COUNT_2 = 2,
    VE_SAMPLE_COUNT_4 = 4,
    VE_SAMPLE_COUNT_8 = 8,
    VE_SAMPLE_COUNT_16 = 16,
    VE_SAMPLE_COUNT_32 = 32,
    VE_SAMPLE_COUNT_64 = 64
} VESampleCount;

// =============================================================================
// Structure Types
// =============================================================================

// Color value for clear operations and debug labels
typedef struct VEColor {
    float r, g, b, a;
} VEColor;

// 2D rectangle
typedef struct VERect2D {
    int32_t x, y;
    uint32_t width, height;
} VERect2D;

// Viewport
typedef struct VEViewport {
    float x, y;
    float width, height;
    float minDepth, maxDepth;
} VEViewport;

// Buffer creation descriptor
typedef struct VEBufferDesc {
    size_t size;
    VEBufferUsage usage;
    const void* initialData;        // Optional initial data
    size_t initialDataSize;
    bool persistentlyMapped;        // Keep CPU-mapped for frequent updates
    const char* debugName;          // Debug name (optional)
} VEBufferDesc;

// Texture creation descriptor  
typedef struct VETextureDesc {
    uint32_t width;
    uint32_t height;
    uint32_t depth;                 // 1 for 2D textures
    uint32_t mipLevels;            // 0 = auto-generate all mips
    uint32_t arrayLayers;          // 1 for single texture
    VEFormat format;
    VETextureUsage usage;
    VESampleCount sampleCount;     // For multisampled textures
    const void* initialData;        // Optional initial data
    size_t initialDataSize;
    const char* debugName;          // Debug name (optional)
} VETextureDesc;

// Sampler creation descriptor
typedef struct VESamplerDesc {
    VEFilter minFilter;
    VEFilter magFilter;
    VEFilter mipmapFilter;
    VEAddressMode addressModeU;
    VEAddressMode addressModeV;
    VEAddressMode addressModeW;
    float maxAnisotropy;           // 1.0 = no anisotropy
    bool compareEnable;            // For shadow mapping
    VECompareOp compareOp;
    float minLod;
    float maxLod;
    const char* debugName;          // Debug name (optional)
} VESamplerDesc;

// =============================================================================
// Render Configuration Structures
// =============================================================================

// Rasterization configuration (VK_EXT_extended_dynamic_state3)
typedef struct VERasterConfig {
    // Basic rasterization
    VECullMode cullMode;                    // VK_DYNAMIC_STATE_CULL_MODE
    VEFrontFace frontFace;                  // VK_DYNAMIC_STATE_FRONT_FACE
    VEPolygonMode polygonMode;              // VK_DYNAMIC_STATE_POLYGON_MODE_EXT
    float lineWidth;                        // VK_DYNAMIC_STATE_LINE_WIDTH
    
    // Depth bias
    bool depthBiasEnable;                   // VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE
    float depthBiasConstantFactor;          // VK_DYNAMIC_STATE_DEPTH_BIAS
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    
    // Advanced rasterization
    bool depthClampEnable;                  // VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT
    bool rasterizerDiscardEnable;           // VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE
} VERasterConfig;

// Depth/stencil configuration
typedef struct VEDepthConfig {
    // Depth testing
    bool depthTestEnable;                   // VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
    bool depthWriteEnable;                  // VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
    VECompareOp depthCompareOp;            // VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
    
    // Depth bounds
    bool depthBoundsTestEnable;            // VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE
    float minDepthBounds;                  // VK_DYNAMIC_STATE_DEPTH_BOUNDS
    float maxDepthBounds;
    
    // Stencil testing
    bool stencilTestEnable;                // VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE
    
    // Front face stencil
    VEStencilOp frontFailOp;
    VEStencilOp frontPassOp;
    VEStencilOp frontDepthFailOp;
    VECompareOp frontCompareOp;
    uint32_t frontCompareMask;             // VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
    uint32_t frontWriteMask;               // VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
    uint32_t frontReference;               // VK_DYNAMIC_STATE_STENCIL_REFERENCE
    
    // Back face stencil  
    VEStencilOp backFailOp;
    VEStencilOp backPassOp;
    VEStencilOp backDepthFailOp;
    VECompareOp backCompareOp;
    uint32_t backCompareMask;
    uint32_t backWriteMask;
    uint32_t backReference;
} VEDepthConfig;

// Color blending configuration (VK_EXT_extended_dynamic_state3)
typedef struct VEBlendAttachment {
    bool blendEnable;                      // VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT
    VEBlendFactor srcColorBlendFactor;     // VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT
    VEBlendFactor dstColorBlendFactor;
    VEBlendOp colorBlendOp;
    VEBlendFactor srcAlphaBlendFactor;
    VEBlendFactor dstAlphaBlendFactor;
    VEBlendOp alphaBlendOp;
    VEColorComponentFlags colorWriteMask;  // VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT
} VEBlendAttachment;

typedef struct VEBlendConfig {
    bool logicOpEnable;                    // VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT
    VELogicOp logicOp;                     // VK_DYNAMIC_STATE_LOGIC_OP_EXT
    
    uint32_t attachmentCount;              // Up to 8 attachments
    VEBlendAttachment attachments[8];
    
    float blendConstants[4];               // VK_DYNAMIC_STATE_BLEND_CONSTANTS
} VEBlendConfig;

// Multisample configuration
typedef struct VEMultisampleConfig {
    VESampleCount rasterizationSamples;
    bool sampleShadingEnable;
    float minSampleShading;
    bool alphaToCoverageEnable;
    bool alphaToOneEnable;
} VEMultisampleConfig;

// Vertex input configuration (VK_EXT_vertex_input_dynamic_state)
typedef struct VEVertexBinding {
    uint32_t binding;
    uint32_t stride;
    VEVertexInputRate inputRate;
    uint32_t divisor;                      // For instanced rendering
} VEVertexBinding;

typedef struct VEVertexAttribute {
    uint32_t location;
    uint32_t binding;
    VEFormat format;
    uint32_t offset;
} VEVertexAttribute;

typedef struct VEVertexInputConfig {
    uint32_t bindingCount;
    VEVertexBinding* bindings;             // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT
    
    uint32_t attributeCount;
    VEVertexAttribute* attributes;         // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT
    
    VEPrimitiveTopology topology;          // VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
    bool primitiveRestartEnable;           // VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE

    uint32_t patchControlPoints;           // VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT
} VEVertexInputConfig;

// Complete render configuration descriptor
typedef struct VERenderConfigDesc {
    VEConfigTypeFlags configTypes;        // Which config categories to include
    
    // Configuration components (NULL = use defaults)
    const VEViewport* viewportConfig;
    const VERect2D* scissorConfig;
    const VERasterConfig* rasterConfig;
    const VEDepthConfig* depthConfig;
    const VEBlendConfig* blendConfig;
    const VEMultisampleConfig* multisampleConfig;
    const VEVertexInputConfig* vertexInputConfig;
    
    // Shader bindings (optional - can bind separately)
    uint32_t shaderCount;
    VEShader* const* shaders;
    
    const char* debugName;
} VERenderConfigDesc;

// Shader configuration descriptor  
typedef struct VEShaderConfigDesc {
    VEShader* vertexShader;
    VEShader* fragmentShader;
    VEShader* geometryShader;              // Optional
    VEShader* tessControlShader;           // Optional
    VEShader* tessEvalShader;              // Optional
    VEShader* computeShader;               // For compute dispatches
    
    const char* debugName;
} VEShaderConfigDesc;

// =============================================================================
// Dynamic Rendering Structures
// =============================================================================

// Rendering attachment (for dynamic rendering)
typedef struct VERenderingAttachment {
    VETextureIndex texture;                // Texture to render to
    VELoadOp loadOp;
    VEStoreOp storeOp;
    VEColor clearValue;                    // Used if loadOp == VE_LOAD_OP_CLEAR
    VETextureIndex resolveTexture;         // For MSAA resolve (optional)
} VERenderingAttachment;

// Dynamic rendering info (replaces render passes)
typedef struct VERenderingInfo {
    uint32_t renderAreaX, renderAreaY;
    uint32_t renderAreaWidth, renderAreaHeight;
    
    uint32_t colorAttachmentCount;
    const VERenderingAttachment* colorAttachments;
    
    const VERenderingAttachment* depthAttachment;     // Optional
    const VERenderingAttachment* stencilAttachment;   // Optional
} VERenderingInfo;

// =============================================================================
// Statistics and Debug Structures
// =============================================================================

// Performance statistics
typedef struct VEPerformanceStats {
    uint64_t frameTime;                    // GPU time in nanoseconds
    uint32_t drawCalls;
    uint32_t computeDispatches;
    uint64_t verticesRendered;
    uint64_t trianglesRendered;
    uint32_t pipelineBinds;               // For legacy compatibility
    uint32_t descriptorBinds;             // For legacy compatibility
} VEPerformanceStats;

// Memory statistics
typedef struct VEMemoryStats {
    uint64_t totalAllocated;              // Total VMA allocations in bytes
    uint64_t totalUsed;                   // Actually used memory in bytes
    uint32_t bufferCount;                 // Number of active buffers
    uint32_t textureCount;                // Number of active textures
    uint32_t samplerCount;                // Number of active samplers
    
    // Per-heap statistics
    struct {
        uint64_t size;
        uint64_t used;
        uint64_t budget;
    } heaps[16];
} VEMemoryStats;

// Render configuration statistics
typedef struct VERenderConfigStats {
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
 * Create VulkEase context - initializes Vulkan 1.3 instance
 * Automatically enables all required extensions:
 * - VK_KHR_buffer_device_address (core 1.2)
 * - VK_EXT_descriptor_indexing (core 1.2)
 * - VK_KHR_dynamic_rendering (core 1.3)
 * - VK_EXT_shader_object
 * - VK_EXT_extended_dynamic_state3
 * - VK_EXT_vertex_input_dynamic_state (if available)
 */
VULKEASE_API VEContext* veCreateContext(const char* applicationName);
VULKEASE_API void veDestroyContext(VEContext* context);

/**
 * Create device - automatically selects best GPU and enables all features
 */
VULKEASE_API VEDevice* veCreateDevice(VEContext* context);
VULKEASE_API void veDestroyDevice(VEDevice* device);

/**
 * Device wait operations
 */
VULKEASE_API VEResult veDeviceWaitIdle(VEDevice* device);

/**
 * Get device information
 */
VULKEASE_API const char* veGetDeviceName(VEDevice* device);
VULKEASE_API const char* veGetDriverVersion(VEDevice* device);
VULKEASE_API uint32_t veGetVulkanVersion(VEDevice* device);

/**
 * Get last error message
 */
VULKEASE_API const char* veGetLastError(void);

// =============================================================================
// Buffer Management (Buffer Device Address)
// =============================================================================

/**
 * Create buffer and return its GPU address
 * All buffers are created with SHADER_DEVICE_ADDRESS usage when supported
 */
VULKEASE_API VEBufferAddress veCreateBuffer(VEDevice* device, const VEBufferDesc* desc);

/**
 * Destroy buffer by address
 */
VULKEASE_API void veDestroyBuffer(VEDevice* device, VEBufferAddress address);

/**
 * Map buffer for CPU access (if created with persistentlyMapped=false)
 */
VULKEASE_API VEResult veMapBuffer(VEDevice* device, VEBufferAddress address, void** mappedData);
VULKEASE_API void veUnmapBuffer(VEDevice* device, VEBufferAddress address);

/**
 * Update buffer data (works with both mapped and unmapped buffers)
 */
VULKEASE_API VEResult veUpdateBuffer(VEDevice* device, VEBufferAddress address,
                                    const void* data, size_t size, size_t offset);

/**
 * Get buffer size and properties
 */
VULKEASE_API size_t veGetBufferSize(VEDevice* device, VEBufferAddress address);
VULKEASE_API VEBufferUsage veGetBufferUsage(VEDevice* device, VEBufferAddress address);

/**
 * Convenience functions for common buffer types
 */
VULKEASE_API VEBufferAddress veCreateVertexBuffer(VEDevice* device, const void* vertices,
                                                 size_t size, const char* debugName);
VULKEASE_API VEBufferAddress veCreateIndexBuffer(VEDevice* device, const void* indices,
                                                size_t size, const char* debugName);
VULKEASE_API VEBufferAddress veCreateUniformBuffer(VEDevice* device, size_t size,
                                                  bool persistentlyMapped, const char* debugName);
VULKEASE_API VEBufferAddress veCreateStorageBuffer(VEDevice* device, size_t size,
                                                  const char* debugName);
VULKEASE_API VEBufferAddress veCreateIndirectBuffer(VEDevice* device, size_t size,
                                                   const char* debugName);

// =============================================================================
// Texture Management (Bindless)
// =============================================================================

/**
 * Create texture and return bindless index
 */
VULKEASE_API VETextureIndex veCreateTexture(VEDevice* device, const VETextureDesc* desc);

/**
 * Destroy texture by index
 */
VULKEASE_API void veDestroyTexture(VEDevice* device, VETextureIndex index);

/**
 * Get texture properties
 */
VULKEASE_API VEResult veGetTextureSize(VEDevice* device, VETextureIndex index,
                                      uint32_t* width, uint32_t* height, uint32_t* depth);
VULKEASE_API VEFormat veGetTextureFormat(VEDevice* device, VETextureIndex index);

/**
 * Convenience functions for common texture types
 */
VULKEASE_API VETextureIndex veCreateTexture1D(VEDevice* device, uint32_t width, VEFormat format,
                                             VETextureUsage usage, const char* debugName);
VULKEASE_API VETextureIndex veCreateTexture2D(VEDevice* device, uint32_t width, uint32_t height,
                                             VEFormat format, VETextureUsage usage, const char* debugName);
VULKEASE_API VETextureIndex veCreateTexture3D(VEDevice* device, uint32_t width, uint32_t height, uint32_t depth,
                                             VEFormat format, VETextureUsage usage, const char* debugName);
VULKEASE_API VETextureIndex veCreateTexture2DArray(VEDevice* device, uint32_t width, uint32_t height,
                                                  uint32_t layers, VEFormat format, VETextureUsage usage,
                                                  const char* debugName);
VULKEASE_API VETextureIndex veCreateTextureCube(VEDevice* device, uint32_t size, VEFormat format,
                                               VETextureUsage usage, const char* debugName);
VULKEASE_API VETextureIndex veCreateTexture2DMultisample(VEDevice* device, uint32_t width, uint32_t height,
                                                        VEFormat format, VESampleCount sampleCount,
                                                        VETextureUsage usage, const char* debugName);

/**
 * Load texture from file using STB Image
 */
VULKEASE_API VETextureIndex veLoadTexture(VEDevice* device, const char* filename,
                                         VETextureUsage usage, bool generateMips);
VULKEASE_API VETextureIndex veLoadHDRTexture(VEDevice* device, const char* filename,
                                            VETextureUsage usage, bool generateMips);
VULKEASE_API VETextureIndex veLoadCubeTexture(VEDevice* device, const char* filenames[6],
                                             VETextureUsage usage, bool generateMips);

/**
 * Generate mipmaps for texture
 */
VULKEASE_API VEResult veGenerateMipmaps(VEDevice* device, VECommandBuffer* cmd, VETextureIndex texture);

/**
 * Generate mipmaps immediately (convenience function)
 * Creates a temporary command buffer, generates mipmaps, and submits immediately
 */
VULKEASE_API VEResult veGenerateMipmapsImmediate(VEDevice* device, VETextureIndex texture);

/**
 * Save texture to file
 */
VULKEASE_API VEResult veSaveTexture(VEDevice* device, VETextureIndex texture, const char* filename);

// =============================================================================
// Sampler Management (Bindless)
// =============================================================================

/**
 * Create sampler and return bindless index
 */
VULKEASE_API VESamplerIndex veCreateSampler(VEDevice* device, const VESamplerDesc* desc);

/**
 * Destroy sampler by index
 */
VULKEASE_API void veDestroySampler(VEDevice* device, VESamplerIndex index);

/**
 * Convenience functions for common samplers
 */
VULKEASE_API VESamplerIndex veCreateLinearSampler(VEDevice* device);
VULKEASE_API VESamplerIndex veCreateNearestSampler(VEDevice* device);
VULKEASE_API VESamplerIndex veCreateAnisotropicSampler(VEDevice* device, float maxAnisotropy);
VULKEASE_API VESamplerIndex veCreateShadowSampler(VEDevice* device);

// =============================================================================
// Shader Objects (VK_EXT_shader_object)
// =============================================================================

/**
 * Create shader object from SPIR-V code
 */
VULKEASE_API VEShader* veCreateShaderFromSPIRV(VEDevice* device, VEShaderStage stage,
                                              const uint32_t* code, size_t codeSize,
                                              const char* entryPoint, const char* debugName);

/**
 * Create shader object from GLSL source (compile to SPIR-V automatically)
 */
VULKEASE_API VEShader* veCreateShaderFromGLSL(VEDevice* device, VEShaderStage stage,
                                             const char* source, const char* entryPoint,
                                             const char* debugName);

/**
 * Load shader from file (.spv for SPIR-V, .glsl/.vert/.frag/.comp etc. for GLSL)
 */
VULKEASE_API VEShader* veLoadShader(VEDevice* device, const char* filename, VEShaderStage stage,
                                   const char* entryPoint, const char* debugName);

/**
 * Destroy shader object
 */
VULKEASE_API void veDestroyShader(VEShader* shader);

/**
 * Shader hot-reload support (for development)
 */
VULKEASE_API VEResult veEnableShaderHotReload(VEShader* shader, const char* sourceFile);
VULKEASE_API VEResult veReloadShader(VEShader* shader);

/**
 * Shader configuration management
 */
VULKEASE_API VEShaderConfig* veCreateShaderConfig(VEDevice* device, const VEShaderConfigDesc* desc);
VULKEASE_API void veDestroyShaderConfig(VEShaderConfig* config);

// =============================================================================
// Render Configuration Management
// =============================================================================

/**
 * Create render configuration - lightweight state template
 */
VULKEASE_API VERenderConfig* veCreateRenderConfig(VEDevice* device, const VERenderConfigDesc* desc);

/**
 * Destroy render configuration
 */
VULKEASE_API void veDestroyRenderConfig(VERenderConfig* config);

/**
 * Create vertex configuration for dynamic vertex input
 */
VULKEASE_API VEVertexConfig* veCreateVertexConfig(VEDevice* device, uint32_t bindingCount,
                                                 const VEVertexBinding* bindings,
                                                 uint32_t attributeCount,
                                                 const VEVertexAttribute* attributes);
VULKEASE_API void veDestroyVertexConfig(VEVertexConfig* config);

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
VULKEASE_API VERenderConfig* veCreateOpaqueRenderConfig(VEDevice* device, const char* debugName);
VULKEASE_API VERenderConfig* veCreateTransparentRenderConfig(VEDevice* device, const char* debugName);
VULKEASE_API VERenderConfig* veCreateWireframeRenderConfig(VEDevice* device, const char* debugName);
VULKEASE_API VERenderConfig* veCreateShadowRenderConfig(VEDevice* device, const char* debugName);
VULKEASE_API VERenderConfig* veCreateUIRenderConfig(VEDevice* device, const char* debugName);

/**
 * Configuration composition and variants
 */
VULKEASE_API VERenderConfig* veCreateConfigVariant(VERenderConfig* baseConfig,
                                                  const VERenderConfigDesc* overrides);
VULKEASE_API VERenderConfig* veMergeRenderConfigs(VEDevice* device, uint32_t configCount,
                                                 VERenderConfig* const* configs,
                                                 const char* debugName);
VULKEASE_API VERenderConfig* veCloneRenderConfig(VERenderConfig* config, const char* debugName);

// =============================================================================
// Command Buffer and Rendering
// =============================================================================

/**
 * Get command buffer for recording
 */
VULKEASE_API VECommandBuffer* veBeginCommandBuffer(VEDevice* device);

/**
 * Submit command buffer
 */
VULKEASE_API VEResult veSubmitCommandBuffer(VECommandBuffer* cmd, bool waitForCompletion);

/**
 * Begin/end dynamic rendering (replaces render passes)
 */
VULKEASE_API void veBeginRendering(VECommandBuffer* cmd, const VERenderingInfo* renderingInfo);
VULKEASE_API void veEndRendering(VECommandBuffer* cmd);

/**
 * Apply render configuration - sets all associated rendering state
 */
VULKEASE_API void veApplyRenderConfig(VECommandBuffer* cmd, VERenderConfig* config);

/**
 * Bind shaders (can mix and match any combination)
 */
VULKEASE_API void veBindShader(VECommandBuffer* cmd, VEShader* shader);
VULKEASE_API void veBindShaders(VECommandBuffer* cmd, uint32_t shaderCount, VEShader* const* shaders);
VULKEASE_API void veBindShaderConfig(VECommandBuffer* cmd, VEShaderConfig* config);
VULKEASE_API void veUnbindShaderStage(VECommandBuffer* cmd, VEShaderStage stage);

/**
 * Set dynamic viewport and scissor (always dynamic)
 */
VULKEASE_API void veSetViewport(VECommandBuffer* cmd, float x, float y, float width, float height,
                               float minDepth, float maxDepth);
VULKEASE_API void veSetScissor(VECommandBuffer* cmd, int32_t x, int32_t y, uint32_t width, uint32_t height);

/**
 * Runtime state overrides (maximum flexibility)
 */
VULKEASE_API void veOverrideRasterState(VECommandBuffer* cmd, const VERasterConfig* raster);
VULKEASE_API void veOverrideDepthState(VECommandBuffer* cmd, const VEDepthConfig* depth);
VULKEASE_API void veOverrideBlendState(VECommandBuffer* cmd, const VEBlendConfig* blend);

/**
 * Quick state toggles for common cases
 */
VULKEASE_API void veSetWireframe(VECommandBuffer* cmd, bool enabled);
VULKEASE_API void veSetAlphaBlending(VECommandBuffer* cmd, bool enabled);
VULKEASE_API void veSetDepthTesting(VECommandBuffer* cmd, bool testEnabled, bool writeEnabled);
VULKEASE_API void veSetCulling(VECommandBuffer* cmd, VECullMode cullMode);

/**
 * Push constants for bindless resource access
 * Contains buffer addresses and texture/sampler indices
 */
VULKEASE_API void vePushConstants(VECommandBuffer* cmd, const void* data, size_t size, size_t offset);

/**
 * Bind an index buffer fro indexed calls
 */
VULKEASE_API void veBindIndexBuffer(VECommandBuffer* cmd, VEBufferAddress indexBuffer, uint32_t offset, VEIndexFormat format);

/**
 * Direct drawing (buffer addresses in push constants - no binding operations!)
 */
VULKEASE_API void veDraw(VECommandBuffer* cmd, uint32_t vertexCount, uint32_t instanceCount,
                        uint32_t firstVertex, uint32_t firstInstance);
VULKEASE_API void veDrawIndexed(VECommandBuffer* cmd, uint32_t indexCount, uint32_t instanceCount,
                               uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

/**
 * Indirect drawing (GPU-driven)
 */
VULKEASE_API void veDrawIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                                size_t offset, uint32_t drawCount, uint32_t stride);
VULKEASE_API void veDrawIndexedIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                                       size_t offset, uint32_t drawCount, uint32_t stride);

/**
 * Multi-draw indirect with count buffer (GPU determines draw count)
 */
VULKEASE_API void veDrawIndirectCount(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                                     size_t indirectOffset, VEBufferAddress countBuffer,
                                     size_t countOffset, uint32_t maxDrawCount, uint32_t stride);
VULKEASE_API void veDrawIndexedIndirectCount(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                                            size_t indirectOffset, VEBufferAddress countBuffer,
                                            size_t countOffset, uint32_t maxDrawCount, uint32_t stride);

// =============================================================================
// Compute Shaders
// =============================================================================

/**
 * Dispatch compute shader
 */
VULKEASE_API void veDispatch(VECommandBuffer* cmd, uint32_t groupCountX, uint32_t groupCountY,
                            uint32_t groupCountZ);

/**
 * Indirect compute dispatch
 */
VULKEASE_API void veDispatchIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer, size_t offset);

// =============================================================================
// Synchronization and Memory Barriers
// =============================================================================

/**
 * Simple memory barriers for common cases
 */
VULKEASE_API void veBarrierVertexToFragment(VECommandBuffer* cmd);
VULKEASE_API void veBarrierComputeToVertex(VECommandBuffer* cmd);
VULKEASE_API void veBarrierComputeToCompute(VECommandBuffer* cmd);
VULKEASE_API void veBarrierGraphicsToPresent(VECommandBuffer* cmd);

/**
 * Texture layout transitions
 */
VULKEASE_API void veTransitionTexture(VECommandBuffer* cmd, VETextureIndex texture,
                                     uint32_t oldLayout, uint32_t newLayout);

/**
 * Common texture layout transition helpers
 */
VULKEASE_API void veTransitionTextureForShaderRead(VECommandBuffer* cmd, VETextureIndex texture);
VULKEASE_API void veTransitionTextureForColorAttachment(VECommandBuffer* cmd, VETextureIndex texture);
VULKEASE_API void veTransitionTextureForDepthAttachment(VECommandBuffer* cmd, VETextureIndex texture);
VULKEASE_API void veTransitionTextureForTransferSrc(VECommandBuffer* cmd, VETextureIndex texture);
VULKEASE_API void veTransitionTextureForTransferDst(VECommandBuffer* cmd, VETextureIndex texture);
VULKEASE_API void veTransitionTextureForPresent(VECommandBuffer* cmd, VETextureIndex texture);

/**
 * Smart layout transition - automatically detects current layout
 */
VULKEASE_API void veTransitionTextureToLayout(VECommandBuffer* cmd, VETextureIndex texture, uint32_t newLayout);

// =============================================================================
// Swapchain and Presentation
// =============================================================================

/**
 * Create swapchain for window
 */
VULKEASE_API VESwapchain* veCreateSwapchain(VEDevice* device, void* windowHandle,
                                           uint32_t width, uint32_t height, VEFormat forma, bool vsync);
VULKEASE_API void veDestroySwapchain(VESwapchain* swapchain);

/**
 * Acquire next swapchain image for rendering
 */
VULKEASE_API VETextureIndex veAcquireNextImage(VESwapchain* swapchain);

/**
 * Present rendered image to screen
 */
VULKEASE_API VEResult vePresentImage(VESwapchain* swapchain, VECommandBuffer* cmd);

/**
 * Handle window resize
 */
VULKEASE_API VEResult veResizeSwapchain(VESwapchain* swapchain, uint32_t width, uint32_t height);

/**
 * Get swapchain information
 */
VULKEASE_API VEResult veGetSwapchainSize(VESwapchain* swapchain, uint32_t* width, uint32_t* height);
VULKEASE_API VEFormat veGetSwapchainFormat(VESwapchain* swapchain);

// =============================================================================
// Debug and Profiling
// =============================================================================

/**
 * Debug labels for command buffers
 */
VULKEASE_API void veBeginDebugLabel(VECommandBuffer* cmd, const char* label, VEColor color);
VULKEASE_API void veEndDebugLabel(VECommandBuffer* cmd);
VULKEASE_API void veInsertDebugLabel(VECommandBuffer* cmd, const char* label, VEColor color);

/**
 * Set debug names for resources
 */
VULKEASE_API VEResult veSetBufferDebugName(VEDevice* device, VEBufferAddress address, const char* name);
VULKEASE_API VEResult veSetTextureDebugName(VEDevice* device, VETextureIndex texture, const char* name);
VULKEASE_API VEResult veSetSamplerDebugName(VEDevice* device, VESamplerIndex sampler, const char* name);

/**
 * Performance and memory statistics
 */
VULKEASE_API VEResult veGetPerformanceStats(VEDevice* device, VEPerformanceStats* stats);
VULKEASE_API VEResult veGetMemoryStats(VEDevice* device, VEMemoryStats* stats);
VULKEASE_API VEResult veGetRenderConfigStats(VEDevice* device, VERenderConfigStats* stats);

/**
 * Debug information
 */
VULKEASE_API void vePrintDebugInfo(VEDevice* device);
VULKEASE_API void vePrintRenderConfig(VERenderConfig* config);
VULKEASE_API VEResult veValidateRenderConfig(VERenderConfig* config);

// =============================================================================
// Utility Macros and Helper Functions
// =============================================================================

/**
 * Common push constants structures
 */

// Standard push constants structure for graphics shaders
typedef struct VEGraphicsPushConstants {
    VEBufferAddress vertexBuffer;          // 8 bytes - vertex data address
    VEBufferAddress indexBuffer;           // 8 bytes - index data address (optional)
    VEBufferAddress uniformBuffers[4];     // 32 bytes - up to 4 uniform buffer addresses
    VETextureIndex textures[8];            // 32 bytes - up to 8 bindless texture indices
    VESamplerIndex samplers[8];            // 32 bytes - up to 8 bindless sampler indices
    float objectScale;                     // 4 bytes - per-object scale
    uint32_t activeTextureCount;           // 4 bytes - number of active textures (0-8)
    uint32_t activeSamplerCount;           // 4 bytes - number of active samplers (0-8)
    uint32_t activeUniformCount;           // 4 bytes - number of active uniform buffers (0-4)
} VEGraphicsPushConstants;                 // Total: 128 bytes

// Standard push constants for compute shaders
typedef struct VEComputePushConstants {
    VEBufferAddress buffers[8];            // 64 bytes - up to 8 buffer addresses (input/output/params)
    VETextureIndex textures[8];            // 32 bytes - up to 8 texture indices
    VESamplerIndex samplers[4];            // 16 bytes - up to 4 sampler indices (usually fewer needed for compute)
    uint32_t elementCount;                 // 4 bytes - number of elements to process
    uint32_t activeBufferCount;            // 4 bytes - number of active buffers (0-8)
    uint32_t activeTextureCount;           // 4 bytes - number of active textures (0-8)
    uint32_t activeSamplerCount;           // 4 bytes - number of active samplers (0-4)
} VEComputePushConstants;                  // Total: 128 bytes

/**
 * Helper macros
 */

// Initialize push constant structures with safe defaults
#define VE_INIT_GRAPHICS_PUSH_CONSTANTS() \
    { .vertexBuffer = VE_INVALID_ADDRESS, .indexBuffer = VE_INVALID_ADDRESS, \
      .uniformBuffers = {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS}, \
      .textures = {VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX}, \
      .samplers = {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, \
                   VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX}, \
      .objectScale = 1.0f, .activeTextureCount = 0, .activeSamplerCount = 0, .activeUniformCount = 0 }

#define VE_INIT_COMPUTE_PUSH_CONSTANTS() \
    { .buffers = {VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, \
                  VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS, VE_INVALID_ADDRESS}, \
      .textures = {VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, \
                   VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX, VE_INVALID_TEXTURE_INDEX}, \
      .samplers = {VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX, VE_INVALID_SAMPLER_INDEX}, \
      .elementCount = 0, .activeBufferCount = 0, .activeTextureCount = 0, .activeSamplerCount = 0 }

// Create default descriptors with common settings
#define VE_DEFAULT_BUFFER_DESC() \
    { .size = 0, .usage = 0, .initialData = NULL, .initialDataSize = 0, .persistentlyMapped = false, .debugName = NULL }

#define VE_DEFAULT_TEXTURE_DESC() \
    { .width = 0, .height = 0, .depth = 1, .mipLevels = 1, .arrayLayers = 1, .format = VE_FORMAT_RGBA8_UNORM, \
      .usage = VE_TEXTURE_USAGE_SAMPLED, .sampleCount = VE_SAMPLE_COUNT_1, .initialData = NULL, .initialDataSize = 0, .debugName = NULL }

#define VE_DEFAULT_SAMPLER_DESC() \
    { .minFilter = VE_FILTER_LINEAR, .magFilter = VE_FILTER_LINEAR, .mipmapFilter = VE_FILTER_LINEAR, \
      .addressModeU = VE_ADDRESS_MODE_REPEAT, .addressModeV = VE_ADDRESS_MODE_REPEAT, .addressModeW = VE_ADDRESS_MODE_REPEAT, \
      .maxAnisotropy = 1.0f, .compareEnable = false, .compareOp = VE_COMPARE_OP_ALWAYS, .minLod = 0.0f, .maxLod = 1000.0f, .debugName = NULL }

// Color constants
#define VE_COLOR_BLACK   ((VEColor){0.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_WHITE   ((VEColor){1.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_RED     ((VEColor){1.0f, 0.0f, 0.0f, 1.0f})
#define VE_COLOR_GREEN   ((VEColor){0.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_BLUE    ((VEColor){0.0f, 0.0f, 1.0f, 1.0f})
#define VE_COLOR_YELLOW  ((VEColor){1.0f, 1.0f, 0.0f, 1.0f})
#define VE_COLOR_CYAN    ((VEColor){0.0f, 1.0f, 1.0f, 1.0f})
#define VE_COLOR_MAGENTA ((VEColor){1.0f, 0.0f, 1.0f, 1.0f})

#ifdef __cplusplus
}
#endif

#endif // VULKEASE2_H