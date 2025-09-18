/**
 * @file ve_render_config.c
 * @brief Render Configuration Implementation
 */

#include "ve_internal.h"

// =============================================================================
// Default Configuration Creators
// =============================================================================

VERasterConfig veDefaultRasterConfig(void) {
    VERasterConfig config = {0};
    config.cullMode = VE_CULL_MODE_BACK;
    config.frontFace = VE_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VE_POLYGON_MODE_FILL;
    config.lineWidth = 1.0f;
    config.depthBiasEnable = false;
    config.depthBiasConstantFactor = 0.0f;
    config.depthBiasClamp = 0.0f;
    config.depthBiasSlopeFactor = 0.0f;
    config.depthClampEnable = false;
    config.rasterizerDiscardEnable = false;
    return config;
}

VEDepthConfig veDefaultDepthConfig(void) {
    VEDepthConfig config = {0};
    config.depthTestEnable = true;
    config.depthWriteEnable = true;
    config.depthCompareOp = VE_COMPARE_OP_LESS;
    config.depthBoundsTestEnable = false;
    config.minDepthBounds = 0.0f;
    config.maxDepthBounds = 1.0f;
    config.stencilTestEnable = false;
    
    // Default stencil ops
    config.frontFailOp = VE_STENCIL_OP_KEEP;
    config.frontPassOp = VE_STENCIL_OP_KEEP;
    config.frontDepthFailOp = VE_STENCIL_OP_KEEP;
    config.frontCompareOp = VE_COMPARE_OP_ALWAYS;
    config.frontCompareMask = 0xFF;
    config.frontWriteMask = 0xFF;
    config.frontReference = 0;
    
    config.backFailOp = VE_STENCIL_OP_KEEP;
    config.backPassOp = VE_STENCIL_OP_KEEP;
    config.backDepthFailOp = VE_STENCIL_OP_KEEP;
    config.backCompareOp = VE_COMPARE_OP_ALWAYS;
    config.backCompareMask = 0xFF;
    config.backWriteMask = 0xFF;
    config.backReference = 0;
    
    return config;
}

VEBlendConfig veDefaultOpaqueBlendConfig(void) {
    VEBlendConfig config = {0};
    config.logicOpEnable = false;
    config.logicOp = VE_LOGIC_OP_COPY;
    config.attachmentCount = 1;
    
    config.attachments[0].blendEnable = false;
    config.attachments[0].srcColorBlendFactor = VE_BLEND_FACTOR_ONE;
    config.attachments[0].dstColorBlendFactor = VE_BLEND_FACTOR_ZERO;
    config.attachments[0].colorBlendOp = VE_BLEND_OP_ADD;
    config.attachments[0].srcAlphaBlendFactor = VE_BLEND_FACTOR_ONE;
    config.attachments[0].dstAlphaBlendFactor = VE_BLEND_FACTOR_ZERO;
    config.attachments[0].alphaBlendOp = VE_BLEND_OP_ADD;
    config.attachments[0].colorWriteMask = VE_COLOR_COMPONENT_ALL;
    
    config.blendConstants[0] = 0.0f;
    config.blendConstants[1] = 0.0f;
    config.blendConstants[2] = 0.0f;
    config.blendConstants[3] = 0.0f;
    
    return config;
}

VEBlendConfig veDefaultAlphaBlendConfig(void) {
    VEBlendConfig config = {0};
    config.logicOpEnable = false;
    config.logicOp = VE_LOGIC_OP_COPY;
    config.attachmentCount = 1;
    
    config.attachments[0].blendEnable = true;
    config.attachments[0].srcColorBlendFactor = VE_BLEND_FACTOR_SRC_ALPHA;
    config.attachments[0].dstColorBlendFactor = VE_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    config.attachments[0].colorBlendOp = VE_BLEND_OP_ADD;
    config.attachments[0].srcAlphaBlendFactor = VE_BLEND_FACTOR_ONE;
    config.attachments[0].dstAlphaBlendFactor = VE_BLEND_FACTOR_ZERO;
    config.attachments[0].alphaBlendOp = VE_BLEND_OP_ADD;
    config.attachments[0].colorWriteMask = VE_COLOR_COMPONENT_ALL;
    
    config.blendConstants[0] = 0.0f;
    config.blendConstants[1] = 0.0f;
    config.blendConstants[2] = 0.0f;
    config.blendConstants[3] = 0.0f;
    
    return config;
}

VEBlendConfig veDefaultAdditiveBlendConfig(void) {
    VEBlendConfig config = {0};
    config.logicOpEnable = false;
    config.logicOp = VE_LOGIC_OP_COPY;
    config.attachmentCount = 1;
    
    config.attachments[0].blendEnable = true;
    config.attachments[0].srcColorBlendFactor = VE_BLEND_FACTOR_SRC_ALPHA;
    config.attachments[0].dstColorBlendFactor = VE_BLEND_FACTOR_ONE;
    config.attachments[0].colorBlendOp = VE_BLEND_OP_ADD;
    config.attachments[0].srcAlphaBlendFactor = VE_BLEND_FACTOR_ZERO;
    config.attachments[0].dstAlphaBlendFactor = VE_BLEND_FACTOR_ONE;
    config.attachments[0].alphaBlendOp = VE_BLEND_OP_ADD;
    config.attachments[0].colorWriteMask = VE_COLOR_COMPONENT_ALL;
    
    config.blendConstants[0] = 0.0f;
    config.blendConstants[1] = 0.0f;
    config.blendConstants[2] = 0.0f;
    config.blendConstants[3] = 0.0f;
    
    return config;
}

VEMultisampleConfig veDefaultMultisampleConfig(void) {
    VEMultisampleConfig config = {0};
    config.rasterizationSamples = VE_SAMPLE_COUNT_1;
    config.sampleShadingEnable = false;
    config.minSampleShading = 1.0f;
    config.alphaToCoverageEnable = false;
    config.alphaToOneEnable = false;
    return config;
}

VEVertexInputConfig veDefaultVertexInputConfig(void) {
    VEVertexInputConfig config = {0};
    config.bindingCount = 0;
    config.bindings = NULL;
    config.attributeCount = 0;
    config.attributes = NULL;
    config.topology = VE_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.primitiveRestartEnable = false;
    return config;
}

// =============================================================================
// Render Configuration Management
// =============================================================================

VERenderConfig* veCreateRenderConfig(VEDevice* device, const VERenderConfigDesc* desc) {
    if (!device || !desc) {
        veSetError("Invalid parameters for render config creation");
        return NULL;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    // Find free slot in render configs array
    uint32_t index = UINT32_MAX;
    for (uint32_t i = 0; i < deviceInternal->maxRenderConfigs; i++) {
        if (!deviceInternal->renderConfigs[i].isValid) {
            index = i;
            break;
        }
    }
    
    if (index == UINT32_MAX) {
        veSetError("No free render config slots available");
        return NULL;
    }
    
    VERenderConfigInternal* config = &deviceInternal->renderConfigs[index];
    memset(config, 0, sizeof(VERenderConfigInternal));
    
    config->configTypes = desc->configTypes;
    
    // Copy configurations or use defaults
    config->rasterConfig = desc->rasterConfig ? *desc->rasterConfig : veDefaultRasterConfig();
    config->depthConfig = desc->depthConfig ? *desc->depthConfig : veDefaultDepthConfig();
    config->blendConfig = desc->blendConfig ? *desc->blendConfig : veDefaultOpaqueBlendConfig();
    config->multisampleConfig = desc->multisampleConfig ? *desc->multisampleConfig : veDefaultMultisampleConfig();
    config->vertexInputConfig = desc->vertexInputConfig ? *desc->vertexInputConfig : veDefaultVertexInputConfig();
    
    // Copy shaders
    config->shaderCount = desc->shaderCount < 6 ? desc->shaderCount : 6;
    for (uint32_t i = 0; i < config->shaderCount; i++) {
        config->shaders[i] = desc->shaders[i];
    }
    
    if (desc->debugName) {
        strncpy(config->debugName, desc->debugName, sizeof(config->debugName) - 1);
    } else {
        snprintf(config->debugName, sizeof(config->debugName), "RenderConfig_%u", index);
    }
    
    config->isValid = true;
    deviceInternal->renderConfigCount++;
    
    return (VERenderConfig*)config;
}

void veDestroyRenderConfig(VERenderConfig* config) {
    if (!config) return;
    
    VERenderConfigInternal* internal = (VERenderConfigInternal*)config;
    memset(internal, 0, sizeof(VERenderConfigInternal));
}

// =============================================================================
// Vertex Configuration Management
// =============================================================================

VEVertexConfig* veCreateVertexConfig(VEDevice* device, uint32_t bindingCount,
                                   const VEVertexBinding* bindings,
                                   uint32_t attributeCount,
                                   const VEVertexAttribute* attributes) {
    if (!device) {
        veSetError("Device cannot be NULL");
        return NULL;
    }
    
    if (bindingCount > 16 || attributeCount > 32) {
        veSetError("Too many vertex bindings or attributes");
        return NULL;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    // Find free slot in vertex configs array
    uint32_t index = UINT32_MAX;
    for (uint32_t i = 0; i < deviceInternal->maxVertexConfigs; i++) {
        if (!deviceInternal->vertexConfigs[i].isValid) {
            index = i;
            break;
        }
    }
    
    if (index == UINT32_MAX) {
        veSetError("No free vertex config slots available");
        return NULL;
    }
    
    VEVertexConfigInternal* config = &deviceInternal->vertexConfigs[index];
    memset(config, 0, sizeof(VEVertexConfigInternal));
    
    config->bindingCount = bindingCount;
    config->attributeCount = attributeCount;
    config->topology = VE_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config->primitiveRestartEnable = false;
    
    // Copy bindings
    for (uint32_t i = 0; i < bindingCount; i++) {
        config->bindings[i] = bindings[i];
    }
    
    // Copy attributes
    for (uint32_t i = 0; i < attributeCount; i++) {
        config->attributes[i] = attributes[i];
    }
    
    snprintf(config->debugName, sizeof(config->debugName), "VertexConfig_%u", index);
    
    config->isValid = true;
    deviceInternal->vertexConfigCount++;
    
    return (VEVertexConfig*)config;
}

void veDestroyVertexConfig(VEVertexConfig* config) {
    if (!config) return;
    
    VEVertexConfigInternal* internal = (VEVertexConfigInternal*)config;
    memset(internal, 0, sizeof(VEVertexConfigInternal));
}

// =============================================================================
// Common Render Configuration Creators
// =============================================================================

VERenderConfig* veCreateOpaqueRenderConfig(VEDevice* device, const char* debugName) {
    VERasterConfig raster = veDefaultRasterConfig();
    VEDepthConfig depth = veDefaultDepthConfig();
    VEBlendConfig blend = veDefaultOpaqueBlendConfig();
    VEMultisampleConfig multisample = veDefaultMultisampleConfig();
    
    VERenderConfigDesc desc = {
        .configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | 
                      VE_CONFIG_TYPE_COLOR_BLEND | VE_CONFIG_TYPE_MULTISAMPLE,
        .rasterConfig = &raster,
        .depthConfig = &depth,
        .blendConfig = &blend,
        .multisampleConfig = &multisample,
        .vertexInputConfig = NULL,
        .shaderCount = 0,
        .shaders = NULL,
        .debugName = debugName ? debugName : "OpaqueRenderConfig"
    };
    
    return veCreateRenderConfig(device, &desc);
}

VERenderConfig* veCreateTransparentRenderConfig(VEDevice* device, const char* debugName) {
    VERasterConfig raster = veDefaultRasterConfig();
    VEDepthConfig depth = veDefaultDepthConfig();
    depth.depthWriteEnable = false; // Don't write depth for transparent objects
    VEBlendConfig blend = veDefaultAlphaBlendConfig();
    VEMultisampleConfig multisample = veDefaultMultisampleConfig();
    
    VERenderConfigDesc desc = {
        .configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | 
                      VE_CONFIG_TYPE_COLOR_BLEND | VE_CONFIG_TYPE_MULTISAMPLE,
        .rasterConfig = &raster,
        .depthConfig = &depth,
        .blendConfig = &blend,
        .multisampleConfig = &multisample,
        .vertexInputConfig = NULL,
        .shaderCount = 0,
        .shaders = NULL,
        .debugName = debugName ? debugName : "TransparentRenderConfig"
    };
    
    return veCreateRenderConfig(device, &desc);
}

VERenderConfig* veCreateWireframeRenderConfig(VEDevice* device, const char* debugName) {
    VERasterConfig raster = veDefaultRasterConfig();
    raster.polygonMode = VE_POLYGON_MODE_LINE;
    raster.lineWidth = 1.0f;
    VEDepthConfig depth = veDefaultDepthConfig();
    VEBlendConfig blend = veDefaultOpaqueBlendConfig();
    VEMultisampleConfig multisample = veDefaultMultisampleConfig();
    
    VERenderConfigDesc desc = {
        .configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | 
                      VE_CONFIG_TYPE_COLOR_BLEND | VE_CONFIG_TYPE_MULTISAMPLE,
        .rasterConfig = &raster,
        .depthConfig = &depth,
        .blendConfig = &blend,
        .multisampleConfig = &multisample,
        .vertexInputConfig = NULL,
        .shaderCount = 0,
        .shaders = NULL,
        .debugName = debugName ? debugName : "WireframeRenderConfig"
    };
    
    return veCreateRenderConfig(device, &desc);
}

VERenderConfig* veCreateShadowRenderConfig(VEDevice* device, const char* debugName) {
    VERasterConfig raster = veDefaultRasterConfig();
    raster.cullMode = VE_CULL_MODE_FRONT; // Reduce peter-panning
    raster.depthBiasEnable = true;
    raster.depthBiasConstantFactor = 2.0f;
    raster.depthBiasSlopeFactor = 1.5f;
    
    VEDepthConfig depth = veDefaultDepthConfig();
    
    VEBlendConfig blend = veDefaultOpaqueBlendConfig();
    // Shadow maps typically don't need color output
    blend.attachments[0].colorWriteMask = 0;
    
    VEMultisampleConfig multisample = veDefaultMultisampleConfig();
    
    VERenderConfigDesc desc = {
        .configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | 
                      VE_CONFIG_TYPE_COLOR_BLEND | VE_CONFIG_TYPE_MULTISAMPLE,
        .rasterConfig = &raster,
        .depthConfig = &depth,
        .blendConfig = &blend,
        .multisampleConfig = &multisample,
        .vertexInputConfig = NULL,
        .shaderCount = 0,
        .shaders = NULL,
        .debugName = debugName ? debugName : "ShadowRenderConfig"
    };
    
    return veCreateRenderConfig(device, &desc);
}

VERenderConfig* veCreateUIRenderConfig(VEDevice* device, const char* debugName) {
    VERasterConfig raster = veDefaultRasterConfig();
    raster.cullMode = VE_CULL_MODE_NONE; // UI elements might be double-sided
    
    VEDepthConfig depth = veDefaultDepthConfig();
    depth.depthTestEnable = false; // UI always on top
    depth.depthWriteEnable = false;
    
    VEBlendConfig blend = veDefaultAlphaBlendConfig();
    VEMultisampleConfig multisample = veDefaultMultisampleConfig();
    
    VERenderConfigDesc desc = {
        .configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | 
                      VE_CONFIG_TYPE_COLOR_BLEND | VE_CONFIG_TYPE_MULTISAMPLE,
        .rasterConfig = &raster,
        .depthConfig = &depth,
        .blendConfig = &blend,
        .multisampleConfig = &multisample,
        .vertexInputConfig = NULL,
        .shaderCount = 0,
        .shaders = NULL,
        .debugName = debugName ? debugName : "UIRenderConfig"
    };
    
    return veCreateRenderConfig(device, &desc);
}

// =============================================================================
// Configuration Composition
// =============================================================================

VERenderConfig* veCreateConfigVariant(VERenderConfig* baseConfig, 
                                     const VERenderConfigDesc* overrides) {
    if (!baseConfig || !overrides) {
        veSetError("Invalid parameters for config variant creation");
        return NULL;
    }
    
    VERenderConfigInternal* base = (VERenderConfigInternal*)baseConfig;
    
    // Create a new config descriptor based on the base config
    VERenderConfigDesc desc = {0};
    desc.configTypes = overrides->configTypes != 0 ? overrides->configTypes : base->configTypes;
    desc.rasterConfig = overrides->rasterConfig ? overrides->rasterConfig : &base->rasterConfig;
    desc.depthConfig = overrides->depthConfig ? overrides->depthConfig : &base->depthConfig;
    desc.blendConfig = overrides->blendConfig ? overrides->blendConfig : &base->blendConfig;
    desc.multisampleConfig = overrides->multisampleConfig ? overrides->multisampleConfig : &base->multisampleConfig;
    desc.vertexInputConfig = overrides->vertexInputConfig ? overrides->vertexInputConfig : &base->vertexInputConfig;
    desc.shaderCount = overrides->shaderCount > 0 ? overrides->shaderCount : base->shaderCount;
    desc.shaders = overrides->shaders ? overrides->shaders : base->shaders;
    desc.debugName = overrides->debugName ? overrides->debugName : "ConfigVariant";
    
    // Find the device from the base config (this is a limitation of the current design)
    // In a real implementation, we'd need a better way to get the device
    veSetError("Config variant creation not fully implemented - need device reference");
    return NULL;
}

VERenderConfig* veMergeRenderConfigs(VEDevice* device, uint32_t configCount,
                                   VERenderConfig* const* configs,
                                   const char* debugName) {
    if (!device || configCount == 0 || !configs) {
        veSetError("Invalid parameters for config merging");
        return NULL;
    }
    
    // TODO: Implement config merging
    veSetError("Config merging not implemented");
    return NULL;
}

VERenderConfig* veCloneRenderConfig(VERenderConfig* config, const char* debugName) {
    if (!config) {
        veSetError("Config cannot be NULL");
        return NULL;
    }
    
    // TODO: Implement config cloning
    veSetError("Config cloning not implemented");
    return NULL;
}