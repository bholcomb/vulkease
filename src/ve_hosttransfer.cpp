#include "ve_internal.h"

#include <limits>

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

// Host image copy requires VK_IMAGE_LAYOUT_GENERAL as the current layout.
// We ensure this via a short GPU-side layout transition (vkCmdPipelineBarrier2) and update the tracked layout.
static VEResult ensureTextureInGeneralLayout(VEDevice *device, VETextureIndex textureIndex, VETextureInternal *texture)
{
   if (!device || !texture || !texture->isValid)
      return VE_ERROR_INVALID_PARAMETER;

   if (texture->currentLayout == VK_IMAGE_LAYOUT_GENERAL)
      return VE_SUCCESS;

   VECommandBuffer *cmd = NULL;
   VEResult r = veBeginCommandBuffer(device, &cmd);
   if (r != VE_SUCCESS || !cmd)
      return r;

   r = veTransitionTextureToLayout(cmd, textureIndex, VK_IMAGE_LAYOUT_GENERAL);
   if (r != VE_SUCCESS)
   {
      (void)veReleaseCommandBuffer(cmd);
      return r;
   }

   r = veEndCommandBuffer(cmd);
   if (r != VE_SUCCESS)
   {
      (void)veReleaseCommandBuffer(cmd);
      return r;
   }

   VESubmitInfo submitInfo{};
   submitInfo.waitForCompletion = true;
   r = veSubmitCommandBuffer(cmd, &submitInfo);
   (void)veReleaseCommandBuffer(cmd);
   return r;
}

// Helper to calculate mip level dimensions
static uint32_t getMipDimension(uint32_t baseDim, uint32_t mipLevel)
{
   uint32_t dim = baseDim >> mipLevel;
   return dim > 0 ? dim : 1;
}

static bool calculateCopySize(uint32_t width, uint32_t height, uint32_t depth, uint32_t layerCount,
                              uint32_t formatSize, size_t *outSize)
{
   size_t size = formatSize;
   const uint32_t factors[] = {width, height, depth, layerCount};
   for (uint32_t factor : factors)
   {
      if (factor == 0 || size > std::numeric_limits<size_t>::max() / factor)
         return false;
      size *= factor;
   }
   *outSize = size;
   return true;
}

/**
 * Copy data from host memory to texture using VK_EXT_host_image_copy
 */
VULKEASE_API VEResult veHostWriteTextureRegion(VEDevice *device, VETextureIndex textureIndex, const void *srcData,
                                               size_t dataSize, const VEBufferTextureCopyRegion *region)
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
   VETextureInternal *texture = deviceInternal->getTexture(textureIndex);

   // Validate support and parameters
   VEResult result = validateHostImageCopySupport(deviceInternal, texture);
   if (result != VE_SUCCESS)
   {
      return result;
   }

   // Extract region parameters with defaults for NULL region
   uint32_t mipLevel = region ? region->mipLevel : 0;
   uint32_t arrayLayer = region ? region->arrayLayer : 0;
   uint32_t layerCount = region ? (region->layerCount > 0 ? region->layerCount : 1) : 1;

   if (mipLevel >= texture->mipLevels)
   {
      veSetError("Mip level %u exceeds texture mip levels (%u)", mipLevel, texture->mipLevels);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Calculate mip-adjusted texture dimensions
   uint32_t mipWidth = getMipDimension(texture->width, mipLevel);
   uint32_t mipHeight = getMipDimension(texture->height, mipLevel);
   uint32_t mipDepth = getMipDimension(texture->depth, mipLevel);

   uint32_t offsetX = region ? region->textureOffsetX : 0;
   uint32_t offsetY = region ? region->textureOffsetY : 0;
   uint32_t offsetZ = region ? region->textureOffsetZ : 0;
   uint32_t width = region ? region->width : mipWidth;
   uint32_t height = region ? region->height : mipHeight;
   uint32_t depth = region ? region->depth : mipDepth;

   // Validate array layer range
   if (arrayLayer >= texture->arrayLayers || layerCount > texture->arrayLayers - arrayLayer)
   {
      veSetError("Array layer range exceeds texture array layers (%u)", texture->arrayLayers);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Validate copy region bounds against mip dimensions
   if (width == 0 || height == 0 || depth == 0 || offsetX >= mipWidth || width > mipWidth - offsetX ||
       offsetY >= mipHeight || height > mipHeight - offsetY || offsetZ >= mipDepth || depth > mipDepth - offsetZ)
   {
      veSetError("Copy region exceeds texture bounds at mip level %u", mipLevel);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Calculate expected data size
   uint32_t formatSize = getFormatBlockSize(texture->format);
   size_t expectedSize = 0;
   if (!calculateCopySize(width, height, depth, layerCount, formatSize, &expectedSize))
   {
      veSetError("Host copy dimensions overflow the addressable data size");
      return VE_ERROR_INVALID_PARAMETER;
   }
   if (dataSize < expectedSize)
   {
      veSetError("Source data size (%zu bytes) is less than required (%zu bytes)", dataSize, expectedSize);
      return VE_ERROR_INVALID_PARAMETER;
   }

   result = ensureTextureInGeneralLayout(device, textureIndex, texture);
   if (result != VE_SUCCESS)
   {
      veSetError("Failed to transition texture to GENERAL layout for host write");
      return result;
   }

   // Prepare copy region
   VkMemoryToImageCopy copyRegion{};
   copyRegion.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY;
   copyRegion.pHostPointer = srcData;
   copyRegion.memoryRowLength = width;    // Tightly packed
   copyRegion.memoryImageHeight = height; // Tightly packed
   copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copyRegion.imageSubresource.mipLevel = mipLevel;
   copyRegion.imageSubresource.baseArrayLayer = arrayLayer;
   copyRegion.imageSubresource.layerCount = layerCount;
   copyRegion.imageOffset.x = (int32_t)offsetX;
   copyRegion.imageOffset.y = (int32_t)offsetY;
   copyRegion.imageOffset.z = (int32_t)offsetZ;
   copyRegion.imageExtent.width = width;
   copyRegion.imageExtent.height = height;
   copyRegion.imageExtent.depth = depth;

   // Setup copy info
   VkCopyMemoryToImageInfo copyInfo{};
   copyInfo.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO;
   copyInfo.flags = 0;
   copyInfo.dstImage = texture->image;
   copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL; // Host copy requires GENERAL layout
   copyInfo.regionCount = 1;
   copyInfo.pRegions = &copyRegion;

   // Perform the host copy
   VkResult vkResult = veFuncs.vkCopyMemoryToImage(deviceInternal->device, &copyInfo);
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
VULKEASE_API VEResult veHostReadTextureRegion(VEDevice *device, VETextureIndex textureIndex, void *dstData,
                                              size_t dataSize, const VEBufferTextureCopyRegion *region)
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
   VETextureInternal *texture = deviceInternal->getTexture(textureIndex);

   // Validate support and parameters
   VEResult result = validateHostImageCopySupport(deviceInternal, texture);
   if (result != VE_SUCCESS)
   {
      return result;
   }

   // Extract region parameters with defaults for NULL region
   uint32_t mipLevel = region ? region->mipLevel : 0;
   uint32_t arrayLayer = region ? region->arrayLayer : 0;
   uint32_t layerCount = region ? (region->layerCount > 0 ? region->layerCount : 1) : 1;

   if (mipLevel >= texture->mipLevels)
   {
      veSetError("Mip level %u exceeds texture mip levels (%u)", mipLevel, texture->mipLevels);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Calculate mip-adjusted texture dimensions
   uint32_t mipWidth = getMipDimension(texture->width, mipLevel);
   uint32_t mipHeight = getMipDimension(texture->height, mipLevel);
   uint32_t mipDepth = getMipDimension(texture->depth, mipLevel);

   uint32_t offsetX = region ? region->textureOffsetX : 0;
   uint32_t offsetY = region ? region->textureOffsetY : 0;
   uint32_t offsetZ = region ? region->textureOffsetZ : 0;
   uint32_t width = region ? region->width : mipWidth;
   uint32_t height = region ? region->height : mipHeight;
   uint32_t depth = region ? region->depth : mipDepth;

   // Validate array layer range
   if (arrayLayer >= texture->arrayLayers || layerCount > texture->arrayLayers - arrayLayer)
   {
      veSetError("Array layer range exceeds texture array layers (%u)", texture->arrayLayers);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Validate copy region bounds against mip dimensions
   if (width == 0 || height == 0 || depth == 0 || offsetX >= mipWidth || width > mipWidth - offsetX ||
       offsetY >= mipHeight || height > mipHeight - offsetY || offsetZ >= mipDepth || depth > mipDepth - offsetZ)
   {
      veSetError("Copy region exceeds texture bounds at mip level %u", mipLevel);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Calculate expected data size
   uint32_t formatSize = getFormatBlockSize(texture->format);
   size_t expectedSize = 0;
   if (!calculateCopySize(width, height, depth, layerCount, formatSize, &expectedSize))
   {
      veSetError("Host copy dimensions overflow the addressable data size");
      return VE_ERROR_INVALID_PARAMETER;
   }
   if (dataSize < expectedSize)
   {
      veSetError("Destination buffer size (%zu bytes) is less than required (%zu bytes)", dataSize, expectedSize);
      return VE_ERROR_INVALID_PARAMETER;
   }

   result = ensureTextureInGeneralLayout(device, textureIndex, texture);
   if (result != VE_SUCCESS)
   {
      veSetError("Failed to transition texture to GENERAL layout for host read");
      return result;
   }

   // Prepare copy region
   VkImageToMemoryCopy copyRegion{};
   copyRegion.sType = VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY;
   copyRegion.pHostPointer = dstData;
   copyRegion.memoryRowLength = width;    // Tightly packed
   copyRegion.memoryImageHeight = height; // Tightly packed
   copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copyRegion.imageSubresource.mipLevel = mipLevel;
   copyRegion.imageSubresource.baseArrayLayer = arrayLayer;
   copyRegion.imageSubresource.layerCount = layerCount;
   copyRegion.imageOffset.x = (int32_t)offsetX;
   copyRegion.imageOffset.y = (int32_t)offsetY;
   copyRegion.imageOffset.z = (int32_t)offsetZ;
   copyRegion.imageExtent.width = width;
   copyRegion.imageExtent.height = height;
   copyRegion.imageExtent.depth = depth;

   // Setup copy info
   VkCopyImageToMemoryInfo copyInfo{};
   copyInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO;
   copyInfo.flags = 0;
   copyInfo.srcImage = texture->image;
   copyInfo.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL; // Host copy requires GENERAL layout
   copyInfo.regionCount = 1;
   copyInfo.pRegions = &copyRegion;

   // Perform the host copy
   VkResult vkResult = veFuncs.vkCopyImageToMemory(deviceInternal->device, &copyInfo);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to copy image to memory (VkResult: %d)", vkResult);
      return VE_ERROR_TRANSFER_FAILED;
   }

   return VE_SUCCESS;
}
