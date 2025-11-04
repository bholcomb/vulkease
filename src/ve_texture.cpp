/**
 * @file ve_texture.c
 * @brief Texture Management Implementation
 */

#include "ve_internal.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "../external/stb_image_write.h"

// =============================================================================
// Bindless Descriptor Management
// =============================================================================

static bool isDepthFormat(VkFormat format)
{
   switch (format)
   {
   case VK_FORMAT_D16_UNORM:
   case VK_FORMAT_X8_D24_UNORM_PACK32:
   case VK_FORMAT_D32_SFLOAT:
   case VK_FORMAT_D24_UNORM_S8_UINT:
   case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return true;
   default:
      return false;
   }
}

static uint32_t bytesPerTexel(VkFormat format)
{
   switch (format)
   {
   case VK_FORMAT_R8_UNORM:
      return 1;
   case VK_FORMAT_R8G8_UNORM:
   case VK_FORMAT_D16_UNORM:
      return 2;
   case VK_FORMAT_R8G8B8_UNORM:
   case VK_FORMAT_B8G8R8_UNORM:
      return 3;
   case VK_FORMAT_R8G8B8A8_UNORM:
   case VK_FORMAT_R8G8B8A8_SRGB:
   case VK_FORMAT_R8G8B8A8_SNORM:
   case VK_FORMAT_R8G8B8A8_UINT:
   case VK_FORMAT_R8G8B8A8_SINT:
   case VK_FORMAT_R8G8B8A8_USCALED:
   case VK_FORMAT_R8G8B8A8_SSCALED:
   case VK_FORMAT_B8G8R8A8_UNORM:
   case VK_FORMAT_B8G8R8A8_SRGB:
   case VK_FORMAT_D32_SFLOAT:
   case VK_FORMAT_X8_D24_UNORM_PACK32:
   case VK_FORMAT_D24_UNORM_S8_UINT:
      return 4;
   case VK_FORMAT_R16G16B16A16_SFLOAT:
      return 8;
   case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return 8;
   case VK_FORMAT_R32G32B32_SFLOAT:
      return 12;
   case VK_FORMAT_R32G32B32A32_SFLOAT:
      return 16;
   default:
      return 0;
   }
}

static uint8_t floatToByte(float value)
{
   float clamped = std::clamp(value, 0.0f, 1.0f);
   return static_cast<uint8_t>(std::round(clamped * 255.0f));
}

static bool convertColorToRGBA8(VkFormat format, const void *srcData, uint32_t width, uint32_t height,
                                std::vector<uint8_t> &out)
{
   const uint8_t *src = static_cast<const uint8_t *>(srcData);
   const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
   out.resize(pixelCount * 4); // RGBA8 target

   if (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB ||
       format == VK_FORMAT_R8G8B8A8_SNORM || format == VK_FORMAT_R8G8B8A8_UINT ||
       format == VK_FORMAT_R8G8B8A8_SINT || format == VK_FORMAT_R8G8B8A8_USCALED ||
       format == VK_FORMAT_R8G8B8A8_SSCALED)
   {
      std::memcpy(out.data(), src, pixelCount * 4);
      return true;
   }

   if (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB)
   {
      for (size_t i = 0; i < pixelCount; ++i)
      {
         out[i * 4 + 0] = src[i * 4 + 2];
         out[i * 4 + 1] = src[i * 4 + 1];
         out[i * 4 + 2] = src[i * 4 + 0];
         out[i * 4 + 3] = src[i * 4 + 3];
      }
      return true;
   }

   if (format == VK_FORMAT_R8G8B8_UNORM || format == VK_FORMAT_B8G8R8_UNORM)
   {
      const bool bgra = (format == VK_FORMAT_B8G8R8_UNORM);
      for (size_t i = 0; i < pixelCount; ++i)
      {
         uint8_t r = src[i * 3 + (bgra ? 2 : 0)];
         uint8_t g = src[i * 3 + 1];
         uint8_t b = src[i * 3 + (bgra ? 0 : 2)];
         out[i * 4 + 0] = r;
         out[i * 4 + 1] = g;
         out[i * 4 + 2] = b;
         out[i * 4 + 3] = 255;
      }
      return true;
   }

   if (format == VK_FORMAT_R8_UNORM)
   {
      for (size_t i = 0; i < pixelCount; ++i)
      {
         uint8_t value = src[i];
         out[i * 4 + 0] = value;
         out[i * 4 + 1] = value;
         out[i * 4 + 2] = value;
         out[i * 4 + 3] = 255;
      }
      return true;
   }

   if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
   {
      const float *f = static_cast<const float *>(srcData);
      for (size_t i = 0; i < pixelCount; ++i)
      {
         out[i * 4 + 0] = floatToByte(f[i * 4 + 0]);
         out[i * 4 + 1] = floatToByte(f[i * 4 + 1]);
         out[i * 4 + 2] = floatToByte(f[i * 4 + 2]);
         out[i * 4 + 3] = floatToByte(f[i * 4 + 3]);
      }
      return true;
   }

   if (format == VK_FORMAT_R32G32B32_SFLOAT)
   {
      const float *f = static_cast<const float *>(srcData);
      for (size_t i = 0; i < pixelCount; ++i)
      {
         out[i * 4 + 0] = floatToByte(f[i * 3 + 0]);
         out[i * 4 + 1] = floatToByte(f[i * 3 + 1]);
         out[i * 4 + 2] = floatToByte(f[i * 3 + 2]);
         out[i * 4 + 3] = 255;
      }
      return true;
   }

   if (format == VK_FORMAT_R16G16B16A16_SFLOAT)
   {
      const uint16_t *half = static_cast<const uint16_t *>(srcData);
      auto halfToFloat = [](uint16_t h) {
         uint16_t hExp = (h & 0x7C00u) >> 10;
         uint16_t hMant = h & 0x03FFu;
         uint16_t hSign = (h & 0x8000u) >> 15;
         float value;
         if (hExp == 0)
         {
            value = hMant ? std::ldexp(static_cast<float>(hMant), -24) : 0.0f;
         }
         else if (hExp == 0x1F)
         {
            value = hMant ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
         }
         else
         {
            value = std::ldexp(static_cast<float>(hMant) / 1024.0f + 1.0f, static_cast<int>(hExp) - 15);
         }
         return hSign ? -value : value;
      };

      for (size_t i = 0; i < pixelCount; ++i)
      {
         out[i * 4 + 0] = floatToByte(halfToFloat(half[i * 4 + 0]));
         out[i * 4 + 1] = floatToByte(halfToFloat(half[i * 4 + 1]));
         out[i * 4 + 2] = floatToByte(halfToFloat(half[i * 4 + 2]));
         out[i * 4 + 3] = floatToByte(halfToFloat(half[i * 4 + 3]));
      }
      return true;
   }

   return false;
}

static bool convertDepthToRGBA8(VkFormat format, const void *srcData, uint32_t width, uint32_t height,
                                std::vector<uint8_t> &out)
{
   const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
   out.resize(pixelCount * 4);
   const uint8_t *srcBytes = static_cast<const uint8_t *>(srcData);

   auto writePixel = [&out](size_t index, float depth) {
      uint8_t value = floatToByte(depth);
      out[index * 4 + 0] = value;
      out[index * 4 + 1] = value;
      out[index * 4 + 2] = value;
      out[index * 4 + 3] = 255;
   };

   switch (format)
   {
   case VK_FORMAT_D16_UNORM:
   {
      const uint16_t *src = reinterpret_cast<const uint16_t *>(srcData);
      for (size_t i = 0; i < pixelCount; ++i)
      {
         float depth = static_cast<float>(src[i]) / 65535.0f;
         writePixel(i, depth);
      }
      return true;
   }
   case VK_FORMAT_D32_SFLOAT:
   {
      const float *src = reinterpret_cast<const float *>(srcData);
      for (size_t i = 0; i < pixelCount; ++i)
      {
         writePixel(i, src[i]);
      }
      return true;
   }
   case VK_FORMAT_X8_D24_UNORM_PACK32:
   case VK_FORMAT_D24_UNORM_S8_UINT:
   {
      for (size_t i = 0; i < pixelCount; ++i)
      {
         uint32_t packed;
         std::memcpy(&packed, srcBytes + i * 4, sizeof(uint32_t));
         float depth = static_cast<float>(packed & 0x00FFFFFFu) / 16777215.0f;
         writePixel(i, depth);
      }
      return true;
   }
   case VK_FORMAT_D32_SFLOAT_S8_UINT:
   {
      for (size_t i = 0; i < pixelCount; ++i)
      {
         float depth;
         std::memcpy(&depth, srcBytes + i * 8, sizeof(float));
         writePixel(i, depth);
      }
      return true;
   }
   default:
      return false;
   }
}

static bool convertImageToRGBA8(VkFormat format, const void *srcData, uint32_t width, uint32_t height,
                                std::vector<uint8_t> &out)
{
   if (isDepthFormat(format))
   {
      return convertDepthToRGBA8(format, srcData, width, height, out);
   }
   return convertColorToRGBA8(format, srcData, width, height, out);
}

static VkImageAspectFlags getCopyAspectMask(VkFormat format)
{
   if (isDepthFormat(format))
   {
      return VK_IMAGE_ASPECT_DEPTH_BIT;
   }
   return VK_IMAGE_ASPECT_COLOR_BIT;
}

static VEResult readTextureLevel0(VEDeviceInternal *deviceInternal, VETextureInternal *texture,
                                  std::vector<uint8_t> &outData)
{
   if (!deviceInternal || !texture)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!(texture->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
   {
      veSetError("Texture must be created with transfer src usage to be saved");
      return VE_ERROR_INVALID_PARAMETER;
   }

   uint32_t bytesPerPixel = bytesPerTexel(texture->format);
   if (bytesPerPixel == 0)
   {
      veSetError("Unsupported texture format (%d) for read-back", texture->format);
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   VkDeviceSize imageSize = static_cast<VkDeviceSize>(texture->width) * static_cast<VkDeviceSize>(texture->height) *
                            static_cast<VkDeviceSize>(bytesPerPixel);

   VkBufferCreateInfo bufferInfo{};
   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size = imageSize;
   bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VmaAllocationCreateInfo allocInfo{};
   allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
   allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

   VkBuffer stagingBuffer = VK_NULL_HANDLE;
   VmaAllocation stagingAllocation = VK_NULL_HANDLE;
   VmaAllocationInfo stagingAllocInfo{};
   VkResult vkResult = vmaCreateBuffer(deviceInternal->allocator, &bufferInfo, &allocInfo, &stagingBuffer,
                                       &stagingAllocation, &stagingAllocInfo);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to create staging buffer for texture read-back (VkResult: %d)", vkResult);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VECommandBuffer *cmdHandle = veBeginCommandBuffer((VEDevice *)deviceInternal);
   if (!cmdHandle)
   {
      vmaDestroyBuffer(deviceInternal->allocator, stagingBuffer, stagingAllocation);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VECommandBufferInternal *cmdInternal = (VECommandBufferInternal *)cmdHandle;
   VkCommandBuffer vkCmd = cmdInternal->commandBuffer;

   VkImageAspectFlags aspectMask = getCopyAspectMask(texture->format);
   VkImageLayout originalLayout = texture->currentLayout;
   bool needsTransition = originalLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

   if (needsTransition)
   {
      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.oldLayout = originalLayout;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = texture->image;
      barrier.subresourceRange.aspectMask = aspectMask;
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;

      vkCmdPipelineBarrier(vkCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                           NULL, 1, &barrier);
   }

   VkBufferImageCopy copyRegion{};
   copyRegion.bufferOffset = 0;
   copyRegion.bufferRowLength = 0;
   copyRegion.bufferImageHeight = 0;
   copyRegion.imageSubresource.aspectMask = aspectMask;
   copyRegion.imageSubresource.mipLevel = 0;
   copyRegion.imageSubresource.baseArrayLayer = 0;
   copyRegion.imageSubresource.layerCount = 1;
   copyRegion.imageOffset = {0, 0, 0};
   copyRegion.imageExtent = {texture->width, texture->height, 1};

   vkCmdCopyImageToBuffer(vkCmd, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

   if (needsTransition)
   {
      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout = originalLayout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = texture->image;
      barrier.subresourceRange.aspectMask = aspectMask;
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;

      vkCmdPipelineBarrier(vkCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
                           NULL, 1, &barrier);
   }

   VEResult submitResult = veSubmitCommandBuffer(cmdHandle, true);
   if (submitResult != VE_SUCCESS)
   {
      vmaDestroyBuffer(deviceInternal->allocator, stagingBuffer, stagingAllocation);
      texture->currentLayout = originalLayout;
      return submitResult;
   }

   void *mappedData = stagingAllocInfo.pMappedData;
   bool needsUnmap = false;
   if (!mappedData)
   {
      if (vmaMapMemory(deviceInternal->allocator, stagingAllocation, &mappedData) != VK_SUCCESS)
      {
         vmaDestroyBuffer(deviceInternal->allocator, stagingBuffer, stagingAllocation);
         veSetError("Failed to map staging buffer for texture save");
         texture->currentLayout = originalLayout;
         return VE_ERROR_OUT_OF_MEMORY;
      }
      needsUnmap = true;
   }

   outData.resize(static_cast<size_t>(imageSize));
   std::memcpy(outData.data(), mappedData, static_cast<size_t>(imageSize));

   if (needsUnmap)
   {
      vmaUnmapMemory(deviceInternal->allocator, stagingAllocation);
   }

   vmaDestroyBuffer(deviceInternal->allocator, stagingBuffer, stagingAllocation);
   texture->currentLayout = originalLayout;

   return VE_SUCCESS;
}

static std::string makePngFilename(const char *filename)
{
   std::string path = filename ? filename : "texture";
   auto dot = path.find_last_of('.');
   if (dot != std::string::npos)
   {
      path.resize(dot);
   }
   path += ".png";
   return path;
}

VEResult VEDeviceInternal::initializeBindlessDescriptors()
{
   VkPhysicalDeviceDescriptorIndexingProperties indexingProps{};
   indexingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

   VkPhysicalDeviceProperties2 props2{};
   props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   props2.pNext = &indexingProps;
   vkGetPhysicalDeviceProperties2(this->physicalDevice, &props2);

   uint32_t textureCapacity = VE_MAX_TEXTURES;
   uint32_t samplerCapacity = VE_MAX_SAMPLERS;

   uint32_t textureSetLimit = this->deviceProperties.limits.maxDescriptorSetSampledImages;
   if (indexingProps.maxDescriptorSetUpdateAfterBindSampledImages > 0)
   {
      textureSetLimit = textureSetLimit < indexingProps.maxDescriptorSetUpdateAfterBindSampledImages
                            ? textureSetLimit
                            : indexingProps.maxDescriptorSetUpdateAfterBindSampledImages;
   }
   if (indexingProps.maxPerStageDescriptorUpdateAfterBindSampledImages > 0)
   {
      textureSetLimit = textureSetLimit < indexingProps.maxPerStageDescriptorUpdateAfterBindSampledImages
                            ? textureSetLimit
                            : indexingProps.maxPerStageDescriptorUpdateAfterBindSampledImages;
   }
   if (textureSetLimit > 0)
   {
      textureCapacity = textureCapacity < textureSetLimit ? textureCapacity : textureSetLimit;
   }

   uint32_t samplerSetLimit = this->deviceProperties.limits.maxDescriptorSetSamplers;
   if (indexingProps.maxDescriptorSetUpdateAfterBindSamplers > 0)
   {
      samplerSetLimit = samplerSetLimit < indexingProps.maxDescriptorSetUpdateAfterBindSamplers
                            ? samplerSetLimit
                            : indexingProps.maxDescriptorSetUpdateAfterBindSamplers;
   }
   if (indexingProps.maxPerStageDescriptorUpdateAfterBindSamplers > 0)
   {
      samplerSetLimit = samplerSetLimit < indexingProps.maxPerStageDescriptorUpdateAfterBindSamplers
                            ? samplerSetLimit
                            : indexingProps.maxPerStageDescriptorUpdateAfterBindSamplers;
   }
   if (samplerSetLimit > 0)
   {
      samplerCapacity = samplerCapacity < samplerSetLimit ? samplerCapacity : samplerSetLimit;
   }

   uint64_t combinedCapacity = (uint64_t)textureCapacity + (uint64_t)samplerCapacity;
   if (indexingProps.maxUpdateAfterBindDescriptorsInAllPools > 0 &&
       combinedCapacity > indexingProps.maxUpdateAfterBindDescriptorsInAllPools)
   {
      uint64_t excess = combinedCapacity - indexingProps.maxUpdateAfterBindDescriptorsInAllPools;

      uint64_t reducibleTextures = textureCapacity > 2 ? (uint64_t)(textureCapacity - 2) : 0;
      uint64_t reduceTextures = excess < reducibleTextures ? excess : reducibleTextures;
      textureCapacity -= (uint32_t)reduceTextures;
      excess -= reduceTextures;

      if (excess > 0)
      {
         uint64_t reducibleSamplers = samplerCapacity > 2 ? (uint64_t)(samplerCapacity - 2) : 0;
         uint64_t reduceSamplers = excess < reducibleSamplers ? excess : reducibleSamplers;
         samplerCapacity -= (uint32_t)reduceSamplers;
         excess -= reduceSamplers;
      }

      if (excess > 0)
      {
         veSetError("Descriptor indexing limits too low for bindless configuration");
         return VE_ERROR_FEATURE_NOT_SUPPORTED;
      }
   }

   if (textureCapacity < 2 || samplerCapacity < 2)
   {
      veSetError("Device descriptor indexing limits are insufficient: textures=%u samplers=%u", textureCapacity,
                 samplerCapacity);
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   // Create descriptor pool for bindless textures and samplers
   VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureCapacity},
                                       {VK_DESCRIPTOR_TYPE_SAMPLER, samplerCapacity}};

   VkDescriptorPoolCreateInfo poolInfo{};
   poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
   poolInfo.maxSets = 2;
   poolInfo.poolSizeCount = 2;
   poolInfo.pPoolSizes = poolSizes;

   VkResult result = vkCreateDescriptorPool(this->device, &poolInfo, NULL, &this->descriptorPool);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create descriptor pool (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Create descriptor set layout for textures
   VkDescriptorBindingFlags textureBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                  VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

   VkDescriptorSetLayoutBinding textureBinding{};
   textureBinding.binding = 0;
   textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
   textureBinding.descriptorCount = textureCapacity;
   textureBinding.stageFlags = VK_SHADER_STAGE_ALL;

   VkDescriptorSetLayoutBindingFlagsCreateInfo textureBindingFlags_info{};
   textureBindingFlags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
   textureBindingFlags_info.bindingCount = 1;
   textureBindingFlags_info.pBindingFlags = &textureBindingFlags;

   VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
   textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   textureLayoutInfo.pNext = &textureBindingFlags_info;
   textureLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
   textureLayoutInfo.bindingCount = 1;
   textureLayoutInfo.pBindings = &textureBinding;

   result = vkCreateDescriptorSetLayout(this->device, &textureLayoutInfo, NULL, &this->textureDescriptorSetLayout);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create texture descriptor set layout (VkResult: %d)", result);
      vkDestroyDescriptorPool(this->device, this->descriptorPool, NULL);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Create descriptor set layout for samplers
   VkDescriptorBindingFlags samplerBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                  VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

   VkDescriptorSetLayoutBinding samplerBinding{};
   samplerBinding.binding = 0;
   samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
   samplerBinding.descriptorCount = samplerCapacity;
   samplerBinding.stageFlags = VK_SHADER_STAGE_ALL;

   VkDescriptorSetLayoutBindingFlagsCreateInfo samplerBindingFlags_info{};
   samplerBindingFlags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
   samplerBindingFlags_info.bindingCount = 1;
   samplerBindingFlags_info.pBindingFlags = &samplerBindingFlags;

   VkDescriptorSetLayoutCreateInfo samplerLayoutInfo{};
   samplerLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   samplerLayoutInfo.pNext = &samplerBindingFlags_info;
   samplerLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
   samplerLayoutInfo.bindingCount = 1;
   samplerLayoutInfo.pBindings = &samplerBinding;

   result = vkCreateDescriptorSetLayout(this->device, &samplerLayoutInfo, NULL, &this->samplerDescriptorSetLayout);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create sampler descriptor set layout (VkResult: %d)", result);
      vkDestroyDescriptorSetLayout(this->device, this->textureDescriptorSetLayout, NULL);
      vkDestroyDescriptorPool(this->device, this->descriptorPool, NULL);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Allocate descriptor sets
   uint32_t maxDescriptorCounts[] = {textureCapacity, samplerCapacity};
   VkDescriptorSetLayout layouts[] = {this->textureDescriptorSetLayout, this->samplerDescriptorSetLayout};

   VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{};
   variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
   variableCountInfo.descriptorSetCount = 2;
   variableCountInfo.pDescriptorCounts = maxDescriptorCounts;

   VkDescriptorSetAllocateInfo allocInfo{};
   allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocInfo.pNext = &variableCountInfo;
   allocInfo.descriptorPool = this->descriptorPool;
   allocInfo.descriptorSetCount = 2;
   allocInfo.pSetLayouts = layouts;

   VkDescriptorSet descriptorSets[2];
   result = vkAllocateDescriptorSets(this->device, &allocInfo, descriptorSets);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to allocate descriptor sets (VkResult: %d)", result);
      cleanupBindlessDescriptors();
      return VE_ERROR_OUT_OF_MEMORY;
   }

   textureDescriptorSet = descriptorSets[0];
   samplerDescriptorSet = descriptorSets[1];

   // Initialize texture management
   maxTextures = textureCapacity;
   textures = static_cast<VETextureInternal *>(calloc(maxTextures, sizeof(VETextureInternal)));
   freeTextureIndices = static_cast<uint32_t *>(calloc(maxTextures, sizeof(uint32_t)));

   if (!textures || !freeTextureIndices)
   {
      veSetError("Failed to allocate texture management memory");
      cleanupBindlessDescriptors();
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Start from index 1 (0 is reserved for invalid)
   for (uint32_t i = 0; i < maxTextures - 1; i++)
   {
      freeTextureIndices[i] = i + 1;
   }
   freeTextureCount = maxTextures - 1;
   textureCount = 0;

   // Initialize sampler management
   maxSamplers = samplerCapacity;
   samplers = static_cast<VESamplerInternal *>(calloc(maxSamplers, sizeof(VESamplerInternal)));
   freeSamplerIndices = static_cast<uint32_t *>(calloc(maxSamplers, sizeof(uint32_t)));

   if (!samplers || !freeSamplerIndices)
   {
      veSetError("Failed to allocate sampler management memory");
      cleanupBindlessDescriptors();
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Start from index 1 (0 is reserved for invalid)
   for (uint32_t i = 0; i < maxSamplers - 1; i++)
   {
      freeSamplerIndices[i] = i + 1;
   }
   freeSamplerCount = maxSamplers - 1;
   samplerCount = 0;

   return VE_SUCCESS;
}

void VEDeviceInternal::cleanupBindlessDescriptors()
{
   if (!device)
   {
      return;
   }

   if (textures)
   {
      for (uint32_t i = 0; i < maxTextures; i++)
      {
         if (textures[i].isValid)
         {
            if (textures[i].imageView)
            {
               vkDestroyImageView(device, textures[i].imageView, NULL);
            }
            if (textures[i].image)
            {
               vmaDestroyImage(allocator, textures[i].image, textures[i].allocation);
            }
         }
      }
      free(textures);
      textures = NULL;
   }

   if (freeTextureIndices)
   {
      free(freeTextureIndices);
      freeTextureIndices = NULL;
   }

   if (samplers)
   {
      for (uint32_t i = 0; i < maxSamplers; i++)
      {
         if (samplers[i].isValid)
         {
            vkDestroySampler(device, samplers[i].sampler, NULL);
         }
      }
      free(samplers);
      samplers = NULL;
   }

   if (freeSamplerIndices)
   {
      free(freeSamplerIndices);
      freeSamplerIndices = NULL;
   }

   if (samplerDescriptorSetLayout)
   {
      vkDestroyDescriptorSetLayout(device, samplerDescriptorSetLayout, NULL);
      samplerDescriptorSetLayout = VK_NULL_HANDLE;
   }

   if (textureDescriptorSetLayout)
   {
      vkDestroyDescriptorSetLayout(device, textureDescriptorSetLayout, NULL);
      textureDescriptorSetLayout = VK_NULL_HANDLE;
   }

   if (descriptorPool)
   {
      vkDestroyDescriptorPool(device, descriptorPool, NULL);
      descriptorPool = VK_NULL_HANDLE;
   }
}

// =============================================================================
// Texture Index Management
// =============================================================================

uint32_t VEDeviceInternal::allocateTextureIndex()
{
   if (freeTextureCount == 0)
   {
      veSetError("No free texture indices available (max: %u)", maxTextures);
      return VE_INVALID_TEXTURE_INDEX;
   }

   freeTextureCount--;
   uint32_t index = freeTextureIndices[freeTextureCount];
   textureCount++;

   return index;
}

void VEDeviceInternal::freeTextureIndex(uint32_t index)
{
   if (index == 0 || index >= maxTextures)
   {
      return;
   }

   freeTextureIndices[freeTextureCount] = index;
   freeTextureCount++;
   textureCount--;
}

VETextureInternal *VEDeviceInternal::getTexture(VETextureIndex index)
{
   if (index == VE_INVALID_TEXTURE_INDEX || index >= maxTextures)
   {
      return NULL;
   }

   VETextureInternal *texture = &textures[index];
   return texture->isValid ? texture : NULL;
}

const VETextureInternal *VEDeviceInternal::getTexture(VETextureIndex index) const
{
   return const_cast<VEDeviceInternal *>(this)->getTexture(index);
}

VkImageView VEDeviceInternal::getImageViewFromTexture(VETextureIndex index) const
{
   const VETextureInternal *texture = getTexture(index);
   return (texture && texture->isValid) ? texture->imageView : VK_NULL_HANDLE;
}

// =============================================================================
// Texture Data Upload
// =============================================================================

static VEResult veUploadTextureData(VEDeviceInternal *device, VETextureInternal *texture, const void *data,
                                    uint64_t dataSize)
{
   if (!device || !texture || !data || dataSize == 0)
   {
      veSetError("Invalid parameters for texture upload");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Create staging buffer for upload
   VkBufferCreateInfo stagingBufferInfo{};
   stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   stagingBufferInfo.size = dataSize;
   stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VmaAllocationCreateInfo stagingAllocInfo{};
   stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
   stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

   VkBuffer stagingBuffer;
   VmaAllocation stagingAllocation;
   VmaAllocationInfo stagingAllocInfo_result;

   VkResult result = vmaCreateBuffer(device->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer,
                                     &stagingAllocation, &stagingAllocInfo_result);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create staging buffer for texture upload (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Copy data to staging buffer
   memcpy(stagingAllocInfo_result.pMappedData, data, dataSize);
   vmaFlushAllocation(device->allocator, stagingAllocation, 0, dataSize);

   // Use the public API to get a command buffer
   VECommandBuffer *cmdPublic = veBeginCommandBuffer((VEDevice *)device);
   if (!cmdPublic)
   {
      vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VECommandBufferInternal *cmd = (VECommandBufferInternal *)cmdPublic;

   // Transition image to transfer destination layout
   VkImageMemoryBarrier barrier{};
   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = texture->image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.baseMipLevel = 0;
   barrier.subresourceRange.levelCount = texture->mipLevels;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount = texture->arrayLayers;
   barrier.srcAccessMask = 0;
   barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

   vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                        NULL, 0, NULL, 1, &barrier);

   // Copy buffer to image
   VkBufferImageCopy region{};
   region.bufferOffset = 0;
   region.bufferRowLength = 0;
   region.bufferImageHeight = 0;
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount = texture->arrayLayers;
   region.imageOffset = (VkOffset3D){0, 0, 0};
   region.imageExtent = (VkExtent3D){texture->width, texture->height, texture->depth};

   vkCmdCopyBufferToImage(cmd->commandBuffer, stagingBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                          &region);

   // Transition image to shader read layout
   barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

   vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                        NULL, 0, NULL, 1, &barrier);

   // Submit command buffer and wait for completion
   VEResult submitResult = veSubmitCommandBuffer(cmdPublic, true);

   // Update tracked layout if command submission succeeded
   if (submitResult == VE_SUCCESS)
   {
      texture->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   }

   // Cleanup staging buffer
   vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);

   if (submitResult != VE_SUCCESS)
   {
      return submitResult;
   }

   return VE_SUCCESS;
}

// =============================================================================
// Texture Creation
// =============================================================================

VETextureIndex veCreateTexture(VEDevice *device, const VETextureDesc *desc)
{
   if (!device || !desc)
   {
      veSetError("Invalid parameters for texture creation");
      return VE_INVALID_TEXTURE_INDEX;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   uint32_t index = deviceInternal->allocateTextureIndex();
   if (index == VE_INVALID_TEXTURE_INDEX)
   {
      return VE_INVALID_TEXTURE_INDEX;
   }

   VETextureInternal *texture = &deviceInternal->textures[index];
   memset(texture, 0, sizeof(VETextureInternal));

   texture->width = desc->width;
   texture->height = desc->height;
   texture->depth = desc->depth;
   texture->mipLevels = desc->mipLevels;
   texture->arrayLayers = desc->arrayLayers;
   texture->format = desc->format;
   texture->usage = desc->usage;
   texture->sampleCount = desc->sampleCount;
   texture->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; // All textures start undefined
   texture->index = index;

   if (desc->debugName)
   {
      strncpy(texture->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
   }
   else
   {
      snprintf(texture->debugName, VE_MAX_DEBUG_NAME_LENGTH, "Texture_%u", index);
   }

   VkImageCreateInfo imageInfo{};
   imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageInfo.imageType = desc->depth > 1 ? VK_IMAGE_TYPE_3D : (desc->height > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
   imageInfo.extent.width = desc->width;
   imageInfo.extent.height = desc->height > 0 ? desc->height : 1;
   imageInfo.extent.depth = desc->depth > 0 ? desc->depth : 1;
   imageInfo.mipLevels = texture->mipLevels;
   imageInfo.arrayLayers = desc->arrayLayers;
   imageInfo.format = desc->format;
   imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
   imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   imageInfo.usage = desc->usage;
   imageInfo.samples = static_cast<VkSampleCountFlagBits>(desc->sampleCount);
   imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Always add transfer dst for potential
                                                       // data uploads

   if (desc->arrayLayers > 1)
   {
      imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
   }

   VmaAllocationCreateInfo allocInfo{};
   allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

   VkResult result = vmaCreateImage(deviceInternal->allocator, &imageInfo, &allocInfo, &texture->image,
                                    &texture->allocation, &texture->allocationInfo);

   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create texture (VkResult: %d)", result);
      deviceInternal->freeTextureIndex(index);
      return VE_INVALID_TEXTURE_INDEX;
   }

   // Create image view
   VkImageViewCreateInfo viewInfo{};
   viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   viewInfo.image = texture->image;
   viewInfo.viewType = imageInfo.imageType == VK_IMAGE_TYPE_3D
                           ? VK_IMAGE_VIEW_TYPE_3D
                           : (imageInfo.imageType == VK_IMAGE_TYPE_2D
                                  ? (desc->arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D)
                                  : VK_IMAGE_VIEW_TYPE_1D);
   viewInfo.format = imageInfo.format;
   viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   viewInfo.subresourceRange.baseMipLevel = 0;
   viewInfo.subresourceRange.levelCount = texture->mipLevels;
   viewInfo.subresourceRange.baseArrayLayer = 0;
   viewInfo.subresourceRange.layerCount = desc->arrayLayers;

   // Handle depth/stencil formats
   if (desc->format >= VK_FORMAT_D16_UNORM && desc->format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
   {
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      if (desc->format == VK_FORMAT_D16_UNORM_S8_UINT || desc->format == VK_FORMAT_D24_UNORM_S8_UINT ||
          desc->format == VK_FORMAT_D32_SFLOAT_S8_UINT)
      {
         viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
   }

   result = vkCreateImageView(deviceInternal->device, &viewInfo, NULL, &texture->imageView);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create image view (VkResult: %d)", result);
      vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
      deviceInternal->freeTextureIndex(index);
      return VE_INVALID_TEXTURE_INDEX;
   }

   // Upload initial data if provided
   if (desc->initialData && desc->initialDataSize > 0)
   {
      VEResult uploadResult = veUploadTextureData(deviceInternal, texture, desc->initialData, desc->initialDataSize);
      if (uploadResult != VE_SUCCESS)
      {
         vkDestroyImageView(deviceInternal->device, texture->imageView, NULL);
         vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
         deviceInternal->freeTextureIndex(index);
         return VE_INVALID_TEXTURE_INDEX;
      }
   }

   if (deviceInternal->context->validationEnabled)
   {
      veSetObjectDebugName(deviceInternal, (uint64_t)texture->image, VK_OBJECT_TYPE_IMAGE, texture->debugName);
      char viewName[VE_MAX_DEBUG_NAME_LENGTH + 16];
      snprintf(viewName, sizeof(viewName), "%s_View", texture->debugName);
      veSetObjectDebugName(deviceInternal, (uint64_t)texture->imageView, VK_OBJECT_TYPE_IMAGE_VIEW, viewName);
   }

   texture->isValid = true;

   // Update bindless descriptor
   deviceInternal->updateTextureDescriptor(index);

   return index;
}

void veDestroyTexture(VEDevice *device, VETextureIndex index)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
   {
      return;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = deviceInternal->getTexture(index);

   if (!texture || !texture->isValid)
   {
      return;
   }

   if (texture->imageView)
   {
      vkDestroyImageView(deviceInternal->device, texture->imageView, NULL);
   }

   if (texture->image)
   {
      vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
   }

   deviceInternal->freeTextureIndex(index);
   memset(texture, 0, sizeof(VETextureInternal));
}

// =============================================================================
// Texture Property Queries
// =============================================================================

VEResult veGetTextureSize(VEDevice *device, VETextureIndex index, uint32_t *width, uint32_t *height, uint32_t *depth)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for texture size query");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = deviceInternal->getTexture(index);

   if (!texture)
   {
      veSetError("Invalid texture index");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (width)
      *width = texture->width;
   if (height)
      *height = texture->height;
   if (depth)
      *depth = texture->depth;

   return VE_SUCCESS;
}

VkFormat veGetTextureFormat(VEDevice *device, VETextureIndex index)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
   {
      return VK_FORMAT_R8G8B8A8_UNORM;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = deviceInternal->getTexture(index);

   return texture ? texture->format : VK_FORMAT_R8G8B8A8_UNORM;
}

// =============================================================================
// Convenience Texture Creation Functions
// =============================================================================

VETextureIndex veCreateTexture1D(VEDevice *device, uint32_t width, VkFormat format, VkImageUsageFlags usage,
                                 const char *debugName)
{
   VETextureDesc desc{};
   desc.width = width;
   desc.height = 1;
   desc.depth = 1;
   desc.mipLevels = 1;
   desc.arrayLayers = 1;
   desc.format = format;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = NULL;
   desc.initialDataSize = 0;
   desc.debugName = debugName;

   return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture2D(VEDevice *device, uint32_t width, uint32_t height, VkFormat format,
                                 VkImageUsageFlags usage, const char *debugName)
{
   VETextureDesc desc{};
   desc.width = width;
   desc.height = height;
   desc.depth = 1;
   desc.mipLevels = 1;
   desc.arrayLayers = 1;
   desc.format = format;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = NULL;
   desc.initialDataSize = 0;
   desc.debugName = debugName;

   return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture3D(VEDevice *device, uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                                 VkImageUsageFlags usage, const char *debugName)
{
   VETextureDesc desc{};
   desc.width = width;
   desc.height = height;
   desc.depth = depth;
   desc.mipLevels = 1;
   desc.arrayLayers = 1;
   desc.format = format;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = NULL;
   desc.initialDataSize = 0;
   desc.debugName = debugName;

   return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture2DArray(VEDevice *device, uint32_t width, uint32_t height, uint32_t layers,
                                      VkFormat format, VkImageUsageFlags usage, const char *debugName)
{
   VETextureDesc desc{};
   desc.width = width;
   desc.height = height;
   desc.depth = 1;
   desc.mipLevels = 1;
   desc.arrayLayers = layers;
   desc.format = format;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = NULL;
   desc.initialDataSize = 0;
   desc.debugName = debugName;

   return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTextureCube(VEDevice *device, uint32_t size, VkFormat format, VkImageUsageFlags usage,
                                   const char *debugName)
{
   VETextureDesc desc{};
   desc.width = size;
   desc.height = size;
   desc.depth = 1;
   desc.mipLevels = 1;
   desc.arrayLayers = 6;
   desc.format = format;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = NULL;
   desc.initialDataSize = 0;
   desc.debugName = debugName;

   return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture2DMultisample(VEDevice *device, uint32_t width, uint32_t height, VkFormat format,
                                            VkSampleCountFlags sampleCount, VkImageUsageFlags usage,
                                            const char *debugName)
{
   VETextureDesc desc{};
   desc.width = width;
   desc.height = height;
   desc.depth = 1;
   desc.mipLevels = 1;
   desc.arrayLayers = 1;
   desc.format = format;
   desc.usage = usage;
   desc.sampleCount = sampleCount;
   desc.initialData = NULL;
   desc.initialDataSize = 0;
   desc.debugName = debugName;

   return veCreateTexture(device, &desc);
}

// =============================================================================
// STB Image Integration
// =============================================================================

VETextureIndex veLoadTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage, bool generateMips)
{
   if (!device || !filename)
   {
      veSetError("Invalid parameters for texture loading");
      return VE_INVALID_TEXTURE_INDEX;
   }

   // flip images for loading into texture memory
   stbi_set_flip_vertically_on_load(true);

   int width, height, channels;
   stbi_uc *pixels = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

   if (!pixels)
   {
      veSetError("Failed to load image: %s", stbi_failure_reason());
      return VE_INVALID_TEXTURE_INDEX;
   }

   uint32_t mipLevels = 0;
   if (generateMips)
   {
      uint32_t longestSide = static_cast<uint32_t>(std::max(width, height));
      mipLevels = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(longestSide)))) + 1;
      usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   }

   VETextureDesc desc{};
   desc.width = static_cast<uint32_t>(width);
   desc.height = static_cast<uint32_t>(height);
   desc.depth = 1;
   desc.mipLevels = mipLevels;
   desc.arrayLayers = 1;
   desc.format = VK_FORMAT_R8G8B8A8_SRGB;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = pixels;
   desc.initialDataSize = static_cast<uint64_t>(width * height * 4);
   desc.debugName = filename;

   VETextureIndex result = veCreateTexture(device, &desc);

   // Generate mipmaps if requested and texture creation succeeded
   if (result != VE_INVALID_TEXTURE_INDEX && generateMips && desc.mipLevels > 1)
   {
      VEResult mipmapResult = veGenerateMipmapsImmediate(device, result);
      if (mipmapResult != VE_SUCCESS)
      {
         veSetError("Failed to generate mipmaps for loaded texture: %s", filename);
         veDestroyTexture(device, result);
         stbi_image_free(pixels);
         return VE_INVALID_TEXTURE_INDEX;
      }
   }

   stbi_image_free(pixels);
   return result;
}

VETextureIndex veLoadHDRTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage, bool generateMips)
{
   if (!device || !filename)
   {
      veSetError("Invalid parameters for HDR texture loading");
      return VE_INVALID_TEXTURE_INDEX;
   }

   int width, height, channels;
   float *pixels = stbi_loadf(filename, &width, &height, &channels, STBI_rgb_alpha);

   if (!pixels)
   {
      veSetError("Failed to load HDR image: %s", stbi_failure_reason());
      return VE_INVALID_TEXTURE_INDEX;
   }

   VETextureDesc desc{};
   desc.width = static_cast<uint32_t>(width);
   desc.height = static_cast<uint32_t>(height);
   desc.depth = 1;
   desc.mipLevels = generateMips ? 0u : 1u;
   desc.arrayLayers = 1;
   desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = pixels;
   desc.initialDataSize = static_cast<uint64_t>(width * height * 4 * sizeof(float));
   desc.debugName = filename;

   VETextureIndex result = veCreateTexture(device, &desc);

   // Generate mipmaps if requested and texture creation succeeded
   if (result != VE_INVALID_TEXTURE_INDEX && generateMips && desc.mipLevels > 1)
   {
      VEResult mipmapResult = veGenerateMipmapsImmediate(device, result);
      if (mipmapResult != VE_SUCCESS)
      {
         veSetError("Failed to generate mipmaps for loaded HDR texture: %s", filename);
         veDestroyTexture(device, result);
         stbi_image_free(pixels);
         return VE_INVALID_TEXTURE_INDEX;
      }
   }

   stbi_image_free(pixels);
   return result;
}

VETextureIndex veLoadCubeTexture(VEDevice *device, const char *filenames[6], VkImageUsageFlags usage, bool generateMips)
{
   if (!device || !filenames)
   {
      veSetError("Invalid parameters for cube texture loading");
      return VE_INVALID_TEXTURE_INDEX;
   }

   int width = 0;
   int height = 0;
   std::vector<uint8_t> combinedData;
   const int desiredChannels = STBI_rgb_alpha;

   size_t faceSize = 0;
   for (int face = 0; face < 6; ++face)
   {
      const char *path = filenames[face];
      if (!path)
      {
         veSetError("Cube texture face %d filename is NULL", face);
         return VE_INVALID_TEXTURE_INDEX;
      }

      int faceWidth = 0;
      int faceHeight = 0;
      int faceChannels = 0;

      stbi_uc *pixels = stbi_load(path, &faceWidth, &faceHeight, &faceChannels, desiredChannels);
      if (!pixels)
      {
         veSetError("Failed to load cube texture face %d: %s", face, stbi_failure_reason());
         return VE_INVALID_TEXTURE_INDEX;
      }

      if (face == 0)
      {
         width = faceWidth;
         height = faceHeight;
         faceSize = static_cast<size_t>(width) * static_cast<size_t>(height) * desiredChannels;
         combinedData.resize(faceSize * 6);
      }
      else
      {
         if (faceWidth != width || faceHeight != height)
         {
            veSetError("Cube texture faces must share identical dimensions");
            stbi_image_free(pixels);
            return VE_INVALID_TEXTURE_INDEX;
         }
      }

      std::memcpy(combinedData.data() + faceSize * face, pixels, faceSize);
      stbi_image_free(pixels);
   }

   if (width == 0 || height == 0)
   {
      veSetError("Cube texture faces have zero size");
      return VE_INVALID_TEXTURE_INDEX;
   }

   VkImageUsageFlags finalUsage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   uint32_t mipLevels = 1;
   if (generateMips)
   {
      uint32_t longestSide = static_cast<uint32_t>(std::max(width, height));
      mipLevels = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(longestSide)))) + 1;
      finalUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   }

   VETextureDesc desc{};
   desc.width = static_cast<uint32_t>(width);
   desc.height = static_cast<uint32_t>(height);
   desc.depth = 1;
   desc.mipLevels = mipLevels;
   desc.arrayLayers = 6;
   desc.format = VK_FORMAT_R8G8B8A8_SRGB;
   desc.usage = finalUsage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = combinedData.data();
   desc.initialDataSize = combinedData.size();
   desc.debugName = filenames[0];

   VETextureIndex result = veCreateTexture(device, &desc);
   if (result == VE_INVALID_TEXTURE_INDEX)
   {
      return VE_INVALID_TEXTURE_INDEX;
   }

   if (generateMips && mipLevels > 1)
   {
      VEResult mipResult = veGenerateMipmapsImmediate(device, result);
      if (mipResult != VE_SUCCESS)
      {
         veDestroyTexture(device, result);
         return VE_INVALID_TEXTURE_INDEX;
      }
   }

   return result;
}

VEResult veGenerateMipmaps(VEDevice *device, VECommandBuffer *cmd, VETextureIndex texture)
{
   if (!device || !cmd || texture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for mipmap generation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VECommandBufferInternal *cmdInternal = (VECommandBufferInternal *)cmd;

   // Get texture and validate
   VETextureInternal *textureInternal = deviceInternal->getTexture(texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Validate mipmap generation requirements
   if (textureInternal->mipLevels <= 1)
   {
      veSetError("Texture has only 1 mip level - no mipmaps to generate");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (textureInternal->sampleCount != VK_SAMPLE_COUNT_1_BIT)
   {
      veSetError("Cannot generate mipmaps for multisampled textures");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Check if format supports linear filtering (required for mipmap generation)
   VkFormatProperties formatProps;
   vkGetPhysicalDeviceFormatProperties(deviceInternal->physicalDevice, textureInternal->format, &formatProps);

   if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
   {
      veSetError("Format does not support linear filtering required for mipmap "
                 "generation");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   // Ensure texture has transfer source usage for blitting
   VkImageUsageFlags usage = textureInternal->usage;
   if (!(usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
   {
      veSetError("Texture must have transfer source usage for mipmap generation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkImage image = textureInternal->image;
   uint32_t mipWidth = textureInternal->width;
   uint32_t mipHeight = textureInternal->height;

   // Transition mip level 0 from its current layout to TRANSFER_SRC_OPTIMAL
   // This handles the case where texture was already transitioned to
   // SHADER_READ_ONLY
   VkImageMemoryBarrier2 initialBarrier{};
   initialBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   initialBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // Where it might be used
   initialBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;            // Current access
   initialBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;        // Where we need it
   initialBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;          // Access we need
   initialBarrier.oldLayout = textureInternal->currentLayout;             // Use tracked layout
   initialBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;       // Ready to be source
   initialBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   initialBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   initialBarrier.image = image;
   initialBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   initialBarrier.subresourceRange.baseMipLevel = 0; // Only mip 0
   initialBarrier.subresourceRange.levelCount = 1;
   initialBarrier.subresourceRange.baseArrayLayer = 0;
   initialBarrier.subresourceRange.layerCount = textureInternal->arrayLayers;

   VkDependencyInfo initialDependencyInfo{};
   initialDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   initialDependencyInfo.imageMemoryBarrierCount = 1;
   initialDependencyInfo.pImageMemoryBarriers = &initialBarrier;

   vkCmdPipelineBarrier2(cmdInternal->commandBuffer, &initialDependencyInfo);

   // Transition all OTHER mip levels (1+) from UNDEFINED to TRANSFER_DST_OPTIMAL
   if (textureInternal->mipLevels > 1)
   {
      VkImageMemoryBarrier2 otherMipsBarrier{};
      otherMipsBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      otherMipsBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; // No prior usage
      otherMipsBarrier.srcAccessMask = 0;                                  // No prior access
      otherMipsBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      otherMipsBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      otherMipsBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;            // Uninitialized
      otherMipsBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // Ready for writes
      otherMipsBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      otherMipsBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      otherMipsBarrier.image = image;
      otherMipsBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      otherMipsBarrier.subresourceRange.baseMipLevel = 1; // Mips 1+
      otherMipsBarrier.subresourceRange.levelCount = textureInternal->mipLevels - 1;
      otherMipsBarrier.subresourceRange.baseArrayLayer = 0;
      otherMipsBarrier.subresourceRange.layerCount = textureInternal->arrayLayers;

      otherMipsBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      initialDependencyInfo.pImageMemoryBarriers = &otherMipsBarrier;
      vkCmdPipelineBarrier2(cmdInternal->commandBuffer, &initialDependencyInfo);
   }

   // Generate mipmaps by blitting from level i-1 to level i
   for (uint32_t i = 1; i < textureInternal->mipLevels; i++)
   {
      // Calculate dimensions for current mip level
      uint32_t nextMipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
      uint32_t nextMipHeight = mipHeight > 1 ? mipHeight / 2 : 1;

      // Blit from previous level to current level
      VkImageBlit blit{};
      blit.srcOffsets[0] = (VkOffset3D){0, 0, 0};
      blit.srcOffsets[1] = (VkOffset3D){(int32_t)mipWidth, (int32_t)mipHeight, 1};
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel = i - 1;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount = textureInternal->arrayLayers;

      blit.dstOffsets[0] = (VkOffset3D){0, 0, 0};
      blit.dstOffsets[1] = (VkOffset3D){(int32_t)nextMipWidth, (int32_t)nextMipHeight, 1};
      blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.mipLevel = i;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount = textureInternal->arrayLayers;

      vkCmdBlitImage(cmdInternal->commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

      // After blit, transition level i from DST to SRC for next iteration
      if (i < textureInternal->mipLevels - 1)
      {
         VkImageMemoryBarrier2 barrier{};
         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
         barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
         barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
         barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
         barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
         barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
         barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.image = image;
         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         barrier.subresourceRange.baseMipLevel = i; // Current level becomes source
         barrier.subresourceRange.levelCount = 1;
         barrier.subresourceRange.baseArrayLayer = 0;
         barrier.subresourceRange.layerCount = textureInternal->arrayLayers;

         VkDependencyInfo dependencyInfo{};
         dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
         dependencyInfo.imageMemoryBarrierCount = 1;
         dependencyInfo.pImageMemoryBarriers = &barrier;

         vkCmdPipelineBarrier2(cmdInternal->commandBuffer, &dependencyInfo);
      }

      // Update dimensions for next iteration
      mipWidth = nextMipWidth;
      mipHeight = nextMipHeight;
   }

   // transition ALL levels to SHADER_READ_ONLY at once
   VkImageMemoryBarrier2 finalBarrier{};
   finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   finalBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   finalBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
   finalBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
   finalBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
   finalBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Mixed layouts
   finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   finalBarrier.image = image;
   finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   finalBarrier.subresourceRange.baseMipLevel = 0;
   finalBarrier.subresourceRange.levelCount = textureInternal->mipLevels; // ALL levels
   finalBarrier.subresourceRange.baseArrayLayer = 0;
   finalBarrier.subresourceRange.layerCount = textureInternal->arrayLayers;

   VkDependencyInfo finalDependencyInfo{};
   finalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   finalDependencyInfo.imageMemoryBarrierCount = 1;
   finalDependencyInfo.pImageMemoryBarriers = &finalBarrier;

   vkCmdPipelineBarrier2(cmdInternal->commandBuffer, &finalDependencyInfo);

   textureInternal->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   return VE_SUCCESS;
}

VEResult veGenerateMipmapsImmediate(VEDevice *device, VETextureIndex texture)
{
   if (!device || texture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for immediate mipmap generation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Create a temporary command buffer for mipmap generation
   VECommandBuffer *cmd = veBeginCommandBuffer(device);
   if (!cmd)
   {
      veSetError("Failed to create command buffer for mipmap generation");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Generate mipmaps
   VEResult result = veGenerateMipmaps(device, cmd, texture);
   if (result != VE_SUCCESS)
   {
      // Note: command buffer will be cleaned up automatically on submission
      // failure
      return result;
   }

   // Submit and wait for completion
   result = veSubmitCommandBuffer(cmd, true);
   if (result != VE_SUCCESS)
   {
      veSetError("Failed to submit mipmap generation command buffer");
      return result;
   }

   return VE_SUCCESS;
}

VEResult veSaveTexture(VEDevice *device, VETextureIndex texture, const char *filename)
{
   if (!device || texture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for texture saving");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *textureInternal = deviceInternal->getTexture(texture);

   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index");
      return VE_ERROR_INVALID_PARAMETER;
   }

   std::vector<uint8_t> rawData;
   VEResult readResult = readTextureLevel0(deviceInternal, textureInternal, rawData);
   if (readResult != VE_SUCCESS)
   {
      return readResult;
   }

   std::vector<uint8_t> rgba8;
   if (!convertImageToRGBA8(textureInternal->format, rawData.data(), textureInternal->width, textureInternal->height,
                            rgba8))
   {
      veSetError("Unsupported texture format for PNG export");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   const uint32_t rowStride = textureInternal->width * 4u;
   const uint32_t halfHeight = textureInternal->height / 2u;
   for (uint32_t y = 0; y < halfHeight; ++y)
   {
      uint8_t *rowTop = rgba8.data() + static_cast<size_t>(y) * rowStride;
      uint8_t *rowBottom = rgba8.data() + static_cast<size_t>(textureInternal->height - 1u - y) * rowStride;
      for (uint32_t x = 0; x < rowStride; ++x)
      {
         std::swap(rowTop[x], rowBottom[x]);
      }
   }

   std::string outputPath = makePngFilename(filename);
   int writeResult = stbi_write_png(outputPath.c_str(), static_cast<int>(textureInternal->width),
                                    static_cast<int>(textureInternal->height), 4, rgba8.data(),
                                    static_cast<int>(textureInternal->width) * 4);

   if (writeResult == 0)
   {
      veSetError("Failed to write PNG file: %s", outputPath.c_str());
      return VE_ERROR_UNKNOWN;
   }

   return VE_SUCCESS;
}

// =============================================================================
// Descriptor Updates
// =============================================================================

VEResult VEDeviceInternal::updateTextureDescriptor(VETextureIndex index)
{
   VETextureInternal *texture = getTexture(index);
   if (!texture)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkDescriptorImageInfo imageInfo{};
   imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   imageInfo.imageView = texture->imageView;

   VkWriteDescriptorSet descriptorWrite{};
   descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   descriptorWrite.dstSet = textureDescriptorSet;
   descriptorWrite.dstBinding = 0;
   descriptorWrite.dstArrayElement = index;
   descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
   descriptorWrite.descriptorCount = 1;
   descriptorWrite.pImageInfo = &imageInfo;

   vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);

   return VE_SUCCESS;
}
