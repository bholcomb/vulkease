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

/**
 * @brief Get the underlying VkInstance handle.
 *
 * Provides access to the raw Vulkan instance for custom extension use or
 * interop with other Vulkan libraries.
 *
 * @param[in] context Valid VulkEase context.
 *
 * @return VkInstance handle, or VK_NULL_HANDLE if context is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Do not destroy it directly.
 *          Lifetime matches the owning VEContext.
 */
VULKEASE_API VkInstance veGetVkInstance(VEContext *context);
/**
 * @brief Get the underlying VkPhysicalDevice handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkPhysicalDevice handle, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkPhysicalDevice veGetVkPhysicalDevice(VEDevice *device);
/**
 * @brief Get the underlying VkDevice handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkDevice handle, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Do not destroy it directly.
 *          Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkDevice veGetVkDevice(VEDevice *device);
/**
 * @brief Get the graphics queue handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkQueue handle for graphics operations, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkQueue veGetVkGraphicsQueue(VEDevice *device);
/**
 * @brief Get the compute queue handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkQueue handle for compute operations, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkQueue veGetVkComputeQueue(VEDevice *device);
/**
 * @brief Get the transfer queue handle.
 *
 * @param[in] device Valid VulkEase device.
 *
 * @return VkQueue handle for transfer operations, or VK_NULL_HANDLE if device is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.
 */
VULKEASE_API VkQueue veGetVkTransferQueue(VEDevice *device);
/**
 * @brief Get the fence associated with a command buffer.
 *
 * Each command buffer has an associated fence for synchronization purposes.
 *
 * @param[in] cmd Valid VulkEase command buffer.
 *
 * @return VkFence handle, or VK_NULL_HANDLE if cmd is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the owning VECommandBuffer.
 */
VULKEASE_API VkFence veGetVkCommandBufferFence(VECommandBuffer *cmd);
/**
 * @brief Get the VkBuffer handle from a buffer device address.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] address Buffer device address obtained from veCreateBuffer().
 *
 * @return VkBuffer handle, or VK_NULL_HANDLE if device is NULL or address is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the buffer.
 */
VULKEASE_API VkBuffer veGetVkBufferFromAddress(VEDevice *device, VEBufferAddress address);
/**
 * @brief Get the VkImage handle from a texture index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture index obtained from veCreateTexture().
 *
 * @return VkImage handle, or VK_NULL_HANDLE if device is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the texture.
 */
VULKEASE_API VkImage veGetVkImageFromTexture(VEDevice *device, VETexture texture);
/**
 * @brief Get the native Vulkan image view for a VulkEase texture view.
 *
 * The returned handle is borrowed. It remains owned by VulkEase and must not
 * be destroyed by the caller. It becomes invalid when the texture view, its
 * owning texture, or the device is destroyed.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] view Valid texture view.
 * @return The underlying VkImageView, or VK_NULL_HANDLE for an invalid handle.
 */
VULKEASE_API VkImageView veGetVkImageView(VEDevice *device, VETextureView view);
/**
 * @brief Get the VkSampler handle from a sampler index.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] sampler Sampler index obtained from veCreateSampler().
 *
 * @return VkSampler handle, or VK_NULL_HANDLE if device is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the sampler.
 */
VULKEASE_API VkSampler veGetVkSamplerFromIndex(VEDevice *device, VESamplerIndex sampler);
/**
 * @brief Get the VkSwapchainKHR handle from a swapchain.
 *
 * @param[in] swapchain Valid VulkEase swapchain.
 *
 * @return VkSwapchainKHR handle, or VK_NULL_HANDLE if swapchain is NULL.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the swapchain.
 */
VULKEASE_API VkSwapchainKHR veGetVkSwapchain(VESwapchain *swapchain);
/**
 * @brief Get a swapchain image by index.
 *
 * @param[in] swapchain Valid VulkEase swapchain.
 * @param[in] imageIndex Index of the swapchain image (0 to image count - 1).
 *
 * @return VkImage handle, or VK_NULL_HANDLE if swapchain is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the swapchain.
 */
VULKEASE_API VkImage veGetVkSwapchainImage(VESwapchain *swapchain, uint32_t imageIndex);
/**
 * @brief Get a swapchain image view by index.
 *
 * @param[in] swapchain Valid VulkEase swapchain.
 * @param[in] imageIndex Index of the swapchain image (0 to image count - 1).
 *
 * @return VkImageView handle, or VK_NULL_HANDLE if swapchain is NULL or index is invalid.
 *
 * @warning The returned handle is owned by VulkEase. Lifetime matches the swapchain.
 */
VULKEASE_API VkImageView veGetVkSwapchainImageView(VESwapchain *swapchain, uint32_t imageIndex);

/**
 * @brief Import an external VkImage as a VulkEase texture.
 *
 * Creates a VulkEase texture wrapper around an externally-owned VkImage.
 * This is useful for integrating with VR runtimes (OpenXR, OpenVR) or other
 * Vulkan libraries that provide their own images.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] desc External texture descriptor with image handle and metadata.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device, desc, or outIndex is NULL
 *         - VE_ERROR_INVALID_PARAMETER if desc->image is VK_NULL_HANDLE
 *         - VE_ERROR_OUT_OF_MEMORY if no free texture slots available
 *
 * @note The VkImage is NOT owned by VulkEase. The caller must ensure the image
 *       remains valid for the lifetime of the imported texture and must destroy
 *       the VkImage after calling veReleaseExternalTexture().
 * @note VulkEase creates and owns the VkImageView for the imported image.
 *
 * @see VEExternalTextureDesc, veReleaseExternalTexture
 */
VULKEASE_API VEResult veImportExternalTexture(VEDevice *device, const VEExternalTextureDesc *desc,
                                               VETexture *outIndex);
/**
 * @brief Release an imported external texture.
 *
 * Releases the VulkEase resources associated with an imported texture (VkImageView,
 * descriptor slot) but does NOT destroy the underlying VkImage since it is externally owned.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] index Imported texture to release. VE_INVALID_TEXTURE is a no-op.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_INVALID_PARAMETER if device is NULL or index is invalid
 *
 * @note This function waits only for prior VulkEase submissions that may use
 *       the imported texture. On success all VulkEase-created views are gone
 *       and the caller may destroy or reuse the external VkImage.
 * @note Non-external and swapchain textures are rejected. Native submissions
 *       made through Vulkan escape hatches must be synchronized by the caller.
 *
 * @see veImportExternalTexture
 */
VULKEASE_API VEResult veReleaseExternalTexture(VEDevice *device, VETexture index);

#ifdef __cplusplus
}
#endif

#endif
