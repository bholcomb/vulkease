/**
 * @file ve_sampler.c
 * @brief Sampler Management Implementation
 */

#include "ve_internal.h"

#include <cstring>

// =============================================================================
// Sampler Index Management
// =============================================================================

uint32_t veAllocateSamplerIndex(VEDeviceInternal *device)
{
   if (device->freeSamplerCount == 0)
   {
      veSetError("No free sampler indices available (max: %u)", device->maxSamplers);
      return VE_INVALID_SAMPLER_INDEX;
   }

   device->freeSamplerCount--;
   uint32_t index = device->freeSamplerIndices[device->freeSamplerCount];
   device->samplerCount++;

   return index;
}

void veFreeSamplerIndex(VEDeviceInternal *device, uint32_t index)
{
   if (index == 0 || index >= device->maxSamplers)
   {
      return;
   }

   device->freeSamplerIndices[device->freeSamplerCount] = index;
   device->freeSamplerCount++;
   device->samplerCount--;
}

VESamplerInternal *veGetSampler(VEDeviceInternal *device, VESamplerIndex index)
{
   if (index == VE_INVALID_SAMPLER_INDEX || index >= device->maxSamplers)
   {
      return NULL;
   }

   VESamplerInternal *sampler = &device->samplers[index];
   return sampler->isValid ? sampler : NULL;
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

   uint32_t index = veAllocateSamplerIndex(deviceInternal);
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
      veFreeSamplerIndex(deviceInternal, index);
      return VE_INVALID_SAMPLER_INDEX;
   }

   if (deviceInternal->context->validationEnabled)
   {
      veSetObjectDebugName(deviceInternal, (uint64_t)sampler->sampler, VK_OBJECT_TYPE_SAMPLER, sampler->debugName);
   }

   sampler->isValid = true;

   // Update bindless descriptor
   veUpdateSamplerDescriptor(deviceInternal, index);

   return index;
}

void veDestroySampler(VEDevice *device, VESamplerIndex index)
{
   if (!device || index == VE_INVALID_SAMPLER_INDEX)
   {
      return;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VESamplerInternal *sampler = veGetSampler(deviceInternal, index);

   if (!sampler || !sampler->isValid)
   {
      return;
   }

   if (sampler->sampler)
   {
      vkDestroySampler(deviceInternal->device, sampler->sampler, NULL);
   }

   veFreeSamplerIndex(deviceInternal, index);
   memset(sampler, 0, sizeof(VESamplerInternal));
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

VEResult veUpdateSamplerDescriptor(VEDeviceInternal *device, VESamplerIndex index)
{
   VESamplerInternal *sampler = veGetSampler(device, index);
   if (!sampler)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkDescriptorImageInfo imageInfo{};
   imageInfo.sampler = sampler->sampler;

   VkWriteDescriptorSet descriptorWrite{};
   descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   descriptorWrite.dstSet = device->samplerDescriptorSet;
   descriptorWrite.dstBinding = 0;
   descriptorWrite.dstArrayElement = index;
   descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
   descriptorWrite.descriptorCount = 1;
   descriptorWrite.pImageInfo = &imageInfo;

   vkUpdateDescriptorSets(device->device, 1, &descriptorWrite, 0, NULL);

   return VE_SUCCESS;
}