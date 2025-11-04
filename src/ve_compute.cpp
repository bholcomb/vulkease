/**
 * @file ve_compute.c
 * @brief Compute Shaders Implementation
 */

#include "ve_internal.h"

// =============================================================================
// Compute Dispatch Functions
// =============================================================================

void veDispatch(VECommandBuffer *cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
   if (!cmd || groupCountX == 0 || groupCountY == 0 || groupCountZ == 0)
   {
      return;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (internal->device)
   {
      internal->device->frameStats.computeDispatches += 1;
   }
   vkCmdDispatch(internal->commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void veDispatchIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS)
   {
      return;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(indirectBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid indirect buffer address for compute dispatch");
      return;
   }

   vkCmdDispatchIndirect(internal->commandBuffer, buffer, offset);
   if (internal->device)
   {
      internal->device->frameStats.computeDispatches += 1;
   }
}

// =============================================================================
// Compute-Specific State Management
// =============================================================================

void veBindComputeShader(VECommandBuffer *cmd, VEShader *shader)
{
   if (!cmd || !shader)
   {
      return;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEShaderInternal *shaderInternal = (VEShaderInternal *)shader;

   // Verify this is a compute shader
   if (shaderInternal->stage != VK_SHADER_STAGE_COMPUTE_BIT)
   {
      veSetError("Shader is not a compute shader");
      return;
   }

   VkShaderStageFlagBits stageBit = VK_SHADER_STAGE_COMPUTE_BIT;

   if (internal->device)
   {
      internal->device->frameStats.pipelineBinds += 1;
   }
   veFuncs.vkCmdBindShadersEXT(internal->commandBuffer, 1, &stageBit, &shaderInternal->shaderObject);
}

// =============================================================================
// Compute Workgroup Size Helpers
// =============================================================================

VEResult veGetComputeWorkgroupSize(VEShader *shader, uint32_t *x, uint32_t *y, uint32_t *z)
{
   if (!shader || !x || !y || !z)
   {
      veSetError("Invalid parameters for compute workgroup size query");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEShaderInternal *shaderInternal = (VEShaderInternal *)shader;

   if (shaderInternal->stage != VK_SHADER_STAGE_COMPUTE_BIT)
   {
      veSetError("Shader is not a compute shader");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // In a full implementation, we would parse the SPIR-V to get workgroup size
   // For now, return default values
   *x = 64; // Common default workgroup size
   *y = 1;
   *z = 1;

   return VE_SUCCESS;
}

void veCalculateDispatchSize(uint32_t totalWork, uint32_t workgroupSize, uint32_t *numGroups)
{
   if (!numGroups || workgroupSize == 0)
   {
      return;
   }

   // Calculate number of workgroups needed
   *numGroups = (totalWork + workgroupSize - 1) / workgroupSize;
}

void veCalculateDispatchSize2D(uint32_t totalWorkX, uint32_t totalWorkY, uint32_t workgroupSizeX,
                               uint32_t workgroupSizeY, uint32_t *numGroupsX, uint32_t *numGroupsY)
{
   if (!numGroupsX || !numGroupsY || workgroupSizeX == 0 || workgroupSizeY == 0)
   {
      return;
   }

   *numGroupsX = (totalWorkX + workgroupSizeX - 1) / workgroupSizeX;
   *numGroupsY = (totalWorkY + workgroupSizeY - 1) / workgroupSizeY;
}

void veCalculateDispatchSize3D(uint32_t totalWorkX, uint32_t totalWorkY, uint32_t totalWorkZ, uint32_t workgroupSizeX,
                               uint32_t workgroupSizeY, uint32_t workgroupSizeZ, uint32_t *numGroupsX,
                               uint32_t *numGroupsY, uint32_t *numGroupsZ)
{
   if (!numGroupsX || !numGroupsY || !numGroupsZ || workgroupSizeX == 0 || workgroupSizeY == 0 || workgroupSizeZ == 0)
   {
      return;
   }

   *numGroupsX = (totalWorkX + workgroupSizeX - 1) / workgroupSizeX;
   *numGroupsY = (totalWorkY + workgroupSizeY - 1) / workgroupSizeY;
   *numGroupsZ = (totalWorkZ + workgroupSizeZ - 1) / workgroupSizeZ;
}

// =============================================================================
// Compute-Graphics Synchronization
// =============================================================================

void veBarrierComputeToGraphics(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierGraphicsToCompute(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierComputeToTransfer(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierTransferToCompute(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkMemoryBarrier2 memoryBarrier{};
   memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
   memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   memoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
   memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = 1;
   dependencyInfo.pMemoryBarriers = &memoryBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

// =============================================================================
// Buffer and Image Barriers for Compute
// =============================================================================

void veBarrierBuffer(VECommandBuffer *cmd, VEBufferAddress buffer, uint32_t srcStage, uint32_t dstStage,
                     uint32_t srcAccess, uint32_t dstAccess)
{
   if (!cmd || buffer == VE_INVALID_ADDRESS)
   {
      return;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return;
   }

   VEBufferInternal *bufferInternal = device->getBufferFromAddress(buffer);
   if (!bufferInternal || !bufferInternal->isValid)
   {
      veSetError("Invalid buffer address: 0x%llx", buffer);
      return;
   }

   VkBufferMemoryBarrier2 bufferBarrier{};
   bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
   bufferBarrier.srcStageMask = srcStage;
   bufferBarrier.srcAccessMask = srcAccess;
   bufferBarrier.dstStageMask = dstStage;
   bufferBarrier.dstAccessMask = dstAccess;
   bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   bufferBarrier.buffer = bufferInternal->buffer;
   bufferBarrier.offset = 0;
   bufferBarrier.size = VK_WHOLE_SIZE;

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.bufferMemoryBarrierCount = 1;
   dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierImage(VECommandBuffer *cmd, VETextureIndex texture, uint32_t oldLayout, uint32_t newLayout,
                    uint32_t srcStage, uint32_t dstStage, uint32_t srcAccess, uint32_t dstAccess)
{
   if (!cmd || texture == VE_INVALID_TEXTURE_INDEX)
   {
      return;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return;
   }

   VETextureInternal *textureInternal = device->getTexture(texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return;
   }

   VkImageMemoryBarrier2 imageBarrier{};
   imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   imageBarrier.srcStageMask = srcStage;
   imageBarrier.srcAccessMask = srcAccess;
   imageBarrier.dstStageMask = dstStage;
   imageBarrier.dstAccessMask = dstAccess;
   imageBarrier.oldLayout = static_cast<VkImageLayout>(oldLayout);
   imageBarrier.newLayout = static_cast<VkImageLayout>(newLayout);
   imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   imageBarrier.image = textureInternal->image;
   imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   imageBarrier.subresourceRange.baseMipLevel = 0;
   imageBarrier.subresourceRange.levelCount = textureInternal->mipLevels;
   imageBarrier.subresourceRange.baseArrayLayer = 0;
   imageBarrier.subresourceRange.layerCount = textureInternal->arrayLayers;

   // Handle depth/stencil formats
   if (textureInternal->format >= VK_FORMAT_D16_UNORM && textureInternal->format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
   {
      imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      if (textureInternal->format == VK_FORMAT_D16_UNORM_S8_UINT ||
          textureInternal->format == VK_FORMAT_D24_UNORM_S8_UINT ||
          textureInternal->format == VK_FORMAT_D32_SFLOAT_S8_UINT)
      {
         imageBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
   }

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.imageMemoryBarrierCount = 1;
   dependencyInfo.pImageMemoryBarriers = &imageBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

// =============================================================================
// Compute Performance Helpers
// =============================================================================

/*VEResult veGetComputeCapabilities(VEDevice* device, VEComputeCapabilities*
capabilities) { if (!device || !capabilities) { veSetError("Invalid parameters
for compute capabilities query"); return VE_ERROR_INVALID_PARAMETER;
    }

    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;

    memset(capabilities, 0, sizeof(VEComputeCapabilities));

    // Get compute properties from Vulkan
    VkPhysicalDeviceProperties props = deviceInternal->deviceProperties;

    capabilities->maxWorkgroupCount[0] =
props.limits.maxComputeWorkGroupCount[0]; capabilities->maxWorkgroupCount[1] =
props.limits.maxComputeWorkGroupCount[1]; capabilities->maxWorkgroupCount[2] =
props.limits.maxComputeWorkGroupCount[2];

    capabilities->maxWorkgroupSize[0] = props.limits.maxComputeWorkGroupSize[0];
    capabilities->maxWorkgroupSize[1] = props.limits.maxComputeWorkGroupSize[1];
    capabilities->maxWorkgroupSize[2] = props.limits.maxComputeWorkGroupSize[2];

    capabilities->maxWorkgroupInvocations =
props.limits.maxComputeWorkGroupInvocations; capabilities->maxSharedMemorySize =
props.limits.maxComputeSharedMemorySize;

    return VE_SUCCESS;
}*/

uint32_t veGetOptimalWorkgroupSize(VEDevice *device)
{
   if (!device)
      return 64;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Use subgroup size as a hint for optimal workgroup size
   // This is a heuristic - actual optimal size depends on the specific compute
   // shader
   VkPhysicalDeviceSubgroupProperties subgroupProps{};
   subgroupProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

   VkPhysicalDeviceProperties2 props2{};
   props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
   props2.pNext = &subgroupProps;

   vkGetPhysicalDeviceProperties2(deviceInternal->physicalDevice, &props2);

   // Return multiple of subgroup size, capped at reasonable maximum
   uint32_t optimalSize = subgroupProps.subgroupSize * 2; // 2x subgroup size is often good

   if (optimalSize == 0 || optimalSize > 256)
   {
      optimalSize = 64; // Fallback to common default
   }

   return optimalSize;
}