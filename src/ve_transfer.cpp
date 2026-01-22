/**
 * @file ve_transfer.cpp
 * @brief Buffer and Texture Transfer Operations Implementation
 *
 * Implements copy, blit, fill, and update operations for buffers and textures.
 */

#include "ve_internal.h"

// =============================================================================
// Buffer Copy Operations
// =============================================================================

VEResult veCmdCopyBuffer(VECommandBuffer *cmd, VEBufferAddress src, VEBufferAddress dst, uint64_t srcOffset,
                         uint64_t dstOffset, uint64_t size)
{
   if (!cmd || src == VE_INVALID_ADDRESS || dst == VE_INVALID_ADDRESS || size == 0)
   {
      veSetError("veCmdCopyBuffer: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   VkBuffer srcBuffer = device->getVkBufferFromAddress(src);
   VkBuffer dstBuffer = device->getVkBufferFromAddress(dst);

   if (srcBuffer == VK_NULL_HANDLE || dstBuffer == VK_NULL_HANDLE)
   {
      veSetError("veCmdCopyBuffer: Invalid buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   VkBufferCopy region{};
   region.srcOffset = srcOffset;
   region.dstOffset = dstOffset;
   region.size = size;

   vkCmdCopyBuffer(internal->commandBuffer, srcBuffer, dstBuffer, 1, &region);

   return VE_SUCCESS;
}

VEResult veCopyBuffer(VEDevice *device, VEBufferAddress src, VEBufferAddress dst, uint64_t srcOffset,
                      uint64_t dstOffset, uint64_t size, VkFence fence)
{
   if (!device || src == VE_INVALID_ADDRESS || dst == VE_INVALID_ADDRESS || size == 0)
   {
      veSetError("veCopyBuffer: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Begin a command buffer
   VECommandBuffer *cmd = nullptr;
   VEResult result = veBeginCommandBuffer(device, &cmd);
   if (result != VE_SUCCESS || !cmd)
   {
      return result;
   }

   // Record the copy
   result = veCmdCopyBuffer(cmd, src, dst, srcOffset, dstOffset, size);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   // End and submit
   result = veEndCommandBuffer(cmd);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   VESubmitInfo submitInfo{};
   submitInfo.fence = fence;
   submitInfo.waitForCompletion = (fence == VK_NULL_HANDLE);

   result = veSubmitCommandBuffer(cmd, &submitInfo);

   // If async (fence provided), don't release yet - user must manage
   // If sync (blocking), command buffer is released by submit
   if (fence != VK_NULL_HANDLE)
   {
      veReleaseCommandBuffer(cmd);
   }

   return result;
}

// =============================================================================
// Buffer Fill and Update Operations
// =============================================================================

VEResult veCmdFillBuffer(VECommandBuffer *cmd, VEBufferAddress dst, uint64_t offset, uint64_t size, uint32_t data)
{
   if (!cmd || dst == VE_INVALID_ADDRESS)
   {
      veSetError("veCmdFillBuffer: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   VkBuffer dstBuffer = device->getVkBufferFromAddress(dst);
   if (dstBuffer == VK_NULL_HANDLE)
   {
      veSetError("veCmdFillBuffer: Invalid buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   // size of 0 means VK_WHOLE_SIZE
   VkDeviceSize vkSize = (size == 0) ? VK_WHOLE_SIZE : size;

   vkCmdFillBuffer(internal->commandBuffer, dstBuffer, offset, vkSize, data);

   return VE_SUCCESS;
}

VEResult veCmdUpdateBuffer(VECommandBuffer *cmd, VEBufferAddress dst, uint64_t offset, uint64_t size, const void *data)
{
   if (!cmd || dst == VE_INVALID_ADDRESS || !data || size == 0)
   {
      veSetError("veCmdUpdateBuffer: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (size > 65536)
   {
      veSetError("veCmdUpdateBuffer: Size exceeds 65536 byte limit (Vulkan spec)");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   VkBuffer dstBuffer = device->getVkBufferFromAddress(dst);
   if (dstBuffer == VK_NULL_HANDLE)
   {
      veSetError("veCmdUpdateBuffer: Invalid buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdUpdateBuffer(internal->commandBuffer, dstBuffer, offset, size, data);

   return VE_SUCCESS;
}

// =============================================================================
// Texture Copy Operations
// =============================================================================

VEResult veCmdCopyTexture(VECommandBuffer *cmd, VETextureIndex src, VETextureIndex dst,
                          const VETextureCopyRegion *region)
{
   if (!cmd || src == VE_INVALID_TEXTURE_INDEX || dst == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veCmdCopyTexture: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   VETextureInternal *srcTex = device->getTexture(src);
   VETextureInternal *dstTex = device->getTexture(dst);

   if (!srcTex || !srcTex->isValid || !dstTex || !dstTex->isValid)
   {
      veSetError("veCmdCopyTexture: Invalid texture index");
      return VE_ERROR_NOT_FOUND;
   }

   // Build copy region
   VkImageCopy copyRegion{};

   if (region)
   {
      copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.srcSubresource.mipLevel = region->srcMipLevel;
      copyRegion.srcSubresource.baseArrayLayer = region->srcArrayLayer;
      copyRegion.srcSubresource.layerCount = region->layerCount > 0 ? region->layerCount : 1;
      copyRegion.srcOffset = {(int32_t)region->srcOffsetX, (int32_t)region->srcOffsetY, (int32_t)region->srcOffsetZ};

      copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.dstSubresource.mipLevel = region->dstMipLevel;
      copyRegion.dstSubresource.baseArrayLayer = region->dstArrayLayer;
      copyRegion.dstSubresource.layerCount = region->layerCount > 0 ? region->layerCount : 1;
      copyRegion.dstOffset = {(int32_t)region->dstOffsetX, (int32_t)region->dstOffsetY, (int32_t)region->dstOffsetZ};

      copyRegion.extent = {region->width, region->height, region->depth > 0 ? region->depth : 1};
   }
   else
   {
      // Full texture copy
      copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.srcSubresource.mipLevel = 0;
      copyRegion.srcSubresource.baseArrayLayer = 0;
      copyRegion.srcSubresource.layerCount = srcTex->arrayLayers;
      copyRegion.srcOffset = {0, 0, 0};

      copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.dstSubresource.mipLevel = 0;
      copyRegion.dstSubresource.baseArrayLayer = 0;
      copyRegion.dstSubresource.layerCount = dstTex->arrayLayers;
      copyRegion.dstOffset = {0, 0, 0};

      copyRegion.extent = {srcTex->width, srcTex->height, srcTex->depth};
   }

   // Transition source to TRANSFER_SRC_OPTIMAL
   VkImageMemoryBarrier2 srcBarrier{};
   srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   srcBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   srcBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
   srcBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   srcBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
   srcBarrier.oldLayout = srcTex->currentLayout;
   srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   srcBarrier.image = srcTex->image;
   srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   srcBarrier.subresourceRange.baseMipLevel = 0;
   srcBarrier.subresourceRange.levelCount = srcTex->mipLevels;
   srcBarrier.subresourceRange.baseArrayLayer = 0;
   srcBarrier.subresourceRange.layerCount = srcTex->arrayLayers;

   // Transition dest to TRANSFER_DST_OPTIMAL
   VkImageMemoryBarrier2 dstBarrier{};
   dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   dstBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   dstBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
   dstBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   dstBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
   dstBarrier.oldLayout = dstTex->currentLayout;
   dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   dstBarrier.image = dstTex->image;
   dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   dstBarrier.subresourceRange.baseMipLevel = 0;
   dstBarrier.subresourceRange.levelCount = dstTex->mipLevels;
   dstBarrier.subresourceRange.baseArrayLayer = 0;
   dstBarrier.subresourceRange.layerCount = dstTex->arrayLayers;

   VkImageMemoryBarrier2 barriers[2] = {srcBarrier, dstBarrier};
   VkDependencyInfo depInfo{};
   depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   depInfo.imageMemoryBarrierCount = 2;
   depInfo.pImageMemoryBarriers = barriers;

   vkCmdPipelineBarrier2(internal->commandBuffer, &depInfo);

   // Copy
   vkCmdCopyImage(internal->commandBuffer, srcTex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstTex->image,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

   // Update tracked layouts
   srcTex->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   dstTex->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

   return VE_SUCCESS;
}

VEResult veCopyTexture(VEDevice *device, VETextureIndex src, VETextureIndex dst, const VETextureCopyRegion *region,
                       VkFence fence)
{
   if (!device || src == VE_INVALID_TEXTURE_INDEX || dst == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veCopyTexture: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBuffer *cmd = nullptr;
   VEResult result = veBeginCommandBuffer(device, &cmd);
   if (result != VE_SUCCESS || !cmd)
   {
      return result;
   }

   result = veCmdCopyTexture(cmd, src, dst, region);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   result = veEndCommandBuffer(cmd);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   VESubmitInfo submitInfo{};
   submitInfo.fence = fence;
   submitInfo.waitForCompletion = (fence == VK_NULL_HANDLE);

   result = veSubmitCommandBuffer(cmd, &submitInfo);

   if (fence != VK_NULL_HANDLE)
   {
      veReleaseCommandBuffer(cmd);
   }

   return result;
}

// =============================================================================
// Texture Blit Operations
// =============================================================================

VEResult veCmdBlitTexture(VECommandBuffer *cmd, VETextureIndex src, VETextureIndex dst,
                          const VETextureBlitRegion *region, VkFilter filter)
{
   if (!cmd || src == VE_INVALID_TEXTURE_INDEX || dst == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veCmdBlitTexture: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   VETextureInternal *srcTex = device->getTexture(src);
   VETextureInternal *dstTex = device->getTexture(dst);

   if (!srcTex || !srcTex->isValid || !dstTex || !dstTex->isValid)
   {
      veSetError("veCmdBlitTexture: Invalid texture index");
      return VE_ERROR_NOT_FOUND;
   }

   // Build blit region
   VkImageBlit blitRegion{};

   if (region)
   {
      blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.srcSubresource.mipLevel = region->srcMipLevel;
      blitRegion.srcSubresource.baseArrayLayer = region->srcArrayLayer;
      blitRegion.srcSubresource.layerCount = region->layerCount > 0 ? region->layerCount : 1;
      blitRegion.srcOffsets[0] = {region->srcOffsetX0, region->srcOffsetY0, region->srcOffsetZ0};
      blitRegion.srcOffsets[1] = {region->srcOffsetX1, region->srcOffsetY1, region->srcOffsetZ1};

      blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.dstSubresource.mipLevel = region->dstMipLevel;
      blitRegion.dstSubresource.baseArrayLayer = region->dstArrayLayer;
      blitRegion.dstSubresource.layerCount = region->layerCount > 0 ? region->layerCount : 1;
      blitRegion.dstOffsets[0] = {region->dstOffsetX0, region->dstOffsetY0, region->dstOffsetZ0};
      blitRegion.dstOffsets[1] = {region->dstOffsetX1, region->dstOffsetY1, region->dstOffsetZ1};
   }
   else
   {
      // Full texture blit
      blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.srcSubresource.mipLevel = 0;
      blitRegion.srcSubresource.baseArrayLayer = 0;
      blitRegion.srcSubresource.layerCount = srcTex->arrayLayers;
      blitRegion.srcOffsets[0] = {0, 0, 0};
      blitRegion.srcOffsets[1] = {(int32_t)srcTex->width, (int32_t)srcTex->height, (int32_t)srcTex->depth};

      blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.dstSubresource.mipLevel = 0;
      blitRegion.dstSubresource.baseArrayLayer = 0;
      blitRegion.dstSubresource.layerCount = dstTex->arrayLayers;
      blitRegion.dstOffsets[0] = {0, 0, 0};
      blitRegion.dstOffsets[1] = {(int32_t)dstTex->width, (int32_t)dstTex->height, (int32_t)dstTex->depth};
   }

   // Transition source to TRANSFER_SRC_OPTIMAL
   VkImageMemoryBarrier2 srcBarrier{};
   srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   srcBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   srcBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
   srcBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
   srcBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
   srcBarrier.oldLayout = srcTex->currentLayout;
   srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   srcBarrier.image = srcTex->image;
   srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   srcBarrier.subresourceRange.baseMipLevel = 0;
   srcBarrier.subresourceRange.levelCount = srcTex->mipLevels;
   srcBarrier.subresourceRange.baseArrayLayer = 0;
   srcBarrier.subresourceRange.layerCount = srcTex->arrayLayers;

   // Transition dest to TRANSFER_DST_OPTIMAL
   VkImageMemoryBarrier2 dstBarrier{};
   dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   dstBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   dstBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
   dstBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
   dstBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
   dstBarrier.oldLayout = dstTex->currentLayout;
   dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   dstBarrier.image = dstTex->image;
   dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   dstBarrier.subresourceRange.baseMipLevel = 0;
   dstBarrier.subresourceRange.levelCount = dstTex->mipLevels;
   dstBarrier.subresourceRange.baseArrayLayer = 0;
   dstBarrier.subresourceRange.layerCount = dstTex->arrayLayers;

   VkImageMemoryBarrier2 barriers[2] = {srcBarrier, dstBarrier};
   VkDependencyInfo depInfo{};
   depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   depInfo.imageMemoryBarrierCount = 2;
   depInfo.pImageMemoryBarriers = barriers;

   vkCmdPipelineBarrier2(internal->commandBuffer, &depInfo);

   // Blit
   vkCmdBlitImage(internal->commandBuffer, srcTex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstTex->image,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, filter);

   // Update tracked layouts
   srcTex->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   dstTex->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

   return VE_SUCCESS;
}

VEResult veBlitTexture(VEDevice *device, VETextureIndex src, VETextureIndex dst, const VETextureBlitRegion *region,
                       VkFilter filter, VkFence fence)
{
   if (!device || src == VE_INVALID_TEXTURE_INDEX || dst == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veBlitTexture: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBuffer *cmd = nullptr;
   VEResult result = veBeginCommandBuffer(device, &cmd);
   if (result != VE_SUCCESS || !cmd)
   {
      return result;
   }

   result = veCmdBlitTexture(cmd, src, dst, region, filter);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   result = veEndCommandBuffer(cmd);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   VESubmitInfo submitInfo{};
   submitInfo.fence = fence;
   submitInfo.waitForCompletion = (fence == VK_NULL_HANDLE);

   result = veSubmitCommandBuffer(cmd, &submitInfo);

   if (fence != VK_NULL_HANDLE)
   {
      veReleaseCommandBuffer(cmd);
   }

   return result;
}

// =============================================================================
// Buffer to Texture Copy Operations
// =============================================================================

VEResult veCmdCopyBufferToTexture(VECommandBuffer *cmd, VEBufferAddress src, VETextureIndex dst,
                                  const VEBufferTextureCopyRegion *region)
{
   if (!cmd || src == VE_INVALID_ADDRESS || dst == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veCmdCopyBufferToTexture: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   VkBuffer srcBuffer = device->getVkBufferFromAddress(src);
   VETextureInternal *dstTex = device->getTexture(dst);

   if (srcBuffer == VK_NULL_HANDLE)
   {
      veSetError("veCmdCopyBufferToTexture: Invalid buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   if (!dstTex || !dstTex->isValid)
   {
      veSetError("veCmdCopyBufferToTexture: Invalid texture index");
      return VE_ERROR_NOT_FOUND;
   }

   // Build copy region
   VkBufferImageCopy copyRegion{};

   if (region)
   {
      copyRegion.bufferOffset = region->bufferOffset;
      copyRegion.bufferRowLength = region->bufferRowLength;
      copyRegion.bufferImageHeight = region->bufferImageHeight;
      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = region->mipLevel;
      copyRegion.imageSubresource.baseArrayLayer = region->arrayLayer;
      copyRegion.imageSubresource.layerCount = region->layerCount > 0 ? region->layerCount : 1;
      copyRegion.imageOffset = {(int32_t)region->textureOffsetX, (int32_t)region->textureOffsetY,
                                (int32_t)region->textureOffsetZ};
      copyRegion.imageExtent = {region->width, region->height, region->depth > 0 ? region->depth : 1};
   }
   else
   {
      // Full texture copy
      copyRegion.bufferOffset = 0;
      copyRegion.bufferRowLength = 0;
      copyRegion.bufferImageHeight = 0;
      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = 0;
      copyRegion.imageSubresource.baseArrayLayer = 0;
      copyRegion.imageSubresource.layerCount = dstTex->arrayLayers;
      copyRegion.imageOffset = {0, 0, 0};
      copyRegion.imageExtent = {dstTex->width, dstTex->height, dstTex->depth};
   }

   // Transition texture to TRANSFER_DST_OPTIMAL
   VkImageMemoryBarrier2 barrier{};
   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
   barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
   barrier.oldLayout = dstTex->currentLayout;
   barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = dstTex->image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.baseMipLevel = 0;
   barrier.subresourceRange.levelCount = dstTex->mipLevels;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount = dstTex->arrayLayers;

   VkDependencyInfo depInfo{};
   depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   depInfo.imageMemoryBarrierCount = 1;
   depInfo.pImageMemoryBarriers = &barrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &depInfo);

   // Copy
   vkCmdCopyBufferToImage(internal->commandBuffer, srcBuffer, dstTex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                          &copyRegion);

   dstTex->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

   return VE_SUCCESS;
}

VEResult veCmdCopyTextureToBuffer(VECommandBuffer *cmd, VETextureIndex src, VEBufferAddress dst,
                                  const VEBufferTextureCopyRegion *region)
{
   if (!cmd || src == VE_INVALID_TEXTURE_INDEX || dst == VE_INVALID_ADDRESS)
   {
      veSetError("veCmdCopyTextureToBuffer: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   VETextureInternal *srcTex = device->getTexture(src);
   VkBuffer dstBuffer = device->getVkBufferFromAddress(dst);

   if (!srcTex || !srcTex->isValid)
   {
      veSetError("veCmdCopyTextureToBuffer: Invalid texture index");
      return VE_ERROR_NOT_FOUND;
   }

   if (dstBuffer == VK_NULL_HANDLE)
   {
      veSetError("veCmdCopyTextureToBuffer: Invalid buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   // Build copy region
   VkBufferImageCopy copyRegion{};

   if (region)
   {
      copyRegion.bufferOffset = region->bufferOffset;
      copyRegion.bufferRowLength = region->bufferRowLength;
      copyRegion.bufferImageHeight = region->bufferImageHeight;
      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = region->mipLevel;
      copyRegion.imageSubresource.baseArrayLayer = region->arrayLayer;
      copyRegion.imageSubresource.layerCount = region->layerCount > 0 ? region->layerCount : 1;
      copyRegion.imageOffset = {(int32_t)region->textureOffsetX, (int32_t)region->textureOffsetY,
                                (int32_t)region->textureOffsetZ};
      copyRegion.imageExtent = {region->width, region->height, region->depth > 0 ? region->depth : 1};
   }
   else
   {
      // Full texture copy
      copyRegion.bufferOffset = 0;
      copyRegion.bufferRowLength = 0;
      copyRegion.bufferImageHeight = 0;
      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = 0;
      copyRegion.imageSubresource.baseArrayLayer = 0;
      copyRegion.imageSubresource.layerCount = srcTex->arrayLayers;
      copyRegion.imageOffset = {0, 0, 0};
      copyRegion.imageExtent = {srcTex->width, srcTex->height, srcTex->depth};
   }

   // Transition texture to TRANSFER_SRC_OPTIMAL
   VkImageMemoryBarrier2 barrier{};
   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
   barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
   barrier.oldLayout = srcTex->currentLayout;
   barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = srcTex->image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.baseMipLevel = 0;
   barrier.subresourceRange.levelCount = srcTex->mipLevels;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount = srcTex->arrayLayers;

   VkDependencyInfo depInfo{};
   depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   depInfo.imageMemoryBarrierCount = 1;
   depInfo.pImageMemoryBarriers = &barrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &depInfo);

   // Copy
   vkCmdCopyImageToBuffer(internal->commandBuffer, srcTex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuffer, 1,
                          &copyRegion);

   srcTex->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

   return VE_SUCCESS;
}

VEResult veCopyBufferToTexture(VEDevice *device, VEBufferAddress src, VETextureIndex dst,
                               const VEBufferTextureCopyRegion *region, VkFence fence)
{
   if (!device || src == VE_INVALID_ADDRESS || dst == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veCopyBufferToTexture: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBuffer *cmd = nullptr;
   VEResult result = veBeginCommandBuffer(device, &cmd);
   if (result != VE_SUCCESS || !cmd)
   {
      return result;
   }

   result = veCmdCopyBufferToTexture(cmd, src, dst, region);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   result = veEndCommandBuffer(cmd);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   VESubmitInfo submitInfo{};
   submitInfo.fence = fence;
   submitInfo.waitForCompletion = (fence == VK_NULL_HANDLE);

   result = veSubmitCommandBuffer(cmd, &submitInfo);

   if (fence != VK_NULL_HANDLE)
   {
      veReleaseCommandBuffer(cmd);
   }

   return result;
}

VEResult veCopyTextureToBuffer(VEDevice *device, VETextureIndex src, VEBufferAddress dst,
                               const VEBufferTextureCopyRegion *region, VkFence fence)
{
   if (!device || src == VE_INVALID_TEXTURE_INDEX || dst == VE_INVALID_ADDRESS)
   {
      veSetError("veCopyTextureToBuffer: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBuffer *cmd = nullptr;
   VEResult result = veBeginCommandBuffer(device, &cmd);
   if (result != VE_SUCCESS || !cmd)
   {
      return result;
   }

   result = veCmdCopyTextureToBuffer(cmd, src, dst, region);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   result = veEndCommandBuffer(cmd);
   if (result != VE_SUCCESS)
   {
      veReleaseCommandBuffer(cmd);
      return result;
   }

   VESubmitInfo submitInfo{};
   submitInfo.fence = fence;
   submitInfo.waitForCompletion = (fence == VK_NULL_HANDLE);

   result = veSubmitCommandBuffer(cmd, &submitInfo);

   if (fence != VK_NULL_HANDLE)
   {
      veReleaseCommandBuffer(cmd);
   }

   return result;
}

// =============================================================================
// Clear Operations
// =============================================================================

VEResult veClearColorAttachment(VECommandBuffer *cmd, uint32_t attachmentIndex, VEColor clearValue,
                                const VERect2D *rect)
{
   if (!cmd)
   {
      veSetError("veClearColorAttachment: Invalid command buffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   if (!internal->inRenderPass)
   {
      veSetError("veClearColorAttachment: Must be called inside veBeginRendering/veEndRendering");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkClearAttachment clearAttachment{};
   clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   clearAttachment.colorAttachment = attachmentIndex;
   clearAttachment.clearValue.color.float32[0] = clearValue.r;
   clearAttachment.clearValue.color.float32[1] = clearValue.g;
   clearAttachment.clearValue.color.float32[2] = clearValue.b;
   clearAttachment.clearValue.color.float32[3] = clearValue.a;

   VkClearRect clearRect{};
   if (rect)
   {
      clearRect.rect.offset.x = rect->x;
      clearRect.rect.offset.y = rect->y;
      clearRect.rect.extent.width = rect->width;
      clearRect.rect.extent.height = rect->height;
   }
   else
   {
      clearRect.rect.offset.x = static_cast<int32_t>(internal->renderAreaX);
      clearRect.rect.offset.y = static_cast<int32_t>(internal->renderAreaY);
      clearRect.rect.extent.width = internal->renderAreaWidth;
      clearRect.rect.extent.height = internal->renderAreaHeight;
   }
   clearRect.baseArrayLayer = 0;
   clearRect.layerCount = 1;

   vkCmdClearAttachments(internal->commandBuffer, 1, &clearAttachment, 1, &clearRect);

   return VE_SUCCESS;
}

VEResult veClearDepthStencilAttachment(VECommandBuffer *cmd, float depth, uint32_t stencil,
                                       VkImageAspectFlags aspectMask, const VERect2D *rect)
{
   if (!cmd)
   {
      veSetError("veClearDepthStencilAttachment: Invalid command buffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   if (!internal->inRenderPass)
   {
      veSetError("veClearDepthStencilAttachment: Must be called inside veBeginRendering/veEndRendering");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkClearAttachment clearAttachment{};
   clearAttachment.aspectMask = aspectMask;
   clearAttachment.clearValue.depthStencil.depth = depth;
   clearAttachment.clearValue.depthStencil.stencil = stencil;

   VkClearRect clearRect{};
   if (rect)
   {
      clearRect.rect.offset.x = rect->x;
      clearRect.rect.offset.y = rect->y;
      clearRect.rect.extent.width = rect->width;
      clearRect.rect.extent.height = rect->height;
   }
   else
   {
      clearRect.rect.offset.x = static_cast<int32_t>(internal->renderAreaX);
      clearRect.rect.offset.y = static_cast<int32_t>(internal->renderAreaY);
      clearRect.rect.extent.width = internal->renderAreaWidth;
      clearRect.rect.extent.height = internal->renderAreaHeight;
   }
   clearRect.baseArrayLayer = 0;
   clearRect.layerCount = 1;

   vkCmdClearAttachments(internal->commandBuffer, 1, &clearAttachment, 1, &clearRect);

   return VE_SUCCESS;
}
