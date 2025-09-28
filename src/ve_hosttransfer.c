#include "ve_internal.h"

// Helper function to get format block size in bytes
static uint32_t getFormatBlockSize(VkFormat format)
{
   switch (format)
   {
   // 8-bit per channel formats
   case VK_FORMAT_R8_UNORM:
   case VK_FORMAT_R8_SNORM:
   case VK_FORMAT_R8_UINT:
   case VK_FORMAT_R8_SINT:
   case VK_FORMAT_R8_SRGB:
      return 1;

   case VK_FORMAT_R8G8_UNORM:
   case VK_FORMAT_R8G8_SNORM:
   case VK_FORMAT_R8G8_UINT:
   case VK_FORMAT_R8G8_SINT:
   case VK_FORMAT_R8G8_SRGB:
      return 2;

   case VK_FORMAT_R8G8B8_UNORM:
   case VK_FORMAT_R8G8B8_SNORM:
   case VK_FORMAT_R8G8B8_UINT:
   case VK_FORMAT_R8G8B8_SINT:
   case VK_FORMAT_R8G8B8_SRGB:
   case VK_FORMAT_B8G8R8_UNORM:
   case VK_FORMAT_B8G8R8_SNORM:
   case VK_FORMAT_B8G8R8_UINT:
   case VK_FORMAT_B8G8R8_SINT:
   case VK_FORMAT_B8G8R8_SRGB:
      return 3;

   case VK_FORMAT_R8G8B8A8_UNORM:
   case VK_FORMAT_R8G8B8A8_SNORM:
   case VK_FORMAT_R8G8B8A8_UINT:
   case VK_FORMAT_R8G8B8A8_SINT:
   case VK_FORMAT_R8G8B8A8_SRGB:
   case VK_FORMAT_B8G8R8A8_UNORM:
   case VK_FORMAT_B8G8R8A8_SNORM:
   case VK_FORMAT_B8G8R8A8_UINT:
   case VK_FORMAT_B8G8R8A8_SINT:
   case VK_FORMAT_B8G8R8A8_SRGB:
   case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
   case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
   case VK_FORMAT_A8B8G8R8_UINT_PACK32:
   case VK_FORMAT_A8B8G8R8_SINT_PACK32:
   case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
      return 4;

   // 16-bit per channel formats
   case VK_FORMAT_R16_UNORM:
   case VK_FORMAT_R16_SNORM:
   case VK_FORMAT_R16_UINT:
   case VK_FORMAT_R16_SINT:
   case VK_FORMAT_R16_SFLOAT:
      return 2;

   case VK_FORMAT_R16G16_UNORM:
   case VK_FORMAT_R16G16_SNORM:
   case VK_FORMAT_R16G16_UINT:
   case VK_FORMAT_R16G16_SINT:
   case VK_FORMAT_R16G16_SFLOAT:
      return 4;

   case VK_FORMAT_R16G16B16_UNORM:
   case VK_FORMAT_R16G16B16_SNORM:
   case VK_FORMAT_R16G16B16_UINT:
   case VK_FORMAT_R16G16B16_SINT:
   case VK_FORMAT_R16G16B16_SFLOAT:
      return 6;

   case VK_FORMAT_R16G16B16A16_UNORM:
   case VK_FORMAT_R16G16B16A16_SNORM:
   case VK_FORMAT_R16G16B16A16_UINT:
   case VK_FORMAT_R16G16B16A16_SINT:
   case VK_FORMAT_R16G16B16A16_SFLOAT:
      return 8;

   // 32-bit per channel formats
   case VK_FORMAT_R32_UINT:
   case VK_FORMAT_R32_SINT:
   case VK_FORMAT_R32_SFLOAT:
      return 4;

   case VK_FORMAT_R32G32_UINT:
   case VK_FORMAT_R32G32_SINT:
   case VK_FORMAT_R32G32_SFLOAT:
      return 8;

   case VK_FORMAT_R32G32B32_UINT:
   case VK_FORMAT_R32G32B32_SINT:
   case VK_FORMAT_R32G32B32_SFLOAT:
      return 12;

   case VK_FORMAT_R32G32B32A32_UINT:
   case VK_FORMAT_R32G32B32A32_SINT:
   case VK_FORMAT_R32G32B32A32_SFLOAT:
      return 16;

   // Depth/stencil formats
   case VK_FORMAT_D16_UNORM:
      return 2;
   case VK_FORMAT_D32_SFLOAT:
      return 4;
   case VK_FORMAT_D24_UNORM_S8_UINT:
   case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return 4;

   // Compressed formats - return 0 to indicate unsupported for host copy
   default:
      return 0;
   }
}

// Helper function to validate host image copy support
static VEResult validateHostImageCopySupport(VEDeviceInternal *device, VETextureInternal *texture)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!texture || !texture->isValid)
   {
      veSetError("Invalid texture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Check if host image copy is supported
   if (!device->features.hostImageCopy)
   {
      veSetError("Host image copy not supported on this device");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   // Check if the texture format is supported for host copy
   if (getFormatBlockSize(texture->format) == 0)
   {
      veSetError("Texture format not supported for host image copy");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   // Check if texture has HOST_TRANSFER usage
   if (!(texture->usage & VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT))
   {
      veSetError("Texture must be created with VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT for host image copy");
      return VE_ERROR_INVALID_PARAMETER;
   }

   return VE_SUCCESS;
}

/**
 * Copy data from host memory to texture using VK_EXT_host_image_copy
 */
VULKEASE_API VEResult veHostCopyToTexture(VEDevice *device, VETextureIndex textureIndex, const void *srcData,
                                          size_t dataSize, uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
                                          uint32_t width, uint32_t height, uint32_t depth)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!srcData)
   {
      veSetError("Source data cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (dataSize == 0)
   {
      veSetError("Data size cannot be zero");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = veGetTexture(deviceInternal, textureIndex);

   // Validate support and parameters
   VEResult result = validateHostImageCopySupport(deviceInternal, texture);
   if (result != VE_SUCCESS)
   {
      return result;
   }

   // Validate copy region bounds
   if (offsetX + width > texture->width || offsetY + height > texture->height || offsetZ + depth > texture->depth)
   {
      veSetError("Copy region exceeds texture bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Calculate expected data size
   uint32_t formatSize = getFormatBlockSize(texture->format);
   size_t expectedSize = (size_t)width * height * depth * formatSize;
   if (dataSize < expectedSize)
   {
      veSetError("Source data size (%zu bytes) is less than required (%zu bytes)", dataSize, expectedSize);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Prepare copy region
   VkMemoryToImageCopyEXT copyRegion = {0};
   copyRegion.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT;
   copyRegion.pHostPointer = srcData;
   copyRegion.memoryRowLength = width;    // Tightly packed
   copyRegion.memoryImageHeight = height; // Tightly packed
   copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copyRegion.imageSubresource.mipLevel = 0;
   copyRegion.imageSubresource.baseArrayLayer = 0;
   copyRegion.imageSubresource.layerCount = 1;
   copyRegion.imageOffset.x = (int32_t)offsetX;
   copyRegion.imageOffset.y = (int32_t)offsetY;
   copyRegion.imageOffset.z = (int32_t)offsetZ;
   copyRegion.imageExtent.width = width;
   copyRegion.imageExtent.height = height;
   copyRegion.imageExtent.depth = depth;

   // Setup copy info
   VkCopyMemoryToImageInfoEXT copyInfo = {0};
   copyInfo.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT;
   copyInfo.flags = 0;
   copyInfo.dstImage = texture->image;
   copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL; // Host copy requires GENERAL layout
   copyInfo.regionCount = 1;
   copyInfo.pRegions = &copyRegion;

   // Perform the host copy
   VkResult vkResult = veFuncs.vkCopyMemoryToImageEXT(deviceInternal->device, &copyInfo);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to copy memory to image (VkResult: %d)", vkResult);
      return VE_ERROR_TRANSFER_FAILED;
   }

   return VE_SUCCESS;
}

/**
 * Copy data from texture to host memory using VK_EXT_host_image_copy
 */
VULKEASE_API VEResult veHostCopyFromTexture(VEDevice *device, VETextureIndex textureIndex, void *dstData,
                                            size_t dataSize, uint32_t offsetX, uint32_t offsetY, uint32_t offsetZ,
                                            uint32_t width, uint32_t height, uint32_t depth)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!dstData)
   {
      veSetError("Destination data cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (dataSize == 0)
   {
      veSetError("Data size cannot be zero");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = veGetTexture(deviceInternal, textureIndex);

   // Validate support and parameters
   VEResult result = validateHostImageCopySupport(deviceInternal, texture);
   if (result != VE_SUCCESS)
   {
      return result;
   }

   // Validate copy region bounds
   if (offsetX + width > texture->width || offsetY + height > texture->height || offsetZ + depth > texture->depth)
   {
      veSetError("Copy region exceeds texture bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Calculate expected data size
   uint32_t formatSize = getFormatBlockSize(texture->format);
   size_t expectedSize = (size_t)width * height * depth * formatSize;
   if (dataSize < expectedSize)
   {
      veSetError("Destination buffer size (%zu bytes) is less than required (%zu bytes)", dataSize, expectedSize);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Prepare copy region
   VkImageToMemoryCopyEXT copyRegion = {0};
   copyRegion.sType = VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT;
   copyRegion.pHostPointer = dstData;
   copyRegion.memoryRowLength = width;    // Tightly packed
   copyRegion.memoryImageHeight = height; // Tightly packed
   copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copyRegion.imageSubresource.mipLevel = 0;
   copyRegion.imageSubresource.baseArrayLayer = 0;
   copyRegion.imageSubresource.layerCount = 1;
   copyRegion.imageOffset.x = (int32_t)offsetX;
   copyRegion.imageOffset.y = (int32_t)offsetY;
   copyRegion.imageOffset.z = (int32_t)offsetZ;
   copyRegion.imageExtent.width = width;
   copyRegion.imageExtent.height = height;
   copyRegion.imageExtent.depth = depth;

   // Setup copy info
   VkCopyImageToMemoryInfoEXT copyInfo = {0};
   copyInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT;
   copyInfo.flags = 0;
   copyInfo.srcImage = texture->image;
   copyInfo.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL; // Host copy requires GENERAL layout
   copyInfo.regionCount = 1;
   copyInfo.pRegions = &copyRegion;

   // Perform the host copy
   VkResult vkResult = veFuncs.vkCopyImageToMemoryEXT(deviceInternal->device, &copyInfo);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to copy image to memory (VkResult: %d)", vkResult);
      return VE_ERROR_TRANSFER_FAILED;
   }

   return VE_SUCCESS;
}

/**
 * Convenience function to copy entire texture to host memory
 */
VEResult veHostCopyEntireTexture(VEDevice *device, VETextureIndex textureIndex, void *dstData, size_t dataSize)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = veGetTexture(deviceInternal, textureIndex);

   if (!texture || !texture->isValid)
   {
      veSetError("Invalid texture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   return veHostCopyFromTexture(device, textureIndex, dstData, dataSize, 0, 0, 0, texture->width, texture->height,
                                texture->depth);
}

/**
 * Convenience function to update entire texture from host memory
 */
VEResult veHostUpdateEntireTexture(VEDevice *device, VETextureIndex textureIndex, const void *srcData, size_t dataSize)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = veGetTexture(deviceInternal, textureIndex);

   if (!texture || !texture->isValid)
   {
      veSetError("Invalid texture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   return veHostCopyToTexture(device, textureIndex, srcData, dataSize, 0, 0, 0, texture->width, texture->height,
                              texture->depth);
}