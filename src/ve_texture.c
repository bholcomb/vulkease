/**
 * @file ve_texture.c
 * @brief Texture Management Implementation
 */

#include "ve_internal.h"
#include <math.h>

// =============================================================================
// Bindless Descriptor Management
// =============================================================================

VEResult veInitializeBindlessDescriptors(VEDeviceInternal* device) {
    // Create descriptor pool for bindless textures and samplers
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VE_MAX_TEXTURES},
        {VK_DESCRIPTOR_TYPE_SAMPLER, VE_MAX_SAMPLERS}
    };
    
    VkDescriptorPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    
    VkResult result = vkCreateDescriptorPool(device->device, &poolInfo, NULL, &device->descriptorPool);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create descriptor pool (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Create descriptor set layout for textures
    VkDescriptorBindingFlags textureBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                   VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                   VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    
    VkDescriptorSetLayoutBinding textureBinding = {0};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textureBinding.descriptorCount = VE_MAX_TEXTURES;
    textureBinding.stageFlags = VK_SHADER_STAGE_ALL;
    
    VkDescriptorSetLayoutBindingFlagsCreateInfo textureBindingFlags_info = {0};
    textureBindingFlags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    textureBindingFlags_info.bindingCount = 1;
    textureBindingFlags_info.pBindingFlags = &textureBindingFlags;
    
    VkDescriptorSetLayoutCreateInfo textureLayoutInfo = {0};
    textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutInfo.pNext = &textureBindingFlags_info;
    textureLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    textureLayoutInfo.bindingCount = 1;
    textureLayoutInfo.pBindings = &textureBinding;
    
    result = vkCreateDescriptorSetLayout(device->device, &textureLayoutInfo, NULL, &device->textureDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create texture descriptor set layout (VkResult: %d)", result);
        vkDestroyDescriptorPool(device->device, device->descriptorPool, NULL);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Create descriptor set layout for samplers  
    VkDescriptorBindingFlags samplerBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                   VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                   VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    
    VkDescriptorSetLayoutBinding samplerBinding = {0};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerBinding.descriptorCount = VE_MAX_SAMPLERS;
    samplerBinding.stageFlags = VK_SHADER_STAGE_ALL;
    
    VkDescriptorSetLayoutBindingFlagsCreateInfo samplerBindingFlags_info = {0};
    samplerBindingFlags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    samplerBindingFlags_info.bindingCount = 1;
    samplerBindingFlags_info.pBindingFlags = &samplerBindingFlags;
    
    VkDescriptorSetLayoutCreateInfo samplerLayoutInfo = {0};
    samplerLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    samplerLayoutInfo.pNext = &samplerBindingFlags_info;
    samplerLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    samplerLayoutInfo.bindingCount = 1;
    samplerLayoutInfo.pBindings = &samplerBinding;
    
    result = vkCreateDescriptorSetLayout(device->device, &samplerLayoutInfo, NULL, &device->samplerDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create sampler descriptor set layout (VkResult: %d)", result);
        vkDestroyDescriptorSetLayout(device->device, device->textureDescriptorSetLayout, NULL);
        vkDestroyDescriptorPool(device->device, device->descriptorPool, NULL);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Allocate descriptor sets
    uint32_t maxDescriptorCounts[] = {VE_MAX_TEXTURES, VE_MAX_SAMPLERS};
    VkDescriptorSetLayout layouts[] = {device->textureDescriptorSetLayout, device->samplerDescriptorSetLayout};
    
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo = {0};
    variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableCountInfo.descriptorSetCount = 2;
    variableCountInfo.pDescriptorCounts = maxDescriptorCounts;
    
    VkDescriptorSetAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = &variableCountInfo;
    allocInfo.descriptorPool = device->descriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts;
    
    VkDescriptorSet descriptorSets[2];
    result = vkAllocateDescriptorSets(device->device, &allocInfo, descriptorSets);
    if (result != VK_SUCCESS) {
        veSetError("Failed to allocate descriptor sets (VkResult: %d)", result);
        veCleanupBindlessDescriptors(device);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    device->textureDescriptorSet = descriptorSets[0];
    device->samplerDescriptorSet = descriptorSets[1];
    
    // Initialize texture management
    device->maxTextures = VE_MAX_TEXTURES;
    device->textures = calloc(device->maxTextures, sizeof(VETextureInternal));
    device->freeTextureIndices = calloc(device->maxTextures, sizeof(uint32_t));
    
    if (!device->textures || !device->freeTextureIndices) {
        veSetError("Failed to allocate texture management memory");
        veCleanupBindlessDescriptors(device);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Start from index 1 (0 is reserved for invalid)
    for (uint32_t i = 0; i < device->maxTextures - 1; i++) {
        device->freeTextureIndices[i] = i + 1;
    }
    device->freeTextureCount = device->maxTextures - 1;
    device->textureCount = 0;
    
    // Initialize sampler management
    device->maxSamplers = VE_MAX_SAMPLERS;
    device->samplers = calloc(device->maxSamplers, sizeof(VESamplerInternal));
    device->freeSamplerIndices = calloc(device->maxSamplers, sizeof(uint32_t));
    
    if (!device->samplers || !device->freeSamplerIndices) {
        veSetError("Failed to allocate sampler management memory");
        veCleanupBindlessDescriptors(device);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Start from index 1 (0 is reserved for invalid)
    for (uint32_t i = 0; i < device->maxSamplers - 1; i++) {
        device->freeSamplerIndices[i] = i + 1;
    }
    device->freeSamplerCount = device->maxSamplers - 1;
    device->samplerCount = 0;
    
    return VE_SUCCESS;
}

void veCleanupBindlessDescriptors(VEDeviceInternal* device) {
    if (!device || !device->device) return;
    
    if (device->textures) {
        for (uint32_t i = 0; i < device->maxTextures; i++) {
            if (device->textures[i].isValid) {
                if (device->textures[i].imageView) {
                    vkDestroyImageView(device->device, device->textures[i].imageView, NULL);
                }
                if (device->textures[i].image) {
                    vmaDestroyImage(device->allocator, device->textures[i].image, device->textures[i].allocation);
                }
            }
        }
        free(device->textures);
        device->textures = NULL;
    }
    
    if (device->freeTextureIndices) {
        free(device->freeTextureIndices);
        device->freeTextureIndices = NULL;
    }
    
    if (device->samplers) {
        for (uint32_t i = 0; i < device->maxSamplers; i++) {
            if (device->samplers[i].isValid) {
                vkDestroySampler(device->device, device->samplers[i].sampler, NULL);
            }
        }
        free(device->samplers);
        device->samplers = NULL;
    }
    
    if (device->freeSamplerIndices) {
        free(device->freeSamplerIndices);
        device->freeSamplerIndices = NULL;
    }
    
    if (device->samplerDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(device->device, device->samplerDescriptorSetLayout, NULL);
        device->samplerDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (device->textureDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(device->device, device->textureDescriptorSetLayout, NULL);
        device->textureDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    if (device->descriptorPool) {
        vkDestroyDescriptorPool(device->device, device->descriptorPool, NULL);
        device->descriptorPool = VK_NULL_HANDLE;
    }
}

// =============================================================================
// Texture Index Management
// =============================================================================

uint32_t veAllocateTextureIndex(VEDeviceInternal* device) {
    if (device->freeTextureCount == 0) {
        veSetError("No free texture indices available (max: %u)", device->maxTextures);
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    device->freeTextureCount--;
    uint32_t index = device->freeTextureIndices[device->freeTextureCount];
    device->textureCount++;
    
    return index;
}

void veFreeTextureIndex(VEDeviceInternal* device, uint32_t index) {
    if (index == 0 || index >= device->maxTextures) {
        return;
    }
    
    device->freeTextureIndices[device->freeTextureCount] = index;
    device->freeTextureCount++;
    device->textureCount--;
}

VETextureInternal* veGetTexture(VEDeviceInternal* device, VETextureIndex index) {
    if (index == VE_INVALID_TEXTURE_INDEX || index >= device->maxTextures) {
        return NULL;
    }
    
    VETextureInternal* texture = &device->textures[index];
    return texture->isValid ? texture : NULL;
}

VkImageView veGetImageViewFromTexture(VEDeviceInternal* device, VETextureIndex index) {
    VETextureInternal* texture = veGetTexture(device, index);
    return (texture && texture->isValid) ? texture->imageView : VK_NULL_HANDLE;
}

// =============================================================================
// Format Conversion Utilities
// =============================================================================

VkFormat veFormatToVk(VEFormat format) {
    return (VkFormat)format;
}

VEFormat veFormatFromVk(VkFormat format) {
    return (VEFormat)format;
}

VkImageUsageFlags veTextureUsageToVk(VETextureUsage usage) {
    VkImageUsageFlags vkUsage = 0;
    
    if (usage & VE_TEXTURE_USAGE_SAMPLED) vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage & VE_TEXTURE_USAGE_STORAGE) vkUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (usage & VE_TEXTURE_USAGE_COLOR_ATTACHMENT) vkUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage & VE_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT) vkUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage & VE_TEXTURE_USAGE_TRANSFER_SRC) vkUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage & VE_TEXTURE_USAGE_TRANSFER_DST) vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usage & VE_TEXTURE_USAGE_INPUT_ATTACHMENT) vkUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    
    return vkUsage;
}

VkSampleCountFlagBits veSampleCountToVk(VESampleCount sampleCount) {
    return (VkSampleCountFlagBits)sampleCount;
}

// =============================================================================
// Texture Data Upload
// =============================================================================

static VEResult veUploadTextureData(VEDeviceInternal* device, VETextureInternal* texture, 
                                   const void* data, size_t dataSize) {
    if (!device || !texture || !data || dataSize == 0) {
        veSetError("Invalid parameters for texture upload");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Create staging buffer for upload
    VkBufferCreateInfo stagingBufferInfo = {0};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = dataSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo stagingAllocInfo = {0};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VmaAllocationInfo stagingAllocInfo_result;
    
    VkResult result = vmaCreateBuffer(device->allocator, &stagingBufferInfo, &stagingAllocInfo,
                                     &stagingBuffer, &stagingAllocation, &stagingAllocInfo_result);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create staging buffer for texture upload (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy data to staging buffer
    memcpy(stagingAllocInfo_result.pMappedData, data, dataSize);
    vmaFlushAllocation(device->allocator, stagingAllocation, 0, dataSize);
    
    // Use the public API to get a command buffer
    VECommandBuffer* cmdPublic = veBeginCommandBuffer((VEDevice*)device);
    if (!cmdPublic) {
        vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    VECommandBufferInternal* cmd = (VECommandBufferInternal*)cmdPublic;
    
    // Transition image to transfer destination layout
    VkImageMemoryBarrier barrier = {0};
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
    
    vkCmdPipelineBarrier(cmd->commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, NULL, 0, NULL, 1, &barrier);
    
    // Copy buffer to image
    VkBufferImageCopy region = {0};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = texture->arrayLayers;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){texture->width, texture->height, texture->depth};
    
    vkCmdCopyBufferToImage(cmd->commandBuffer, stagingBuffer, texture->image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition image to shader read layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(cmd->commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, NULL, 0, NULL, 1, &barrier);
    
    // Submit command buffer and wait for completion
    VEResult submitResult = veSubmitCommandBuffer(cmdPublic, true);
    
    // Update tracked layout if command submission succeeded
    if (submitResult == VE_SUCCESS) {
        texture->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    
    // Cleanup staging buffer
    vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);
    
    if (submitResult != VE_SUCCESS) {
        return submitResult;
    }
    
    return VE_SUCCESS;
}

// =============================================================================
// Texture Creation
// =============================================================================

VETextureIndex veCreateTexture(VEDevice* device, const VETextureDesc* desc) {
    if (!device || !desc) {
        veSetError("Invalid parameters for texture creation");
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    uint32_t index = veAllocateTextureIndex(deviceInternal);
    if (index == VE_INVALID_TEXTURE_INDEX) {
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    VETextureInternal* texture = &deviceInternal->textures[index];
    memset(texture, 0, sizeof(VETextureInternal));
    
    texture->width = desc->width;
    texture->height = desc->height;
    texture->depth = desc->depth;
    texture->mipLevels = desc->mipLevels == 0 ? 1 : desc->mipLevels;
    texture->arrayLayers = desc->arrayLayers;
    texture->format = desc->format;
    texture->usage = desc->usage;
    texture->sampleCount = desc->sampleCount;
    texture->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // All textures start undefined
    texture->index = index;
    
    if (desc->debugName) {
        strncpy(texture->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
    } else {
        snprintf(texture->debugName, VE_MAX_DEBUG_NAME_LENGTH, "Texture_%u", index);
    }
    
    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = desc->depth > 1 ? VK_IMAGE_TYPE_3D : (desc->height > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
    imageInfo.extent.width = desc->width;
    imageInfo.extent.height = desc->height > 0 ? desc->height : 1;
    imageInfo.extent.depth = desc->depth > 0 ? desc->depth : 1;
    imageInfo.mipLevels = texture->mipLevels;
    imageInfo.arrayLayers = desc->arrayLayers;
    imageInfo.format = veFormatToVk(desc->format);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = veTextureUsageToVk(desc->usage);
    imageInfo.samples = veSampleCountToVk(desc->sampleCount);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    // Always add transfer dst for potential data uploads
    imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    // Auto-generate mipmaps if requested
    if (desc->mipLevels == 0 && (desc->usage & VE_TEXTURE_USAGE_SAMPLED)) {
        texture->mipLevels = (uint32_t)floor(log2(fmax(desc->width, desc->height))) + 1;
        imageInfo.mipLevels = texture->mipLevels;
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    
    VmaAllocationCreateInfo allocInfo = {0};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    
    VkResult result = vmaCreateImage(deviceInternal->allocator, &imageInfo, &allocInfo,
                                   &texture->image, &texture->allocation, &texture->allocationInfo);
    
    if (result != VK_SUCCESS) {
        veSetError("Failed to create texture (VkResult: %d)", result);
        veFreeTextureIndex(deviceInternal, index);
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    // Create image view
    VkImageViewCreateInfo viewInfo = {0};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture->image;
    viewInfo.viewType = imageInfo.imageType == VK_IMAGE_TYPE_3D ? VK_IMAGE_VIEW_TYPE_3D :
                       (imageInfo.imageType == VK_IMAGE_TYPE_2D ? 
                        (desc->arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D) :
                        VK_IMAGE_VIEW_TYPE_1D);
    viewInfo.format = imageInfo.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = texture->mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = desc->arrayLayers;
    
    // Handle depth/stencil formats
    if (desc->format >= VE_FORMAT_D16_UNORM && desc->format <= VE_FORMAT_D32_SFLOAT_S8_UINT) {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (desc->format == VE_FORMAT_D16_UNORM_S8_UINT || 
            desc->format == VE_FORMAT_D24_UNORM_S8_UINT ||
            desc->format == VE_FORMAT_D32_SFLOAT_S8_UINT) {
            viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    
    result = vkCreateImageView(deviceInternal->device, &viewInfo, NULL, &texture->imageView);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create image view (VkResult: %d)", result);
        vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
        veFreeTextureIndex(deviceInternal, index);
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    // Upload initial data if provided
    if (desc->initialData && desc->initialDataSize > 0) {
        VEResult uploadResult = veUploadTextureData(deviceInternal, texture, desc->initialData, desc->initialDataSize);
        if (uploadResult != VE_SUCCESS) {
            vkDestroyImageView(deviceInternal->device, texture->imageView, NULL);
            vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
            veFreeTextureIndex(deviceInternal, index);
            return VE_INVALID_TEXTURE_INDEX;
        }
    }
    
    if (deviceInternal->context->validationEnabled) {
        veSetObjectDebugName(deviceInternal, (uint64_t)texture->image, VK_OBJECT_TYPE_IMAGE, texture->debugName);
        char viewName[VE_MAX_DEBUG_NAME_LENGTH + 16];
        snprintf(viewName, sizeof(viewName), "%s_View", texture->debugName);
        veSetObjectDebugName(deviceInternal, (uint64_t)texture->imageView, VK_OBJECT_TYPE_IMAGE_VIEW, viewName);
    }
    
    texture->isValid = true;
    
    // Update bindless descriptor
    veUpdateTextureDescriptor(deviceInternal, index);
    
    return index;
}

void veDestroyTexture(VEDevice* device, VETextureIndex index) {
    if (!device || index == VE_INVALID_TEXTURE_INDEX) {
        return;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VETextureInternal* texture = veGetTexture(deviceInternal, index);
    
    if (!texture || !texture->isValid) {
        return;
    }
    
    if (texture->imageView) {
        vkDestroyImageView(deviceInternal->device, texture->imageView, NULL);
    }
    
    if (texture->image) {
        vmaDestroyImage(deviceInternal->allocator, texture->image, texture->allocation);
    }
    
    veFreeTextureIndex(deviceInternal, index);
    memset(texture, 0, sizeof(VETextureInternal));
}

// =============================================================================
// Texture Property Queries
// =============================================================================

VEResult veGetTextureSize(VEDevice* device, VETextureIndex index,
                         uint32_t* width, uint32_t* height, uint32_t* depth) {
    if (!device || index == VE_INVALID_TEXTURE_INDEX) {
        veSetError("Invalid parameters for texture size query");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VETextureInternal* texture = veGetTexture(deviceInternal, index);
    
    if (!texture) {
        veSetError("Invalid texture index");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    if (width) *width = texture->width;
    if (height) *height = texture->height;
    if (depth) *depth = texture->depth;
    
    return VE_SUCCESS;
}

VEFormat veGetTextureFormat(VEDevice* device, VETextureIndex index) {
    if (!device || index == VE_INVALID_TEXTURE_INDEX) {
        return VE_FORMAT_RGBA8_UNORM;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VETextureInternal* texture = veGetTexture(deviceInternal, index);
    
    return texture ? texture->format : VE_FORMAT_RGBA8_UNORM;
}

// =============================================================================
// Convenience Texture Creation Functions
// =============================================================================

VETextureIndex veCreateTexture1D(VEDevice* device, uint32_t width, VEFormat format,
                                VETextureUsage usage, const char* debugName) {
    VETextureDesc desc = {
        .width = width,
        .height = 1,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .usage = usage,
        .sampleCount = VE_SAMPLE_COUNT_1,
        .initialData = NULL,
        .initialDataSize = 0,
        .debugName = debugName
    };
    
    return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture2D(VEDevice* device, uint32_t width, uint32_t height,
                                VEFormat format, VETextureUsage usage, const char* debugName) {
    VETextureDesc desc = {
        .width = width,
        .height = height,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .usage = usage,
        .sampleCount = VE_SAMPLE_COUNT_1,
        .initialData = NULL,
        .initialDataSize = 0,
        .debugName = debugName
    };
    
    return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture3D(VEDevice* device, uint32_t width, uint32_t height, uint32_t depth,
                                VEFormat format, VETextureUsage usage, const char* debugName) {
    VETextureDesc desc = {
        .width = width,
        .height = height,
        .depth = depth,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .usage = usage,
        .sampleCount = VE_SAMPLE_COUNT_1,
        .initialData = NULL,
        .initialDataSize = 0,
        .debugName = debugName
    };
    
    return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture2DArray(VEDevice* device, uint32_t width, uint32_t height,
                                     uint32_t layers, VEFormat format, VETextureUsage usage,
                                     const char* debugName) {
    VETextureDesc desc = {
        .width = width,
        .height = height,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = layers,
        .format = format,
        .usage = usage,
        .sampleCount = VE_SAMPLE_COUNT_1,
        .initialData = NULL,
        .initialDataSize = 0,
        .debugName = debugName
    };
    
    return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTextureCube(VEDevice* device, uint32_t size, VEFormat format,
                                  VETextureUsage usage, const char* debugName) {
    VETextureDesc desc = {
        .width = size,
        .height = size,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 6,
        .format = format,
        .usage = usage,
        .sampleCount = VE_SAMPLE_COUNT_1,
        .initialData = NULL,
        .initialDataSize = 0,
        .debugName = debugName
    };
    
    return veCreateTexture(device, &desc);
}

VETextureIndex veCreateTexture2DMultisample(VEDevice* device, uint32_t width, uint32_t height,
                                           VEFormat format, VESampleCount sampleCount,
                                           VETextureUsage usage, const char* debugName) {
    VETextureDesc desc = {
        .width = width,
        .height = height,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .usage = usage,
        .sampleCount = sampleCount,
        .initialData = NULL,
        .initialDataSize = 0,
        .debugName = debugName
    };
    
    return veCreateTexture(device, &desc);
}

// =============================================================================
// STB Image Integration
// =============================================================================

VETextureIndex veLoadTexture(VEDevice* device, const char* filename,
                            VETextureUsage usage, bool generateMips) {
    if (!device || !filename) {
        veSetError("Invalid parameters for texture loading");
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
    
    if (!pixels) {
        veSetError("Failed to load image: %s", stbi_failure_reason());
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    VETextureDesc desc = {
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .depth = 1,
        .mipLevels = generateMips ? 0 : 1,
        .arrayLayers = 1,
        .format = VE_FORMAT_RGBA8_SRGB,
        .usage = usage,
        .sampleCount = VE_SAMPLE_COUNT_1,
        .initialData = pixels,
        .initialDataSize = width * height * 4,
        .debugName = filename
    };
    
    VETextureIndex result = veCreateTexture(device, &desc);
    
    // Generate mipmaps if requested and texture creation succeeded
    if (result != VE_INVALID_TEXTURE_INDEX && generateMips && desc.mipLevels > 1) {
        VEResult mipmapResult = veGenerateMipmapsImmediate(device, result);
        if (mipmapResult != VE_SUCCESS) {
            veSetError("Failed to generate mipmaps for loaded texture: %s", filename);
            veDestroyTexture(device, result);
            stbi_image_free(pixels);
            return VE_INVALID_TEXTURE_INDEX;
        }
    }
    
    stbi_image_free(pixels);
    return result;
}

VETextureIndex veLoadHDRTexture(VEDevice* device, const char* filename,
                               VETextureUsage usage, bool generateMips) {
    if (!device || !filename) {
        veSetError("Invalid parameters for HDR texture loading");
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    int width, height, channels;
    float* pixels = stbi_loadf(filename, &width, &height, &channels, STBI_rgb_alpha);
    
    if (!pixels) {
        veSetError("Failed to load HDR image: %s", stbi_failure_reason());
        return VE_INVALID_TEXTURE_INDEX;
    }
    
    VETextureDesc desc = {
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .depth = 1,
        .mipLevels = generateMips ? 0 : 1,
        .arrayLayers = 1,
        .format = VE_FORMAT_RGBA32_SFLOAT,
        .usage = usage,
        .sampleCount = VE_SAMPLE_COUNT_1,
        .initialData = pixels,
        .initialDataSize = width * height * 4 * sizeof(float),
        .debugName = filename
    };
    
    VETextureIndex result = veCreateTexture(device, &desc);
    
    // Generate mipmaps if requested and texture creation succeeded
    if (result != VE_INVALID_TEXTURE_INDEX && generateMips && desc.mipLevels > 1) {
        VEResult mipmapResult = veGenerateMipmapsImmediate(device, result);
        if (mipmapResult != VE_SUCCESS) {
            veSetError("Failed to generate mipmaps for loaded HDR texture: %s", filename);
            veDestroyTexture(device, result);
            stbi_image_free(pixels);
            return VE_INVALID_TEXTURE_INDEX;
        }
    }
    
    stbi_image_free(pixels);
    return result;
}

VETextureIndex veLoadCubeTexture(VEDevice* device, const char* filenames[6],
                               VETextureUsage usage, bool generateMips) {
    veSetError("Cube texture loading not implemented");
    return VE_INVALID_TEXTURE_INDEX;
}

VEResult veGenerateMipmaps(VEDevice* device, VECommandBuffer* cmd, VETextureIndex texture) {
    if (!device || !cmd || texture == VE_INVALID_TEXTURE_INDEX) {
        veSetError("Invalid parameters for mipmap generation");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VECommandBufferInternal* cmdInternal = (VECommandBufferInternal*)cmd;
    
    // Get texture and validate
    VETextureInternal* textureInternal = veGetTexture(deviceInternal, texture);
    if (!textureInternal || !textureInternal->isValid) {
        veSetError("Invalid texture index: %u", texture);
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Validate mipmap generation requirements
    if (textureInternal->mipLevels <= 1) {
        veSetError("Texture has only 1 mip level - no mipmaps to generate");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    if (textureInternal->sampleCount != VE_SAMPLE_COUNT_1) {
        veSetError("Cannot generate mipmaps for multisampled textures");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Check if format supports linear filtering (required for mipmap generation)
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(deviceInternal->physicalDevice, 
                                       veFormatToVk(textureInternal->format), &formatProps);
    
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        veSetError("Format does not support linear filtering required for mipmap generation");
        return VE_ERROR_FEATURE_NOT_SUPPORTED;
    }
    
    // Ensure texture has transfer source usage for blitting
    VkImageUsageFlags usage = veTextureUsageToVk(textureInternal->usage);
    if (!(usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
        veSetError("Texture must have transfer source usage for mipmap generation");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VkImage image = textureInternal->image;
    uint32_t mipWidth = textureInternal->width;
    uint32_t mipHeight = textureInternal->height;
    
    // Generate mipmaps by blitting from level i to level i+1
    for (uint32_t i = 1; i < textureInternal->mipLevels; i++) {
        // Transition previous mip level to transfer source layout
        VkImageMemoryBarrier2 barrier = {0};
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
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = textureInternal->arrayLayers;
        
        VkDependencyInfo dependencyInfo = {0};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        
        vkCmdPipelineBarrier2(cmdInternal->commandBuffer, &dependencyInfo);
        
        // Calculate dimensions for current mip level
        uint32_t nextMipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        uint32_t nextMipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
        
        // Blit from previous level to current level
        VkImageBlit blit = {0};
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
        
        vkCmdBlitImage(cmdInternal->commandBuffer,
                      image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      1, &blit, VK_FILTER_LINEAR);
        
        // Update dimensions for next iteration
        mipWidth = nextMipWidth;
        mipHeight = nextMipHeight;
    }
    
    // Transition all mip levels to shader read optimal layout
    VkImageMemoryBarrier2 finalBarrier = {0};
    finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    finalBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    finalBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
    finalBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.image = image;
    finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    finalBarrier.subresourceRange.baseMipLevel = 0;
    finalBarrier.subresourceRange.levelCount = textureInternal->mipLevels;
    finalBarrier.subresourceRange.baseArrayLayer = 0;
    finalBarrier.subresourceRange.layerCount = textureInternal->arrayLayers;
    
    VkDependencyInfo finalDependencyInfo = {0};
    finalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    finalDependencyInfo.imageMemoryBarrierCount = 1;
    finalDependencyInfo.pImageMemoryBarriers = &finalBarrier;
    
    vkCmdPipelineBarrier2(cmdInternal->commandBuffer, &finalDependencyInfo);
    
    // Update tracked layout
    textureInternal->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    return VE_SUCCESS;
}

VEResult veGenerateMipmapsImmediate(VEDevice* device, VETextureIndex texture) {
    if (!device || texture == VE_INVALID_TEXTURE_INDEX) {
        veSetError("Invalid parameters for immediate mipmap generation");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Create a temporary command buffer for mipmap generation
    VECommandBuffer* cmd = veBeginCommandBuffer(device);
    if (!cmd) {
        veSetError("Failed to create command buffer for mipmap generation");
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Generate mipmaps
    VEResult result = veGenerateMipmaps(device, cmd, texture);
    if (result != VE_SUCCESS) {
        // Note: command buffer will be cleaned up automatically on submission failure
        return result;
    }
    
    // Submit and wait for completion
    result = veSubmitCommandBuffer(cmd, true);
    if (result != VE_SUCCESS) {
        veSetError("Failed to submit mipmap generation command buffer");
        return result;
    }
    
    return VE_SUCCESS;
}

VEResult veSaveTexture(VEDevice* device, VETextureIndex texture, const char* filename) {
    veSetError("Texture saving not implemented");
    return VE_ERROR_FEATURE_NOT_SUPPORTED;
}

// =============================================================================
// Descriptor Updates
// =============================================================================

VEResult veUpdateTextureDescriptor(VEDeviceInternal* device, VETextureIndex index) {
    VETextureInternal* texture = veGetTexture(device, index);
    if (!texture) {
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VkDescriptorImageInfo imageInfo = {0};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture->imageView;
    
    VkWriteDescriptorSet descriptorWrite = {0};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = device->textureDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = index;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(device->device, 1, &descriptorWrite, 0, NULL);
    
    return VE_SUCCESS;
}