/**
 * @file ve_render_config.c
 * @brief Render Configuration Implementation
 */

#include "ve_internal.h"

VkColorComponentFlagBits VK_COLOR_COMPONENT_ALL = 
    VK_COLOR_COMPONENT_R_BIT |
    VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT |
    VK_COLOR_COMPONENT_A_BIT;

// =============================================================================
// Default Configuration Creators
// =============================================================================

VERasterConfig veDefaultRasterConfig(void) {
    VERasterConfig config = {0};
    config.cullMode = VK_CULL_MODE_BACK_BIT;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
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
    config.depthCompareOp = VK_COMPARE_OP_LESS;
    config.depthBoundsTestEnable = false;
    config.minDepthBounds = 0.0f;
    config.maxDepthBounds = 1.0f;
    config.stencilTestEnable = false;
    
    // Default stencil ops
    config.frontFailOp = VK_STENCIL_OP_KEEP;
    config.frontPassOp = VK_STENCIL_OP_KEEP;
    config.frontDepthFailOp = VK_STENCIL_OP_KEEP;
    config.frontCompareOp = VK_COMPARE_OP_ALWAYS;
    config.frontCompareMask = 0xFF;
    config.frontWriteMask = 0xFF;
    config.frontReference = 0;
    
    config.backFailOp = VK_STENCIL_OP_KEEP;
    config.backPassOp = VK_STENCIL_OP_KEEP;
    config.backDepthFailOp = VK_STENCIL_OP_KEEP;
    config.backCompareOp = VK_COMPARE_OP_ALWAYS;
    config.backCompareMask = 0xFF;
    config.backWriteMask = 0xFF;
    config.backReference = 0;
    
    return config;
}

VEBlendConfig veDefaultOpaqueBlendConfig(void) {
    VEBlendConfig config = {0};
    config.logicOpEnable = false;
    config.logicOp = VK_LOGIC_OP_COPY;
    config.attachmentCount = 1;
    
    config.attachments[0].blendEnable = false;
    config.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    config.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    config.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    config.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    config.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    config.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    config.attachments[0].colorWriteMask = VK_COLOR_COMPONENT_ALL;
    
    config.blendConstants[0] = 0.0f;
    config.blendConstants[1] = 0.0f;
    config.blendConstants[2] = 0.0f;
    config.blendConstants[3] = 0.0f;
    
    return config;
}

VEBlendConfig veDefaultAlphaBlendConfig(void) {
    VEBlendConfig config = {0};
    config.logicOpEnable = false;
    config.logicOp = VK_LOGIC_OP_COPY;
    config.attachmentCount = 1;
    
    config.attachments[0].blendEnable = true;
    config.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    config.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    config.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    config.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    config.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    config.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    config.attachments[0].colorWriteMask = VK_COLOR_COMPONENT_ALL;
    
    config.blendConstants[0] = 0.0f;
    config.blendConstants[1] = 0.0f;
    config.blendConstants[2] = 0.0f;
    config.blendConstants[3] = 0.0f;
    
    return config;
}

VEBlendConfig veDefaultAdditiveBlendConfig(void) {
    VEBlendConfig config = {0};
    config.logicOpEnable = false;
    config.logicOp = VK_LOGIC_OP_COPY;
    config.attachmentCount = 1;
    
    config.attachments[0].blendEnable = true;
    config.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    config.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    config.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    config.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    config.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    config.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    config.attachments[0].colorWriteMask = VK_COLOR_COMPONENT_ALL;
    
    config.blendConstants[0] = 0.0f;
    config.blendConstants[1] = 0.0f;
    config.blendConstants[2] = 0.0f;
    config.blendConstants[3] = 0.0f;
    
    return config;
}

VEMultisampleConfig veDefaultMultisampleConfig(void) {
    VEMultisampleConfig config = {0};
    config.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
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
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.primitiveRestartEnable = false;
    return config;
}

VEViewport veDefaultViewportConfig() {
    VEViewport config = {0};
    config.x = 0.0f;
    config.y = 0.0f;
    config.width = 800.0f;
    config.height = 600.0f;
    config.minDepth = 0.0f;
    config.maxDepth = 1.0f;
    return config;
}

VERect2D veDefaultScissorConfig() {
    VERect2D config = {0};
    config.x = 0; 
    config.y = 0;
    config.width = 800;
    config.height = 600;
    return config;
}

// =============================================================================
// Render Configuration Management
// =============================================================================

VEVertexInputConfig copyVertexInputConfig(VEVertexInputConfig* orig)
{
    VEVertexInputConfig config = {0};
    config.bindingCount = orig->bindingCount;
    config.bindings = calloc(orig->bindingCount, sizeof(VEVertexBinding));
    for (int i=0; i< orig->bindingCount; i++)
    {
        config.bindings[i].binding = orig->bindings[i].binding;
        config.bindings[i].stride = orig->bindings[i].stride;
        config.bindings[i].inputRate = orig->bindings[i].inputRate;
        config.bindings[i].divisor = orig->bindings[i].divisor;
    }
    config.attributeCount = orig->attributeCount;
    config.attributes = calloc(orig->attributeCount, sizeof(VEVertexAttribute));
    for (int i=0; i < orig->attributeCount; i++)
    {
        config.attributes[i].binding = orig->attributes[i].binding;
        config.attributes[i].format = orig->attributes[i].format;
        config.attributes[i].location = orig->attributes[i].location;
        config.attributes[i].offset = orig->attributes[i].offset;
    }
    config.topology = orig->topology;
    config.primitiveRestartEnable = orig->primitiveRestartEnable;
    config.patchControlPoints = orig->patchControlPoints;
    return config;

}

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
    
    //reset the render config
    VERenderConfigInternal* config = &deviceInternal->renderConfigs[index]; 
    config->configTypes = desc->configTypes;
    
    // Copy configurations or use defaults
    config->rasterConfig = desc->rasterConfig ? *desc->rasterConfig : veDefaultRasterConfig();
    config->depthConfig = desc->depthConfig ? *desc->depthConfig : veDefaultDepthConfig();
    config->blendConfig = desc->blendConfig ? *desc->blendConfig : veDefaultOpaqueBlendConfig();
    config->multisampleConfig = desc->multisampleConfig ? *desc->multisampleConfig : veDefaultMultisampleConfig();
    // TODO: this is going to leak a little
    config->vertexInputConfig = desc->vertexInputConfig ? copyVertexInputConfig(desc->vertexInputConfig) : veDefaultVertexInputConfig();
    config->viewport = desc->viewportConfig ? *desc->viewportConfig : veDefaultViewportConfig();
    config->scissor = desc->scissorConfig ? *desc->scissorConfig : veDefaultScissorConfig();
    
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

    config->device = (VEDeviceInternal*)device; 
    config->isValid = true;
    deviceInternal->renderConfigCount++;
    
    return (VERenderConfig*)config;
}

void veDestroyRenderConfig(VERenderConfig* config) {
    if (!config) return;
    
    VERenderConfigInternal* internal = (VERenderConfigInternal*)config;
    internal->device->renderConfigCount--;

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
    config->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    
    config->device = (VEDeviceInternal*)device;
    config->isValid = true;
    deviceInternal->vertexConfigCount++;
    
    return (VEVertexConfig*)config;
}

void veDestroyVertexConfig(VEVertexConfig* config) {
    if (!config) return;
    
    VEVertexConfigInternal* internal = (VEVertexConfigInternal*)config;
    internal->device->vertexConfigCount--;
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
    raster.polygonMode = VK_POLYGON_MODE_LINE;
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
    raster.cullMode = VK_CULL_MODE_FRONT_BIT; // Reduce peter-panning
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
    raster.cullMode = VK_CULL_MODE_NONE; // UI elements might be double-sided
    
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