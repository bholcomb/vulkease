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


VkIndexType veIndexFormatToVk(VEIndexFormat format)
{
    switch(format)
    {
        case VE_INDEX_UINT16: return VK_INDEX_TYPE_UINT16;
        case VE_INDEX_UINT32: return VK_INDEX_TYPE_UINT32;
    }

    return VK_INDEX_TYPE_NONE_KHR;
}