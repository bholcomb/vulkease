/**
 * @file ve_draw.c
 * @brief Drawing and State Management Implementation
 */

#include "ve_internal.h"

// =============================================================================
// Viewport and Scissor Management
// =============================================================================

void veSetViewport(VECommandBuffer *cmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkViewport viewport{};
   viewport.x = x;
   viewport.y = y;
   viewport.width = width;
   viewport.height = height;
   viewport.minDepth = minDepth;
   viewport.maxDepth = maxDepth;

   vkCmdSetViewportWithCount(internal->commandBuffer, 1, &viewport);
}

void veSetScissor(VECommandBuffer *cmd, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkRect2D scissor{};
   scissor.offset.x = x;
   scissor.offset.y = y;
   scissor.extent.width = width;
   scissor.extent.height = height;

   vkCmdSetScissorWithCount(internal->commandBuffer, 1, &scissor);
}

// =============================================================================
// Runtime State Overrides
// =============================================================================

void veOverrideRasterState(VECommandBuffer *cmd, const VERasterConfig *raster)
{
   if (!cmd || !raster)
      return;

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
}

void veOverrideDepthState(VECommandBuffer *cmd, const VEDepthConfig *depth)
{
   if (!cmd || !depth)
      return;

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
}

void veOverrideBlendState(VECommandBuffer *cmd, const VEBlendConfig *blend)
{
   if (!cmd || !blend)
      return;

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
}

// =============================================================================
// Quick State Toggles
// =============================================================================

void veSetWireframe(VECommandBuffer *cmd, bool enabled)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkPolygonMode mode = enabled ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
   veFuncs.vkCmdSetPolygonModeEXT(internal->commandBuffer, mode);
}

void veSetAlphaBlending(VECommandBuffer *cmd, bool enabled)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkBool32 blendEnable = enabled ? VK_TRUE : VK_FALSE;
   veFuncs.vkCmdSetColorBlendEnableEXT(internal->commandBuffer, 0, 1, &blendEnable);
}

void veSetDepthTesting(VECommandBuffer *cmd, bool testEnabled, bool writeEnabled)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   vkCmdSetDepthTestEnable(internal->commandBuffer, testEnabled);
   vkCmdSetDepthWriteEnable(internal->commandBuffer, writeEnabled);
}

void veSetCulling(VECommandBuffer *cmd, VkCullModeFlags cullMode)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetCullMode(internal->commandBuffer, cullMode);
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

void vePushConstants(VECommandBuffer *cmd, const void *data, uint64_t size, uint64_t offset)
{
   if (!cmd || !data || size == 0)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // Alignment and bounds checks required by spec
   if ((offset & 3u) || (size & 3u))
   {
      veSetError("Push constants offset/size must be multiples of 4 (offset=%zu "
                 "size=%zu)",
                 offset, size);
      return; // 4-byte alignment is required [web:626]
   }

   // Simple bounds check for VulkEase's standard push constant limit
   if (offset + size > VE_MAX_PUSH_CONSTANT_BYTES)
   { // Standard guaranteed minimum
      veSetError("Push constant size exceeds limit (max 256 bytes, requested %zu+%zu)", offset, size);
      return;
   }

   // With VK_EXT_shader_object: Push constants are specified per-shader, not
   // per-pipeline We need to track which shader stages are currently bound and
   // determine stage flags
   VkShaderStageFlags stageFlags = veGetBoundShaderStages(internal);

   if (stageFlags == 0)
   {
      veSetError("No shader objects bound - cannot push constants");
      return;
   }

   // find the device's global push constant layout (created at device init)
   VkPipelineLayout pushConstantLayout;
   if (stageFlags == VK_SHADER_STAGE_ALL_GRAPHICS)
      pushConstantLayout = internal->device->globalGraphicsPipelineLayout;
   else
      pushConstantLayout = internal->device->globalComputePipelineLayout;

   vkCmdPushConstants(internal->commandBuffer, pushConstantLayout, stageFlags, (uint32_t)offset, (uint32_t)size, data);
}

// =============================================================================
// Drawing Commands
// =============================================================================

void veDraw(VECommandBuffer *cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance)
{
   if (!cmd || vertexCount == 0)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdDraw(internal->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void veBindIndexBuffer(VECommandBuffer *cmd, VEBufferAddress indexBuffer, uint64_t offset, VkIndexType format)
{
   if (!cmd || indexBuffer == 0)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = veGetVkBufferFromAddress(internal->device, indexBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid index buffer address");
      return;
   }

   vkCmdBindIndexBuffer(internal->commandBuffer, buffer, offset, format);
}

void veDrawIndexed(VECommandBuffer *cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                   int32_t vertexOffset, uint32_t firstInstance)
{
   if (!cmd || indexCount == 0)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdDrawIndexed(internal->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void veDrawIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset, uint32_t drawCount,
                    uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = veGetVkBufferFromAddress(internal->device, indirectBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid indirect buffer address");
      return;
   }

   vkCmdDrawIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
}

void veDrawIndexedIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset, uint32_t drawCount,
                           uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = veGetVkBufferFromAddress(internal->device, indirectBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid indirect buffer address");
      return;
   }

   vkCmdDrawIndexedIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
}

void veDrawIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                         VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer indirectBuf = veGetVkBufferFromAddress(internal->device, indirectBuffer);
   VkBuffer countBuf = veGetVkBufferFromAddress(internal->device, countBuffer);

   if (indirectBuf == VK_NULL_HANDLE || countBuf == VK_NULL_HANDLE)
   {
      veSetError("Invalid buffer address for indirect count draw");
      return;
   }

   vkCmdDrawIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset, countBuf, countOffset, maxDrawCount,
                          stride);
}

void veDrawIndexedIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                                VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer indirectBuf = veGetVkBufferFromAddress(internal->device, indirectBuffer);
   VkBuffer countBuf = veGetVkBufferFromAddress(internal->device, countBuffer);

   if (indirectBuf == VK_NULL_HANDLE || countBuf == VK_NULL_HANDLE)
   {
      veSetError("Invalid buffer address for indexed indirect count draw");
      return;
   }

   vkCmdDrawIndexedIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset, countBuf, countOffset,
                                 maxDrawCount, stride);
}

// =============================================================================
// Synchronization and Memory Barriers
// =============================================================================

void veBarrierVertexToFragment(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierComputeToVertex(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierComputeToCompute(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierGraphicsToPresent(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veTransitionTexture(VECommandBuffer *cmd, VETextureIndex texture, VkImageLayout oldLayout, VkImageLayout newLayout)
{
   if (!cmd || texture == VE_INVALID_TEXTURE_INDEX)
   {
      return;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return;
   }

   VETextureInternal *textureInternal = veGetTexture(device, texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return;
   }

   // Skip if layouts are the same
   if (oldLayout == newLayout)
   {
      return;
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
}

// =============================================================================
// Common Texture Layout Transition Helpers
// =============================================================================

void veTransitionTextureForShaderRead(VECommandBuffer *cmd, VETextureIndex texture)
{
   veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void veTransitionTextureForColorAttachment(VECommandBuffer *cmd, VETextureIndex texture)
{
   veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void veTransitionTextureForDepthAttachment(VECommandBuffer *cmd, VETextureIndex texture)
{
   veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void veTransitionTextureForTransferSrc(VECommandBuffer *cmd, VETextureIndex texture)
{
   veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

void veTransitionTextureForTransferDst(VECommandBuffer *cmd, VETextureIndex texture)
{
   veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void veTransitionTextureForPresent(VECommandBuffer *cmd, VETextureIndex texture)
{
   veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void veTransitionTextureToLayout(VECommandBuffer *cmd, VETextureIndex texture, VkImageLayout newLayout)
{
   if (!cmd || texture == VE_INVALID_TEXTURE_INDEX)
   {
      return;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return;
   }

   VETextureInternal *textureInternal = veGetTexture(device, texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return;
   }

   // Use the tracked current layout, or UNDEFINED if not set
   VkImageLayout currentLayout = textureInternal->currentLayout;
   veTransitionTexture(cmd, texture, currentLayout, newLayout);
}