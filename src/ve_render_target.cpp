/**
 * @file ve_render_target.cpp
 * @brief Render Target Implementation (Offscreen Rendering)
 */

#include "ve_internal.h"
#include <cstring>

// =============================================================================
// VERenderTargetInternal Implementation
// =============================================================================

VERenderTargetInternal::~VERenderTargetInternal()
{
   destroy();
}

VEResult VERenderTargetInternal::create(VEDeviceInternal *deviceInternal, const VERenderTargetDesc *desc)
{
   if (!deviceInternal || !desc)
   {
      veSetError("Invalid parameters for render target creation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (desc->width == 0 || desc->height == 0)
   {
      veSetError("Render target dimensions must be greater than zero");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (desc->colorFormat == VK_FORMAT_UNDEFINED)
   {
      veSetError("Render target color format must be specified");
      return VE_ERROR_INVALID_PARAMETER;
   }

   device = deviceInternal;
   width = desc->width;
   height = desc->height;
   colorFormat = desc->colorFormat;
   depthFormat = desc->depthFormat;
   hasResolveTarget = desc->hasResolveTarget;

   // Determine sample count
   if (desc->sampleCount <= 1)
   {
      sampleCount = VK_SAMPLE_COUNT_1_BIT;
   }
   else
   {
      sampleCount = static_cast<VkSampleCountFlagBits>(desc->sampleCount);
   }

   // Store debug name
   if (desc->debugName)
   {
      strncpy(debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
      debugName[VE_MAX_DEBUG_NAME_LENGTH - 1] = '\0';
   }
   else
   {
      debugName[0] = '\0';
   }

   return createTextures();
}

VEResult VERenderTargetInternal::createTextures()
{
   // Create color texture
   VETextureDesc colorDesc{};
   colorDesc.width = width;
   colorDesc.height = height;
   colorDesc.depth = 1;
   colorDesc.mipLevels = 1;
   colorDesc.arrayLayers = 1;
   colorDesc.format = colorFormat;
   colorDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   colorDesc.sampleCount = static_cast<VkSampleCountFlags>(sampleCount);

   char colorName[VE_MAX_DEBUG_NAME_LENGTH + 16];
   if (debugName[0] != '\0')
   {
      snprintf(colorName, sizeof(colorName), "%s_color", debugName);
      colorDesc.debugName = colorName;
   }

   colorTexture = veCreateTexture(reinterpret_cast<VEDevice *>(device), &colorDesc);
   if (colorTexture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Failed to create render target color texture");
      return VE_ERROR_UNKNOWN;
   }

   // Create resolve texture if MSAA with resolve
   if (sampleCount > VK_SAMPLE_COUNT_1_BIT && hasResolveTarget)
   {
      VETextureDesc resolveDesc{};
      resolveDesc.width = width;
      resolveDesc.height = height;
      resolveDesc.depth = 1;
      resolveDesc.mipLevels = 1;
      resolveDesc.arrayLayers = 1;
      resolveDesc.format = colorFormat;
      resolveDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      resolveDesc.sampleCount = VK_SAMPLE_COUNT_1_BIT;

      char resolveName[VE_MAX_DEBUG_NAME_LENGTH + 16];
      if (debugName[0] != '\0')
      {
         snprintf(resolveName, sizeof(resolveName), "%s_resolve", debugName);
         resolveDesc.debugName = resolveName;
      }

      resolveTexture = veCreateTexture(reinterpret_cast<VEDevice *>(device), &resolveDesc);
      if (resolveTexture == VE_INVALID_TEXTURE_INDEX)
      {
         veSetError("Failed to create render target resolve texture");
         destroyTextures();
         return VE_ERROR_UNKNOWN;
      }
   }

   // Create depth texture if requested
   if (depthFormat != VK_FORMAT_UNDEFINED)
   {
      VETextureDesc depthDesc{};
      depthDesc.width = width;
      depthDesc.height = height;
      depthDesc.depth = 1;
      depthDesc.mipLevels = 1;
      depthDesc.arrayLayers = 1;
      depthDesc.format = depthFormat;
      depthDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      depthDesc.sampleCount = static_cast<VkSampleCountFlags>(sampleCount);

      char depthName[VE_MAX_DEBUG_NAME_LENGTH + 16];
      if (debugName[0] != '\0')
      {
         snprintf(depthName, sizeof(depthName), "%s_depth", debugName);
         depthDesc.debugName = depthName;
      }

      depthTexture = veCreateTexture(reinterpret_cast<VEDevice *>(device), &depthDesc);
      if (depthTexture == VE_INVALID_TEXTURE_INDEX)
      {
         veSetError("Failed to create render target depth texture");
         destroyTextures();
         return VE_ERROR_UNKNOWN;
      }
   }

   return VE_SUCCESS;
}

void VERenderTargetInternal::destroyTextures()
{
   if (device)
   {
      if (colorTexture != VE_INVALID_TEXTURE_INDEX)
      {
         veDestroyTexture(reinterpret_cast<VEDevice *>(device), colorTexture);
         colorTexture = VE_INVALID_TEXTURE_INDEX;
      }
      if (depthTexture != VE_INVALID_TEXTURE_INDEX)
      {
         veDestroyTexture(reinterpret_cast<VEDevice *>(device), depthTexture);
         depthTexture = VE_INVALID_TEXTURE_INDEX;
      }
      if (resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         veDestroyTexture(reinterpret_cast<VEDevice *>(device), resolveTexture);
         resolveTexture = VE_INVALID_TEXTURE_INDEX;
      }
   }
}

VEResult VERenderTargetInternal::resize(uint32_t newWidth, uint32_t newHeight)
{
   if (newWidth == 0 || newHeight == 0)
   {
      veSetError("Render target dimensions must be greater than zero");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (newWidth == width && newHeight == height)
   {
      return VE_SUCCESS;
   }

   destroyTextures();

   width = newWidth;
   height = newHeight;

   return createTextures();
}

void VERenderTargetInternal::destroy()
{
   destroyTextures();
   device = nullptr;
}

// =============================================================================
// Public API
// =============================================================================

VERenderTarget *veCreateRenderTarget(VEDevice *device, const VERenderTargetDesc *desc)
{
   if (!device || !desc)
   {
      veSetError("Invalid parameters for veCreateRenderTarget");
      return nullptr;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);

   VERenderTargetInternal *rt = new (std::nothrow) VERenderTargetInternal();
   if (!rt)
   {
      veSetError("Failed to allocate render target");
      return nullptr;
   }

   VEResult result = rt->create(deviceInternal, desc);
   if (result != VE_SUCCESS)
   {
      delete rt;
      return nullptr;
   }

   return reinterpret_cast<VERenderTarget *>(rt);
}

void veDestroyRenderTarget(VERenderTarget *renderTarget)
{
   if (!renderTarget)
      return;

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   delete internal;
}

VEResult veResizeRenderTarget(VERenderTarget *renderTarget, uint32_t width, uint32_t height)
{
   if (!renderTarget)
   {
      veSetError("Render target cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   return internal->resize(width, height);
}

VETextureIndex veGetRenderTargetColorTexture(VERenderTarget *renderTarget)
{
   if (!renderTarget)
      return VE_INVALID_TEXTURE_INDEX;

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   return internal->colorTexture;
}

VETextureIndex veGetRenderTargetDepthTexture(VERenderTarget *renderTarget)
{
   if (!renderTarget)
      return VE_INVALID_TEXTURE_INDEX;

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   return internal->depthTexture;
}

VETextureIndex veGetRenderTargetResolveTexture(VERenderTarget *renderTarget)
{
   if (!renderTarget)
      return VE_INVALID_TEXTURE_INDEX;

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   return internal->resolveTexture;
}

VEResult veGetRenderTargetSize(VERenderTarget *renderTarget, uint32_t *outWidth, uint32_t *outHeight)
{
   if (!renderTarget)
   {
      veSetError("Render target cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);

   if (outWidth)
      *outWidth = internal->width;
   if (outHeight)
      *outHeight = internal->height;

   return VE_SUCCESS;
}

VkFormat veGetRenderTargetColorFormat(VERenderTarget *renderTarget)
{
   if (!renderTarget)
      return VK_FORMAT_UNDEFINED;

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   return internal->colorFormat;
}

VkFormat veGetRenderTargetDepthFormat(VERenderTarget *renderTarget)
{
   if (!renderTarget)
      return VK_FORMAT_UNDEFINED;

   VERenderTargetInternal *internal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   return internal->depthFormat;
}

VEResult veBlitToSwapchain(VECommandBuffer *cmd, VERenderTarget *renderTarget,
                           VESwapchain *swapchain, VkFilter filter)
{
   if (!cmd || !renderTarget || !swapchain)
   {
      veSetError("Invalid parameters for veBlitToSwapchain");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *cmdInternal = reinterpret_cast<VECommandBufferInternal *>(cmd);
   VERenderTargetInternal *rtInternal = reinterpret_cast<VERenderTargetInternal *>(renderTarget);
   VESwapchainInternal *swapInternal = reinterpret_cast<VESwapchainInternal *>(swapchain);

   // Acquire next swapchain image
   VETextureIndex backbuffer = veAcquireNextImage(swapchain);
   if (backbuffer == VE_INVALID_TEXTURE_INDEX)
   {
      return VE_ERROR_SWAPCHAIN_OUT_OF_DATE;
   }
   (void)backbuffer; // We use the swapchain's currentImageIndex directly

   // Determine source texture (use resolve if available for MSAA, otherwise color)
   VETextureIndex srcTexture = (rtInternal->resolveTexture != VE_INVALID_TEXTURE_INDEX)
                                   ? rtInternal->resolveTexture
                                   : rtInternal->colorTexture;

   if (srcTexture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Render target has no valid source texture for blit");
      return VE_ERROR_INVALID_PARAMETER;
   }

   uint32_t imageIndex = swapInternal->currentImageIndex;
   VkImage dstImage = swapInternal->images[imageIndex];

   // Get source texture info
   VETextureInternal *srcTex = cmdInternal->device->getTexture(srcTexture);
   if (!srcTex)
   {
      veSetError("Failed to get source texture for blit");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Transition source to transfer src
   veTransitionTextureForTransferSrc(cmd, srcTexture);

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

   // Blit from render target to swapchain
   VkImageBlit blitRegion{};
   blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   blitRegion.srcSubresource.mipLevel = 0;
   blitRegion.srcSubresource.baseArrayLayer = 0;
   blitRegion.srcSubresource.layerCount = 1;
   blitRegion.srcOffsets[0] = {0, 0, 0};
   blitRegion.srcOffsets[1] = {static_cast<int32_t>(rtInternal->width),
                               static_cast<int32_t>(rtInternal->height), 1};

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

