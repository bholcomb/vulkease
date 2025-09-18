/**
 * @file ve_debug.c
 * @brief Debug and Profiling Implementation
 */

#include "ve_internal.h"
#include <time.h>

// =============================================================================
// Debug Utilities
// =============================================================================

void veSetDebugName(VEDevice* device, uint64_t objectHandle, VEObjectType objectType, const char* name) {
    if (!device || !name) return;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    if (!deviceInternal->context->validationEnabled) {
        return;
    }
    
    VkObjectType vkObjectType;
    switch (objectType) {
        case VE_OBJECT_TYPE_BUFFER: vkObjectType = VK_OBJECT_TYPE_BUFFER; break;
        case VE_OBJECT_TYPE_IMAGE: vkObjectType = VK_OBJECT_TYPE_IMAGE; break;
        case VE_OBJECT_TYPE_IMAGE_VIEW: vkObjectType = VK_OBJECT_TYPE_IMAGE_VIEW; break;
        case VE_OBJECT_TYPE_SAMPLER: vkObjectType = VK_OBJECT_TYPE_SAMPLER; break;
        case VE_OBJECT_TYPE_SHADER: vkObjectType = VK_OBJECT_TYPE_SHADER_EXT; break;
        case VE_OBJECT_TYPE_COMMAND_BUFFER: vkObjectType = VK_OBJECT_TYPE_COMMAND_BUFFER; break;
        case VE_OBJECT_TYPE_QUEUE: vkObjectType = VK_OBJECT_TYPE_QUEUE; break;
        case VE_OBJECT_TYPE_DEVICE: vkObjectType = VK_OBJECT_TYPE_DEVICE; break;
        case VE_OBJECT_TYPE_INSTANCE: vkObjectType = VK_OBJECT_TYPE_INSTANCE; break;
        case VE_OBJECT_TYPE_PHYSICAL_DEVICE: vkObjectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE; break;
        default: return;
    }
    
    veSetObjectDebugName(deviceInternal, objectHandle, vkObjectType, name);
}

void veInsertDebugLabel(VECommandBuffer* cmd, const char* labelName, float color[4]) {
    if (!cmd || !labelName) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT = 
        (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdInsertDebugUtilsLabelEXT");
    
    if (!vkCmdInsertDebugUtilsLabelEXT) return;
    
    VkDebugUtilsLabelEXT labelInfo = {0};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = labelName;
    
    if (color) {
        labelInfo.color[0] = color[0];
        labelInfo.color[1] = color[1];
        labelInfo.color[2] = color[2];
        labelInfo.color[3] = color[3];
    } else {
        labelInfo.color[0] = 1.0f;
        labelInfo.color[1] = 1.0f;
        labelInfo.color[2] = 1.0f;
        labelInfo.color[3] = 1.0f;
    }
    
    vkCmdInsertDebugUtilsLabelEXT(internal->commandBuffer, &labelInfo);
}

void veBeginDebugRegion(VECommandBuffer* cmd, const char* regionName, float color[4]) {
    if (!cmd || !regionName) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = 
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdBeginDebugUtilsLabelEXT");
    
    if (!vkCmdBeginDebugUtilsLabelEXT) return;
    
    VkDebugUtilsLabelEXT labelInfo = {0};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = regionName;
    
    if (color) {
        labelInfo.color[0] = color[0];
        labelInfo.color[1] = color[1];
        labelInfo.color[2] = color[2];
        labelInfo.color[3] = color[3];
    } else {
        labelInfo.color[0] = 0.0f;
        labelInfo.color[1] = 0.8f;
        labelInfo.color[2] = 1.0f;
        labelInfo.color[3] = 1.0f;
    }
    
    vkCmdBeginDebugUtilsLabelEXT(internal->commandBuffer, &labelInfo);
}

void veEndDebugRegion(VECommandBuffer* cmd) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = 
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdEndDebugUtilsLabelEXT");
    
    if (!vkCmdEndDebugUtilsLabelEXT) return;
    
    vkCmdEndDebugUtilsLabelEXT(internal->commandBuffer);
}

void veLogMessage(VEDevice* device, VEMessageSeverity severity, const char* message) {
    if (!device || !message) return;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    if (!deviceInternal->context->validationEnabled) {
        // Still log to console even without validation layers
        const char* severityStr = "INFO";
        switch (severity) {
            case VE_MESSAGE_SEVERITY_VERBOSE: severityStr = "VERBOSE"; break;
            case VE_MESSAGE_SEVERITY_INFO: severityStr = "INFO"; break;
            case VE_MESSAGE_SEVERITY_WARNING: severityStr = "WARNING"; break;
            case VE_MESSAGE_SEVERITY_ERROR: severityStr = "ERROR"; break;
        }
        printf("[VulkEase %s] %s\\n", severityStr, message);
        return;
    }
    
    PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT = 
        (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(deviceInternal->context->instance, "vkSubmitDebugUtilsMessageEXT");
    
    if (!vkSubmitDebugUtilsMessageEXT) {
        printf("[VulkEase] %s\\n", message);
        return;
    }
    
    VkDebugUtilsMessageSeverityFlagBitsEXT vkSeverity;
    switch (severity) {
        case VE_MESSAGE_SEVERITY_VERBOSE: vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT; break;
        case VE_MESSAGE_SEVERITY_INFO: vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT; break;
        case VE_MESSAGE_SEVERITY_WARNING: vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT; break;
        case VE_MESSAGE_SEVERITY_ERROR: vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; break;
        default: vkSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT; break;
    }
    
    VkDebugUtilsMessengerCallbackDataEXT callbackData = {0};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pMessage = message;
    callbackData.messageIdNumber = 0;
    callbackData.pMessageIdName = "VulkEase User Message";
    
    vkSubmitDebugUtilsMessageEXT(deviceInternal->context->instance, vkSeverity, 
                                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callbackData);
}

// =============================================================================
// Performance Statistics Implementation
// =============================================================================

static double getCurrentTimeSeconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

VEPerformanceStats veGetPerformanceStats(VEDevice* device) {
    VEPerformanceStats stats = {0};
    
    if (!device) {
        return stats;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    stats = deviceInternal->performanceStats;
    
    return stats;
}

void veResetPerformanceStats(VEDevice* device) {
    if (!device) return;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    memset(&deviceInternal->performanceStats, 0, sizeof(VEPerformanceStats));
}

VEMemoryStats veGetMemoryStats(VEDevice* device) {
    VEMemoryStats stats = {0};
    
    if (!device) {
        return stats;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    // Get VMA statistics
    VmaStats vmaStats;
    vmaCalculateStats(deviceInternal->allocator, &vmaStats);
    
    stats.totalAllocatedBytes = vmaStats.total.usedBytes;
    stats.totalUsedBytes = vmaStats.total.usedBytes;
    stats.totalFreeBytes = vmaStats.total.unusedBytes;
    stats.allocationCount = vmaStats.total.allocationCount;
    stats.unusedRangeCount = vmaStats.total.unusedRangeCount;
    
    // Calculate buffer and texture memory usage
    for (uint32_t i = 0; i < deviceInternal->maxBuffers; i++) {
        if (deviceInternal->buffers[i].isValid) {
            stats.bufferMemoryUsage += deviceInternal->buffers[i].allocationInfo.size;
        }
    }
    
    for (uint32_t i = 0; i < deviceInternal->maxTextures; i++) {
        if (deviceInternal->textures[i].isValid) {
            stats.textureMemoryUsage += deviceInternal->textures[i].allocationInfo.size;
        }
    }
    
    // Get heap information
    VkPhysicalDeviceMemoryProperties memProps = deviceInternal->memoryProperties;
    for (uint32_t i = 0; i < memProps.memoryHeapCount && i < 16; i++) {
        stats.heapSizes[i] = memProps.memoryHeaps[i].size;
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            stats.deviceLocalHeapSize += memProps.memoryHeaps[i].size;
        } else {
            stats.hostVisibleHeapSize += memProps.memoryHeaps[i].size;
        }
    }
    stats.heapCount = memProps.memoryHeapCount;
    
    return stats;
}

VERenderConfigStats veGetRenderConfigStats(VEDevice* device) {
    VERenderConfigStats stats = {0};
    
    if (!device) {
        return stats;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    stats.totalConfigs = deviceInternal->renderConfigCount;
    stats.activeConfigs = 0;
    
    // Count active configurations
    for (uint32_t i = 0; i < deviceInternal->maxRenderConfigs; i++) {
        if (deviceInternal->renderConfigs[i].isValid) {
            stats.activeConfigs++;
        }
    }
    
    stats.vertexConfigs = deviceInternal->vertexConfigCount;
    stats.shaderConfigs = deviceInternal->shaderConfigCount;
    stats.maxConfigs = deviceInternal->maxRenderConfigs;
    
    return stats;
}

// =============================================================================
// GPU Timing Implementation  
// =============================================================================

VEResult veBeginGPUTiming(VECommandBuffer* cmd, uint32_t queryIndex) {
    if (!cmd) {
        veSetError("Command buffer cannot be NULL");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    // In a full implementation, we would:
    // 1. Create timestamp query pool if not exists
    // 2. Reset query at queryIndex
    // 3. Write timestamp at pipeline stage
    
    // For now, record the CPU time as a fallback
    double currentTime = getCurrentTimeSeconds();
    
    // Store timing data in command buffer (would need to extend internal structure)
    // internal->timingData[queryIndex].startTime = currentTime;
    
    return VE_SUCCESS;
}

VEResult veEndGPUTiming(VECommandBuffer* cmd, uint32_t queryIndex) {
    if (!cmd) {
        veSetError("Command buffer cannot be NULL");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    // In a full implementation, we would:
    // 1. Write end timestamp
    // 2. Calculate duration when results are available
    
    double currentTime = getCurrentTimeSeconds();
    // internal->timingData[queryIndex].endTime = currentTime;
    
    return VE_SUCCESS;
}

VEResult veGetGPUTimingResults(VEDevice* device, uint32_t queryIndex, double* timeInMilliseconds) {
    if (!device || !timeInMilliseconds) {
        veSetError("Invalid parameters for GPU timing results");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
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

uint32_t veGetBufferCount(VEDevice* device) {
    if (!device) return 0;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    return deviceInternal->bufferCount;
}

uint32_t veGetTextureCount(VEDevice* device) {
    if (!device) return 0;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    return deviceInternal->textureCount;
}

uint32_t veGetSamplerCount(VEDevice* device) {
    if (!device) return 0;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    return deviceInternal->samplerCount;
}

uint32_t veGetShaderCount(VEDevice* device) {
    if (!device) return 0;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    return deviceInternal->shaderCount;
}

// =============================================================================
// Performance Profiling Helpers
// =============================================================================

static void updateFrameStats(VEDeviceInternal* device) {
    static double lastFrameTime = 0.0;
    double currentTime = getCurrentTimeSeconds();
    
    if (lastFrameTime > 0.0) {
        double frameTime = currentTime - lastFrameTime;
        device->performanceStats.frameTime = (float)(frameTime * 1000.0); // Convert to ms
        device->performanceStats.fps = frameTime > 0.0 ? (float)(1.0 / frameTime) : 0.0f;
        
        // Update averages (simple moving average over 60 frames)
        const float alpha = 1.0f / 60.0f;
        device->performanceStats.averageFrameTime = device->performanceStats.averageFrameTime * (1.0f - alpha) + 
                                                   device->performanceStats.frameTime * alpha;
        device->performanceStats.averageFPS = device->performanceStats.averageFPS * (1.0f - alpha) + 
                                            device->performanceStats.fps * alpha;
        
        // Track min/max
        if (device->performanceStats.frameTime < device->performanceStats.minFrameTime || 
            device->performanceStats.minFrameTime == 0.0f) {
            device->performanceStats.minFrameTime = device->performanceStats.frameTime;
        }
        
        if (device->performanceStats.frameTime > device->performanceStats.maxFrameTime) {
            device->performanceStats.maxFrameTime = device->performanceStats.frameTime;
        }
    }
    
    lastFrameTime = currentTime;
    device->performanceStats.totalFrames++;
}

// Call this at the end of each frame to update statistics
void veUpdateFrameStats(VEDevice* device) {
    if (!device) return;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    updateFrameStats(deviceInternal);
}

// =============================================================================
// Debug Validation Helpers
// =============================================================================

bool veValidateDevice(VEDevice* device) {
    if (!device) {
        veSetError("Device is NULL");
        return false;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    // Check if device is still valid
    if (!deviceInternal->device) {
        veSetError("Device handle is invalid");
        return false;
    }
    
    // Check if allocator is valid
    if (!deviceInternal->allocator) {
        veSetError("VMA allocator is invalid");
        return false;
    }
    
    // Validate resource counts
    if (deviceInternal->bufferCount > deviceInternal->maxBuffers) {
        veSetError("Buffer count exceeds maximum");
        return false;
    }
    
    if (deviceInternal->textureCount > deviceInternal->maxTextures) {
        veSetError("Texture count exceeds maximum");
        return false;
    }
    
    if (deviceInternal->samplerCount > deviceInternal->maxSamplers) {
        veSetError("Sampler count exceeds maximum");
        return false;
    }
    
    return true;
}

bool veValidateBuffer(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) {
        return false;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    return veValidateBufferAddress(deviceInternal, address);
}

bool veValidateTexture(VEDevice* device, VETextureIndex index) {
    if (!device || index == VE_INVALID_TEXTURE_INDEX) {
        return false;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VETextureInternal* texture = veGetTexture(deviceInternal, index);
    
    return texture != NULL;
}

bool veValidateSampler(VEDevice* device, VESamplerIndex index) {
    if (!device || index == VE_INVALID_SAMPLER_INDEX) {
        return false;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VESamplerInternal* sampler = veGetSampler(deviceInternal, index);
    
    return sampler != NULL;
}