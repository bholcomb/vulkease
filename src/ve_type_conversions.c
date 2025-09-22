/**
 * VulkEase to Vulkan Type Conversion Functions
 * Converts VulkEase enums to their corresponding Vulkan types
 */

#include "ve_internal.h"

// =============================================================================
// Render State Conversions
// =============================================================================

VkCullModeFlags veConvertCullMode(VECullMode cullMode) {
    switch (cullMode) {
        case VE_CULL_MODE_NONE:              return VK_CULL_MODE_NONE;
        case VE_CULL_MODE_FRONT:             return VK_CULL_MODE_FRONT_BIT;
        case VE_CULL_MODE_BACK:              return VK_CULL_MODE_BACK_BIT;
        case VE_CULL_MODE_FRONT_AND_BACK:    return VK_CULL_MODE_FRONT_AND_BACK;
        default:                             return VK_CULL_MODE_BACK_BIT;
    }
}

VkFrontFace veConvertFrontFace(VEFrontFace frontFace) {
    switch (frontFace) {
        case VE_FRONT_FACE_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case VE_FRONT_FACE_CLOCKWISE:         return VK_FRONT_FACE_CLOCKWISE;
        default:                              return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

VkPolygonMode veConvertPolygonMode(VEPolygonMode polygonMode) {
    switch (polygonMode) {
        case VE_POLYGON_MODE_FILL:   return VK_POLYGON_MODE_FILL;
        case VE_POLYGON_MODE_LINE:   return VK_POLYGON_MODE_LINE;
        case VE_POLYGON_MODE_POINT:  return VK_POLYGON_MODE_POINT;
        default:                     return VK_POLYGON_MODE_FILL;
    }
}

// =============================================================================
// Depth/Stencil Conversions
// =============================================================================

VkCompareOp veConvertCompareOp(VECompareOp compareOp) {
    switch (compareOp) {
        case VE_COMPARE_OP_NEVER:            return VK_COMPARE_OP_NEVER;
        case VE_COMPARE_OP_LESS:             return VK_COMPARE_OP_LESS;
        case VE_COMPARE_OP_EQUAL:            return VK_COMPARE_OP_EQUAL;
        case VE_COMPARE_OP_LESS_OR_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case VE_COMPARE_OP_GREATER:          return VK_COMPARE_OP_GREATER;
        case VE_COMPARE_OP_NOT_EQUAL:        return VK_COMPARE_OP_NOT_EQUAL;
        case VE_COMPARE_OP_GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case VE_COMPARE_OP_ALWAYS:           return VK_COMPARE_OP_ALWAYS;
        default:                             return VK_COMPARE_OP_LESS;
    }
}

VkStencilOp veConvertStencilOp(VEStencilOp stencilOp) {
    switch (stencilOp) {
        case VE_STENCIL_OP_KEEP:                return VK_STENCIL_OP_KEEP;
        case VE_STENCIL_OP_ZERO:                return VK_STENCIL_OP_ZERO;
        case VE_STENCIL_OP_REPLACE:             return VK_STENCIL_OP_REPLACE;
        case VE_STENCIL_OP_INCREMENT_AND_CLAMP: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case VE_STENCIL_OP_DECREMENT_AND_CLAMP: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case VE_STENCIL_OP_INVERT:              return VK_STENCIL_OP_INVERT;
        case VE_STENCIL_OP_INCREMENT_AND_WRAP:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case VE_STENCIL_OP_DECREMENT_AND_WRAP:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:                                return VK_STENCIL_OP_KEEP;
    }
}

// =============================================================================
// Color Blending Conversions
// =============================================================================

VkBlendFactor veConvertBlendFactor(VEBlendFactor blendFactor) {
    switch (blendFactor) {
        case VE_BLEND_FACTOR_ZERO:                      return VK_BLEND_FACTOR_ZERO;
        case VE_BLEND_FACTOR_ONE:                       return VK_BLEND_FACTOR_ONE;
        case VE_BLEND_FACTOR_SRC_COLOR:                 return VK_BLEND_FACTOR_SRC_COLOR;
        case VE_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:       return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case VE_BLEND_FACTOR_DST_COLOR:                 return VK_BLEND_FACTOR_DST_COLOR;
        case VE_BLEND_FACTOR_ONE_MINUS_DST_COLOR:       return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case VE_BLEND_FACTOR_SRC_ALPHA:                 return VK_BLEND_FACTOR_SRC_ALPHA;
        case VE_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:       return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case VE_BLEND_FACTOR_DST_ALPHA:                 return VK_BLEND_FACTOR_DST_ALPHA;
        case VE_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:       return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case VE_BLEND_FACTOR_CONSTANT_COLOR:            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case VE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case VE_BLEND_FACTOR_CONSTANT_ALPHA:            return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case VE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        default:                                        return VK_BLEND_FACTOR_ONE;
    }
}

VkBlendOp veConvertBlendOp(VEBlendOp blendOp) {
    switch (blendOp) {
        case VE_BLEND_OP_ADD:              return VK_BLEND_OP_ADD;
        case VE_BLEND_OP_SUBTRACT:         return VK_BLEND_OP_SUBTRACT;
        case VE_BLEND_OP_REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case VE_BLEND_OP_MIN:              return VK_BLEND_OP_MIN;
        case VE_BLEND_OP_MAX:              return VK_BLEND_OP_MAX;
        default:                           return VK_BLEND_OP_ADD;
    }
}

VkLogicOp veConvertLogicOp(VELogicOp logicOp) {
    switch (logicOp) {
        case VE_LOGIC_OP_CLEAR:         return VK_LOGIC_OP_CLEAR;
        case VE_LOGIC_OP_AND:           return VK_LOGIC_OP_AND;
        case VE_LOGIC_OP_AND_REVERSE:   return VK_LOGIC_OP_AND_REVERSE;
        case VE_LOGIC_OP_COPY:          return VK_LOGIC_OP_COPY;
        case VE_LOGIC_OP_AND_INVERTED:  return VK_LOGIC_OP_AND_INVERTED;
        case VE_LOGIC_OP_NO_OP:         return VK_LOGIC_OP_NO_OP;
        case VE_LOGIC_OP_XOR:           return VK_LOGIC_OP_XOR;
        case VE_LOGIC_OP_OR:            return VK_LOGIC_OP_OR;
        case VE_LOGIC_OP_NOR:           return VK_LOGIC_OP_NOR;
        case VE_LOGIC_OP_EQUIVALENT:    return VK_LOGIC_OP_EQUIVALENT;
        case VE_LOGIC_OP_INVERT:        return VK_LOGIC_OP_INVERT;
        case VE_LOGIC_OP_OR_REVERSE:    return VK_LOGIC_OP_OR_REVERSE;
        case VE_LOGIC_OP_COPY_INVERTED: return VK_LOGIC_OP_COPY_INVERTED;
        case VE_LOGIC_OP_OR_INVERTED:   return VK_LOGIC_OP_OR_INVERTED;
        case VE_LOGIC_OP_NAND:          return VK_LOGIC_OP_NAND;
        case VE_LOGIC_OP_SET:           return VK_LOGIC_OP_SET;
        default:                        return VK_LOGIC_OP_COPY;
    }
}

VkColorComponentFlags veConvertColorComponentFlags(VEColorComponentFlags flags) {
    VkColorComponentFlags vkFlags = 0;
    
    if (flags & VE_COLOR_COMPONENT_R_BIT) vkFlags |= VK_COLOR_COMPONENT_R_BIT;
    if (flags & VE_COLOR_COMPONENT_G_BIT) vkFlags |= VK_COLOR_COMPONENT_G_BIT;
    if (flags & VE_COLOR_COMPONENT_B_BIT) vkFlags |= VK_COLOR_COMPONENT_B_BIT;
    if (flags & VE_COLOR_COMPONENT_A_BIT) vkFlags |= VK_COLOR_COMPONENT_A_BIT;
    
    return vkFlags;
}

// =============================================================================
// Vertex Input Conversions
// =============================================================================

VkVertexInputRate veConvertVertexInputRate(VEVertexInputRate inputRate) {
    switch (inputRate) {
        case VE_VERTEX_INPUT_RATE_VERTEX:   return VK_VERTEX_INPUT_RATE_VERTEX;
        case VE_VERTEX_INPUT_RATE_INSTANCE: return VK_VERTEX_INPUT_RATE_INSTANCE;
        default:                            return VK_VERTEX_INPUT_RATE_VERTEX;
    }
}

VkPrimitiveTopology veConvertPrimitiveTopology(VEPrimitiveTopology topology) {
    switch (topology) {
        case VE_PRIMITIVE_TOPOLOGY_POINT_LIST:                    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case VE_PRIMITIVE_TOPOLOGY_LINE_LIST:                     return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case VE_PRIMITIVE_TOPOLOGY_LINE_STRIP:                    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case VE_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:                 return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case VE_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case VE_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:                  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case VE_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        case VE_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
        case VE_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        case VE_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
        case VE_PRIMITIVE_TOPOLOGY_PATCH_LIST:                    return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        default:                                                  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

// =============================================================================
// Format Conversion (partial - add more as needed)
// =============================================================================

VkFormat veConvertFormat(VEFormat format) {
    switch (format) {
        // 8-bit formats
        case VE_FORMAT_R8_UNORM:        return VK_FORMAT_R8_UNORM;
        case VE_FORMAT_RG8_UNORM:       return VK_FORMAT_R8G8_UNORM;
        case VE_FORMAT_RGBA8_UNORM:     return VK_FORMAT_R8G8B8A8_UNORM;
        case VE_FORMAT_RGBA8_SRGB:      return VK_FORMAT_R8G8B8A8_SRGB;
        case VE_FORMAT_BGRA8_UNORM:     return VK_FORMAT_B8G8R8A8_UNORM;
        case VE_FORMAT_BGRA8_SRGB:      return VK_FORMAT_B8G8R8A8_SRGB;
        
        // 16-bit formats
        case VE_FORMAT_R16_UNORM:       return VK_FORMAT_R16_UNORM;
        case VE_FORMAT_R16_SFLOAT:      return VK_FORMAT_R16_SFLOAT;
        case VE_FORMAT_RG16_UNORM:      return VK_FORMAT_R16G16_UNORM;
        case VE_FORMAT_RG16_SFLOAT:     return VK_FORMAT_R16G16_SFLOAT;
        case VE_FORMAT_RGBA16_UNORM:    return VK_FORMAT_R16G16B16A16_UNORM;
        case VE_FORMAT_RGBA16_SFLOAT:   return VK_FORMAT_R16G16B16A16_SFLOAT;
        
        // 32-bit formats
        case VE_FORMAT_R32_UINT:        return VK_FORMAT_R32_UINT;
        case VE_FORMAT_R32_SINT:        return VK_FORMAT_R32_SINT;
        case VE_FORMAT_R32_SFLOAT:      return VK_FORMAT_R32_SFLOAT;
        case VE_FORMAT_RG32_UINT:       return VK_FORMAT_R32G32_UINT;
        case VE_FORMAT_RG32_SINT:       return VK_FORMAT_R32G32_SINT;
        case VE_FORMAT_RG32_SFLOAT:     return VK_FORMAT_R32G32_SFLOAT;
        case VE_FORMAT_RGB32_UINT:      return VK_FORMAT_R32G32B32_UINT;
        case VE_FORMAT_RGB32_SINT:      return VK_FORMAT_R32G32B32_SINT;
        case VE_FORMAT_RGB32_SFLOAT:    return VK_FORMAT_R32G32B32_SFLOAT;
        case VE_FORMAT_RGBA32_UINT:     return VK_FORMAT_R32G32B32A32_UINT;
        case VE_FORMAT_RGBA32_SINT:     return VK_FORMAT_R32G32B32A32_SINT;
        case VE_FORMAT_RGBA32_SFLOAT:   return VK_FORMAT_R32G32B32A32_SFLOAT;
        
        // Depth/stencil formats
        case VE_FORMAT_D16_UNORM:           return VK_FORMAT_D16_UNORM;
        case VE_FORMAT_X8_D24_UNORM_PACK32: return VK_FORMAT_X8_D24_UNORM_PACK32;
        case VE_FORMAT_D32_SFLOAT:          return VK_FORMAT_D32_SFLOAT;
        case VE_FORMAT_S8_UINT:             return VK_FORMAT_S8_UINT;
        case VE_FORMAT_D16_UNORM_S8_UINT:   return VK_FORMAT_D16_UNORM_S8_UINT;
        case VE_FORMAT_D24_UNORM_S8_UINT:   return VK_FORMAT_D24_UNORM_S8_UINT;
        case VE_FORMAT_D32_SFLOAT_S8_UINT:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
        
        // Compressed formats
        case VE_FORMAT_BC1_RGB_UNORM:   return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case VE_FORMAT_BC1_RGB_SRGB:    return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case VE_FORMAT_BC1_RGBA_UNORM:  return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case VE_FORMAT_BC1_RGBA_SRGB:   return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case VE_FORMAT_BC2_UNORM:       return VK_FORMAT_BC2_UNORM_BLOCK;
        case VE_FORMAT_BC2_SRGB:        return VK_FORMAT_BC2_SRGB_BLOCK;
        case VE_FORMAT_BC3_UNORM:       return VK_FORMAT_BC3_UNORM_BLOCK;
        case VE_FORMAT_BC3_SRGB:        return VK_FORMAT_BC3_SRGB_BLOCK;
        case VE_FORMAT_BC4_UNORM:       return VK_FORMAT_BC4_UNORM_BLOCK;
        case VE_FORMAT_BC4_SNORM:       return VK_FORMAT_BC4_SNORM_BLOCK;
        case VE_FORMAT_BC5_UNORM:       return VK_FORMAT_BC5_UNORM_BLOCK;
        case VE_FORMAT_BC5_SNORM:       return VK_FORMAT_BC5_SNORM_BLOCK;
        case VE_FORMAT_BC6H_UFLOAT:     return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case VE_FORMAT_BC6H_SFLOAT:     return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case VE_FORMAT_BC7_UNORM:       return VK_FORMAT_BC7_UNORM_BLOCK;
        case VE_FORMAT_BC7_SRGB:        return VK_FORMAT_BC7_SRGB_BLOCK;
        
        default:                        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

// =============================================================================
// Sample Count Conversion
// =============================================================================

VkSampleCountFlagBits veConvertSampleCount(VESampleCount sampleCount) {
    switch (sampleCount) {
        case VE_SAMPLE_COUNT_1:  return VK_SAMPLE_COUNT_1_BIT;
        case VE_SAMPLE_COUNT_2:  return VK_SAMPLE_COUNT_2_BIT;
        case VE_SAMPLE_COUNT_4:  return VK_SAMPLE_COUNT_4_BIT;
        case VE_SAMPLE_COUNT_8:  return VK_SAMPLE_COUNT_8_BIT;
        case VE_SAMPLE_COUNT_16: return VK_SAMPLE_COUNT_16_BIT;
        case VE_SAMPLE_COUNT_32: return VK_SAMPLE_COUNT_32_BIT;
        case VE_SAMPLE_COUNT_64: return VK_SAMPLE_COUNT_64_BIT;
        default:                 return VK_SAMPLE_COUNT_1_BIT;
    }
}

// =============================================================================
// Additional Utility Functions
// =============================================================================

// Convert VulkEase buffer usage to Vulkan buffer usage flags
VkBufferUsageFlags veConvertBufferUsage(VEBufferUsage usage) {
    VkBufferUsageFlags vkUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT; // Always add this
    
    if (usage & VE_BUFFER_USAGE_VERTEX)     vkUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_INDEX)      vkUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_UNIFORM)    vkUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_STORAGE)    vkUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_INDIRECT)   vkUsage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_TRANSFER_SRC) vkUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage & VE_BUFFER_USAGE_TRANSFER_DST) vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    return vkUsage;
}

// Convert VulkEase texture usage to Vulkan image usage flags
VkImageUsageFlags veConvertTextureUsage(VETextureUsage usage) {
    VkImageUsageFlags vkUsage = 0;
    
    if (usage & VE_TEXTURE_USAGE_SAMPLED)                  vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage & VE_TEXTURE_USAGE_STORAGE)                  vkUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (usage & VE_TEXTURE_USAGE_COLOR_ATTACHMENT)         vkUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage & VE_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT) vkUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage & VE_TEXTURE_USAGE_TRANSFER_SRC)             vkUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage & VE_TEXTURE_USAGE_TRANSFER_DST)             vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage & VE_TEXTURE_USAGE_INPUT_ATTACHMENT)         vkUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    
    return vkUsage;
}