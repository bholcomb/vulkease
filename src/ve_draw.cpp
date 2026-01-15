/**
 * @file ve_draw.c
 * @brief Drawing and State Management Implementation
 */

#include "ve_internal.h"

// =============================================================================
// Viewport and Scissor Management
// =============================================================================

VEResult veSetViewport(VECommandBuffer *cmd, float x, float y, float width, float height, float minDepth,
                       float maxDepth)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkViewport viewport{};
   viewport.x = x;
   viewport.y = y;
   viewport.width = width;
   viewport.height = height;
   viewport.minDepth = minDepth;
   viewport.maxDepth = maxDepth;

   vkCmdSetViewportWithCount(internal->commandBuffer, 1, &viewport);
   return VE_SUCCESS;
}

VEResult veSetScissor(VECommandBuffer *cmd, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkRect2D scissor{};
   scissor.offset.x = x;
   scissor.offset.y = y;
   scissor.extent.width = width;
   scissor.extent.height = height;

   vkCmdSetScissorWithCount(internal->commandBuffer, 1, &scissor);
   return VE_SUCCESS;
}

// =============================================================================
// Combined Render State
// =============================================================================

VEResult veApplyRenderState(VECommandBuffer *cmd, const VERenderState *state)
{
   if (!cmd || !state)
   {
      veSetError("Invalid parameters for veApplyRenderState");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // Validate we're inside a render pass (required for default viewport/scissor)
   if (!internal->inRenderPass && (state->viewport == NULL || state->scissor == NULL))
   {
      veSetError("veApplyRenderState: must be called inside veBeginRendering when viewport or scissor is NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Bind shader config if provided
   if (state->shaderConfig)
   {
      VEResult r = veBindShaderConfig(cmd, state->shaderConfig);
      if (r != VE_SUCCESS)
         return r;
   }

   // Apply render config if provided
   if (state->renderConfig)
   {
      VEResult r = veApplyRenderConfig(cmd, state->renderConfig);
      if (r != VE_SUCCESS)
         return r;
   }

   // Set viewport (use render area if NULL)
   if (state->viewport)
   {
      VEResult r = veSetViewport(cmd, state->viewport->x, state->viewport->y, state->viewport->width,
                                 state->viewport->height, state->viewport->minDepth, state->viewport->maxDepth);
      if (r != VE_SUCCESS)
         return r;
   }
   else
   {
      VEResult r = veSetViewport(cmd, static_cast<float>(internal->renderAreaX), static_cast<float>(internal->renderAreaY),
                                 static_cast<float>(internal->renderAreaWidth), static_cast<float>(internal->renderAreaHeight), 0.0f, 1.0f);
      if (r != VE_SUCCESS)
         return r;
   }

   // Set scissor (use render area if NULL)
   if (state->scissor)
   {
      VEResult r = veSetScissor(cmd, state->scissor->x, state->scissor->y, state->scissor->width,
                                state->scissor->height);
      if (r != VE_SUCCESS)
         return r;
   }
   else
   {
      VEResult r = veSetScissor(cmd, static_cast<int32_t>(internal->renderAreaX), static_cast<int32_t>(internal->renderAreaY),
                                internal->renderAreaWidth, internal->renderAreaHeight);
      if (r != VE_SUCCESS)
         return r;
   }

   return VE_SUCCESS;
}

// =============================================================================
// Runtime State Overrides
// =============================================================================

VEResult veOverrideRasterState(VECommandBuffer *cmd, const VERasterConfig *raster)
{
   if (!cmd || !raster)
   {
      veSetError("Invalid parameters for veOverrideRasterState");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   vkCmdSetCullMode(internal->commandBuffer, (VkCullModeFlags)raster->cullMode);
   vkCmdSetFrontFace(internal->commandBuffer, (VkFrontFace)raster->frontFace);
   vkCmdSetLineWidth(internal->commandBuffer, raster->lineWidth);

   if (raster->depthBiasEnable)
   {
      vkCmdSetDepthBias(internal->commandBuffer, raster->depthBiasConstantFactor, raster->depthBiasClamp,
                        raster->depthBiasSlopeFactor);
   }

   // Extended dynamic state 3
   veFuncs.vkCmdSetPolygonModeEXT(internal->commandBuffer, (VkPolygonMode)raster->polygonMode);
   return VE_SUCCESS;
}

VEResult veOverrideDepthState(VECommandBuffer *cmd, const VEDepthConfig *depth)
{
   if (!cmd || !depth)
   {
      veSetError("Invalid parameters for veOverrideDepthState");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   vkCmdSetDepthTestEnable(internal->commandBuffer, depth->depthTestEnable);
   vkCmdSetDepthWriteEnable(internal->commandBuffer, depth->depthWriteEnable);
   vkCmdSetDepthCompareOp(internal->commandBuffer, (VkCompareOp)depth->depthCompareOp);

   vkCmdSetStencilTestEnable(internal->commandBuffer, depth->stencilTestEnable);

   if (depth->stencilTestEnable)
   {
      vkCmdSetStencilCompareMask(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, depth->frontCompareMask);
      vkCmdSetStencilCompareMask(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, depth->backCompareMask);

      vkCmdSetStencilWriteMask(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, depth->frontWriteMask);
      vkCmdSetStencilWriteMask(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, depth->backWriteMask);

      vkCmdSetStencilReference(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, depth->frontReference);
      vkCmdSetStencilReference(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, depth->backReference);
   }

   if (depth->depthBoundsTestEnable)
   {
      vkCmdSetDepthBounds(internal->commandBuffer, depth->minDepthBounds, depth->maxDepthBounds);
   }
   return VE_SUCCESS;
}

VEResult veOverrideBlendState(VECommandBuffer *cmd, const VEBlendConfig *blend)
{
   if (!cmd || !blend)
   {
      veSetError("Invalid parameters for veOverrideBlendState");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   vkCmdSetBlendConstants(internal->commandBuffer, blend->blendConstants);

   if (blend->attachmentCount > 0)
   {
      VkBool32 colorBlendEnables[8];
      for (uint32_t i = 0; i < blend->attachmentCount && i < 8; i++)
      {
         colorBlendEnables[i] = blend->attachments[i].blendEnable;
      }
      veFuncs.vkCmdSetColorBlendEnableEXT(internal->commandBuffer, 0, blend->attachmentCount, colorBlendEnables);
   }
   return VE_SUCCESS;
}

// =============================================================================
// Quick State Toggles
// =============================================================================

VEResult veSetWireframe(VECommandBuffer *cmd, bool enabled)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkPolygonMode mode = enabled ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
   veFuncs.vkCmdSetPolygonModeEXT(internal->commandBuffer, mode);
   return VE_SUCCESS;
}

VEResult veSetAlphaBlending(VECommandBuffer *cmd, bool enabled)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkBool32 blendEnable = enabled ? VK_TRUE : VK_FALSE;
   veFuncs.vkCmdSetColorBlendEnableEXT(internal->commandBuffer, 0, 1, &blendEnable);
   return VE_SUCCESS;
}

VEResult veSetDepthTesting(VECommandBuffer *cmd, bool testEnabled, bool writeEnabled)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   vkCmdSetDepthTestEnable(internal->commandBuffer, testEnabled);
   vkCmdSetDepthWriteEnable(internal->commandBuffer, writeEnabled);
   return VE_SUCCESS;
}

VEResult veSetCulling(VECommandBuffer *cmd, VkCullModeFlags cullMode)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetCullMode(internal->commandBuffer, cullMode);
   return VE_SUCCESS;
}

// =============================================================================
// Push Constants
// =============================================================================

// Helper function to determine which shader stages are bound
static VkShaderStageFlags veGetBoundShaderStages(VECommandBufferInternal *cmd)
{
   VkShaderStageFlags stages = 0;

   if (cmd->boundShaders & VK_SHADER_STAGE_COMPUTE_BIT)
      stages = VK_SHADER_STAGE_COMPUTE_BIT;

   // Check which shader objects are currently bound
   // This would be tracked when veBindShaderConfig is called
   if (cmd->boundShaders & VK_SHADER_STAGE_ALL_GRAPHICS)
      stages = VK_SHADER_STAGE_ALL_GRAPHICS;

   return stages;
}

VEResult vePushConstants(VECommandBuffer *cmd, const void *data, uint64_t size, uint64_t offset)
{
   if (!cmd || !data || size == 0)
   {
      veSetError("Invalid parameters for vePushConstants");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // Alignment and bounds checks required by spec
   if ((offset & 3u) || (size & 3u))
   {
      veSetError("Push constants offset/size must be multiples of 4 (offset=%zu "
                 "size=%zu)",
                 offset, size);
      return VE_ERROR_INVALID_PARAMETER; // 4-byte alignment is required
   }

   // Simple bounds check for VulkEase's standard push constant limit
   if (offset + size > VE_MAX_PUSH_CONSTANT_BYTES)
   { // Standard guaranteed minimum
      veSetError("Push constant size exceeds limit (max 256 bytes, requested %zu+%zu)", offset, size);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // With VK_EXT_shader_object: Push constants are specified per-shader, not
   // per-pipeline We need to track which shader stages are currently bound and
   // determine stage flags
   VkShaderStageFlags stageFlags = veGetBoundShaderStages(internal);

   if (stageFlags == 0)
   {
      veSetError("No shader objects bound - cannot push constants");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // find the device's global push constant layout (created at device init)
   VkPipelineLayout pushConstantLayout;
   if (stageFlags == VK_SHADER_STAGE_ALL_GRAPHICS)
      pushConstantLayout = internal->device->globalGraphicsPipelineLayout;
   else
      pushConstantLayout = internal->device->globalComputePipelineLayout;

   vkCmdPushConstants(internal->commandBuffer, pushConstantLayout, stageFlags, (uint32_t)offset, (uint32_t)size, data);
   return VE_SUCCESS;
}

// =============================================================================
// Drawing Commands
// =============================================================================

static uint64_t calculateTriangleContribution(VkPrimitiveTopology topology, uint32_t elementCount,
                                              uint32_t instanceCount, uint32_t patchControlPoints)
{
   uint64_t baseTriangles = 0;

   switch (topology)
   {
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
      baseTriangles = (elementCount / 3u);
      break;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
      baseTriangles = (elementCount > 2u) ? (elementCount - 2u) : 0u;
      break;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      // Adjacency primitives include extra vertices; treat contribution as unknown for now.
      break;
   case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
      if (patchControlPoints >= 3u)
      {
         baseTriangles = elementCount / patchControlPoints;
      }
      break;
   default:
      break;
   }

   return baseTriangles * instanceCount;
}

VEResult veDraw(VECommandBuffer *cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
                uint32_t firstInstance)
{
   if (!cmd || vertexCount == 0)
   {
      veSetError("Invalid parameters for veDraw");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += 1;
      internal->device->frameStats.verticesRendered += static_cast<uint64_t>(vertexCount) * instanceCount;
      internal->device->frameStats.trianglesRendered += calculateTriangleContribution(
          internal->currentTopology, vertexCount, instanceCount, internal->currentPatchControlPoints);
   }
   vkCmdDraw(internal->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
   return VE_SUCCESS;
}

VEResult veBindIndexBuffer(VECommandBuffer *cmd, VEBufferAddress indexBuffer, uint64_t offset, VkIndexType format)
{
   if (!cmd || indexBuffer == 0)
   {
      veSetError("Invalid parameters for veBindIndexBuffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(indexBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid index buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdBindIndexBuffer(internal->commandBuffer, buffer, offset, format);
   return VE_SUCCESS;
}

VEResult veDrawIndexed(VECommandBuffer *cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                       int32_t vertexOffset, uint32_t firstInstance)
{
   if (!cmd || indexCount == 0)
   {
      veSetError("Invalid parameters for veDrawIndexed");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += 1;
      internal->device->frameStats.verticesRendered += static_cast<uint64_t>(indexCount) * instanceCount;
      internal->device->frameStats.trianglesRendered += calculateTriangleContribution(
          internal->currentTopology, indexCount, instanceCount, internal->currentPatchControlPoints);
   }
   vkCmdDrawIndexed(internal->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
   return VE_SUCCESS;
}

VEResult veDrawIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset, uint32_t drawCount,
                        uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0)
   {
      veSetError("Invalid parameters for veDrawIndirect");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(indirectBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid indirect buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += drawCount;
   }
   return VE_SUCCESS;
}

VEResult veDrawIndexedIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                               uint32_t drawCount, uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0)
   {
      veSetError("Invalid parameters for veDrawIndexedIndirect");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(indirectBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid indirect buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndexedIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += drawCount;
   }
   return VE_SUCCESS;
}

VEResult veDrawIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                             VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS)
   {
      veSetError("Invalid parameters for veDrawIndirectCount");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer indirectBuf = internal->device->getVkBufferFromAddress(indirectBuffer);
   VkBuffer countBuf = internal->device->getVkBufferFromAddress(countBuffer);

   if (indirectBuf == VK_NULL_HANDLE || countBuf == VK_NULL_HANDLE)
   {
      veSetError("Invalid buffer address for indirect count draw");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset, countBuf, countOffset, maxDrawCount,
                          stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += maxDrawCount;
   }
   return VE_SUCCESS;
}

VEResult veDrawIndexedIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                                    VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount,
                                    uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS)
   {
      veSetError("Invalid parameters for veDrawIndexedIndirectCount");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer indirectBuf = internal->device->getVkBufferFromAddress(indirectBuffer);
   VkBuffer countBuf = internal->device->getVkBufferFromAddress(countBuffer);

   if (indirectBuf == VK_NULL_HANDLE || countBuf == VK_NULL_HANDLE)
   {
      veSetError("Invalid buffer address for indexed indirect count draw");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndexedIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset, countBuf, countOffset,
                                 maxDrawCount, stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += maxDrawCount;
   }
   return VE_SUCCESS;
}

// =============================================================================
// Synchronization and Memory Barriers
// =============================================================================

VEResult veBarrier(VECommandBuffer *cmd, const VEBarrierDesc *desc)
{
   if (!cmd || !desc)
   {
      veSetError("Command buffer and barrier desc cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;
   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if ((desc->memoryBarrierCount > 0 && !desc->memoryBarriers) || (desc->bufferBarrierCount > 0 && !desc->bufferBarriers) ||
       (desc->imageBarrierCount > 0 && !desc->imageBarriers))
   {
      veSetError("Barrier arrays cannot be NULL when their counts are > 0");
      return VE_ERROR_INVALID_PARAMETER;
   }

   std::vector<VkMemoryBarrier2> memoryBarriers;
   memoryBarriers.reserve(desc->memoryBarrierCount);
   for (uint32_t i = 0; i < desc->memoryBarrierCount; ++i)
   {
      const VEMemoryBarrier &src = desc->memoryBarriers[i];
      VkMemoryBarrier2 b{};
      b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
      b.srcStageMask = src.srcStageMask;
      b.srcAccessMask = src.srcAccessMask;
      b.dstStageMask = src.dstStageMask;
      b.dstAccessMask = src.dstAccessMask;
      memoryBarriers.push_back(b);
   }

   std::vector<VkBufferMemoryBarrier2> bufferBarriers;
   bufferBarriers.reserve(desc->bufferBarrierCount);
   for (uint32_t i = 0; i < desc->bufferBarrierCount; ++i)
   {
      const VEBufferBarrier &src = desc->bufferBarriers[i];
      if (src.buffer == VE_INVALID_ADDRESS)
      {
         veSetError("Buffer barrier %u has invalid buffer address", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VkBuffer buf = device->getVkBufferFromAddress(src.buffer);
      if (buf == VK_NULL_HANDLE)
      {
         veSetError("Buffer barrier %u references unknown buffer address", i);
         return VE_ERROR_NOT_FOUND;
      }

      VkBufferMemoryBarrier2 b{};
      b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
      b.srcStageMask = src.srcStageMask;
      b.srcAccessMask = src.srcAccessMask;
      b.dstStageMask = src.dstStageMask;
      b.dstAccessMask = src.dstAccessMask;
      b.srcQueueFamilyIndex = src.srcQueueFamilyIndex;
      b.dstQueueFamilyIndex = src.dstQueueFamilyIndex;
      b.buffer = buf;
      b.offset = src.offset;
      b.size = (src.size == 0) ? VK_WHOLE_SIZE : src.size;
      bufferBarriers.push_back(b);
   }

   std::vector<VkImageMemoryBarrier2> imageBarriers;
   imageBarriers.reserve(desc->imageBarrierCount);
   for (uint32_t i = 0; i < desc->imageBarrierCount; ++i)
   {
      const VEImageBarrier &src = desc->imageBarriers[i];
      if (src.image == VE_INVALID_TEXTURE_INDEX)
      {
         veSetError("Image barrier %u has invalid texture index", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VETextureInternal *tex = device->getTexture(src.image);
      if (!tex || !tex->isValid || tex->image == VK_NULL_HANDLE)
      {
         veSetError("Image barrier %u references unknown texture index", i);
         return VE_ERROR_NOT_FOUND;
      }

      if (src.subresourceRange.aspectMask == 0)
      {
         veSetError("Image barrier %u subresourceRange.aspectMask cannot be 0", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VkImageLayout oldLayout = src.oldLayout;
      if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && tex->currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
      {
         oldLayout = tex->currentLayout;
      }

      VkImageMemoryBarrier2 b{};
      b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      b.srcStageMask = src.srcStageMask;
      b.srcAccessMask = src.srcAccessMask;
      b.dstStageMask = src.dstStageMask;
      b.dstAccessMask = src.dstAccessMask;
      b.srcQueueFamilyIndex = src.srcQueueFamilyIndex;
      b.dstQueueFamilyIndex = src.dstQueueFamilyIndex;
      b.oldLayout = oldLayout;
      b.newLayout = src.newLayout;
      b.image = tex->image;
      b.subresourceRange = src.subresourceRange;
      imageBarriers.push_back(b);

      // Update tracked layout if this barrier is a layout transition.
      if (src.newLayout != VK_IMAGE_LAYOUT_UNDEFINED)
      {
         tex->currentLayout = src.newLayout;
      }
   }

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = (uint32_t)memoryBarriers.size();
   dependencyInfo.pMemoryBarriers = memoryBarriers.empty() ? nullptr : memoryBarriers.data();
   dependencyInfo.bufferMemoryBarrierCount = (uint32_t)bufferBarriers.size();
   dependencyInfo.pBufferMemoryBarriers = bufferBarriers.empty() ? nullptr : bufferBarriers.data();
   dependencyInfo.imageMemoryBarrierCount = (uint32_t)imageBarriers.size();
   dependencyInfo.pImageMemoryBarriers = imageBarriers.empty() ? nullptr : imageBarriers.data();

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
   return VE_SUCCESS;
}

VEResult veBarrierVertexToFragment(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
   b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veBarrierComputeToVertex(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veBarrierComputeToCompute(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veBarrierGraphicsToPresent(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
   b.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veTransitionTexture(VECommandBuffer *cmd, VETextureIndex texture, VkImageLayout oldLayout,
                            VkImageLayout newLayout)
{
   if (!cmd || texture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for veTransitionTexture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VETextureInternal *textureInternal = device->getTexture(texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return VE_ERROR_NOT_FOUND;
   }

   // Skip if layouts are the same
   if (oldLayout == newLayout)
   {
      return VE_SUCCESS;
   }

   // If oldLayout is VK_IMAGE_LAYOUT_UNDEFINED, use the tracked current layout
   VkImageLayout effectiveOldLayout = (VkImageLayout)oldLayout;
   if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && textureInternal->currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
   {
      effectiveOldLayout = textureInternal->currentLayout;
   }

   // Auto-detect stage masks and access flags based on common transitions
   VkPipelineStageFlags2 srcStage, dstStage;
   VkAccessFlags2 srcAccess, dstAccess;

   // Source stage and access
   switch (effectiveOldLayout)
   {
   case VK_IMAGE_LAYOUT_UNDEFINED:
      srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
      srcAccess = 0;
      break;
   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      srcAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      srcAccess = VK_ACCESS_2_SHADER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      srcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      srcStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      srcAccess = 0;
      break;
   default:
      srcStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      srcAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
      break;
   }

   // Destination stage and access
   switch (newLayout)
   {
   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      dstAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      dstAccess = 0;
      break;
   case VK_IMAGE_LAYOUT_GENERAL:
      dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
      break;
   default:
      dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
      break;
   }

   // Create image memory barrier
   VkImageMemoryBarrier2 imageBarrier{};
   imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   imageBarrier.srcStageMask = srcStage;
   imageBarrier.srcAccessMask = srcAccess;
   imageBarrier.dstStageMask = dstStage;
   imageBarrier.dstAccessMask = dstAccess;
   imageBarrier.oldLayout = effectiveOldLayout;
   imageBarrier.newLayout = (VkImageLayout)newLayout;
   imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   imageBarrier.image = textureInternal->image;
   imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Default to color aspect
   if (textureInternal->format >= VK_FORMAT_D16_UNORM && textureInternal->format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
   {
      imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      // Add stencil aspect for depth-stencil formats
      if (textureInternal->format == VK_FORMAT_D16_UNORM_S8_UINT ||
          textureInternal->format == VK_FORMAT_D24_UNORM_S8_UINT ||
          textureInternal->format == VK_FORMAT_D32_SFLOAT_S8_UINT)
      {
         imageBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
   }
   imageBarrier.subresourceRange.baseMipLevel = 0;
   imageBarrier.subresourceRange.levelCount = textureInternal->mipLevels;
   imageBarrier.subresourceRange.baseArrayLayer = 0;
   imageBarrier.subresourceRange.layerCount = textureInternal->arrayLayers;

   // Submit the barrier
   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.imageMemoryBarrierCount = 1;
   dependencyInfo.pImageMemoryBarriers = &imageBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);

   // Update the tracked layout
   textureInternal->currentLayout = (VkImageLayout)newLayout;
   return VE_SUCCESS;
}

// =============================================================================
// Common Texture Layout Transition Helpers
// =============================================================================

VEResult veTransitionTextureForShaderRead(VECommandBuffer *cmd, VETextureIndex texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

VEResult veTransitionTextureForColorAttachment(VECommandBuffer *cmd, VETextureIndex texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

VEResult veTransitionTextureForDepthAttachment(VECommandBuffer *cmd, VETextureIndex texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VEResult veTransitionTextureForTransferSrc(VECommandBuffer *cmd, VETextureIndex texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

VEResult veTransitionTextureForTransferDst(VECommandBuffer *cmd, VETextureIndex texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

VEResult veTransitionTextureForPresent(VECommandBuffer *cmd, VETextureIndex texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

VEResult veTransitionTextureToLayout(VECommandBuffer *cmd, VETextureIndex texture, VkImageLayout newLayout)
{
   if (!cmd || texture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for veTransitionTextureToLayout");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VETextureInternal *textureInternal = device->getTexture(texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return VE_ERROR_NOT_FOUND;
   }

   // Use the tracked current layout, or UNDEFINED if not set
   VkImageLayout currentLayout = textureInternal->currentLayout;
   return veTransitionTexture(cmd, texture, currentLayout, newLayout);
}