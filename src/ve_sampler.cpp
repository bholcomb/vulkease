/**
 * @file ve_sampler.c
 * @brief Sampler Management Implementation
 */

#include "ve_internal.h"

#include <cstring>

// =============================================================================
// Sampler Index Management
// =============================================================================

uint32_t VEDeviceInternal::allocateSamplerIndex()
{
   if (!samplerIndexMutex)
   {
      veSetError("Sampler index mutex not initialized");
      return VE_INVALID_SAMPLER_INDEX;
   }

   std::lock_guard<std::mutex> lock(*samplerIndexMutex);
   
   uint32_t currentFreeCount = freeSamplerCount.load(std::memory_order_relaxed);
   if (currentFreeCount == 0)
   {
      veSetError("No free sampler indices available (max: %u)", maxSamplers);
      return VE_INVALID_SAMPLER_INDEX;
   }

   uint32_t newFreeCount = currentFreeCount - 1;
   uint32_t index = freeSamplerIndices[newFreeCount];
   freeSamplerCount.store(newFreeCount, std::memory_order_relaxed);
   samplerCount.fetch_add(1, std::memory_order_relaxed);

   return index;
}

void VEDeviceInternal::freeSamplerIndex(uint32_t index)
{
   if (index == 0 || index >= maxSamplers || !samplerIndexMutex)
   {
      return;
   }

   std::lock_guard<std::mutex> lock(*samplerIndexMutex);
   
   uint32_t currentFreeCount = freeSamplerCount.load(std::memory_order_relaxed);
   freeSamplerIndices[currentFreeCount] = index;
   freeSamplerCount.store(currentFreeCount + 1, std::memory_order_relaxed);
   samplerCount.fetch_sub(1, std::memory_order_relaxed);
}

VESamplerInternal *VEDeviceInternal::getSampler(VESamplerIndex index)
{
   if (index == VE_INVALID_SAMPLER_INDEX || index >= maxSamplers)
   {
      return NULL;
   }

   VESamplerInternal *sampler = &samplers[index];
   return sampler->isValid ? sampler : NULL;
}

const VESamplerInternal *VEDeviceInternal::getSampler(VESamplerIndex index) const
{
   return const_cast<VEDeviceInternal *>(this)->getSampler(index);
}

// =============================================================================
// Sampler Creation
// =============================================================================

VESamplerIndex veCreateSampler(VEDevice *device, const VESamplerDesc *desc)
{
   if (!device || !desc)
   {
      veSetError("Invalid parameters for sampler creation");
      return VE_INVALID_SAMPLER_INDEX;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   uint32_t index = deviceInternal->allocateSamplerIndex();
   if (index == VE_INVALID_SAMPLER_INDEX)
   {
      return VE_INVALID_SAMPLER_INDEX;
   }

   VESamplerInternal *sampler = &deviceInternal->samplers[index];
   memset(sampler, 0, sizeof(VESamplerInternal));

   sampler->desc = *desc;
   sampler->index = index;

   if (desc->debugName)
   {
      strncpy(sampler->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
   }
   else
   {
      snprintf(sampler->debugName, VE_MAX_DEBUG_NAME_LENGTH, "Sampler_%u", index);
   }

   VkSamplerCreateInfo samplerInfo{};
   samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   samplerInfo.magFilter = desc->magFilter;
   samplerInfo.minFilter = desc->minFilter;
   samplerInfo.mipmapMode = desc->mipmapFilter;
   samplerInfo.addressModeU = desc->addressModeU;
   samplerInfo.addressModeV = desc->addressModeV;
   samplerInfo.addressModeW = desc->addressModeW;
   samplerInfo.mipLodBias = 0.0f;
   samplerInfo.anisotropyEnable = desc->maxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
   samplerInfo.maxAnisotropy = desc->maxAnisotropy;
   samplerInfo.compareEnable = desc->compareEnable ? VK_TRUE : VK_FALSE;
   samplerInfo.compareOp = desc->compareOp;
   samplerInfo.minLod = desc->minLod;
   samplerInfo.maxLod = desc->maxLod;
   samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
   samplerInfo.unnormalizedCoordinates = VK_FALSE;

   VkResult result = vkCreateSampler(deviceInternal->device, &samplerInfo, NULL, &sampler->sampler);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create sampler (VkResult: %d)", result);
      deviceInternal->freeSamplerIndex(index);
      return VE_INVALID_SAMPLER_INDEX;
   }

   if (deviceInternal->context->validationEnabled)
   {
      veSetObjectDebugName(deviceInternal, (uint64_t)sampler->sampler, VK_OBJECT_TYPE_SAMPLER, sampler->debugName);
   }

   sampler->isValid = true;

   // Update bindless descriptor
   deviceInternal->updateSamplerDescriptor(index);

   return index;
}

void veDestroySamplerImmediate(VEDeviceInternal *deviceInternal, VESamplerIndex index)
{
   if (!deviceInternal || index == VE_INVALID_SAMPLER_INDEX)
   {
      return;
   }

   VESamplerInternal *sampler = deviceInternal->getSampler(index);

   if (!sampler || !sampler->isValid)
   {
      return;
   }

   if (sampler->sampler)
   {
      vkDestroySampler(deviceInternal->device, sampler->sampler, NULL);
   }

   deviceInternal->freeSamplerIndex(index);
   memset(sampler, 0, sizeof(VESamplerInternal));
}

void veDestroySampler(VEDevice *device, VESamplerIndex index)
{
   if (!device || index == VE_INVALID_SAMPLER_INDEX)
   {
      return;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   deviceInternal->deferredDeletionQueue->enqueueSampler(index);
}

// =============================================================================
// Convenience Sampler Creation Functions
// =============================================================================

VESamplerIndex veCreateLinearSampler(VEDevice *device)
{
   VESamplerDesc desc{};
   desc.minFilter = VK_FILTER_LINEAR;
   desc.magFilter = VK_FILTER_LINEAR;
   desc.mipmapFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
   desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.maxAnisotropy = 1.0f;
   desc.compareEnable = false;
   desc.compareOp = VK_COMPARE_OP_ALWAYS;
   desc.minLod = 0.0f;
   desc.maxLod = 1000.0f;
   desc.debugName = "LinearSampler";

   return veCreateSampler(device, &desc);
}

VESamplerIndex veCreateNearestSampler(VEDevice *device)
{
   VESamplerDesc desc{};
   desc.minFilter = VK_FILTER_NEAREST;
   desc.magFilter = VK_FILTER_NEAREST;
   desc.mipmapFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.maxAnisotropy = 1.0f;
   desc.compareEnable = false;
   desc.compareOp = VK_COMPARE_OP_ALWAYS;
   desc.minLod = 0.0f;
   desc.maxLod = 1000.0f;
   desc.debugName = "NearestSampler";

   return veCreateSampler(device, &desc);
}

VESamplerIndex veCreateAnisotropicSampler(VEDevice *device, float maxAnisotropy)
{
   VESamplerDesc desc{};
   desc.minFilter = VK_FILTER_LINEAR;
   desc.magFilter = VK_FILTER_LINEAR;
   desc.mipmapFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
   desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   desc.maxAnisotropy = maxAnisotropy;
   desc.compareEnable = false;
   desc.compareOp = VK_COMPARE_OP_ALWAYS;
   desc.minLod = 0.0f;
   desc.maxLod = 1000.0f;
   desc.debugName = "AnisotropicSampler";

   return veCreateSampler(device, &desc);
}

VESamplerIndex veCreateShadowSampler(VEDevice *device)
{
   VESamplerDesc desc{};
   desc.minFilter = VK_FILTER_LINEAR;
   desc.magFilter = VK_FILTER_LINEAR;
   desc.mipmapFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
   desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
   desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
   desc.maxAnisotropy = 1.0f;
   desc.compareEnable = true;
   desc.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
   desc.minLod = 0.0f;
   desc.maxLod = 1000.0f;
   desc.debugName = "ShadowSampler";

   return veCreateSampler(device, &desc);
}

// =============================================================================
// Descriptor Updates
// =============================================================================

VEResult VEDeviceInternal::updateSamplerDescriptor(VESamplerIndex index)
{
   VESamplerInternal *sampler = getSampler(index);
   if (!sampler)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkDescriptorImageInfo imageInfo{};
   imageInfo.sampler = sampler->sampler;

   VkWriteDescriptorSet descriptorWrite{};
   descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   descriptorWrite.dstSet = samplerDescriptorSet;
   descriptorWrite.dstBinding = 0;
   descriptorWrite.dstArrayElement = index;
   descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
   descriptorWrite.descriptorCount = 1;
   descriptorWrite.pImageInfo = &imageInfo;

   vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);

   return VE_SUCCESS;
}