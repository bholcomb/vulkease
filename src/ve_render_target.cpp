/**
 * @file ve_render_target.cpp
 * @brief Render Target Helper Functions
 */

#include "ve_internal.h"
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

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
   std::vector<VETextureView *> attachmentViews;
   for (uint32_t i = 0; i < target->colorAttachmentCount; ++i)
   {
      attachmentViews.push_back(&target->attachments[i].view);
      attachmentViews.push_back(&target->attachments[i].resolveView);
   }
   if (target->hasDepthAttachment)
      attachmentViews.push_back(&target->attachments[VE_DEPTH_ATTACHMENT_INDEX].view);
   if (target->hasStencilAttachment)
      attachmentViews.push_back(&target->attachments[VE_STENCIL_ATTACHMENT_INDEX].view);

   std::unordered_map<VETexture, VETexture> replacementTextures;
   std::unordered_map<VETextureView, VETextureView> replacementViews;
   std::vector<std::pair<VETextureView *, VETextureView>> updates;

   auto cleanupReplacements = [&]()
   {
      for (const auto &entry : replacementTextures)
         veDestroyTextureImmediate(deviceInternal, entry.second);
   };

   for (VETextureView *attachmentView : attachmentViews)
   {
      if (*attachmentView == VE_INVALID_TEXTURE_VIEW)
         continue;

      auto existingReplacement = replacementViews.find(*attachmentView);
      if (existingReplacement != replacementViews.end())
      {
         updates.emplace_back(attachmentView, existingReplacement->second);
         continue;
      }

      VETextureViewInternal *oldView = deviceInternal->getTextureView(*attachmentView);
      VETextureInternal *oldTexture = oldView ? deviceInternal->getTexture(oldView->texture) : nullptr;
      if (!oldView || !oldTexture || oldTexture->isExternal)
      {
         cleanupReplacements();
         veSetError("veResizeRenderTarget: attachment view %u is invalid or externally owned", *attachmentView);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VETexture newTexture = VE_INVALID_TEXTURE;
      auto existingTexture = replacementTextures.find(oldView->texture);
      if (existingTexture == replacementTextures.end())
      {
         VETextureDesc desc{};
         desc.width = width;
         desc.height = height;
         desc.depth = oldTexture->depth;
         desc.mipLevels = oldTexture->mipLevels;
         desc.arrayLayers = oldTexture->arrayLayers;
         desc.format = oldTexture->format;
         desc.usage = oldTexture->usage;
         desc.sampleCount = oldTexture->sampleCount;
         desc.debugName = oldTexture->debugName;
         VEResult result = veCreateTexture(device, &desc, &newTexture);
         if (result != VE_SUCCESS)
         {
            cleanupReplacements();
            veSetError("veResizeRenderTarget: failed to recreate attachment texture");
            return result;
         }
         replacementTextures.emplace(oldView->texture, newTexture);
      }
      else
      {
         newTexture = existingTexture->second;
      }

      VETextureView newView = VE_INVALID_TEXTURE_VIEW;
      if (*attachmentView == oldTexture->defaultView)
      {
         newView = veGetDefaultTextureView(device, newTexture);
      }
      else
      {
         VETextureViewDesc viewDesc{};
         viewDesc.viewType = oldView->viewType;
         viewDesc.format = oldView->format;
         viewDesc.components = oldView->components;
         viewDesc.aspectMask = oldView->subresourceRange.aspectMask;
         viewDesc.baseMipLevel = oldView->subresourceRange.baseMipLevel;
         viewDesc.mipLevelCount = oldView->subresourceRange.levelCount;
         viewDesc.baseArrayLayer = oldView->subresourceRange.baseArrayLayer;
         viewDesc.arrayLayerCount = oldView->subresourceRange.layerCount;
         VEResult result = veCreateTextureView(device, newTexture, &viewDesc, &newView);
         if (result != VE_SUCCESS)
         {
            cleanupReplacements();
            veSetError("veResizeRenderTarget: failed to recreate attachment view");
            return result;
         }
      }

      replacementViews.emplace(*attachmentView, newView);
      updates.emplace_back(attachmentView, newView);
   }

   for (const auto &update : updates)
      *update.first = update.second;
   for (const auto &entry : replacementTextures)
      veDestroyTexture(device, entry.first);

   // Update render area dimensions
   target->renderAreaWidth = width;
   target->renderAreaHeight = height;

   return VE_SUCCESS;
}

// =============================================================================
// Simple Render Target Creation
// =============================================================================

VERenderTarget veCreateSimpleRenderTarget(VEDevice *device, uint32_t width, uint32_t height, VkFormat colorFormat,
                                          VkFormat depthFormat, VEColor clearColor, float clearDepth,
                                          VETexture *outColorTexture, VETexture *outDepthTexture)
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
      VETexture colorTex = VE_INVALID_TEXTURE;
      VEResult result = veCreateTexture2D(device, width, height, colorFormat,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                              VK_IMAGE_USAGE_SAMPLED_BIT,
                                          "SimpleRT_Color", &colorTex);
      if (result == VE_SUCCESS && colorTex != VE_INVALID_TEXTURE)
      {
         veRenderTargetAddColorAttachment(&target, veGetDefaultTextureView(device, colorTex),
                                          VK_ATTACHMENT_LOAD_OP_CLEAR, clearColor);
         if (outColorTexture)
         {
            *outColorTexture = colorTex;
         }
      }
   }

   // Create and add depth texture
   if (depthFormat != VK_FORMAT_UNDEFINED)
   {
      VETexture depthTex = VE_INVALID_TEXTURE;
      VEResult result = veCreateTexture2D(device, width, height, depthFormat,
                                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                          "SimpleRT_Depth", &depthTex);
      if (result == VE_SUCCESS && depthTex != VE_INVALID_TEXTURE)
      {
         veRenderTargetSetDepthAttachment(&target, veGetDefaultTextureView(device, depthTex),
                                          VK_ATTACHMENT_LOAD_OP_CLEAR, clearDepth);
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

VEResult veBlitTextureToSwapchain(VECommandBuffer *cmd, VETexture texture, VESwapchain *swapchain, VkFilter filter)
{
   if (!cmd || texture == VE_INVALID_TEXTURE || !swapchain)
   {
      veSetError("veBlitTextureToSwapchain: invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *cmdInternal = reinterpret_cast<VECommandBufferInternal *>(cmd);
   VESwapchainInternal *swapInternal = reinterpret_cast<VESwapchainInternal *>(swapchain);

   // Acquire next swapchain image
   VETexture backbuffer = veAcquireNextImage(swapchain);
   if (backbuffer == VE_INVALID_TEXTURE)
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

   vkCmdPipelineBarrier(cmdInternal->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &dstBarrier);

   // Blit from source texture to swapchain
   VkImageBlit blitRegion{};
   blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   blitRegion.srcSubresource.mipLevel = 0;
   blitRegion.srcSubresource.baseArrayLayer = 0;
   blitRegion.srcSubresource.layerCount = 1;
   blitRegion.srcOffsets[0] = {0, 0, 0};
   blitRegion.srcOffsets[1] = {static_cast<int32_t>(srcTex->width), static_cast<int32_t>(srcTex->height), 1};

   blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   blitRegion.dstSubresource.mipLevel = 0;
   blitRegion.dstSubresource.baseArrayLayer = 0;
   blitRegion.dstSubresource.layerCount = 1;
   blitRegion.dstOffsets[0] = {0, 0, 0};
   blitRegion.dstOffsets[1] = {static_cast<int32_t>(swapInternal->width), static_cast<int32_t>(swapInternal->height),
                               1};

   vkCmdBlitImage(cmdInternal->commandBuffer, srcTex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, filter);

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

   vkCmdPipelineBarrier(cmdInternal->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

   return VE_SUCCESS;
}
