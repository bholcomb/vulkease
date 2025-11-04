/**
 * @file ve_command.c
 * @brief Command Buffer and Rendering Implementation
 */

#include "ve_internal.h"

#include <mutex>

// =============================================================================
// Command Buffer Operations
// =============================================================================

VECommandBuffer *veBeginCommandBuffer(VEDevice *device)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return NULL;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   VECommandBufferInternal *cmd;
   VEResult result = veAllocateCommandBuffer(deviceInternal, deviceInternal->graphicsCommandPool, &cmd);
   if (result != VE_SUCCESS)
   {
      return NULL;
   }

   VkCommandBufferBeginInfo beginInfo{};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   VkResult vkResult = vkBeginCommandBuffer(cmd->commandBuffer, &beginInfo);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to begin command buffer (VkResult: %d)", vkResult);
      veFreeCommandBuffer(cmd);
      return NULL;
   }

   cmd->isRecording = true;
   cmd->isOneTime = true;

   return (VECommandBuffer *)cmd;
}

VEResult veSubmitCommandBuffer(VECommandBuffer *cmd, bool waitForCompletion)
{
   if (!cmd)
   {
      veSetError("Command buffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   if (!internal->isRecording)
   {
      veSetError("Command buffer is not recording");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkResult result = vkEndCommandBuffer(internal->commandBuffer);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to end command buffer (VkResult: %d)", result);
      return VE_ERROR_UNKNOWN;
   }

   internal->isRecording = false;

   // Submit command buffer using stored device reference
   VkSubmitInfo submitInfo{};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &internal->commandBuffer;

   VkFence fence = internal->inFlightFence;
   if (fence == VK_NULL_HANDLE)
   {
      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      VkResult fenceResult = vkCreateFence(internal->device->device, &fenceInfo, NULL, &fence);
      if (fenceResult != VK_SUCCESS)
      {
         veSetError("Failed to create command buffer fence (VkResult: %d)", fenceResult);
         return VE_ERROR_OUT_OF_MEMORY;
      }
      internal->inFlightFence = fence;
   }

   result = vkResetFences(internal->device->device, 1, &fence);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to reset command buffer fence (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VEDeviceQueueLocks *locks = internal->device->queueLocks.get();
   if (!locks)
   {
      veSetError("Device queue locks not initialized");
      return VE_ERROR_INVALID_PARAMETER;
   }

   {
      std::lock_guard<std::mutex> lock(locks->graphicsMutex());
      result = vkQueueSubmit(internal->device->graphicsQueue, 1, &submitInfo, fence);
   }
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to submit command buffer (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   if (waitForCompletion)
   {
      result = vkWaitForFences(internal->device->device, 1, &fence, VK_TRUE, UINT64_MAX);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to wait for command buffer completion (VkResult: %d)", result);
         return VE_ERROR_OUT_OF_MEMORY;
      }

      internal->clearFenceTracking();
      veFreeCommandBuffer(internal);
   }
   else
   {
      internal->markFenceActive(fence);
   }

   return VE_SUCCESS;
}

// =============================================================================
// Dynamic Rendering
// =============================================================================

void veBeginRendering(VECommandBuffer *cmd, const VERenderingInfo *renderingInfo)
{
   if (!cmd || !renderingInfo)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // Convert to Vulkan rendering info
   VkRenderingInfo vkRenderingInfo{};
   vkRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
   vkRenderingInfo.renderArea.offset.x = renderingInfo->renderAreaX;
   vkRenderingInfo.renderArea.offset.y = renderingInfo->renderAreaY;
   vkRenderingInfo.renderArea.extent.width = renderingInfo->renderAreaWidth;
   vkRenderingInfo.renderArea.extent.height = renderingInfo->renderAreaHeight;
   vkRenderingInfo.layerCount = 1;

   // Convert color attachments
   VkRenderingAttachmentInfo colorAttachments[8] = {};
   for (uint32_t i = 0; i < renderingInfo->colorAttachmentCount && i < 8; i++)
   {
      colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      // Get image view from texture index
      VkImageView imageView = internal->device->getImageViewFromTexture(renderingInfo->colorAttachments[i].texture);
      if (imageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for color attachment %u", renderingInfo->colorAttachments[i].texture, i);
         return;
      }
      colorAttachments[i].imageView = imageView;

      colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      colorAttachments[i].loadOp = (VkAttachmentLoadOp)renderingInfo->colorAttachments[i].loadOp;
      colorAttachments[i].storeOp = (VkAttachmentStoreOp)renderingInfo->colorAttachments[i].storeOp;
      colorAttachments[i].clearValue.color.float32[0] = renderingInfo->colorAttachments[i].clearValue.r;
      colorAttachments[i].clearValue.color.float32[1] = renderingInfo->colorAttachments[i].clearValue.g;
      colorAttachments[i].clearValue.color.float32[2] = renderingInfo->colorAttachments[i].clearValue.b;
      colorAttachments[i].clearValue.color.float32[3] = renderingInfo->colorAttachments[i].clearValue.a;

      // Handle resolve attachment if present
      if (renderingInfo->colorAttachments[i].resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         VkImageView resolveImageView =
             internal->device->getImageViewFromTexture(renderingInfo->colorAttachments[i].resolveTexture);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for color attachment %u",
                       renderingInfo->colorAttachments[i].resolveTexture, i);
            return;
         }
         colorAttachments[i].resolveImageView = resolveImageView;
         colorAttachments[i].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
      }
   }

   vkRenderingInfo.colorAttachmentCount = renderingInfo->colorAttachmentCount;
   vkRenderingInfo.pColorAttachments = colorAttachments;

   // Convert depth attachment
   VkRenderingAttachmentInfo depthAttachment{};
   if (renderingInfo->depthAttachment)
   {
      depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      // Get image view from texture index
      VkImageView depthImageView = internal->device->getImageViewFromTexture(renderingInfo->depthAttachment->texture);
      if (depthImageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for depth attachment", renderingInfo->depthAttachment->texture);
         return;
      }
      depthAttachment.imageView = depthImageView;

      depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthAttachment.loadOp = (VkAttachmentLoadOp)renderingInfo->depthAttachment->loadOp;
      depthAttachment.storeOp = (VkAttachmentStoreOp)renderingInfo->depthAttachment->storeOp;
      depthAttachment.clearValue.depthStencil.depth = 1.0f;
      depthAttachment.clearValue.depthStencil.stencil = 0;

      // Handle resolve attachment if present
      if (renderingInfo->depthAttachment->resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         VkImageView resolveImageView =
             internal->device->getImageViewFromTexture(renderingInfo->depthAttachment->resolveTexture);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for depth attachment",
                       renderingInfo->depthAttachment->resolveTexture);
            return;
         }
         depthAttachment.resolveImageView = resolveImageView;
         depthAttachment.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
      }

      vkRenderingInfo.pDepthAttachment = &depthAttachment;
   }

   // Convert stencil attachment
   VkRenderingAttachmentInfo stencilAttachment{};
   if (renderingInfo->stencilAttachment)
   {
      stencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      // Get image view from texture index
      VkImageView stencilImageView =
          internal->device->getImageViewFromTexture(renderingInfo->stencilAttachment->texture);
      if (stencilImageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for stencil attachment", renderingInfo->stencilAttachment->texture);
         return;
      }
      stencilAttachment.imageView = stencilImageView;

      stencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      stencilAttachment.loadOp = (VkAttachmentLoadOp)renderingInfo->stencilAttachment->loadOp;
      stencilAttachment.storeOp = (VkAttachmentStoreOp)renderingInfo->stencilAttachment->storeOp;
      stencilAttachment.clearValue.depthStencil.depth = 1.0f;
      stencilAttachment.clearValue.depthStencil.stencil = 0;

      // Handle resolve attachment if present
      if (renderingInfo->stencilAttachment->resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         VkImageView resolveImageView =
             internal->device->getImageViewFromTexture(renderingInfo->stencilAttachment->resolveTexture);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for stencil attachment",
                       renderingInfo->stencilAttachment->resolveTexture);
            return;
         }
         stencilAttachment.resolveImageView = resolveImageView;
         stencilAttachment.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
      }

      vkRenderingInfo.pStencilAttachment = &stencilAttachment;
   }

   vkCmdBeginRendering(internal->commandBuffer, &vkRenderingInfo);

   // Bind the descriptors once per begin rendering call
   VkDescriptorSet descriptorSets[2] = {internal->device->textureDescriptorSet, internal->device->samplerDescriptorSet};

   vkCmdBindDescriptorSets(internal->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           internal->device->globalGraphicsPipelineLayout,
                           0,              // first set
                           2,              // set count
                           descriptorSets, // sets
                           0, NULL);       // dynamic offsset
  if (internal->device)
  {
     internal->device->frameStats.descriptorBinds += 1;
  }
}

void veEndRendering(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdEndRendering(internal->commandBuffer);
}

// =============================================================================
// Render Configuration Application
// =============================================================================

void veApplyRenderConfig(VECommandBuffer *cmd, VERenderConfig *config)
{
   if (!cmd || !config)
      return;

   VERenderConfigInternal *configInternal = (VERenderConfigInternal *)config;
   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkCommandBuffer vkCmd = internal->commandBuffer;

   // ==========================================================================
   // REQUIRED DYNAMIC STATE FOR SHADER OBJECTS (Vulkan 1.3 Core)
   // ==========================================================================

   // 1. VIEWPORT AND SCISSOR (REQUIRED - must use WithCount variants)
   // This is MANDATORY when shader objects are bound
   if (configInternal->configTypes & VE_CONFIG_TYPE_VIEWPORT)
   {
      VkViewport viewport{};
      viewport.x = configInternal->viewport.x;
      viewport.y = configInternal->viewport.y;
      viewport.width = configInternal->viewport.width;
      viewport.height = configInternal->viewport.height;
      viewport.minDepth = configInternal->viewport.minDepth;
      viewport.maxDepth = configInternal->viewport.maxDepth;
      vkCmdSetViewportWithCount(vkCmd, 1, &viewport);
   }

   if (configInternal->configTypes & VE_CONFIG_TYPE_SCISSOR)
   {
      VkRect2D scissor{};
      scissor.offset.x = configInternal->scissor.x;
      scissor.offset.y = configInternal->scissor.y;
      scissor.extent.width = configInternal->scissor.width;
      scissor.extent.height = configInternal->scissor.height;

      vkCmdSetScissorWithCount(vkCmd, 1, &scissor);
   }

   // 2. PRIMITIVE TOPOLOGY (REQUIRED)
   VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      topology = configInternal->vertexInputConfig.topology;
   }
   vkCmdSetPrimitiveTopology(vkCmd, topology);
   internal->currentTopology = topology;

   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      internal->currentPatchControlPoints = configInternal->vertexInputConfig.patchControlPoints;
   }
   else
   {
      internal->currentPatchControlPoints = 0;
   }

   // 3. PRIMITIVE RESTART (REQUIRED)
   VkBool32 primitiveRestart = VK_FALSE;
   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      primitiveRestart = configInternal->vertexInputConfig.primitiveRestartEnable ? VK_TRUE : VK_FALSE;
   }
   vkCmdSetPrimitiveRestartEnable(vkCmd, primitiveRestart);

   // ==========================================================================
   // RASTERIZATION STATE (Core + VK_EXT_extended_dynamic_state3)
   // ==========================================================================

   if (configInternal->configTypes & VE_CONFIG_TYPE_RASTERIZATION)
   {
      const VERasterConfig *raster = &configInternal->rasterConfig;

      // CORE DYNAMIC STATE (Vulkan 1.3)
      vkCmdSetCullMode(vkCmd, raster->cullMode);
      vkCmdSetFrontFace(vkCmd, raster->frontFace);
      vkCmdSetLineWidth(vkCmd, raster->lineWidth);
      vkCmdSetRasterizerDiscardEnable(vkCmd, raster->rasterizerDiscardEnable ? VK_TRUE : VK_FALSE);
      vkCmdSetDepthBiasEnable(vkCmd, raster->depthBiasEnable ? VK_TRUE : VK_FALSE);

      if (raster->depthBiasEnable)
      {
         vkCmdSetDepthBias(vkCmd, raster->depthBiasConstantFactor, raster->depthBiasClamp,
                           raster->depthBiasSlopeFactor);
      }

      // EXTENDED DYNAMIC STATE 3 (when available)
      // Polygon mode (VK_EXT_extended_dynamic_state3)
      veFuncs.vkCmdSetPolygonModeEXT(vkCmd, raster->polygonMode);

      // Depth clamp (VK_EXT_extended_dynamic_state3)
      veFuncs.vkCmdSetDepthClampEnableEXT(vkCmd, raster->depthClampEnable ? VK_TRUE : VK_FALSE);
   }
   else
   {
      // Set safe defaults when raster config not provided
      vkCmdSetCullMode(vkCmd, VK_CULL_MODE_BACK_BIT);
      vkCmdSetFrontFace(vkCmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
      vkCmdSetLineWidth(vkCmd, 1.0f);
      vkCmdSetRasterizerDiscardEnable(vkCmd, VK_FALSE);
      vkCmdSetDepthBiasEnable(vkCmd, VK_FALSE);

      veFuncs.vkCmdSetPolygonModeEXT(vkCmd, VK_POLYGON_MODE_FILL);
      veFuncs.vkCmdSetDepthClampEnableEXT(vkCmd, VK_FALSE);
   }

   // ==========================================================================
   // DEPTH/STENCIL STATE (Vulkan 1.3 Core)
   // ==========================================================================
   if (configInternal->configTypes & VE_CONFIG_TYPE_DEPTH_STENCIL)
   {
      const VEDepthConfig *depth = &configInternal->depthConfig;
      // Depth test state
      vkCmdSetDepthTestEnable(internal->commandBuffer, depth->depthTestEnable);
      vkCmdSetDepthWriteEnable(internal->commandBuffer, depth->depthWriteEnable);
      vkCmdSetDepthCompareOp(internal->commandBuffer, (VkCompareOp)depth->depthCompareOp);

      // Depth bounds test
      vkCmdSetDepthBoundsTestEnable(vkCmd, depth->depthBoundsTestEnable ? VK_TRUE : VK_FALSE);
      if (depth->depthBoundsTestEnable)
      {
         vkCmdSetDepthBounds(internal->commandBuffer, depth->minDepthBounds, depth->maxDepthBounds);
      }

      // Stencil test state
      vkCmdSetStencilTestEnable(vkCmd, depth->stencilTestEnable ? VK_TRUE : VK_FALSE);

      if (depth->stencilTestEnable)
      {
         // Front face stencil operations
         vkCmdSetStencilOp(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontFailOp, depth->frontPassOp,
                           depth->frontDepthFailOp, depth->frontCompareOp);

         vkCmdSetStencilCompareMask(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontCompareMask);
         vkCmdSetStencilWriteMask(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontWriteMask);
         vkCmdSetStencilReference(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontReference);

         // Back face stencil operations
         vkCmdSetStencilOp(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backFailOp, depth->backPassOp,
                           depth->backDepthFailOp, depth->backCompareOp);

         vkCmdSetStencilCompareMask(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backCompareMask);
         vkCmdSetStencilWriteMask(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backWriteMask);
         vkCmdSetStencilReference(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backReference);
      }
   }
   else
   {
      // Set safe defaults when depth config not provided
      vkCmdSetDepthTestEnable(vkCmd, VK_TRUE);
      vkCmdSetDepthWriteEnable(vkCmd, VK_TRUE);
      vkCmdSetDepthCompareOp(vkCmd, VK_COMPARE_OP_LESS);
      vkCmdSetDepthBoundsTestEnable(vkCmd, VK_FALSE);
      vkCmdSetStencilTestEnable(vkCmd, VK_FALSE);
   }

   // ==========================================================================
   // COLOR BLENDING STATE (VK_EXT_extended_dynamic_state3)
   // ==========================================================================

   uint32_t colorAttachmentCount = 1; // Default assumption
   if (configInternal->configTypes & VE_CONFIG_TYPE_COLOR_BLEND)
   {
      const VEBlendConfig *blend = &configInternal->blendConfig;
      colorAttachmentCount = blend->attachmentCount;

      // Set blend equations for ALL attachments at once
      // Per-attachment blend enables and color write masks
      VkBool32 blendEnables[VE_MAX_COLOR_ATTACHMENTS];
      VkColorComponentFlags colorWriteMasks[VE_MAX_COLOR_ATTACHMENTS];
      VkColorBlendEquationEXT blendEquations[VE_MAX_COLOR_ATTACHMENTS];

      for (uint32_t i = 0; i < colorAttachmentCount; i++)
      {
         blendEnables[i] = blend->attachments[i].blendEnable ? VK_TRUE : VK_FALSE;
         colorWriteMasks[i] = blend->attachments[i].colorWriteMask;

         // Set blend equation for this attachment (even if blending is disabled)
         VkColorBlendEquationEXT equation{};
         equation.srcColorBlendFactor = blend->attachments[i].srcColorBlendFactor;
         equation.dstColorBlendFactor = blend->attachments[i].dstColorBlendFactor;
         equation.colorBlendOp = blend->attachments[i].colorBlendOp;
         equation.srcAlphaBlendFactor = blend->attachments[i].srcAlphaBlendFactor;
         equation.dstAlphaBlendFactor = blend->attachments[i].dstAlphaBlendFactor;
         equation.alphaBlendOp = blend->attachments[i].alphaBlendOp;
         blendEquations[i] = equation;
      }

      // Set all blend state at once
      veFuncs.vkCmdSetColorBlendEnableEXT(vkCmd, 0, colorAttachmentCount, blendEnables);
      veFuncs.vkCmdSetColorWriteMaskEXT(vkCmd, 0, colorAttachmentCount, colorWriteMasks);
      veFuncs.vkCmdSetColorBlendEquationEXT(vkCmd, 0, colorAttachmentCount, blendEquations);

      // Logic operations (if supported)
      veFuncs.vkCmdSetLogicOpEnableEXT(vkCmd, blend->logicOpEnable ? VK_TRUE : VK_FALSE);
      if (blend->logicOpEnable)
      {
         veFuncs.vkCmdSetLogicOpEXT(vkCmd, blend->logicOp);
      }

      // Blend constants (always available in core)
      vkCmdSetBlendConstants(vkCmd, blend->blendConstants);
   }
   else
   {
      // Set safe defaults for color blending
      VkBool32 blendEnable = VK_FALSE;
      VkColorComponentFlags colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

      veFuncs.vkCmdSetColorBlendEnableEXT(vkCmd, 0, 1, &blendEnable);
      veFuncs.vkCmdSetColorWriteMaskEXT(vkCmd, 0, 1, &colorWriteMask);
      veFuncs.vkCmdSetLogicOpEnableEXT(vkCmd, VK_FALSE);

      // Default blend constants
      float blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      vkCmdSetBlendConstants(vkCmd, blendConstants);
   }

   // ==========================================================================
   // VERTEX INPUT STATE (VK_EXT_shader_object)
   // ==========================================================================

   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      const VEVertexInputConfig *vertexInput = &configInternal->vertexInputConfig;

      // Convert VulkEase vertex input to Vulkan EXT format
      VkVertexInputBindingDescription2EXT bindings[VE_MAX_VERTEX_BINDINGS];
      VkVertexInputAttributeDescription2EXT attributes[VE_MAX_VERTEX_ATTRIBUTES];

      // Convert bindings
      for (uint32_t i = 0; i < vertexInput->bindingCount; i++)
      {
         VkVertexInputBindingDescription2EXT binding{};
         binding.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
         binding.binding = vertexInput->bindings[i].binding;
         binding.stride = vertexInput->bindings[i].stride;
         binding.inputRate = vertexInput->bindings[i].inputRate;
         binding.divisor = vertexInput->bindings[i].divisor;
         bindings[i] = binding;
      }

      // Convert attributes
      for (uint32_t i = 0; i < vertexInput->attributeCount; i++)
      {
         VkVertexInputAttributeDescription2EXT attribute{};
         attribute.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
         attribute.location = vertexInput->attributes[i].location;
         attribute.binding = vertexInput->attributes[i].binding;
         attribute.format = vertexInput->attributes[i].format;
         attribute.offset = vertexInput->attributes[i].offset;
         attributes[i] = attribute;
      }

      // Set vertex input layout dynamically
      veFuncs.vkCmdSetVertexInputEXT(vkCmd, vertexInput->bindingCount, bindings, vertexInput->attributeCount,
                                     attributes);
   }
   else
   {
      // No vertex input when not specified (e.g., fullscreen triangle)
      veFuncs.vkCmdSetVertexInputEXT(vkCmd, 0, NULL, 0, NULL);
   }

   // ==========================================================================
   // MULTISAMPLE STATE (VK_EXT_extended_dynamic_state3)
   // ==========================================================================

   if (configInternal->configTypes & VE_CONFIG_TYPE_MULTISAMPLE)
   {
      const VEMultisampleConfig *msaa = &configInternal->multisampleConfig;

      VkSampleCountFlagBits sampleCountBits = static_cast<VkSampleCountFlagBits>(msaa->rasterizationSamples);
      veFuncs.vkCmdSetRasterizationSamplesEXT(vkCmd, sampleCountBits);
      veFuncs.vkCmdSetAlphaToCoverageEnableEXT(vkCmd, msaa->alphaToCoverageEnable ? VK_TRUE : VK_FALSE);
      veFuncs.vkCmdSetAlphaToOneEnableEXT(vkCmd, msaa->alphaToOneEnable ? VK_TRUE : VK_FALSE);

      // Generate default sample mask (all samples enabled)
      uint32_t sampleCount = static_cast<uint32_t>(msaa->rasterizationSamples);
      uint32_t maskWords = (sampleCount + 31) / 32; // Calculate number of 32-bit words needed
      VkSampleMask sampleMask[16];                  // Maximum reasonable sample count (512 samples =
                                                    // 16 words)

      for (uint32_t i = 0; i < maskWords; i++)
      {
         sampleMask[i] = 0xFFFFFFFF; // All bits set = all samples enabled
      }

      veFuncs.vkCmdSetSampleMaskEXT(vkCmd, sampleCountBits, sampleMask);
   }
   else
   {
      // Default MSAA settings
      veFuncs.vkCmdSetRasterizationSamplesEXT(vkCmd, VK_SAMPLE_COUNT_1_BIT);
      veFuncs.vkCmdSetAlphaToCoverageEnableEXT(vkCmd, VK_FALSE);
      veFuncs.vkCmdSetAlphaToOneEnableEXT(vkCmd, VK_FALSE);

      VkSampleMask sampleMask = 0xFFFFFFFF;
      veFuncs.vkCmdSetSampleMaskEXT(vkCmd, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
   }

   // ==========================================================================
   // TESSELLATION STATE (VK_EXT_extended_dynamic_state2/3)
   // ==========================================================================

   if (topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
   {
      uint32_t patchControlPoints = 3; // Default
      if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
      {
         patchControlPoints = configInternal->vertexInputConfig.patchControlPoints;
      }

      veFuncs.vkCmdSetPatchControlPointsEXT(vkCmd, patchControlPoints);
   }

   // ==========================================================================
   // ADDITIONAL REQUIRED STATE
   // ==========================================================================

   // Conservative rasterization (VK_EXT_extended_dynamic_state3)
   veFuncs.vkCmdSetConservativeRasterizationModeEXT(vkCmd, VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT);

   // Line rasterization (VK_EXT_extended_dynamic_state3)
   veFuncs.vkCmdSetLineRasterizationModeEXT(vkCmd, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT);

   veFuncs.vkCmdSetProvokingVertexModeEXT(vkCmd, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);
}

// =============================================================================
// Shader Binding
// =============================================================================

void veBindShader(VECommandBuffer *cmd, VEShader *shader)
{
   if (!cmd || !shader)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEShaderInternal *shaderInternal = (VEShaderInternal *)shader;

   internal->boundShaders = internal->boundShaders | shaderInternal->stage;

   VkShaderStageFlagBits stageBit = static_cast<VkShaderStageFlagBits>(shaderInternal->stage);

   if (internal->device)
   {
      internal->device->frameStats.pipelineBinds += 1;
   }
   veFuncs.vkCmdBindShadersEXT(internal->commandBuffer, 1, &stageBit, &shaderInternal->shaderObject);
}

void veBindShaders(VECommandBuffer *cmd, uint32_t shaderCount, VEShader *const *shaders)
{
   if (!cmd || shaderCount == 0 || !shaders)
      return;

   VkShaderStageFlagBits stageBits[16];
   VkShaderEXT shaderObjects[16];
   uint32_t validCount = 0;

   for (uint32_t i = 0; i < shaderCount && i < 16; i++)
   {
      if (shaders[i])
      {
         VEShaderInternal *shaderInternal = (VEShaderInternal *)shaders[i];
         stageBits[validCount] = static_cast<VkShaderStageFlagBits>(shaderInternal->stage);
         shaderObjects[validCount] = shaderInternal->shaderObject;
         validCount++;
      }
   }

   if (validCount > 0)
   {
      VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
      if (internal->device)
      {
         internal->device->frameStats.pipelineBinds += validCount;
      }
      veFuncs.vkCmdBindShadersEXT(internal->commandBuffer, validCount, stageBits, shaderObjects);
   }
}

void veBindShaderConfig(VECommandBuffer *cmd, VEShaderConfig *config)
{
   if (!cmd || !config)
      return;

   VEShaderConfigInternal *configInternal = (VEShaderConfigInternal *)config;
   VECommandBufferInternal *cmdInternal = (VECommandBufferInternal *)cmd;

   VEShader *shaders[6];
   uint32_t shaderCount = 0;

   if (configInternal->vertexShader)
   {
      shaders[shaderCount++] = configInternal->vertexShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_VERTEX_BIT;
   }
   if (configInternal->fragmentShader)
   {
      shaders[shaderCount++] = configInternal->fragmentShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_FRAGMENT_BIT;
   }
   if (configInternal->geometryShader)
   {
      shaders[shaderCount++] = configInternal->geometryShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_GEOMETRY_BIT;
   }
   if (configInternal->tessControlShader)
   {
      shaders[shaderCount++] = configInternal->tessControlShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
   }
   if (configInternal->tessEvalShader)
   {
      shaders[shaderCount++] = configInternal->tessEvalShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
   }
   if (configInternal->computeShader)
   {
      shaders[shaderCount++] = configInternal->computeShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_COMPUTE_BIT;
   }

   if (shaderCount > 0)
   {
      veBindShaders(cmd, shaderCount, shaders);
   }
}

void veUnbindShaderStage(VECommandBuffer *cmd, VkShaderStageFlags stage)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkShaderEXT nullShader = VK_NULL_HANDLE;
   uint32_t remainingBits = static_cast<uint32_t>(stage);

   while (remainingBits)
   {
      uint32_t lowestBit = remainingBits & (~(remainingBits - 1u));
      VkShaderStageFlagBits stageBit = static_cast<VkShaderStageFlagBits>(lowestBit);
      veFuncs.vkCmdBindShadersEXT(internal->commandBuffer, 1, &stageBit, &nullShader);
      remainingBits &= ~lowestBit;
   }
}