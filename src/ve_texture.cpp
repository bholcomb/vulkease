/**
 * @file ve_texture.c
 * @brief Texture Management Implementation
 */

#include "ve_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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

   if (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SNORM ||
       format == VK_FORMAT_R8G8B8A8_UINT || format == VK_FORMAT_R8G8B8A8_SINT || format == VK_FORMAT_R8G8B8A8_USCALED ||
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
      auto halfToFloat = [](uint16_t h)
      {
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

   auto writePixel = [&out](size_t index, float depth)
   {
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

   VECommandBuffer *cmdHandle = nullptr;
   if (veBeginCommandBuffer((VEDevice *)deviceInternal, &cmdHandle) != VE_SUCCESS || !cmdHandle)
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

   VESubmitInfo submitInfo{};
   submitInfo.waitForCompletion = true;
   VEResult submitResult = veSubmitCommandBuffer(cmdHandle, &submitInfo);
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

bool veQueryBindlessDescriptorCapacities(VkPhysicalDevice physicalDevice, uint32_t *outTextureCapacity,
                                         uint32_t *outSamplerCapacity)
{
   if (physicalDevice == VK_NULL_HANDLE || !outTextureCapacity || !outSamplerCapacity)
      return false;

   VkPhysicalDeviceDescriptorIndexingProperties indexingProps{};
   indexingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

   VkPhysicalDeviceProperties2 props2{};
   props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   props2.pNext = &indexingProps;
   vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

   uint32_t textureCapacity = std::min(
       static_cast<uint32_t>(VE_MAX_TEXTURES), std::min(props2.properties.limits.maxDescriptorSetSampledImages,
                                                        std::min(indexingProps.maxDescriptorSetUpdateAfterBindSampledImages,
                                                                 indexingProps.maxPerStageDescriptorUpdateAfterBindSampledImages)));
   uint32_t samplerCapacity =
       std::min(static_cast<uint32_t>(VE_MAX_SAMPLERS),
                std::min(props2.properties.limits.maxDescriptorSetSamplers,
                         std::min(indexingProps.maxDescriptorSetUpdateAfterBindSamplers,
                                  indexingProps.maxPerStageDescriptorUpdateAfterBindSamplers)));

   uint32_t totalLimit = std::min(indexingProps.maxPerStageUpdateAfterBindResources,
                                  indexingProps.maxUpdateAfterBindDescriptorsInAllPools);
   uint64_t combinedCapacity = static_cast<uint64_t>(textureCapacity) + samplerCapacity;
   if (combinedCapacity > totalLimit)
   {
      uint64_t excess = combinedCapacity - totalLimit;
      uint64_t textureReduction = std::min<uint64_t>(excess, textureCapacity > 2 ? textureCapacity - 2 : 0);
      textureCapacity -= static_cast<uint32_t>(textureReduction);
      excess -= textureReduction;

      uint64_t samplerReduction = std::min<uint64_t>(excess, samplerCapacity > 2 ? samplerCapacity - 2 : 0);
      samplerCapacity -= static_cast<uint32_t>(samplerReduction);
   }

   *outTextureCapacity = textureCapacity;
   *outSamplerCapacity = samplerCapacity;
   return textureCapacity >= 2 && samplerCapacity >= 2 &&
          static_cast<uint64_t>(textureCapacity) + samplerCapacity <= totalLimit;
}

VEResult VEDeviceInternal::initializeBindlessDescriptors()
{
   uint32_t textureCapacity = 0;
   uint32_t samplerCapacity = 0;
   if (!veQueryBindlessDescriptorCapacities(physicalDevice, &textureCapacity, &samplerCapacity))
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
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                  VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

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
                                                  VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                  VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

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
   VkDescriptorSetLayout layouts[] = {this->textureDescriptorSetLayout, this->samplerDescriptorSetLayout};

   VkDescriptorSetAllocateInfo allocInfo{};
   allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
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

   if (textureDescriptorSetLayout == VK_NULL_HANDLE || samplerDescriptorSetLayout == VK_NULL_HANDLE ||
       textureDescriptorSet == VK_NULL_HANDLE || samplerDescriptorSet == VK_NULL_HANDLE)
   {
      veSetError("Bindless descriptor initialization produced null handles");
      cleanupBindlessDescriptors();
      return VE_ERROR_UNKNOWN;
   }

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
   freeTextureCount.store(maxTextures - 1, std::memory_order_relaxed);
   textureCount.store(0, std::memory_order_relaxed);

   // Initialize texture index mutex
   textureIndexMutex = std::make_unique<std::mutex>();
   if (!textureIndexMutex)
   {
      veSetError("Failed to allocate texture index mutex");
      cleanupBindlessDescriptors();
      return VE_ERROR_OUT_OF_MEMORY;
   }

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
   freeSamplerCount.store(maxSamplers - 1, std::memory_order_relaxed);
   samplerCount.store(0, std::memory_order_relaxed);

   // Initialize sampler index mutex
   samplerIndexMutex = std::make_unique<std::mutex>();
   if (!samplerIndexMutex)
   {
      veSetError("Failed to allocate sampler index mutex");
      cleanupBindlessDescriptors();
      return VE_ERROR_OUT_OF_MEMORY;
   }

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
      for (uint32_t i = 1; i < maxTextures; i++)
      {
         if (textures[i].isValid)
         {
            veDestroyTextureImmediate(this, i);
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
      for (uint32_t i = 1; i < maxSamplers; i++)
      {
         if (samplers[i].isValid)
         {
            veDestroySamplerImmediate(this, i);
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
   if (!textureIndexMutex)
   {
      veSetError("Texture index mutex not initialized");
      return VE_INVALID_TEXTURE_INDEX;
   }

   std::lock_guard<std::mutex> lock(*textureIndexMutex);

   uint32_t currentFreeCount = freeTextureCount.load(std::memory_order_relaxed);
   if (currentFreeCount == 0)
   {
      veSetError("No free texture indices available (max: %u)", maxTextures);
      return VE_INVALID_TEXTURE_INDEX;
   }

   uint32_t newFreeCount = currentFreeCount - 1;
   uint32_t index = freeTextureIndices[newFreeCount];
   freeTextureCount.store(newFreeCount, std::memory_order_relaxed);
   textureCount.fetch_add(1, std::memory_order_relaxed);

   return index;
}

void VEDeviceInternal::freeTextureIndex(uint32_t index)
{
   if (index == 0 || index >= maxTextures || !textureIndexMutex)
   {
      return;
   }

   std::lock_guard<std::mutex> lock(*textureIndexMutex);

   uint32_t currentFreeCount = freeTextureCount.load(std::memory_order_relaxed);
   freeTextureIndices[currentFreeCount] = index;
   freeTextureCount.store(currentFreeCount + 1, std::memory_order_relaxed);
   textureCount.fetch_sub(1, std::memory_order_relaxed);
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
   VECommandBuffer *cmdPublic = nullptr;
   if (veBeginCommandBuffer((VEDevice *)device, &cmdPublic) != VE_SUCCESS || !cmdPublic)
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
   // Initial data contains only mip 0. Leave the remaining levels undefined
   // until mip generation (or an explicit upload) initializes them.
   barrier.subresourceRange.levelCount = 1;
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
   region.imageOffset = {0, 0, 0};
   region.imageExtent = {texture->width, texture->height, texture->depth};

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
   VESubmitInfo submitInfo{};
   submitInfo.waitForCompletion = true;
   VEResult submitResult = veSubmitCommandBuffer(cmdPublic, &submitInfo);

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

static VEResult veCreateTextureInternal(VEDevice *device, const VETextureDesc *desc, VkImageType imageType,
                                        bool cubeCompatible, VETextureIndex *outIndex)
{
   if (!outIndex)
   {
      veSetError("outIndex cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device || !desc)
   {
      veSetError("Invalid parameters for texture creation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   if (desc->width == 0 || desc->height == 0 || desc->depth == 0 || desc->arrayLayers == 0 ||
       desc->format == VK_FORMAT_UNDEFINED || desc->sampleCount == 0)
   {
      veSetError("Texture dimensions, layers, format, and sample count must be valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (imageType == VK_IMAGE_TYPE_3D && desc->arrayLayers != 1)
   {
      veSetError("3D textures cannot have array layers");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if ((imageType == VK_IMAGE_TYPE_1D && (desc->height != 1 || desc->depth != 1)) ||
       (imageType == VK_IMAGE_TYPE_2D && desc->depth != 1) ||
       (cubeCompatible &&
        (imageType != VK_IMAGE_TYPE_2D || desc->arrayLayers != 6 || desc->width != desc->height)))
   {
      veSetError("Texture dimensions/layers are incompatible with the requested image type");
      return VE_ERROR_INVALID_PARAMETER;
   }

   uint32_t longestSide = std::max(desc->width, std::max(desc->height, desc->depth));
   uint32_t maximumMipLevels = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(longestSide)))) + 1;
   uint32_t mipLevels = desc->mipLevels == 0 ? maximumMipLevels : desc->mipLevels;
   if (mipLevels > maximumMipLevels || (desc->sampleCount != VK_SAMPLE_COUNT_1_BIT && mipLevels != 1))
   {
      veSetError("Invalid mip level count %u for the texture dimensions/sample count", mipLevels);
      return VE_ERROR_INVALID_PARAMETER;
   }

   uint32_t index = deviceInternal->allocateTextureIndex();
   if (index == VE_INVALID_TEXTURE_INDEX)
   {
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VETextureInternal *texture = &deviceInternal->textures[index];
   memset(texture, 0, sizeof(VETextureInternal));

   texture->width = desc->width;
   texture->height = desc->height;
   texture->depth = desc->depth;
   texture->mipLevels = mipLevels;
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
   imageInfo.imageType = imageType;
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

   if (cubeCompatible)
   {
      imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
   }

   // Handle sparse texture creation
   texture->isSparse = desc->sparse;
   texture->sparseData = nullptr;

   if (desc->sparse)
   {
      // Check for sparse support
      if (!deviceInternal->supportsSparseBinding())
      {
         veSetError("Sparse binding not supported on this device");
         deviceInternal->freeTextureIndex(index);
         return VE_ERROR_FEATURE_NOT_SUPPORTED;
      }

      if (imageInfo.imageType == VK_IMAGE_TYPE_2D && !deviceInternal->supportsSparseResidencyImage2D())
      {
         veSetError("Sparse residency for 2D images not supported on this device");
         deviceInternal->freeTextureIndex(index);
         return VE_ERROR_FEATURE_NOT_SUPPORTED;
      }

      if (imageInfo.imageType == VK_IMAGE_TYPE_3D && !deviceInternal->features.sparseResidencyImage3D)
      {
         veSetError("Sparse residency for 3D images not supported on this device");
         deviceInternal->freeTextureIndex(index);
         return VE_ERROR_FEATURE_NOT_SUPPORTED;
      }

      imageInfo.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;

      // Create sparse image without memory
      VkResult result = vkCreateImage(deviceInternal->device, &imageInfo, nullptr, &texture->image);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to create sparse image (VkResult: %d)", result);
         deviceInternal->freeTextureIndex(index);
         return VE_ERROR_OUT_OF_MEMORY;
      }

      // Query sparse memory requirements
      uint32_t sparseMemReqCount = 0;
      vkGetImageSparseMemoryRequirements(deviceInternal->device, texture->image, &sparseMemReqCount, nullptr);

      if (sparseMemReqCount == 0)
      {
         veSetError("No sparse memory requirements returned");
         vkDestroyImage(deviceInternal->device, texture->image, nullptr);
         deviceInternal->freeTextureIndex(index);
         return VE_ERROR_UNKNOWN;
      }

      std::vector<VkSparseImageMemoryRequirements> sparseReqs(sparseMemReqCount);
      vkGetImageSparseMemoryRequirements(deviceInternal->device, texture->image, &sparseMemReqCount, sparseReqs.data());

      // Find the color aspect requirements
      VkSparseImageMemoryRequirements *colorReqs = nullptr;
      for (auto &req : sparseReqs)
      {
         if (req.formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
         {
            colorReqs = &req;
            break;
         }
      }

      if (!colorReqs)
      {
         veSetError("No sparse memory requirements for color aspect");
         vkDestroyImage(deviceInternal->device, texture->image, nullptr);
         deviceInternal->freeTextureIndex(index);
         return VE_ERROR_UNKNOWN;
      }

      // Initialize sparse texture data
      texture->sparseData = new VESparseTextureData();
      VESparseTextureData *sparseData = texture->sparseData;

      sparseData->requirements = *colorReqs;
      sparseData->pageWidth = colorReqs->formatProperties.imageGranularity.width;
      sparseData->pageHeight = colorReqs->formatProperties.imageGranularity.height;
      sparseData->pageDepth = colorReqs->formatProperties.imageGranularity.depth;
      sparseData->pagesX = (texture->width + sparseData->pageWidth - 1) / sparseData->pageWidth;
      sparseData->pagesY = (texture->height + sparseData->pageHeight - 1) / sparseData->pageHeight;
      sparseData->pagesZ = (texture->depth + sparseData->pageDepth - 1) / sparseData->pageDepth;
      sparseData->mipTailFirstLod = colorReqs->imageMipTailFirstLod;
      sparseData->mipTailAllocation = VK_NULL_HANDLE;
      sparseData->committedPageCount = 0;

      // Get memory requirements for page size
      VkMemoryRequirements memReqs;
      vkGetImageMemoryRequirements(deviceInternal->device, texture->image, &memReqs);
      sparseData->pageSize = memReqs.alignment; // Typically 64KB

      // Calculate total page count (excluding mip tail)
      sparseData->totalPageCount = 0;
      for (uint32_t mip = 0; mip < sparseData->mipTailFirstLod && mip < texture->mipLevels; mip++)
      {
         uint32_t mipPagesX = std::max(1u, sparseData->pagesX >> mip);
         uint32_t mipPagesY = std::max(1u, sparseData->pagesY >> mip);
         uint32_t mipPagesZ = std::max(1u, sparseData->pagesZ >> mip);
         sparseData->totalPageCount += mipPagesX * mipPagesY * mipPagesZ;
      }
      sparseData->totalPageCount *= texture->arrayLayers;

      // Initialize page tracking
      sparseData->pages.resize(sparseData->totalPageCount);
      for (auto &page : sparseData->pages)
      {
         page.allocation = VK_NULL_HANDLE;
         page.uncommitTime = 0;
         page.isCommitted = false;
      }

      // Auto-commit mip tail if present
      if (sparseData->mipTailFirstLod < texture->mipLevels && colorReqs->imageMipTailSize > 0)
      {
         // Find suitable memory type
         uint32_t memoryTypeIndex = UINT32_MAX;
         for (uint32_t i = 0; i < deviceInternal->memoryProperties.memoryTypeCount; i++)
         {
            if ((memReqs.memoryTypeBits & (1u << i)) &&
                (deviceInternal->memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
               memoryTypeIndex = i;
               break;
            }
         }

         if (memoryTypeIndex == UINT32_MAX)
         {
            veSetError("No device-local memory type is available for the sparse mip tail");
            delete sparseData;
            texture->sparseData = nullptr;
            vkDestroyImage(deviceInternal->device, texture->image, nullptr);
            texture->image = VK_NULL_HANDLE;
            deviceInternal->freeTextureIndex(index);
            return VE_ERROR_FEATURE_NOT_SUPPORTED;
         }

         if (memoryTypeIndex != UINT32_MAX)
         {
            VmaAllocationCreateInfo mipTailAllocInfo{};
            mipTailAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            mipTailAllocInfo.memoryTypeBits = (1u << memoryTypeIndex);

            VkMemoryRequirements mipTailReqs = memReqs;
            mipTailReqs.size = colorReqs->imageMipTailSize;

            VmaAllocationInfo allocInfoOut;
            VkResult allocResult = vmaAllocateMemory(deviceInternal->allocator, &mipTailReqs, &mipTailAllocInfo,
                                                     &sparseData->mipTailAllocation, &allocInfoOut);
            if (allocResult == VK_SUCCESS)
            {
               // Bind mip tail using opaque bind
               VkSparseMemoryBind opaqueBind{};
               opaqueBind.resourceOffset = colorReqs->imageMipTailOffset;
               opaqueBind.size = colorReqs->imageMipTailSize;
               opaqueBind.memory = allocInfoOut.deviceMemory;
               opaqueBind.memoryOffset = allocInfoOut.offset;

               VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo{};
               opaqueBindInfo.image = texture->image;
               opaqueBindInfo.bindCount = 1;
               opaqueBindInfo.pBinds = &opaqueBind;

               VkBindSparseInfo bindSparseInfo{};
               bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
               bindSparseInfo.imageOpaqueBindCount = 1;
               bindSparseInfo.pImageOpaqueBinds = &opaqueBindInfo;

               VkResult bindResult = VK_ERROR_INITIALIZATION_FAILED;
               VEDeviceQueueLocks *locks = deviceInternal->queueLocks.get();
               if (locks)
               {
                  std::lock_guard<std::mutex> queueLock(locks->graphicsMutex());
                  bindResult = vkQueueBindSparse(deviceInternal->sparseBindingQueue, 1, &bindSparseInfo,
                                                 VK_NULL_HANDLE);
                  if (bindResult == VK_SUCCESS)
                     bindResult = vkQueueWaitIdle(deviceInternal->sparseBindingQueue);
               }

               if (bindResult != VK_SUCCESS)
               {
                  veSetError("Failed to bind sparse mip tail (VkResult: %d)", bindResult);
                  vmaFreeMemory(deviceInternal->allocator, sparseData->mipTailAllocation);
                  sparseData->mipTailAllocation = VK_NULL_HANDLE;
                  delete sparseData;
                  texture->sparseData = nullptr;
                  vkDestroyImage(deviceInternal->device, texture->image, nullptr);
                  texture->image = VK_NULL_HANDLE;
                  deviceInternal->freeTextureIndex(index);
                  return VE_ERROR_UNKNOWN;
               }
            }
            else
            {
               veSetError("Failed to allocate sparse mip-tail memory (VkResult: %d)", allocResult);
               delete sparseData;
               texture->sparseData = nullptr;
               vkDestroyImage(deviceInternal->device, texture->image, nullptr);
               texture->image = VK_NULL_HANDLE;
               deviceInternal->freeTextureIndex(index);
               return VE_ERROR_OUT_OF_MEMORY;
            }
         }
      }

      texture->allocation = VK_NULL_HANDLE; // Sparse textures don't use VMA for the main allocation
   }
   else
   {
      VmaAllocationCreateInfo allocInfo{};
      allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

      VkResult result = vmaCreateImage(deviceInternal->allocator, &imageInfo, &allocInfo, &texture->image,
                                       &texture->allocation, &texture->allocationInfo);

      if (result != VK_SUCCESS)
      {
         veSetError("Failed to create texture (VkResult: %d)", result);
         deviceInternal->freeTextureIndex(index);
         return VE_ERROR_OUT_OF_MEMORY;
      }
   }

   // Create image view
   VkImageViewCreateInfo viewInfo{};
   viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   viewInfo.image = texture->image;
   viewInfo.viewType = imageInfo.imageType == VK_IMAGE_TYPE_3D
                           ? VK_IMAGE_VIEW_TYPE_3D
                           : (imageInfo.imageType == VK_IMAGE_TYPE_2D
                                  ? (cubeCompatible ? VK_IMAGE_VIEW_TYPE_CUBE
                                                    : (desc->arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                                             : VK_IMAGE_VIEW_TYPE_2D))
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

   VkResult viewResult = vkCreateImageView(deviceInternal->device, &viewInfo, NULL, &texture->imageView);
   if (viewResult != VK_SUCCESS)
   {
      veSetError("Failed to create image view (VkResult: %d)", viewResult);
      if (texture->isSparse)
      {
         if (texture->sparseData)
         {
            if (texture->sparseData->mipTailAllocation)
               vmaFreeMemory(deviceInternal->allocator, texture->sparseData->mipTailAllocation);
            delete texture->sparseData;
         }
         vkDestroyImage(deviceInternal->device, texture->image, nullptr);
      }
      else
      {
         vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
      }
      deviceInternal->freeTextureIndex(index);
      return VE_ERROR_UNKNOWN;
   }

   // Upload initial data if provided (ignored for sparse textures)
   if (!texture->isSparse && desc->initialData && desc->initialDataSize > 0)
   {
      VEResult uploadResult = veUploadTextureData(deviceInternal, texture, desc->initialData, desc->initialDataSize);
      if (uploadResult != VE_SUCCESS)
      {
         vkDestroyImageView(deviceInternal->device, texture->imageView, NULL);
         vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
         deviceInternal->freeTextureIndex(index);
         return uploadResult;
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

   *outIndex = index;
   return VE_SUCCESS;
}

VEResult veCreateTexture(VEDevice *device, const VETextureDesc *desc, VETextureIndex *outIndex)
{
   VkImageType imageType = desc && desc->depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
   return veCreateTextureInternal(device, desc, imageType, false, outIndex);
}

void veDestroyTextureImmediate(VEDeviceInternal *deviceInternal, VETextureIndex index)
{
   if (!deviceInternal || index == VE_INVALID_TEXTURE_INDEX)
   {
      return;
   }

   VETextureInternal *texture = deviceInternal->getTexture(index);

   if (!texture || !texture->isValid)
   {
      return;
   }

   if (texture->imageView)
   {
      vkDestroyImageView(deviceInternal->device, texture->imageView, NULL);
   }

   // Handle sparse texture cleanup
   if (texture->isSparse && texture->sparseData)
   {
      VESparseTextureData *sparseData = texture->sparseData;

      // Free all committed page allocations
      for (auto &page : sparseData->pages)
      {
         if (page.allocation != VK_NULL_HANDLE)
         {
            vmaFreeMemory(deviceInternal->allocator, page.allocation);
         }
      }

      // Free mip tail allocation
      if (sparseData->mipTailAllocation != VK_NULL_HANDLE)
      {
         vmaFreeMemory(deviceInternal->allocator, sparseData->mipTailAllocation);
      }

      delete sparseData;
      texture->sparseData = nullptr;

      // Destroy the sparse image
      if (texture->image)
      {
         vkDestroyImage(deviceInternal->device, texture->image, nullptr);
      }
   }
   // Only destroy VkImage and VMA allocation if we own it (not external)
   else if (!texture->isExternal && texture->image)
   {
      vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
   }

   deviceInternal->freeTextureIndex(index);
   memset(texture, 0, sizeof(VETextureInternal));
}

VEResult veDestroyTexture(VEDevice *device, VETextureIndex index)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for texture destruction");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   if (!deviceInternal->deferredDeletionQueue)
   {
      veSetError("Deferred deletion queue not initialized");
      return VE_ERROR_NOT_INITIALIZED;
   }
   deviceInternal->deferredDeletionQueue->enqueueTexture(index);
   return VE_SUCCESS;
}

// =============================================================================
// Texture Property Queries
// =============================================================================

VkExtent3D veGetTextureSize(VEDevice *device, VETextureIndex index)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veGetTextureSize: invalid device or texture index");
      return VkExtent3D{0, 0, 0};
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = deviceInternal->getTexture(index);
   if (!texture || !texture->isValid)
   {
      veSetError("veGetTextureSize: texture not found for index=%u", index);
      return VkExtent3D{0, 0, 0};
   }

   return VkExtent3D{texture->width, texture->height, texture->depth};
}

VkFormat veGetTextureFormat(VEDevice *device, VETextureIndex index)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veGetTextureFormat: invalid device or texture index");
      return VK_FORMAT_UNDEFINED;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = deviceInternal->getTexture(index);
   if (!texture || !texture->isValid)
   {
      veSetError("veGetTextureFormat: texture not found for index=%u", index);
      return VK_FORMAT_UNDEFINED;
   }

   return texture->format;
}

// =============================================================================
// Convenience Texture Creation Functions
// =============================================================================

VEResult veCreateTexture1D(VEDevice *device, uint32_t width, VkFormat format, VkImageUsageFlags usage,
                           const char *debugName, VETextureIndex *outIndex)
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

   return veCreateTextureInternal(device, &desc, VK_IMAGE_TYPE_1D, false, outIndex);
}

VEResult veCreateTexture2D(VEDevice *device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
                           const char *debugName, VETextureIndex *outIndex)
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

   return veCreateTexture(device, &desc, outIndex);
}

VEResult veCreateTexture3D(VEDevice *device, uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                           VkImageUsageFlags usage, const char *debugName, VETextureIndex *outIndex)
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

   return veCreateTexture(device, &desc, outIndex);
}

VEResult veCreateTexture2DArray(VEDevice *device, uint32_t width, uint32_t height, uint32_t layers, VkFormat format,
                                VkImageUsageFlags usage, const char *debugName, VETextureIndex *outIndex)
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

   return veCreateTexture(device, &desc, outIndex);
}

VEResult veCreateTextureCube(VEDevice *device, uint32_t size, VkFormat format, VkImageUsageFlags usage,
                             const char *debugName, VETextureIndex *outIndex)
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

   return veCreateTextureInternal(device, &desc, VK_IMAGE_TYPE_2D, true, outIndex);
}

VEResult veCreateTexture2DMultisample(VEDevice *device, uint32_t width, uint32_t height, VkFormat format,
                                      VkSampleCountFlags sampleCount, VkImageUsageFlags usage, const char *debugName,
                                      VETextureIndex *outIndex)
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

   return veCreateTexture(device, &desc, outIndex);
}

// =============================================================================
// STB Image Integration
// =============================================================================

VEResult veLoadTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage, bool generateMips,
                       VETextureIndex *outIndex)
{
   if (!outIndex)
   {
      veSetError("outIndex cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device || !filename)
   {
      veSetError("Invalid parameters for texture loading");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // flip images for loading into texture memory
   stbi_set_flip_vertically_on_load(true);

   int width, height, channels;
   stbi_uc *pixels = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

   if (!pixels)
   {
      veSetError("Failed to load image: %s", stbi_failure_reason());
      return VE_ERROR_UNKNOWN;
   }

   uint32_t mipLevels = 1;
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

   VETextureIndex created = VE_INVALID_TEXTURE_INDEX;
   VEResult createResult = veCreateTexture(device, &desc, &created);

   // Generate mipmaps if requested and texture creation succeeded
   if (createResult == VE_SUCCESS && created != VE_INVALID_TEXTURE_INDEX && generateMips && desc.mipLevels > 1)
   {
      VEResult mipmapResult = veGenerateMipmaps(device, created);
      if (mipmapResult != VE_SUCCESS)
      {
         veSetError("Failed to generate mipmaps for loaded texture: %s", filename);
         (void)veDestroyTexture(device, created);
         stbi_image_free(pixels);
         return mipmapResult;
      }
   }

   stbi_image_free(pixels);
   if (createResult == VE_SUCCESS)
   {
      *outIndex = created;
   }
   return createResult;
}

VEResult veLoadHDRTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage, bool generateMips,
                          VETextureIndex *outIndex)
{
   if (!outIndex)
   {
      veSetError("outIndex cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device || !filename)
   {
      veSetError("Invalid parameters for HDR texture loading");
      return VE_ERROR_INVALID_PARAMETER;
   }

   int width, height, channels;
   float *pixels = stbi_loadf(filename, &width, &height, &channels, STBI_rgb_alpha);

   if (!pixels)
   {
      veSetError("Failed to load HDR image: %s", stbi_failure_reason());
      return VE_ERROR_UNKNOWN;
   }

   uint32_t mipLevels = 1;
   if (generateMips)
   {
      uint32_t longestSide = static_cast<uint32_t>(std::max(width, height));
      mipLevels = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(longestSide)))) + 1;
      usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   }

   VETextureDesc desc{};
   desc.width = static_cast<uint32_t>(width);
   desc.height = static_cast<uint32_t>(height);
   desc.depth = 1;
   desc.mipLevels = mipLevels;
   desc.arrayLayers = 1;
   desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
   desc.usage = usage;
   desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
   desc.initialData = pixels;
   desc.initialDataSize = static_cast<uint64_t>(width * height * 4 * sizeof(float));
   desc.debugName = filename;

   VETextureIndex created = VE_INVALID_TEXTURE_INDEX;
   VEResult createResult = veCreateTexture(device, &desc, &created);

   // Generate mipmaps if requested and texture creation succeeded
   if (createResult == VE_SUCCESS && created != VE_INVALID_TEXTURE_INDEX && generateMips && desc.mipLevels > 1)
   {
      VEResult mipmapResult = veGenerateMipmaps(device, created);
      if (mipmapResult != VE_SUCCESS)
      {
         veSetError("Failed to generate mipmaps for loaded HDR texture: %s", filename);
         (void)veDestroyTexture(device, created);
         stbi_image_free(pixels);
         return mipmapResult;
      }
   }

   stbi_image_free(pixels);
   if (createResult == VE_SUCCESS)
   {
      *outIndex = created;
   }
   return createResult;
}

VEResult veLoadCubeTexture(VEDevice *device, const char *filenames[6], VkImageUsageFlags usage, bool generateMips,
                           VETextureIndex *outIndex)
{
   if (!outIndex)
   {
      veSetError("outIndex cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device || !filenames)
   {
      veSetError("Invalid parameters for cube texture loading");
      return VE_ERROR_INVALID_PARAMETER;
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
         return VE_ERROR_INVALID_PARAMETER;
      }

      int faceWidth = 0;
      int faceHeight = 0;
      int faceChannels = 0;

      stbi_uc *pixels = stbi_load(path, &faceWidth, &faceHeight, &faceChannels, desiredChannels);
      if (!pixels)
      {
         veSetError("Failed to load cube texture face %d: %s", face, stbi_failure_reason());
         return VE_ERROR_UNKNOWN;
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
            return VE_ERROR_INVALID_PARAMETER;
         }
      }

      std::memcpy(combinedData.data() + faceSize * face, pixels, faceSize);
      stbi_image_free(pixels);
   }

   if (width == 0 || height == 0)
   {
      veSetError("Cube texture faces have zero size");
      return VE_ERROR_INVALID_PARAMETER;
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

   VETextureIndex created = VE_INVALID_TEXTURE_INDEX;
   VEResult createResult = veCreateTextureInternal(device, &desc, VK_IMAGE_TYPE_2D, true, &created);
   if (createResult != VE_SUCCESS)
   {
      return createResult;
   }

   if (generateMips && mipLevels > 1)
   {
      VEResult mipResult = veGenerateMipmaps(device, created);
      if (mipResult != VE_SUCCESS)
      {
         (void)veDestroyTexture(device, created);
         return mipResult;
      }
   }

   *outIndex = created;
   return VE_SUCCESS;
}

// =============================================================================
// External Texture Import
// =============================================================================

VEResult veImportExternalTexture(VEDevice *device, const VEExternalTextureDesc *desc, VETextureIndex *outIndex)
{
   if (!device || !desc || !outIndex)
   {
      veSetError("veImportExternalTexture: NULL parameter");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (desc->image == VK_NULL_HANDLE)
   {
      veSetError("veImportExternalTexture: desc->image cannot be VK_NULL_HANDLE");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);

   // Allocate a texture index
   uint32_t index = deviceInternal->allocateTextureIndex();
   if (index == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("veImportExternalTexture: no free texture slots");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VETextureInternal *texture = &deviceInternal->textures[index];

   // Create VkImageView for the external image
   VkImageViewCreateInfo viewInfo{};
   viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   viewInfo.image = desc->image;
   viewInfo.viewType = desc->viewType;
   viewInfo.format = desc->format;
   viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
   viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
   viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
   viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
   viewInfo.subresourceRange.aspectMask = desc->aspectMask;
   viewInfo.subresourceRange.baseMipLevel = 0;
   viewInfo.subresourceRange.levelCount = desc->mipLevels > 0 ? desc->mipLevels : 1;
   viewInfo.subresourceRange.baseArrayLayer = 0;
   viewInfo.subresourceRange.layerCount = desc->arrayLayers > 0 ? desc->arrayLayers : 1;

   VkResult result = vkCreateImageView(deviceInternal->device, &viewInfo, nullptr, &texture->imageView);
   if (result != VK_SUCCESS)
   {
      deviceInternal->freeTextureIndex(index);
      veSetError("veImportExternalTexture: failed to create image view (%s)", veVkResultToString(result));
      return VE_ERROR_UNKNOWN;
   }

   // Fill in texture metadata
   texture->image = desc->image;
   texture->allocation = VK_NULL_HANDLE; // No VMA allocation - external image
   texture->allocationInfo = {};
   texture->width = desc->width;
   texture->height = desc->height;
   texture->depth = desc->depth > 0 ? desc->depth : 1;
   texture->mipLevels = desc->mipLevels > 0 ? desc->mipLevels : 1;
   texture->arrayLayers = desc->arrayLayers > 0 ? desc->arrayLayers : 1;
   texture->format = desc->format;
   texture->usage = 0; // Unknown for external images
   texture->sampleCount = VK_SAMPLE_COUNT_1_BIT;
   texture->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Unknown initial layout
   texture->isValid = true;
   texture->isExternal = true; // Mark as external - do not destroy VkImage
   texture->index = index;

   if (desc->debugName)
   {
      strncpy(texture->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
      texture->debugName[VE_MAX_DEBUG_NAME_LENGTH - 1] = '\0';

      // Set debug name on the image view
      VkDebugUtilsObjectNameInfoEXT nameInfo{};
      nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
      nameInfo.objectHandle = (uint64_t)(uintptr_t)texture->imageView;
      nameInfo.pObjectName = texture->debugName;
      if (veFuncs.vkSetDebugUtilsObjectNameEXT)
      {
         veFuncs.vkSetDebugUtilsObjectNameEXT(deviceInternal->device, &nameInfo);
      }
   }
   else
   {
      texture->debugName[0] = '\0';
   }

   // Update bindless descriptor set
   deviceInternal->updateTextureDescriptor(index);

   deviceInternal->textureCount++;
   *outIndex = index;
   return VE_SUCCESS;
}

VEResult veReleaseExternalTexture(VEDevice *device, VETextureIndex index)
{
   if (!device)
   {
      veSetError("veReleaseExternalTexture: device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (index == VE_INVALID_TEXTURE_INDEX)
   {
      return VE_SUCCESS; // No-op for invalid index
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);
   VETextureInternal *texture = deviceInternal->getTexture(index);

   if (!texture || !texture->isValid)
   {
      veSetError("veReleaseExternalTexture: invalid texture index %u", index);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Destroy the image view (we created this)
   if (texture->imageView != VK_NULL_HANDLE)
   {
      vkDestroyImageView(deviceInternal->device, texture->imageView, nullptr);
      texture->imageView = VK_NULL_HANDLE;
   }

   // Do NOT destroy VkImage - it's externally owned
   // Do NOT free VMA allocation - there isn't one for external textures

   // Clear the texture slot
   texture->image = VK_NULL_HANDLE;
   texture->isValid = false;
   texture->isExternal = false;
   texture->debugName[0] = '\0';

   deviceInternal->freeTextureIndex(index);
   deviceInternal->textureCount--;

   return VE_SUCCESS;
}

VEResult veCmdGenerateMipmaps(VECommandBuffer *cmd, VETextureIndex texture)
{
   if (!cmd || texture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for mipmap generation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *cmdInternal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *deviceInternal = cmdInternal->device;

   if (!deviceInternal || !cmdInternal->isRecording)
   {
      veSetError("Mipmap generation requires a recording command buffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

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

   if (textureInternal->depth != 1)
   {
      veSetError("Blit-based mipmap generation does not support 3D textures");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   // Check if format supports linear filtering (required for mipmap generation)
   VkFormatProperties formatProps;
   vkGetPhysicalDeviceFormatProperties(deviceInternal->physicalDevice, textureInternal->format, &formatProps);

   VkFormatFeatureFlags requiredFormatFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                                 VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
   if ((formatProps.optimalTilingFeatures & requiredFormatFeatures) != requiredFormatFeatures)
   {
      veSetError("Format does not support linear filtering required for mipmap "
                 "generation");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   // Ensure texture has transfer source usage for blitting
   VkImageUsageFlags usage = textureInternal->usage;
   if ((usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) !=
       (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
   {
      veSetError("Texture must have transfer source and destination usage for mipmap generation");
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
   initialBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   initialBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
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
      blit.srcOffsets[0] = {0, 0, 0};
      blit.srcOffsets[1] = {static_cast<int32_t>(mipWidth), static_cast<int32_t>(mipHeight), 1};
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel = i - 1;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount = textureInternal->arrayLayers;

      blit.dstOffsets[0] = {0, 0, 0};
      blit.dstOffsets[1] = {static_cast<int32_t>(nextMipWidth), static_cast<int32_t>(nextMipHeight), 1};
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
         barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
         barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
         barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
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

   // All but the last mip are transfer sources; the last mip remains a
   // transfer destination. They require distinct old layouts.
   VkImageMemoryBarrier2 finalBarriers[2]{};
   finalBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   finalBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   finalBarriers[0].srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
   finalBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   finalBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
   finalBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   finalBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   finalBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   finalBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   finalBarriers[0].image = image;
   finalBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   finalBarriers[0].subresourceRange.baseMipLevel = 0;
   finalBarriers[0].subresourceRange.levelCount = textureInternal->mipLevels - 1;
   finalBarriers[0].subresourceRange.baseArrayLayer = 0;
   finalBarriers[0].subresourceRange.layerCount = textureInternal->arrayLayers;

   finalBarriers[1] = finalBarriers[0];
   finalBarriers[1].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
   finalBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   finalBarriers[1].subresourceRange.baseMipLevel = textureInternal->mipLevels - 1;
   finalBarriers[1].subresourceRange.levelCount = 1;

   VkDependencyInfo finalDependencyInfo{};
   finalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   finalDependencyInfo.imageMemoryBarrierCount = 2;
   finalDependencyInfo.pImageMemoryBarriers = finalBarriers;

   vkCmdPipelineBarrier2(cmdInternal->commandBuffer, &finalDependencyInfo);

   textureInternal->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   return VE_SUCCESS;
}

VEResult veGenerateMipmaps(VEDevice *device, VETextureIndex texture)
{
   if (!device || texture == VE_INVALID_TEXTURE_INDEX)
   {
      veSetError("Invalid parameters for mipmap generation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Create a temporary command buffer for mipmap generation
   VECommandBuffer *cmd = nullptr;
   if (veBeginCommandBuffer(device, &cmd) != VE_SUCCESS || !cmd)
   {
      veSetError("Failed to create command buffer for mipmap generation");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Generate mipmaps
   VEResult result = veCmdGenerateMipmaps(cmd, texture);
   if (result != VE_SUCCESS)
   {
      // Note: command buffer will be cleaned up automatically on submission
      // failure
      return result;
   }

   // Submit and wait for completion
   VESubmitInfo submitInfo{};
   submitInfo.waitForCompletion = true;
   result = veSubmitCommandBuffer(cmd, &submitInfo);
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
