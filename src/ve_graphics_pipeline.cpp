/**
 * @file ve_graphics_pipeline.cpp
 * @brief Graphics Pipeline and Draw State Implementation
 */

#include "ve_internal.h"

const VkColorComponentFlagBits VK_COLOR_COMPONENT_ALL = static_cast<VkColorComponentFlagBits>(
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

// =============================================================================
// Default Descriptor Creators
// =============================================================================

VEGraphicsPipelineDesc veDefaultGraphicsPipelineDesc(void)
{
   VEGraphicsPipelineDesc desc{};

   // Shaders - must be provided by user
   desc.vertexShader = nullptr;
   desc.fragmentShader = nullptr;
   desc.geometryShader = nullptr;
   desc.tessControlShader = nullptr;
   desc.tessEvalShader = nullptr;
   desc.taskShader = nullptr;
   desc.meshShader = nullptr;

   // Vertex input - must be provided by user
   desc.vertexBindingCount = 0;
   desc.vertexBindings = nullptr;
   desc.vertexAttributeCount = 0;
   desc.vertexAttributes = nullptr;

   // Depth config - opaque defaults
   desc.depthTestEnable = true;
   desc.depthWriteEnable = true;
   desc.depthCompareOp = VK_COMPARE_OP_LESS;
   desc.depthBoundsTestEnable = false;
   desc.minDepthBounds = 0.0f;
   desc.maxDepthBounds = 1.0f;

   // Stencil config - disabled
   desc.stencilTestEnable = false;
   desc.frontStencil = {
       VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFF, 0xFF, 0};
   desc.backStencil = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFF, 0xFF, 0};

   // Blend config - opaque (no blending)
   desc.logicOpEnable = false;
   desc.logicOp = VK_LOGIC_OP_COPY;
   desc.blendAttachmentCount = 1;
   desc.blendAttachments[0].blendEnable = false;
   desc.blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
   desc.blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
   desc.blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
   desc.blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
   desc.blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
   desc.blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
   desc.blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_ALL;
   desc.blendConstants[0] = 0.0f;
   desc.blendConstants[1] = 0.0f;
   desc.blendConstants[2] = 0.0f;
   desc.blendConstants[3] = 0.0f;

   // Rasterization - backface culled
   desc.cullMode = VK_CULL_MODE_BACK_BIT;
   desc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

   // Multisampling - disabled
   desc.sampleShadingEnable = false;
   desc.minSampleShading = 1.0f;

   desc.debugName = nullptr;

   return desc;
}

VEDrawStateDesc veDefaultDrawStateDesc(void)
{
   VEDrawStateDesc desc{};

   // Geometry interpretation
   desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   desc.primitiveRestartEnable = false;
   desc.patchControlPoints = 0;

   // Rasterization
   desc.polygonMode = VK_POLYGON_MODE_FILL;
   desc.lineWidth = 1.0f;
   desc.cullMode = VE_CULL_MODE_USE_PIPELINE; // Use pipeline default
   desc.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   desc.rasterizerDiscardEnable = false;

   // Depth bias - disabled
   desc.depthBiasEnable = false;
   desc.depthBiasConstantFactor = 0.0f;
   desc.depthBiasClamp = 0.0f;
   desc.depthBiasSlopeFactor = 0.0f;
   desc.depthClampEnable = false;

   // Coverage - disabled
   desc.alphaToCoverageEnable = false;
   desc.alphaToOneEnable = false;

   desc.debugName = nullptr;

   return desc;
}

// =============================================================================
// Graphics Pipeline Management
// =============================================================================

VEResult veCreateGraphicsPipeline(VEDevice *device, const VEGraphicsPipelineDesc *desc,
                                  VEGraphicsPipeline **outPipeline)
{
   if (!device || !desc || !outPipeline)
   {
      veSetError("veCreateGraphicsPipeline: NULL parameter");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);

   // Find free slot
   if (deviceInternal->graphicsPipelineCount >= deviceInternal->maxGraphicsPipelines)
   {
      veSetError("veCreateGraphicsPipeline: Maximum graphics pipelines reached (%u)",
                 deviceInternal->maxGraphicsPipelines);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Find first invalid slot
   VEGraphicsPipelineInternal *pipeline = nullptr;
   for (uint32_t i = 0; i < deviceInternal->maxGraphicsPipelines; i++)
   {
      if (!deviceInternal->graphicsPipelines[i].isValid)
      {
         pipeline = &deviceInternal->graphicsPipelines[i];
         break;
      }
   }

   if (!pipeline)
   {
      veSetError("veCreateGraphicsPipeline: No free pipeline slots");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Initialize pipeline
   memset(pipeline, 0, sizeof(VEGraphicsPipelineInternal));

   // Copy shaders
   pipeline->vertexShader = desc->vertexShader;
   pipeline->fragmentShader = desc->fragmentShader;
   pipeline->geometryShader = desc->geometryShader;
   pipeline->tessControlShader = desc->tessControlShader;
   pipeline->tessEvalShader = desc->tessEvalShader;
   pipeline->taskShader = desc->taskShader;
   pipeline->meshShader = desc->meshShader;

   // Copy vertex input
   pipeline->vertexBindingCount = desc->vertexBindingCount;
   if (desc->vertexBindingCount > 0 && desc->vertexBindings)
   {
      uint32_t count =
          desc->vertexBindingCount < VE_MAX_VERTEX_BINDINGS ? desc->vertexBindingCount : VE_MAX_VERTEX_BINDINGS;
      memcpy(pipeline->vertexBindings, desc->vertexBindings, count * sizeof(VEVertexBinding));
      pipeline->vertexBindingCount = count;
   }

   pipeline->vertexAttributeCount = desc->vertexAttributeCount;
   if (desc->vertexAttributeCount > 0 && desc->vertexAttributes)
   {
      uint32_t count =
          desc->vertexAttributeCount < VE_MAX_VERTEX_ATTRIBUTES ? desc->vertexAttributeCount : VE_MAX_VERTEX_ATTRIBUTES;
      memcpy(pipeline->vertexAttributes, desc->vertexAttributes, count * sizeof(VEVertexAttribute));
      pipeline->vertexAttributeCount = count;
   }

   // Copy depth config
   pipeline->depthTestEnable = desc->depthTestEnable;
   pipeline->depthWriteEnable = desc->depthWriteEnable;
   pipeline->depthCompareOp = desc->depthCompareOp;
   pipeline->depthBoundsTestEnable = desc->depthBoundsTestEnable;
   pipeline->minDepthBounds = desc->minDepthBounds;
   pipeline->maxDepthBounds = desc->maxDepthBounds;

   // Copy stencil config
   pipeline->stencilTestEnable = desc->stencilTestEnable;
   pipeline->frontStencil = desc->frontStencil;
   pipeline->backStencil = desc->backStencil;

   // Copy blend config
   pipeline->logicOpEnable = desc->logicOpEnable;
   pipeline->logicOp = desc->logicOp;
   pipeline->blendAttachmentCount = desc->blendAttachmentCount;
   if (desc->blendAttachmentCount > 0)
   {
      uint32_t count =
          desc->blendAttachmentCount < VE_MAX_COLOR_ATTACHMENTS ? desc->blendAttachmentCount : VE_MAX_COLOR_ATTACHMENTS;
      memcpy(pipeline->blendAttachments, desc->blendAttachments, count * sizeof(VEBlendAttachment));
      pipeline->blendAttachmentCount = count;
   }
   memcpy(pipeline->blendConstants, desc->blendConstants, sizeof(pipeline->blendConstants));

   // Copy rasterization defaults
   pipeline->cullMode = desc->cullMode;
   pipeline->frontFace = desc->frontFace;

   // Copy multisampling
   pipeline->sampleShadingEnable = desc->sampleShadingEnable;
   pipeline->minSampleShading = desc->minSampleShading;

   // Debug name
   if (desc->debugName)
   {
      strncpy(pipeline->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
   }

   pipeline->isValid = true;
   pipeline->device = deviceInternal;
   deviceInternal->graphicsPipelineCount++;

   *outPipeline = reinterpret_cast<VEGraphicsPipeline *>(pipeline);
   return VE_SUCCESS;
}

VEResult veDestroyGraphicsPipeline(VEGraphicsPipeline *pipeline)
{
   if (!pipeline)
   {
      return VE_SUCCESS; // NULL is valid (no-op)
   }

   VEGraphicsPipelineInternal *pipelineInternal = reinterpret_cast<VEGraphicsPipelineInternal *>(pipeline);

   if (!pipelineInternal->isValid)
   {
      veSetError("veDestroyGraphicsPipeline: Pipeline already destroyed");
      return VE_ERROR_INVALID_PARAMETER;
   }

   pipelineInternal->isValid = false;
   pipelineInternal->device->graphicsPipelineCount--;

   return VE_SUCCESS;
}

// =============================================================================
// Convenience Pipeline Creators
// =============================================================================

static VEResult createPipelineWithBlend(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                        const VEVertexBinding *bindings, uint32_t bindingCount,
                                        const VEVertexAttribute *attrs, uint32_t attrCount, bool blendEnable,
                                        VkBlendFactor srcColor, VkBlendFactor dstColor, bool depthTest, bool depthWrite,
                                        const char *debugName, VEGraphicsPipeline **outPipeline)
{
   VEGraphicsPipelineDesc desc = veDefaultGraphicsPipelineDesc();
   desc.vertexShader = vertexShader;
   desc.fragmentShader = fragmentShader;
   desc.vertexBindings = bindings;
   desc.vertexBindingCount = bindingCount;
   desc.vertexAttributes = attrs;
   desc.vertexAttributeCount = attrCount;

   desc.blendAttachments[0].blendEnable = blendEnable;
   desc.blendAttachments[0].srcColorBlendFactor = srcColor;
   desc.blendAttachments[0].dstColorBlendFactor = dstColor;

   desc.depthTestEnable = depthTest;
   desc.depthWriteEnable = depthWrite;

   desc.debugName = debugName;

   return veCreateGraphicsPipeline(device, &desc, outPipeline);
}

VEResult veCreateOpaquePipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                const VEVertexBinding *bindings, uint32_t bindingCount, const VEVertexAttribute *attrs,
                                uint32_t attrCount, const char *debugName, VEGraphicsPipeline **outPipeline)
{
   return createPipelineWithBlend(device, vertexShader, fragmentShader, bindings, bindingCount, attrs, attrCount, false,
                                  VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, true, true, debugName, outPipeline);
}

VEResult veCreateTransparentPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                     const VEVertexBinding *bindings, uint32_t bindingCount,
                                     const VEVertexAttribute *attrs, uint32_t attrCount, const char *debugName,
                                     VEGraphicsPipeline **outPipeline)
{
   VEGraphicsPipelineDesc desc = veDefaultGraphicsPipelineDesc();
   desc.vertexShader = vertexShader;
   desc.fragmentShader = fragmentShader;
   desc.vertexBindings = bindings;
   desc.vertexBindingCount = bindingCount;
   desc.vertexAttributes = attrs;
   desc.vertexAttributeCount = attrCount;

   // Alpha blending
   desc.blendAttachments[0].blendEnable = true;
   desc.blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   desc.blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
   desc.blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
   desc.blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
   desc.blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
   desc.blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

   // Depth test but no write (for correct transparency sorting)
   desc.depthTestEnable = true;
   desc.depthWriteEnable = false;

   desc.debugName = debugName;

   return veCreateGraphicsPipeline(device, &desc, outPipeline);
}

VEResult veCreateAdditivePipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                  const VEVertexBinding *bindings, uint32_t bindingCount,
                                  const VEVertexAttribute *attrs, uint32_t attrCount, const char *debugName,
                                  VEGraphicsPipeline **outPipeline)
{
   VEGraphicsPipelineDesc desc = veDefaultGraphicsPipelineDesc();
   desc.vertexShader = vertexShader;
   desc.fragmentShader = fragmentShader;
   desc.vertexBindings = bindings;
   desc.vertexBindingCount = bindingCount;
   desc.vertexAttributes = attrs;
   desc.vertexAttributeCount = attrCount;

   // Additive blending
   desc.blendAttachments[0].blendEnable = true;
   desc.blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   desc.blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
   desc.blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
   desc.blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
   desc.blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
   desc.blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

   desc.depthTestEnable = true;
   desc.depthWriteEnable = false;

   desc.debugName = debugName;

   return veCreateGraphicsPipeline(device, &desc, outPipeline);
}

VEResult veCreateShadowPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                const VEVertexBinding *bindings, uint32_t bindingCount, const VEVertexAttribute *attrs,
                                uint32_t attrCount, const char *debugName, VEGraphicsPipeline **outPipeline)
{
   VEGraphicsPipelineDesc desc = veDefaultGraphicsPipelineDesc();
   desc.vertexShader = vertexShader;
   desc.fragmentShader = fragmentShader;
   desc.vertexBindings = bindings;
   desc.vertexBindingCount = bindingCount;
   desc.vertexAttributes = attrs;
   desc.vertexAttributeCount = attrCount;

   // No color output, depth only
   desc.blendAttachmentCount = 0;

   // Front face culling for shadow mapping (Peter Panning fix)
   desc.cullMode = VK_CULL_MODE_FRONT_BIT;

   desc.depthTestEnable = true;
   desc.depthWriteEnable = true;

   desc.debugName = debugName;

   return veCreateGraphicsPipeline(device, &desc, outPipeline);
}

VEResult veCreateUIOverlayPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                   const VEVertexBinding *bindings, uint32_t bindingCount,
                                   const VEVertexAttribute *attrs, uint32_t attrCount, const char *debugName,
                                   VEGraphicsPipeline **outPipeline)
{
   VEGraphicsPipelineDesc desc = veDefaultGraphicsPipelineDesc();
   desc.vertexShader = vertexShader;
   desc.fragmentShader = fragmentShader;
   desc.vertexBindings = bindings;
   desc.vertexBindingCount = bindingCount;
   desc.vertexAttributes = attrs;
   desc.vertexAttributeCount = attrCount;

   // Alpha blending
   desc.blendAttachments[0].blendEnable = true;
   desc.blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   desc.blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
   desc.blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
   desc.blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
   desc.blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
   desc.blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

   // No depth testing for UI
   desc.depthTestEnable = false;
   desc.depthWriteEnable = false;

   // No culling for UI
   desc.cullMode = VK_CULL_MODE_NONE;

   desc.debugName = debugName;

   return veCreateGraphicsPipeline(device, &desc, outPipeline);
}

// =============================================================================
// Draw State Management
// =============================================================================

VEResult veCreateDrawState(VEDevice *device, const VEDrawStateDesc *desc, VEDrawState **outDrawState)
{
   if (!device || !desc || !outDrawState)
   {
      veSetError("veCreateDrawState: NULL parameter");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);

   // Find free slot
   if (deviceInternal->drawStateCount >= deviceInternal->maxDrawStates)
   {
      veSetError("veCreateDrawState: Maximum draw states reached (%u)", deviceInternal->maxDrawStates);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Find first invalid slot
   VEDrawStateInternal *drawState = nullptr;
   for (uint32_t i = 0; i < deviceInternal->maxDrawStates; i++)
   {
      if (!deviceInternal->drawStates[i].isValid)
      {
         drawState = &deviceInternal->drawStates[i];
         break;
      }
   }

   if (!drawState)
   {
      veSetError("veCreateDrawState: No free draw state slots");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Initialize draw state
   memset(drawState, 0, sizeof(VEDrawStateInternal));

   drawState->topology = desc->topology;
   drawState->primitiveRestartEnable = desc->primitiveRestartEnable;
   drawState->patchControlPoints = desc->patchControlPoints;

   drawState->polygonMode = desc->polygonMode;
   drawState->lineWidth = desc->lineWidth;
   drawState->cullMode = desc->cullMode;
   drawState->frontFace = desc->frontFace;
   drawState->rasterizerDiscardEnable = desc->rasterizerDiscardEnable;

   drawState->depthBiasEnable = desc->depthBiasEnable;
   drawState->depthBiasConstantFactor = desc->depthBiasConstantFactor;
   drawState->depthBiasClamp = desc->depthBiasClamp;
   drawState->depthBiasSlopeFactor = desc->depthBiasSlopeFactor;
   drawState->depthClampEnable = desc->depthClampEnable;

   drawState->alphaToCoverageEnable = desc->alphaToCoverageEnable;
   drawState->alphaToOneEnable = desc->alphaToOneEnable;

   if (desc->debugName)
   {
      strncpy(drawState->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
   }

   drawState->isValid = true;
   drawState->device = deviceInternal;
   deviceInternal->drawStateCount++;

   *outDrawState = reinterpret_cast<VEDrawState *>(drawState);
   return VE_SUCCESS;
}

VEResult veDestroyDrawState(VEDrawState *drawState)
{
   if (!drawState)
   {
      return VE_SUCCESS; // NULL is valid (no-op)
   }

   VEDrawStateInternal *drawStateInternal = reinterpret_cast<VEDrawStateInternal *>(drawState);

   if (!drawStateInternal->isValid)
   {
      veSetError("veDestroyDrawState: Draw state already destroyed");
      return VE_ERROR_INVALID_PARAMETER;
   }

   drawStateInternal->isValid = false;
   drawStateInternal->device->drawStateCount--;

   return VE_SUCCESS;
}

// =============================================================================
// Convenience Draw State Creators
// =============================================================================

VEResult veCreateDefaultDrawState(VEDevice *device, VEDrawState **outDrawState)
{
   VEDrawStateDesc desc = veDefaultDrawStateDesc();
   desc.debugName = "DefaultDrawState";
   return veCreateDrawState(device, &desc, outDrawState);
}

VEResult veCreateWireframeDrawState(VEDevice *device, VEDrawState **outDrawState)
{
   VEDrawStateDesc desc = veDefaultDrawStateDesc();
   desc.polygonMode = VK_POLYGON_MODE_LINE;
   desc.cullMode = VK_CULL_MODE_NONE;
   desc.debugName = "WireframeDrawState";
   return veCreateDrawState(device, &desc, outDrawState);
}

VEResult veCreateShadowDrawState(VEDevice *device, VEDrawState **outDrawState)
{
   VEDrawStateDesc desc = veDefaultDrawStateDesc();
   desc.depthBiasEnable = true;
   desc.depthBiasConstantFactor = 1.25f;
   desc.depthBiasClamp = 0.0f;
   desc.depthBiasSlopeFactor = 1.75f;
   desc.cullMode = VK_CULL_MODE_FRONT_BIT; // Front face culling for shadow mapping
   desc.debugName = "ShadowDrawState";
   return veCreateDrawState(device, &desc, outDrawState);
}
