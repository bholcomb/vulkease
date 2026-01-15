/**
 * @file ve_render_config.c
 * @brief Render Configuration Implementation
 */

#include "ve_internal.h"

const VkColorComponentFlagBits VK_COLOR_COMPONENT_ALL = static_cast<VkColorComponentFlagBits>(
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

// =============================================================================
// Default Configuration Creators
// =============================================================================

VERasterConfig veDefaultRasterConfig(void)
{
   VERasterConfig config{};
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

VEDepthConfig veDefaultDepthConfig(void)
{
   VEDepthConfig config{};
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

VEBlendConfig veDefaultOpaqueBlendConfig(void)
{
   VEBlendConfig config{};
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

VEBlendConfig veDefaultAlphaBlendConfig(void)
{
   VEBlendConfig config{};
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

VEBlendConfig veDefaultAdditiveBlendConfig(void)
{
   VEBlendConfig config{};
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

VEMultisampleConfig veDefaultMultisampleConfig(void)
{
   VEMultisampleConfig config{};
   config.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
   config.sampleShadingEnable = false;
   config.minSampleShading = 1.0f;
   config.alphaToCoverageEnable = false;
   config.alphaToOneEnable = false;
   return config;
}

VEVertexInputConfig veDefaultVertexInputConfig(void)
{
   VEVertexInputConfig config{};
   config.bindingCount = 0;
   config.bindings = NULL;
   config.attributeCount = 0;
   config.attributes = NULL;
   config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   config.primitiveRestartEnable = false;
   return config;
}

VEViewport veDefaultViewportConfig()
{
   VEViewport config{};
   config.x = 0.0f;
   config.y = 0.0f;
   config.width = 800.0f;
   config.height = 600.0f;
   config.minDepth = 0.0f;
   config.maxDepth = 1.0f;
   return config;
}

VERect2D veDefaultScissorConfig()
{
   VERect2D config{};
   config.x = 0;
   config.y = 0;
   config.width = 800;
   config.height = 600;
   return config;
}

// =============================================================================
// Render Configuration Management
// =============================================================================

VEVertexInputConfig copyVertexInputConfig(const VEVertexInputConfig *orig)
{
   VEVertexInputConfig config{};
   config.bindingCount = orig->bindingCount;
   config.bindings = static_cast<VEVertexBinding *>(calloc(orig->bindingCount, sizeof(VEVertexBinding)));
   for (uint32_t i = 0; i < orig->bindingCount; i++)
   {
      config.bindings[i].binding = orig->bindings[i].binding;
      config.bindings[i].stride = orig->bindings[i].stride;
      config.bindings[i].inputRate = orig->bindings[i].inputRate;
      config.bindings[i].divisor = orig->bindings[i].divisor;
   }
   config.attributeCount = orig->attributeCount;
   config.attributes = static_cast<VEVertexAttribute *>(calloc(orig->attributeCount, sizeof(VEVertexAttribute)));
   for (uint32_t i = 0; i < orig->attributeCount; i++)
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

static void destroyVertexInputConfigData(VEVertexInputConfig &config)
{
   if (config.bindings)
   {
      free(config.bindings);
      config.bindings = nullptr;
   }

   if (config.attributes)
   {
      free(config.attributes);
      config.attributes = nullptr;
   }

   config.bindingCount = 0;
   config.attributeCount = 0;
   config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   config.primitiveRestartEnable = false;
   config.patchControlPoints = 0;
}

VEResult veCreateRenderConfig(VEDevice *device, const VERenderConfigDesc *desc, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device || !desc)
   {
      veSetError("Invalid parameters for render config creation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Find free slot in render configs array
   uint32_t index = UINT32_MAX;
   for (uint32_t i = 0; i < deviceInternal->maxRenderConfigs; i++)
   {
      if (!deviceInternal->renderConfigs[i].isValid)
      {
         index = i;
         break;
      }
   }

   if (index == UINT32_MAX)
   {
      veSetError("No free render config slots available");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // reset the render config
   VERenderConfigInternal *config = &deviceInternal->renderConfigs[index];
   config->configTypes = desc->configTypes;

   // Copy configurations or use defaults
   if (desc->rasterConfig)
   {
      config->rasterConfig = *desc->rasterConfig;
   }
   else
   {
      config->rasterConfig = veDefaultRasterConfig();
   }

   if (desc->depthConfig)
   {
      config->depthConfig = *desc->depthConfig;
   }
   else
   {
      config->depthConfig = veDefaultDepthConfig();
   }

   if (desc->blendConfig)
   {
      config->blendConfig = *desc->blendConfig;
   }
   else
   {
      config->blendConfig = veDefaultOpaqueBlendConfig();
   }

   if (desc->multisampleConfig)
   {
      config->multisampleConfig = *desc->multisampleConfig;
   }
   else
   {
      config->multisampleConfig = veDefaultMultisampleConfig();
   }

   if (desc->vertexInputConfig)
   {
      config->vertexInputConfig = copyVertexInputConfig(desc->vertexInputConfig);
   }
   else
   {
      config->vertexInputConfig = veDefaultVertexInputConfig();
   }
   config->viewport = desc->viewportConfig ? *desc->viewportConfig : veDefaultViewportConfig();
   config->scissor = desc->scissorConfig ? *desc->scissorConfig : veDefaultScissorConfig();

   // Copy shaders
   config->shaderCount = desc->shaderCount < 6 ? desc->shaderCount : 6;
   for (uint32_t i = 0; i < config->shaderCount; i++)
   {
      config->shaders[i] = desc->shaders[i];
   }

   if (desc->debugName)
   {
      strncpy(config->debugName, desc->debugName, sizeof(config->debugName) - 1);
   }
   else
   {
      snprintf(config->debugName, sizeof(config->debugName), "RenderConfig_%u", index);
   }

   config->device = (VEDeviceInternal *)device;
   config->isValid = true;
   deviceInternal->renderConfigCount++;

   *outConfig = (VERenderConfig *)config;
   return VE_SUCCESS;
}

VEResult veDestroyRenderConfig(VERenderConfig *config)
{
   if (!config)
   {
      veSetError("RenderConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderConfigInternal *internal = (VERenderConfigInternal *)config;
   destroyVertexInputConfigData(internal->vertexInputConfig);
   if (!internal->device)
   {
      veSetError("RenderConfig has no device");
      return VE_ERROR_INVALID_PARAMETER;
   }
   internal->device->renderConfigCount--;

   memset(internal, 0, sizeof(VERenderConfigInternal));
   return VE_SUCCESS;
}

// =============================================================================
// Vertex Configuration Management
// =============================================================================

VEResult veCreateVertexConfig(VEDevice *device, uint32_t bindingCount, const VEVertexBinding *bindings,
                              uint32_t attributeCount, const VEVertexAttribute *attributes, VEVertexConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (bindingCount > 16 || attributeCount > 32)
   {
      veSetError("Too many vertex bindings or attributes");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Find free slot in vertex configs array
   uint32_t index = UINT32_MAX;
   for (uint32_t i = 0; i < deviceInternal->maxVertexConfigs; i++)
   {
      if (!deviceInternal->vertexConfigs[i].isValid)
      {
         index = i;
         break;
      }
   }

   if (index == UINT32_MAX)
   {
      veSetError("No free vertex config slots available");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VEVertexConfigInternal *config = &deviceInternal->vertexConfigs[index];
   memset(config, 0, sizeof(VEVertexConfigInternal));

   config->bindingCount = bindingCount;
   config->attributeCount = attributeCount;
   config->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   config->primitiveRestartEnable = false;

   // Copy bindings
   for (uint32_t i = 0; i < bindingCount; i++)
   {
      config->bindings[i] = bindings[i];
   }

   // Copy attributes
   for (uint32_t i = 0; i < attributeCount; i++)
   {
      config->attributes[i] = attributes[i];
   }

   snprintf(config->debugName, sizeof(config->debugName), "VertexConfig_%u", index);

   config->device = (VEDeviceInternal *)device;
   config->isValid = true;
   deviceInternal->vertexConfigCount++;

   *outConfig = (VEVertexConfig *)config;
   return VE_SUCCESS;
}

VEResult veDestroyVertexConfig(VEVertexConfig *config)
{
   if (!config)
   {
      veSetError("VertexConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEVertexConfigInternal *internal = (VEVertexConfigInternal *)config;
   if (!internal->device)
   {
      veSetError("VertexConfig has no device");
      return VE_ERROR_INVALID_PARAMETER;
   }
   internal->device->vertexConfigCount--;
   memset(internal, 0, sizeof(VEVertexConfigInternal));
   return VE_SUCCESS;
}

// =============================================================================
// Common Render Configuration Creators
// =============================================================================

VEResult veCreateOpaqueRenderConfig(VEDevice *device, const char *debugName, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERasterConfig raster{};
   VEDepthConfig depth{};
   VEBlendConfig blend{};
   VEMultisampleConfig multisample{};
   raster = veDefaultRasterConfig();
   depth = veDefaultDepthConfig();
   blend = veDefaultOpaqueBlendConfig();
   multisample = veDefaultMultisampleConfig();

   VERenderConfigDesc desc{};
   desc.configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | VE_CONFIG_TYPE_COLOR_BLEND |
                      VE_CONFIG_TYPE_MULTISAMPLE;
   desc.rasterConfig = &raster;
   desc.depthConfig = &depth;
   desc.blendConfig = &blend;
   desc.multisampleConfig = &multisample;
   desc.vertexInputConfig = NULL;
   desc.shaderCount = 0;
   desc.shaders = NULL;
   desc.debugName = debugName ? debugName : "OpaqueRenderConfig";

   return veCreateRenderConfig(device, &desc, outConfig);
}

VEResult veCreateTransparentRenderConfig(VEDevice *device, const char *debugName, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERasterConfig raster{};
   VEDepthConfig depth{};
   VEBlendConfig blend{};
   VEMultisampleConfig multisample{};
   raster = veDefaultRasterConfig();
   depth = veDefaultDepthConfig();
   depth.depthWriteEnable = false; // Don't write depth for transparent objects
   blend = veDefaultAlphaBlendConfig();
   multisample = veDefaultMultisampleConfig();

   VERenderConfigDesc desc{};
   desc.configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | VE_CONFIG_TYPE_COLOR_BLEND |
                      VE_CONFIG_TYPE_MULTISAMPLE;
   desc.rasterConfig = &raster;
   desc.depthConfig = &depth;
   desc.blendConfig = &blend;
   desc.multisampleConfig = &multisample;
   desc.vertexInputConfig = NULL;
   desc.shaderCount = 0;
   desc.shaders = NULL;
   desc.debugName = debugName ? debugName : "TransparentRenderConfig";

   return veCreateRenderConfig(device, &desc, outConfig);
}

VEResult veCreateWireframeRenderConfig(VEDevice *device, const char *debugName, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERasterConfig raster{};
   VEDepthConfig depth{};
   VEBlendConfig blend{};
   VEMultisampleConfig multisample{};
   raster = veDefaultRasterConfig();
   raster.polygonMode = VK_POLYGON_MODE_LINE;
   raster.lineWidth = 1.0f;
   depth = veDefaultDepthConfig();
   blend = veDefaultOpaqueBlendConfig();
   multisample = veDefaultMultisampleConfig();

   VERenderConfigDesc desc{};
   desc.configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | VE_CONFIG_TYPE_COLOR_BLEND |
                      VE_CONFIG_TYPE_MULTISAMPLE;
   desc.rasterConfig = &raster;
   desc.depthConfig = &depth;
   desc.blendConfig = &blend;
   desc.multisampleConfig = &multisample;
   desc.vertexInputConfig = NULL;
   desc.shaderCount = 0;
   desc.shaders = NULL;
   desc.debugName = debugName ? debugName : "WireframeRenderConfig";

   return veCreateRenderConfig(device, &desc, outConfig);
}

VEResult veCreateShadowRenderConfig(VEDevice *device, const char *debugName, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERasterConfig raster{};
   VEDepthConfig depth{};
   VEBlendConfig blend{};
   VEMultisampleConfig multisample{};
   raster = veDefaultRasterConfig();
   raster.cullMode = VK_CULL_MODE_FRONT_BIT; // Reduce peter-panning
   raster.depthBiasEnable = true;
   raster.depthBiasConstantFactor = 2.0f;
   raster.depthBiasSlopeFactor = 1.5f;

   depth = veDefaultDepthConfig();

   blend = veDefaultOpaqueBlendConfig();
   // Shadow maps typically don't need color output
   blend.attachments[0].colorWriteMask = 0;

   multisample = veDefaultMultisampleConfig();

   VERenderConfigDesc desc{};
   desc.configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | VE_CONFIG_TYPE_COLOR_BLEND |
                      VE_CONFIG_TYPE_MULTISAMPLE;
   desc.rasterConfig = &raster;
   desc.depthConfig = &depth;
   desc.blendConfig = &blend;
   desc.multisampleConfig = &multisample;
   desc.vertexInputConfig = NULL;
   desc.shaderCount = 0;
   desc.shaders = NULL;
   desc.debugName = debugName ? debugName : "ShadowRenderConfig";

   return veCreateRenderConfig(device, &desc, outConfig);
}

VEResult veCreateUIRenderConfig(VEDevice *device, const char *debugName, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERasterConfig raster{};
   VEDepthConfig depth{};
   VEBlendConfig blend{};
   VEMultisampleConfig multisample{};
   raster = veDefaultRasterConfig();
   raster.cullMode = VK_CULL_MODE_NONE; // UI elements might be double-sided

   depth = veDefaultDepthConfig();
   depth.depthTestEnable = false; // UI always on top
   depth.depthWriteEnable = false;

   blend = veDefaultAlphaBlendConfig();
   multisample = veDefaultMultisampleConfig();

   VERenderConfigDesc desc{};
   desc.configTypes = VE_CONFIG_TYPE_RASTERIZATION | VE_CONFIG_TYPE_DEPTH_STENCIL | VE_CONFIG_TYPE_COLOR_BLEND |
                      VE_CONFIG_TYPE_MULTISAMPLE;
   desc.rasterConfig = &raster;
   desc.depthConfig = &depth;
   desc.blendConfig = &blend;
   desc.multisampleConfig = &multisample;
   desc.vertexInputConfig = NULL;
   desc.shaderCount = 0;
   desc.shaders = NULL;
   desc.debugName = debugName ? debugName : "UIRenderConfig";

   return veCreateRenderConfig(device, &desc, outConfig);
}

// =============================================================================
// Configuration Composition
// =============================================================================

VEResult veCreateConfigVariant(VERenderConfig *baseConfig, const VERenderConfigDesc *overrides,
                               VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }
   if (!baseConfig || !overrides)
   {
      veSetError("Invalid parameters for config variant creation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderConfigInternal *base = (VERenderConfigInternal *)baseConfig;

   // Create a new config descriptor based on the base config
   VERenderConfigDesc desc{};
   desc.configTypes = overrides->configTypes != 0 ? overrides->configTypes : base->configTypes;
   desc.rasterConfig = overrides->rasterConfig ? overrides->rasterConfig : &base->rasterConfig;
   desc.depthConfig = overrides->depthConfig ? overrides->depthConfig : &base->depthConfig;
   desc.blendConfig = overrides->blendConfig ? overrides->blendConfig : &base->blendConfig;
   desc.multisampleConfig = overrides->multisampleConfig ? overrides->multisampleConfig : &base->multisampleConfig;
   desc.vertexInputConfig = overrides->vertexInputConfig ? overrides->vertexInputConfig : &base->vertexInputConfig;
   desc.shaderCount = overrides->shaderCount > 0 ? overrides->shaderCount : base->shaderCount;
   desc.shaders = overrides->shaders ? overrides->shaders : base->shaders;
   desc.debugName = overrides->debugName ? overrides->debugName : "ConfigVariant";

   return veCreateRenderConfig((VEDevice *)base->device, &desc, outConfig);
}

VEResult veMergeRenderConfigs(VEDevice *device, uint32_t configCount, VERenderConfig *const *configs,
                              const char *debugName, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }
   if (!device || configCount == 0 || !configs)
   {
      veSetError("Invalid parameters for config merging");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   VEViewport mergedViewport{};
   VERect2D mergedScissor{};
   VERasterConfig mergedRaster{};
   VEDepthConfig mergedDepth{};
   VEBlendConfig mergedBlend{};
   VEMultisampleConfig mergedMultisample{};

   const VEVertexInputConfig *vertexInputSource = nullptr;
   VEShader *mergedShaders[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
   uint32_t mergedShaderCount = 0;

   bool hasViewport = false;
   bool hasScissor = false;
   bool hasRaster = false;
   bool hasDepth = false;
   bool hasBlend = false;
   bool hasMultisample = false;
   bool hasVertexInput = false;
   bool hasShaders = false;

   VEConfigTypeFlags combinedTypes = 0;

   for (uint32_t i = 0; i < configCount; ++i)
   {
      VERenderConfigInternal *internal = (VERenderConfigInternal *)configs[i];
      if (!internal || !internal->isValid)
      {
         veSetError("Render config %u is invalid", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      if (internal->device != deviceInternal)
      {
         veSetError("Render config device mismatch during merge");
         return VE_ERROR_INVALID_PARAMETER;
      }

      if (internal->configTypes & VE_CONFIG_TYPE_VIEWPORT)
      {
         mergedViewport = internal->viewport;
         hasViewport = true;
      }

      if (internal->configTypes & VE_CONFIG_TYPE_SCISSOR)
      {
         mergedScissor = internal->scissor;
         hasScissor = true;
      }

      if (internal->configTypes & VE_CONFIG_TYPE_RASTERIZATION)
      {
         mergedRaster = internal->rasterConfig;
         hasRaster = true;
      }

      if (internal->configTypes & VE_CONFIG_TYPE_DEPTH_STENCIL)
      {
         mergedDepth = internal->depthConfig;
         hasDepth = true;
      }

      if (internal->configTypes & VE_CONFIG_TYPE_COLOR_BLEND)
      {
         mergedBlend = internal->blendConfig;
         hasBlend = true;
      }

      if (internal->configTypes & VE_CONFIG_TYPE_MULTISAMPLE)
      {
         mergedMultisample = internal->multisampleConfig;
         hasMultisample = true;
      }

      if (internal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
      {
         vertexInputSource = &internal->vertexInputConfig;
         hasVertexInput = true;
      }

      if ((internal->configTypes & VE_CONFIG_TYPE_SHADERS) && internal->shaderCount > 0)
      {
         mergedShaderCount = internal->shaderCount > 6 ? 6 : internal->shaderCount;
         for (uint32_t s = 0; s < mergedShaderCount; ++s)
         {
            mergedShaders[s] = internal->shaders[s];
         }
         hasShaders = true;
      }

      combinedTypes |= internal->configTypes;
   }

   VERenderConfigDesc desc{};
   desc.configTypes = combinedTypes;
   desc.viewportConfig = hasViewport ? &mergedViewport : NULL;
   desc.scissorConfig = hasScissor ? &mergedScissor : NULL;
   desc.rasterConfig = hasRaster ? &mergedRaster : NULL;
   desc.depthConfig = hasDepth ? &mergedDepth : NULL;
   desc.blendConfig = hasBlend ? &mergedBlend : NULL;
   desc.multisampleConfig = hasMultisample ? &mergedMultisample : NULL;
   desc.vertexInputConfig = hasVertexInput ? vertexInputSource : NULL;

   if (hasShaders)
   {
      desc.shaderCount = mergedShaderCount;
      desc.shaders = mergedShaders;
   }

   desc.debugName = debugName ? debugName : "MergedRenderConfig";

   return veCreateRenderConfig(device, &desc, outConfig);
}

VEResult veCloneRenderConfig(VERenderConfig *config, const char *debugName, VERenderConfig **outConfig)
{
   if (!outConfig)
   {
      veSetError("outConfig cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }
   if (!config)
   {
      veSetError("Config cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderConfigInternal *internal = (VERenderConfigInternal *)config;
   if (!internal->isValid || !internal->device)
   {
      veSetError("Cannot clone invalid render config");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderConfigDesc desc{};
   desc.configTypes = internal->configTypes;
   desc.viewportConfig = (internal->configTypes & VE_CONFIG_TYPE_VIEWPORT) ? &internal->viewport : NULL;
   desc.scissorConfig = (internal->configTypes & VE_CONFIG_TYPE_SCISSOR) ? &internal->scissor : NULL;
   desc.rasterConfig = (internal->configTypes & VE_CONFIG_TYPE_RASTERIZATION) ? &internal->rasterConfig : NULL;
   desc.depthConfig = (internal->configTypes & VE_CONFIG_TYPE_DEPTH_STENCIL) ? &internal->depthConfig : NULL;
   desc.blendConfig = (internal->configTypes & VE_CONFIG_TYPE_COLOR_BLEND) ? &internal->blendConfig : NULL;
   desc.multisampleConfig = (internal->configTypes & VE_CONFIG_TYPE_MULTISAMPLE) ? &internal->multisampleConfig : NULL;
   desc.vertexInputConfig = (internal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT) ? &internal->vertexInputConfig : NULL;

   if ((internal->configTypes & VE_CONFIG_TYPE_SHADERS) && internal->shaderCount > 0)
   {
      desc.shaderCount = internal->shaderCount;
      desc.shaders = internal->shaders;
   }

   desc.debugName = debugName ? debugName : internal->debugName;

   return veCreateRenderConfig((VEDevice *)internal->device, &desc, outConfig);
}