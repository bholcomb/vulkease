/**
 * @file vulkease_vk.h
 * @brief Native Vulkan interoperability for VulkEase.
 */

#ifndef VULKEASE_VK_H
#define VULKEASE_VK_H

#include "vulkease.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct VEExternalTextureDesc
{
   VkImage image;
   VkFormat format;
   uint32_t width;
   uint32_t height;
   uint32_t depth;
   uint32_t mipLevels;
   uint32_t arrayLayers;
   VkImageViewType viewType;
   VkImageAspectFlags aspectMask;
   const char *debugName;
} VEExternalTextureDesc;

VULKEASE_API VkInstance veGetVkInstance(VEContext *context);
VULKEASE_API VkPhysicalDevice veGetVkPhysicalDevice(VEDevice *device);
VULKEASE_API VkDevice veGetVkDevice(VEDevice *device);
VULKEASE_API VkQueue veGetVkGraphicsQueue(VEDevice *device);
VULKEASE_API VkQueue veGetVkComputeQueue(VEDevice *device);
VULKEASE_API VkQueue veGetVkTransferQueue(VEDevice *device);
VULKEASE_API VkFence veGetVkCommandBufferFence(VECommandBuffer *cmd);
VULKEASE_API VkBuffer veGetVkBufferFromAddress(VEDevice *device, VEBufferAddress address);
VULKEASE_API VkImage veGetVkImageFromTexture(VEDevice *device, VETexture texture);
VULKEASE_API VkImageView veGetVkImageView(VEDevice *device, VETextureView view);
VULKEASE_API VkSampler veGetVkSamplerFromIndex(VEDevice *device, VESamplerIndex sampler);
VULKEASE_API VkSwapchainKHR veGetVkSwapchain(VESwapchain *swapchain);
VULKEASE_API VkImage veGetVkSwapchainImage(VESwapchain *swapchain, uint32_t imageIndex);
VULKEASE_API VkImageView veGetVkSwapchainImageView(VESwapchain *swapchain, uint32_t imageIndex);

VULKEASE_API VEResult veImportExternalTexture(VEDevice *device, const VEExternalTextureDesc *desc,
                                               VETexture *outIndex);
VULKEASE_API VEResult veReleaseExternalTexture(VEDevice *device, VETexture index);

#ifdef __cplusplus
}
#endif

#endif
