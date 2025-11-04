/**
 * @file ve_debug.c
 * @brief Debug and Profiling Implementation
 */

// For clock_gettime
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "ve_internal.h"
#include <time.h>

// =============================================================================
// Debug Utilities
// =============================================================================

void veSetDebugName(VEDevice *device, uint64_t objectHandle, VkObjectType objectType, const char *name)
{
   if (!device || !name)
      return;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   if (!deviceInternal->context->validationEnabled)
   {
      return;
   }

   veSetObjectDebugName(deviceInternal, objectHandle, objectType, name);
}

// =============================================================================
// Resource Debug Name Functions
// =============================================================================

VEResult veSetBufferDebugName(VEDevice *device, VEBufferAddress address, const char *name)
{
   if (!device || address == VE_INVALID_ADDRESS || !name)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Get the VkBuffer handle from the address
   VkBuffer buffer = deviceInternal->getVkBufferFromAddress(address);
   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid buffer address: 0x%llx", address);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Set the debug name on the Vulkan buffer object
   veSetObjectDebugName(deviceInternal, (uint64_t)buffer, VK_OBJECT_TYPE_BUFFER, name);

   return VE_SUCCESS;
}

VEResult veSetTextureDebugName(VEDevice *device, VETextureIndex texture, const char *name)
{
   if (!device || texture == VE_INVALID_TEXTURE_INDEX || !name)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Get the texture internal structure
   VETextureInternal *textureInternal = deviceInternal->getTexture(texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Set debug names on both the image and image view
   veSetObjectDebugName(deviceInternal, (uint64_t)textureInternal->image, VK_OBJECT_TYPE_IMAGE, name);
   veSetObjectDebugName(deviceInternal, (uint64_t)textureInternal->imageView, VK_OBJECT_TYPE_IMAGE_VIEW, name);

   return VE_SUCCESS;
}

VEResult veSetSamplerDebugName(VEDevice *device, VESamplerIndex sampler, const char *name)
{
   if (!device || sampler == VE_INVALID_SAMPLER_INDEX || !name)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Get the sampler internal structure
   VESamplerInternal *samplerInternal = deviceInternal->getSampler(sampler);
   if (!samplerInternal || !samplerInternal->isValid)
   {
      veSetError("Invalid sampler index: %u", sampler);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Set the debug name on the Vulkan sampler object
   veSetObjectDebugName(deviceInternal, (uint64_t)samplerInternal->sampler, VK_OBJECT_TYPE_SAMPLER, name);

   return VE_SUCCESS;
}

// Legacy debug label function (kept for compatibility)
void veInsertDebugLabelLegacy(VECommandBuffer *cmd, const char *labelName, float color[4])
{
   if (!cmd || !labelName)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkDebugUtilsLabelEXT labelInfo{};
   labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
   labelInfo.pLabelName = labelName;

   if (color)
   {
      labelInfo.color[0] = color[0];
      labelInfo.color[1] = color[1];
      labelInfo.color[2] = color[2];
      labelInfo.color[3] = color[3];
   }
   else
   {
      labelInfo.color[0] = 1.0f;
      labelInfo.color[1] = 1.0f;
      labelInfo.color[2] = 1.0f;
      labelInfo.color[3] = 1.0f;
   }

   veFuncs.vkCmdInsertDebugUtilsLabelEXT(internal->commandBuffer, &labelInfo);
}

// Modern debug label functions with VEColor support
void veBeginDebugLabel(VECommandBuffer *cmd, const char *label, VEColor color)
{
   if (!cmd || !label)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkDebugUtilsLabelEXT labelInfo{};
   labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
   labelInfo.pLabelName = label;
   labelInfo.color[0] = color.r;
   labelInfo.color[1] = color.g;
   labelInfo.color[2] = color.b;
   labelInfo.color[3] = color.a;

   veFuncs.vkCmdBeginDebugUtilsLabelEXT(internal->commandBuffer, &labelInfo);
}

void veEndDebugLabel(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   veFuncs.vkCmdEndDebugUtilsLabelEXT(internal->commandBuffer);
}

void veInsertDebugLabel(VECommandBuffer *cmd, const char *label, VEColor color)
{
   if (!cmd || !label)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkDebugUtilsLabelEXT labelInfo{};
   labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
   labelInfo.pLabelName = label;
   labelInfo.color[0] = color.r;
   labelInfo.color[1] = color.g;
   labelInfo.color[2] = color.b;
   labelInfo.color[3] = color.a;

   veFuncs.vkCmdInsertDebugUtilsLabelEXT(internal->commandBuffer, &labelInfo);
}

void veBeginDebugRegion(VECommandBuffer *cmd, const char *regionName, float color[4])
{
   if (!cmd || !regionName)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkDebugUtilsLabelEXT labelInfo{};
   labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
   labelInfo.pLabelName = regionName;

   if (color)
   {
      labelInfo.color[0] = color[0];
      labelInfo.color[1] = color[1];
      labelInfo.color[2] = color[2];
      labelInfo.color[3] = color[3];
   }
   else
   {
      labelInfo.color[0] = 0.0f;
      labelInfo.color[1] = 0.8f;
      labelInfo.color[2] = 1.0f;
      labelInfo.color[3] = 1.0f;
   }

   veFuncs.vkCmdBeginDebugUtilsLabelEXT(internal->commandBuffer, &labelInfo);
}

void veEndDebugRegion(VECommandBuffer *cmd)
{
   if (!cmd)
      return;

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   veFuncs.vkCmdEndDebugUtilsLabelEXT(internal->commandBuffer);
}

void veLogMessage(VEDevice *device, VEMessageSeverity severity, const char *message)
{
   if (!device || !message)
      return;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   if (!deviceInternal->context->validationEnabled)
   {
      // Still log to console even without validation layers
      const char *severityStr = "INFO";
      switch (severity)
      {
      case VE_MESSAGE_SEVERITY_VERBOSE:
         severityStr = "VERBOSE";
         break;
      case VE_MESSAGE_SEVERITY_INFO:
         severityStr = "INFO";
         break;
      case VE_MESSAGE_SEVERITY_WARNING:
         severityStr = "WARNING";
         break;
      case VE_MESSAGE_SEVERITY_ERROR:
         severityStr = "ERROR";
         break;
      }
      printf("[VulkEase %s] %s\\n", severityStr, message);
      return;
   }

   VkDebugUtilsMessageSeverityFlagBitsEXT vkSeverity;
   switch (severity)
   {
   case VE_MESSAGE_SEVERITY_VERBOSE:
      vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
      break;
   case VE_MESSAGE_SEVERITY_INFO:
      vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
      break;
   case VE_MESSAGE_SEVERITY_WARNING:
      vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
      break;
   case VE_MESSAGE_SEVERITY_ERROR:
      vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      break;
   default:
      vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
      break;
   }

   VkDebugUtilsMessengerCallbackDataEXT callbackData{};
   callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
   callbackData.pMessage = message;
   callbackData.messageIdNumber = 0;
   callbackData.pMessageIdName = "VulkEase User Message";

   veFuncs.vkSubmitDebugUtilsMessageEXT(deviceInternal->context->instance, vkSeverity,
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callbackData);
}

// =============================================================================
// Performance Statistics Implementation
// =============================================================================

static double getCurrentTimeSeconds(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec + (double)ts.tv_nsec / (double)1e9;
}

VEResult veGetPerformanceStats(VEDevice *device, VEPerformanceStats *stats)
{
   if (!device || !stats)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   *stats = deviceInternal->performanceStats;

   return VE_SUCCESS;
}

void veResetPerformanceStats(VEDevice *device)
{
   if (!device)
      return;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   deviceInternal->performanceStats = {};
   deviceInternal->frameStats = {};
   deviceInternal->lastFrameTimestampSeconds = 0.0;
}

VEResult veGetMemoryStats(VEDevice *device, VEMemoryStats *stats)
{
   if (!device || !stats)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   memset(stats, 0, sizeof(VEMemoryStats));
   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Get VMA statistics (simplified for now)
   VmaTotalStatistics vmaStats;
   vmaCalculateStatistics(deviceInternal->allocator, &vmaStats);

   stats->totalAllocated = vmaStats.total.statistics.allocationBytes;
   stats->totalUsed = vmaStats.total.statistics.allocationBytes;

   // Set resource counts (simplified - would need proper tracking)
   stats->bufferCount = 0; // Would need proper tracking in device
   stats->textureCount = deviceInternal->textureCount;
   stats->samplerCount = deviceInternal->samplerCount;

   // Get heap information
   VkPhysicalDeviceMemoryProperties memProps = deviceInternal->memoryProperties;
   for (uint32_t i = 0; i < memProps.memoryHeapCount && i < 16; i++)
   {
      stats->heaps[i].size = memProps.memoryHeaps[i].size;
      stats->heaps[i].used = 0;                              // Would need more detailed tracking
      stats->heaps[i].budget = memProps.memoryHeaps[i].size; // Simplified
   }

   return VE_SUCCESS;
}

VEResult veGetRenderConfigStats(VEDevice *device, VERenderConfigStats *stats)
{
   if (!device || !stats)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   memset(stats, 0, sizeof(VERenderConfigStats));
   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   stats->totalConfigs = deviceInternal->renderConfigCount;
   stats->activeConfigs = 0;

   // Count active configurations
   for (uint32_t i = 0; i < deviceInternal->maxRenderConfigs; i++)
   {
      if (deviceInternal->renderConfigs[i].isValid)
      {
         stats->activeConfigs++;
      }
   }

   // These would be tracked by the device if implemented
   stats->configSwitches = 0;
   stats->stateSwitches = 0;
   stats->overrides = 0;

   return VE_SUCCESS;
}

// =============================================================================
// GPU Timing Implementation
// =============================================================================

VEResult veBeginGPUTiming(VECommandBuffer *cmd, uint32_t queryIndex)
{
   (void)queryIndex;

   if (!cmd)
   {
      veSetError("Command buffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // In a full implementation, we would:
   // 1. Create timestamp query pool if not exists
   // 2. Reset query at queryIndex
   // 3. Write timestamp at pipeline stage

   // For now, record the CPU time as a fallback
   // double currentTime = getCurrentTimeSeconds();

   // Store timing data in command buffer (would need to extend internal
   // structure) internal->timingData[queryIndex].startTime = currentTime;

   return VE_SUCCESS;
}

VEResult veEndGPUTiming(VECommandBuffer *cmd, uint32_t queryIndex)
{
   (void)queryIndex;

   if (!cmd)
   {
      veSetError("Command buffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // In a full implementation, we would:
   // 1. Write end timestamp
   // 2. Calculate duration when results are available

   // double currentTime = getCurrentTimeSeconds();
   //  internal->timingData[queryIndex].endTime = currentTime;

   return VE_SUCCESS;
}

VEResult veGetGPUTimingResults(VEDevice *device, uint32_t queryIndex, double *timeInMilliseconds)
{
   (void)queryIndex;

   if (!device || !timeInMilliseconds)
   {
      veSetError("Invalid parameters for GPU timing results");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // In a full implementation, we would:
   // 1. Get query results from timestamp query pool
   // 2. Convert to milliseconds using timestamp period
   // 3. Return actual GPU timing

   // For now, return 0 as placeholder
   *timeInMilliseconds = 0.0;

   return VE_SUCCESS;
}

// =============================================================================
// Resource Usage Tracking
// =============================================================================

uint32_t veGetBufferCount(VEDevice *device)
{
   if (!device)
      return 0;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   return deviceInternal->graphicsCommandPool->commandBufferCount +
          deviceInternal->computeCommandPool->commandBufferCount +
          deviceInternal->transferCommandPool->commandBufferCount;
}

uint32_t veGetTextureCount(VEDevice *device)
{
   if (!device)
      return 0;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   return deviceInternal->textureCount;
}

uint32_t veGetSamplerCount(VEDevice *device)
{
   if (!device)
      return 0;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   return deviceInternal->samplerCount;
}

uint32_t veGetShaderCount(VEDevice *device)
{
   if (!device)
      return 0;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   return deviceInternal->shaderCount;
}

// =============================================================================
// Performance Profiling Helpers
// =============================================================================

static void updateFrameStats(VEDeviceInternal *device)
{
   static double lastFrameTime = 0.0;
   double currentTime = getCurrentTimeSeconds();

   if (lastFrameTime > 0.0)
   {
      double frameTime = currentTime - lastFrameTime;
      device->performanceStats.frameTime = (uint64_t)(frameTime * 1000000000.0); // Convert to nanoseconds

      // Note: The current VEPerformanceStats structure doesn't have fps,
      // averages, etc. These would need to be added to the structure if needed
   }

   lastFrameTime = currentTime;
   // Note: totalFrames not in current structure
}

// Call this at the end of each frame to update statistics
void veUpdateFrameStats(VEDevice *device)
{
   if (!device)
      return;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   updateFrameStats(deviceInternal);
}

// =============================================================================
// Debug Validation Helpers
// =============================================================================

bool veValidateDevice(VEDevice *device)
{
   if (!device)
   {
      veSetError("Device is NULL");
      return false;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Check if device is still valid
   if (!deviceInternal->device)
   {
      veSetError("Device handle is invalid");
      return false;
   }

   // Check if allocator is valid
   if (!deviceInternal->allocator)
   {
      veSetError("VMA allocator is invalid");
      return false;
   }

   // Validate resource counts (simplified - would need proper tracking)
   if (deviceInternal->textureCount > deviceInternal->maxTextures)
   {
      veSetError("Texture count exceeds maximum");
      return false;
   }

   if (deviceInternal->samplerCount > deviceInternal->maxSamplers)
   {
      veSetError("Sampler count exceeds maximum");
      return false;
   }

   return true;
}

bool veValidateBuffer(VEDevice *device, VEBufferAddress address)
{
   if (!device || address == VE_INVALID_ADDRESS)
   {
      return false;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   return deviceInternal->validateBufferAddress(address);
}

bool veValidateTexture(VEDevice *device, VETextureIndex index)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
   {
      return false;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *texture = deviceInternal->getTexture(index);

   return texture != NULL;
}

bool veValidateSampler(VEDevice *device, VESamplerIndex index)
{
   if (!device || index == VE_INVALID_SAMPLER_INDEX)
   {
      return false;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VESamplerInternal *sampler = deviceInternal->getSampler(index);

   return sampler != NULL;
}

// =============================================================================
// Debug Information Functions
// =============================================================================

void vePrintDebugInfo(VEDevice *device)
{
   if (!device)
   {
      printf("VulkEase Debug Info: Device is NULL\n");
      return;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   printf("=== VulkEase Debug Information ===\n");
   printf("Device Name: %s\n", veGetDeviceName(device));
   printf("Driver Version: %s\n", veGetDriverVersion(device));
   printf("Vulkan Version: %u\n", veGetVulkanVersion(device));

   // Resource counts
   printf("\nResource Usage:\n");
   printf("  Buffers: %u\n", veGetBufferCount(device));
   printf("  Textures: %u / %u\n", deviceInternal->textureCount, deviceInternal->maxTextures);
   printf("  Samplers: %u / %u\n", deviceInternal->samplerCount, deviceInternal->maxSamplers);
   printf("  Shaders: %u / %u\n", deviceInternal->shaderCount, deviceInternal->maxShaders);
   printf("  Render Configs: %u / %u\n", deviceInternal->renderConfigCount, deviceInternal->maxRenderConfigs);

   // Memory information
   VEMemoryStats memStats;
   if (veGetMemoryStats(device, &memStats) == VE_SUCCESS)
   {
      printf("\nMemory Usage:\n");
      printf("  Total Allocated: %llu bytes (%.2f MB)\n", (unsigned long long)memStats.totalAllocated,
             (double)memStats.totalAllocated / (1024.0 * 1024.0));
      printf("  Total Used: %llu bytes (%.2f MB)\n", (unsigned long long)memStats.totalUsed,
             (double)memStats.totalUsed / (1024.0 * 1024.0));
   }

   // Feature support
   printf("\nFeature Support:\n");
   printf("  Buffer Device Address: %s\n", deviceInternal->supportsBufferDeviceAddress() ? "Yes" : "No");
   printf("  Descriptor Indexing: %s\n", deviceInternal->supportsDescriptorIndexing() ? "Yes" : "No");
   printf("  Shader Objects: %s\n", deviceInternal->supportsShaderObjects() ? "Yes" : "No");
   printf("  Extended Dynamic State 3: %s\n", deviceInternal->supportsExtendedDynamicState3() ? "Yes" : "No");
   printf("  Vertex Input Dynamic State: %s\n", deviceInternal->supportsVertexInputDynamicState() ? "Yes" : "No");

   printf("================================\n");
}

void vePrintProfileInfo(VEDevice *device)
{
   if (!device)
   {
      printf("VulkEase Profile Info: Device is NULL\n");
      return;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Resource counts
   printf("\nResource Usage:\n");
   printf("  Buffers: %u\n", veGetBufferCount(device));
   printf("  Textures: %u / %u\n", deviceInternal->textureCount, deviceInternal->maxTextures);
   printf("  Samplers: %u / %u\n", deviceInternal->samplerCount, deviceInternal->maxSamplers);
   printf("  Shaders: %u / %u\n", deviceInternal->shaderCount, deviceInternal->maxShaders);
   printf("  Render Configs: %u / %u\n", deviceInternal->renderConfigCount, deviceInternal->maxRenderConfigs);

   // Memory information
   VEMemoryStats memStats;
   if (veGetMemoryStats(device, &memStats) == VE_SUCCESS)
   {
      printf("\nMemory Usage:\n");
      printf("  Total Allocated: %llu bytes (%.2f MB)\n", (unsigned long long)memStats.totalAllocated,
             (double)memStats.totalAllocated / (1024.0 * 1024.0));
      printf("  Total Used: %llu bytes (%.2f MB)\n", (unsigned long long)memStats.totalUsed,
             (double)memStats.totalUsed / (1024.0 * 1024.0));
   }

   // Performance stats
   VEPerformanceStats perfStats;
   if (veGetPerformanceStats(device, &perfStats) == VE_SUCCESS)
   {
      printf("\nPerformance Stats:\n");
      printf("  Frame Time: %llu ns (%.2f ms)\n", (unsigned long long)perfStats.frameTime,
             (double)perfStats.frameTime / 1000000.0);
      printf("  Draw Calls: %u\n", perfStats.drawCalls);
      printf("  Compute Dispatches: %u\n", perfStats.computeDispatches);
      printf("  Vertices Rendered: %llu\n", (unsigned long long)perfStats.verticesRendered);
      printf("  Triangles Rendered: %llu\n", (unsigned long long)perfStats.trianglesRendered);
   }

   printf("================================\n");
}

void vePrintRenderConfig(VERenderConfig *config)
{
   if (!config)
   {
      printf("VulkEase Render Config: Config is NULL\n");
      return;
   }

   VERenderConfigInternal *configInternal = (VERenderConfigInternal *)config;

   printf("=== VulkEase Render Configuration ===\n");
   printf("Debug Name: %s\n", configInternal->debugName);
   printf("Valid: %s\n", configInternal->isValid ? "Yes" : "No");

   if (configInternal->isValid)
   {
      printf("Configuration Types: 0x%08X\n", configInternal->configTypes);

      if (configInternal->configTypes & VE_CONFIG_TYPE_RASTERIZATION)
      {
         printf("  - Rasterization: Enabled\n");
      }
      if (configInternal->configTypes & VE_CONFIG_TYPE_DEPTH_STENCIL)
      {
         printf("  - Depth/Stencil: Enabled\n");
      }
      if (configInternal->configTypes & VE_CONFIG_TYPE_COLOR_BLEND)
      {
         printf("  - Color Blend: Enabled\n");
      }
      if (configInternal->configTypes & VE_CONFIG_TYPE_MULTISAMPLE)
      {
         printf("  - Multisample: Enabled\n");
      }
      if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
      {
         printf("  - Vertex Input: Enabled\n");
      }
      if (configInternal->configTypes & VE_CONFIG_TYPE_SHADERS)
      {
         printf("  - Shaders: Enabled\n");
      }
   }

   printf("====================================\n");
}

VEResult veValidateRenderConfig(VERenderConfig *config)
{
   if (!config)
   {
      veSetError("Render config is NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderConfigInternal *configInternal = (VERenderConfigInternal *)config;

   if (!configInternal->isValid)
   {
      veSetError("Render config is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Validate configuration types
   if (configInternal->configTypes == 0)
   {
      veSetError("Render config has no configuration types set");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Additional validation could be added here:
   // - Check if required configurations are present
   // - Validate configuration compatibility
   // - Check resource limits

   return VE_SUCCESS;
}

void veSetObjectDebugName(VEDeviceInternal *device, uint64_t objectHandle, VkObjectType objectType, const char *name)
{
   if (!device->context->validationEnabled || !name)
   {
      return;
   }

   VkDebugUtilsObjectNameInfoEXT nameInfo = {};
   nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
   nameInfo.objectType = objectType;
   nameInfo.objectHandle = objectHandle;
   nameInfo.pObjectName = name;

   veFuncs.vkSetDebugUtilsObjectNameEXT(device->device, &nameInfo);
}