/**
 * @file ve_render_target.cpp
 * @brief Render Target Helper Functions
 */

#include "ve_internal.h"
#include <cstring>

// =============================================================================
// Resize Render Target
// =============================================================================

VEResult veResizeRenderTarget(VEDevice *device, VERenderTarget *target, uint32_t width, uint32_t height)
{
   if (!device || !target)
   {
      veSetError("veResizeRenderTarget: device and target must be non-null");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (width == 0 || height == 0)
   {
      veSetError("veResizeRenderTarget: width and height must be greater than zero");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // If size hasn't changed, nothing to do
   if (width == target->renderAreaWidth && height == target->renderAreaHeight)
   {
      return VE_SUCCESS;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);

   // Resize all attached textures
   // Color attachments
   for (uint32_t i = 0; i < target->colorAttachmentCount; i++)
   {
      VERenderTargetAttachment *attachment = &target->attachments[i];
      if (attachment->texture != VE_INVALID_TEXTURE_INDEX)
      {
         VETextureInternal *tex = deviceInternal->getTexture(attachment->texture);
         if (tex)
         {
            // Get current texture properties
            VETextureDesc desc{};
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mipLevels = tex->mipLevels;
            desc.arrayLayers = tex->arrayLayers;
            desc.format = tex->format;
            desc.usage = tex->usage;
            desc.sampleCount = tex->sampleCount;

            // Destroy old texture and create new one
            VETextureIndex oldTex = attachment->texture;
            VETextureIndex newTex = VE_INVALID_TEXTURE_INDEX;

            VEResult result = veCreateTexture(device, &desc, &newTex);
            if (result != VE_SUCCESS)
            {
               veSetError("veResizeRenderTarget: failed to resize color texture %u", i);
               return result;
            }

            attachment->texture = newTex;
            veDestroyTexture(device, oldTex);
         }
      }

      // Resize resolve texture if present
      if (attachment->resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         VETextureInternal *tex = deviceInternal->getTexture(attachment->resolveTexture);
         if (tex)
         {
            VETextureDesc desc{};
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mipLevels = tex->mipLevels;
            desc.arrayLayers = tex->arrayLayers;
            desc.format = tex->format;
            desc.usage = tex->usage;
            desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;

            VETextureIndex oldTex = attachment->resolveTexture;
            VETextureIndex newTex = VE_INVALID_TEXTURE_INDEX;

            VEResult result = veCreateTexture(device, &desc, &newTex);
            if (result != VE_SUCCESS)
            {
               veSetError("veResizeRenderTarget: failed to resize resolve texture %u", i);
               return result;
            }

            attachment->resolveTexture = newTex;
            veDestroyTexture(device, oldTex);
         }
      }
   }

   // Depth attachment
   if (target->hasDepthAttachment)
   {
      VERenderTargetAttachment *attachment = &target->attachments[VE_DEPTH_ATTACHMENT_INDEX];
      if (attachment->texture != VE_INVALID_TEXTURE_INDEX)
      {
         VETextureInternal *tex = deviceInternal->getTexture(attachment->texture);
         if (tex)
         {
            VETextureDesc desc{};
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mipLevels = tex->mipLevels;
            desc.arrayLayers = tex->arrayLayers;
            desc.format = tex->format;
            desc.usage = tex->usage;
            desc.sampleCount = tex->sampleCount;

            VETextureIndex oldTex = attachment->texture;
            VETextureIndex newTex = VE_INVALID_TEXTURE_INDEX;

            VEResult result = veCreateTexture(device, &desc, &newTex);
            if (result != VE_SUCCESS)
            {
               veSetError("veResizeRenderTarget: failed to resize depth texture");
               return result;
            }

            attachment->texture = newTex;
            veDestroyTexture(device, oldTex);
         }
      }
   }

   // Stencil attachment (if separate from depth)
   if (target->hasStencilAttachment)
   {
      VERenderTargetAttachment *attachment = &target->attachments[VE_STENCIL_ATTACHMENT_INDEX];
      // Only resize if it's a different texture from depth
      if (attachment->texture != VE_INVALID_TEXTURE_INDEX &&
          (!target->hasDepthAttachment ||
           attachment->texture != target->attachments[VE_DEPTH_ATTACHMENT_INDEX].texture))
      {
         VETextureInternal *tex = deviceInternal->getTexture(attachment->texture);
         if (tex)
         {
            VETextureDesc desc{};
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mipLevels = tex->mipLevels;
            desc.arrayLayers = tex->arrayLayers;
            desc.format = tex->format;
            desc.usage = tex->usage;
            desc.sampleCount = tex->sampleCount;

            VETextureIndex oldTex = attachment->texture;
            VETextureIndex newTex = VE_INVALID_TEXTURE_INDEX;

            VEResult result = veCreateTexture(device, &desc, &newTex);
            if (result != VE_SUCCESS)
            {
               veSetError("veResizeRenderTarget: failed to resize stencil texture");
               return result;
            }

            attachment->texture = newTex;
            veDestroyTexture(device, oldTex);
         }
      }
   }

   // Update render area dimensions
   target->renderAreaWidth = width;
   target->renderAreaHeight = height;

   return VE_SUCCESS;
}

// =============================================================================
// Simple Render Target Creation
// =============================================================================

VERenderTarget veCreateSimpleRenderTarget(VEDevice *device, uint32_t width, uint32_t height,
                                          VkFormat colorFormat, VkFormat depthFormat,
                                          VEColor clearColor, float clearDepth,
                                          VETextureIndex *outColorTexture,
                                          VETextureIndex *outDepthTexture)
{
   VERenderTarget target = veCreateRenderTarget(width, height);

   if (!device || width == 0 || height == 0)
   {
      veSetError("veCreateSimpleRenderTarget: invalid parameters");
      return target;
   }

   // Create and add color texture
   if (colorFormat != VK_FORMAT_UNDEFINED)
   {
      VETextureIndex colorTex = VE_INVALID_TEXTURE_INDEX;
      VEResult result = veCreateTexture2D(device, width, height, colorFormat,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                              VK_IMAGE_USAGE_SAMPLED_BIT,
                                          "SimpleRT_Color", &colorTex);
      if (result == VE_SUCCESS && colorTex != VE_INVALID_TEXTURE_INDEX)
      {
         veRenderTargetAddColorAttachment(&target, colorTex, VK_ATTACHMENT_LOAD_OP_CLEAR, clearColor);
         if (outColorTexture)
         {
            *outColorTexture = colorTex;
         }
      }
   }

   // Create and add depth texture
   if (depthFormat != VK_FORMAT_UNDEFINED)
   {
      VETextureIndex depthTex = VE_INVALID_TEXTURE_INDEX;
      VEResult result = veCreateTexture2D(device, width, height, depthFormat,
                                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_SAMPLED_BIT,
                                          "SimpleRT_Depth", &depthTex);
      if (result == VE_SUCCESS && depthTex != VE_INVALID_TEXTURE_INDEX)
      {
         veRenderTargetSetDepthAttachment(&target, depthTex, VK_ATTACHMENT_LOAD_OP_CLEAR, clearDepth);
         if (outDepthTexture)
         {
            *outDepthTexture = depthTex;
         }
      }
   }

   return target;
}

// =============================================================================
// Blit Texture to Swapchain
// =============================================================================

VEResult veBlitTextureToSwapchain(VECommandBuffer *cmd, VETextureIndex texture,
                                  VESwapchain *swapchain, VkFilter filter)
{
   if (!cmd || texture == VE_INVALID_TEXTURE_INDEX || !swapchain)
   {
      veSetError("veBlitTextureToSwapchain: invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *cmdInternal = reinterpret_cast<VECommandBufferInternal *>(cmd);
   VESwapchainInternal *swapInternal = reinterpret_cast<VESwapchainInternal *>(swapchain);

   // Acquire next swapchain image
   VETextureIndex backbuffer = veAcquireNextImage(swapchain);
   if (backbuffer == VE_INVALID_TEXTURE_INDEX)
   {
      return VE_ERROR_SWAPCHAIN_OUT_OF_DATE;
   }
   (void)backbuffer; // We use the swapchain's currentImageIndex directly

   // Get source texture info
   VETextureInternal *srcTex = cmdInternal->device->getTexture(texture);
   if (!srcTex)
   {
      veSetError("veBlitTextureToSwapchain: invalid source texture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   uint32_t imageIndex = swapInternal->currentImageIndex;
   VkImage dstImage = swapInternal->images[imageIndex];

   // Transition source to transfer src
   veTransitionTextureForTransferSrc(cmd, texture);

   // Transition destination to transfer dst
   VkImageMemoryBarrier dstBarrier{};
   dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   dstBarrier.srcAccessMask = 0;
   dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   dstBarrier.image = dstImage;
   dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   dstBarrier.subresourceRange.baseMipLevel = 0;
   dstBarrier.subresourceRange.levelCount = 1;
   dstBarrier.subresourceRange.baseArrayLayer = 0;
   dstBarrier.subresourceRange.layerCount = 1;

   vkCmdPipelineBarrier(cmdInternal->commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &dstBarrier);

   // Blit from source texture to swapchain
   VkImageBlit blitRegion{};
   blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   blitRegion.srcSubresource.mipLevel = 0;
   blitRegion.srcSubresource.baseArrayLayer = 0;
   blitRegion.srcSubresource.layerCount = 1;
   blitRegion.srcOffsets[0] = {0, 0, 0};
   blitRegion.srcOffsets[1] = {static_cast<int32_t>(srcTex->width),
                               static_cast<int32_t>(srcTex->height), 1};

   blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   blitRegion.dstSubresource.mipLevel = 0;
   blitRegion.dstSubresource.baseArrayLayer = 0;
   blitRegion.dstSubresource.layerCount = 1;
   blitRegion.dstOffsets[0] = {0, 0, 0};
   blitRegion.dstOffsets[1] = {static_cast<int32_t>(swapInternal->width),
                               static_cast<int32_t>(swapInternal->height), 1};

   vkCmdBlitImage(cmdInternal->commandBuffer,
                  srcTex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  1, &blitRegion, filter);

   // Transition destination to present
   VkImageMemoryBarrier presentBarrier{};
   presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   presentBarrier.dstAccessMask = 0;
   presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
   presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   presentBarrier.image = dstImage;
   presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   presentBarrier.subresourceRange.baseMipLevel = 0;
   presentBarrier.subresourceRange.levelCount = 1;
   presentBarrier.subresourceRange.baseArrayLayer = 0;
   presentBarrier.subresourceRange.layerCount = 1;

   vkCmdPipelineBarrier(cmdInternal->commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

   return VE_SUCCESS;
}
