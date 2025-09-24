/**
 * @file ve_buffer.cpp  
 * @brief Buffer Management Implementation (C++ with STL for simplicity)
 */

#include "ve_internal.h"
#include <unordered_map>
#include <memory>
#include <vector>

// Type aliases for cleaner code
using BufferMap = std::unordered_map<VEBufferAddress, std::unique_ptr<VEBufferInternal>>;

// Helper to get the buffer map from the opaque pointer
static BufferMap* getBufferMap(VEDeviceInternal* device) {
    return static_cast<BufferMap*>(device->bufferMap);
}

// =============================================================================
// VMA Integration
// =============================================================================

extern "C" VEResult veInitializeVMA(VEDeviceInternal* device) {
    // Require buffer device address support - no fallback
    if (!device->features.bufferDeviceAddress) {
        veSetError("Buffer device address support is required - no fallback strategy");
        return VE_ERROR_UNSUPPORTED;
    }

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = device->physicalDevice;
    allocatorInfo.device = device->device;
    allocatorInfo.instance = device->context->instance;
    allocatorInfo.vulkanApiVersion = device->deviceProperties.apiVersion;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    
    VkResult result = vmaCreateAllocator(&allocatorInfo, &device->allocator);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create VMA allocator (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Create the buffer map using STL
    device->bufferMap = new(std::nothrow) BufferMap();
    if (!device->bufferMap) {
        veSetError("Failed to allocate buffer map");
        vmaDestroyAllocator(device->allocator);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    return VE_SUCCESS;
}

extern "C" void veCleanupVMA(VEDeviceInternal* device) {
    if (!device) return;
    
    if (device->device) {
        vkDeviceWaitIdle(device->device);
    }
    
    // Clean up all buffers using STL map
    if (device->bufferMap) {
        BufferMap* bufferMap = getBufferMap(device);
        for (auto& pair : *bufferMap) {
            auto& buffer = pair.second;
            if (buffer && buffer->isValid) {
                vmaDestroyBuffer(device->allocator, buffer->buffer, buffer->allocation);
            }
        }
        delete bufferMap;
        device->bufferMap = nullptr;
    }
    
    if (device->allocator) {
        vmaDestroyAllocator(device->allocator);
        device->allocator = VK_NULL_HANDLE;
    }
}

// =============================================================================
// Buffer Address Management (Simplified with STL)
// =============================================================================

// Get buffer from address using O(1) hash map lookup
VEBufferInternal* veGetBufferFromAddress(VEDeviceInternal* device, VEBufferAddress address) {
    if (address == VE_INVALID_ADDRESS || !device->bufferMap) {
        return nullptr;
    }
    
    BufferMap* bufferMap = getBufferMap(device);
    auto it = bufferMap->find(address);
    return (it != bufferMap->end()) ? it->second.get() : nullptr;
}

extern "C" bool veValidateBufferAddress(VEDeviceInternal* device, VEBufferAddress address) {
    return veGetBufferFromAddress(device, address) != nullptr;
}

// Get VkBuffer handle from buffer address for command buffer operations
extern "C" VkBuffer veGetVkBufferFromAddress(VEDeviceInternal* device, VEBufferAddress address) {
    VEBufferInternal* buffer = veGetBufferFromAddress(device, address);
    return (buffer && buffer->isValid) ? buffer->buffer : VK_NULL_HANDLE;
}

// =============================================================================
// Buffer Creation
// =============================================================================

extern "C" VEBufferAddress veCreateBuffer(VEDevice* device, const VEBufferDesc* desc) {
    if (!device || !desc || desc->size == 0) {
        veSetError("Invalid parameters for buffer creation");
        return VE_INVALID_ADDRESS;
    }
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    
    // Buffer device address is required - no fallback
    if (!deviceInternal->features.bufferDeviceAddress) {
        veSetError("Buffer device address support is required");
        return VE_INVALID_ADDRESS;
    }
    
    BufferMap* bufferMap = getBufferMap(deviceInternal);
    if (!bufferMap) {
        veSetError("Buffer map not initialized");
        return VE_INVALID_ADDRESS;
    }
    
    // Create buffer object using smart pointer for automatic cleanup
    auto buffer = std::make_unique<VEBufferInternal>();
    memset(buffer.get(), 0, sizeof(VEBufferInternal));
    
    buffer->size = desc->size;
    buffer->usage = desc->usage;
    buffer->persistentlyMapped = desc->persistentlyMapped;
    
    if (desc->debugName) {
        strncpy(buffer->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
    } else {
        snprintf(buffer->debugName, VE_MAX_DEBUG_NAME_LENGTH, "Buffer_%p", buffer.get());
    }
    
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc->size;
    bufferInfo.usage = desc->usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
    if (desc->persistentlyMapped) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    else
    {
        // Prefer GPU memory for better performance, but allow fallback
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }
    
    VkResult result = vmaCreateBuffer(deviceInternal->allocator, &bufferInfo, &allocInfo,
                                     &buffer->buffer, &buffer->allocation, &buffer->allocationInfo);
    
    if (result != VK_SUCCESS) {
        veSetError("Failed to create buffer (VkResult: %d)", result);
        return VE_INVALID_ADDRESS;
    }
    
    // Get the device address (guaranteed to work since we require buffer device address)
    VkBufferDeviceAddressInfo addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer->buffer;
    
    VEBufferAddress deviceAddress = vkGetBufferDeviceAddress(deviceInternal->device, &addressInfo);
    buffer->deviceAddress = deviceAddress;

    if (deviceInternal->context->validationEnabled) {
        veSetObjectDebugName(deviceInternal, (uint64_t)buffer->buffer, VK_OBJECT_TYPE_BUFFER, buffer->debugName);
    }
    
    buffer->isValid = true;
    
    //hold on to the raw pointer for initializing
    VEBufferInternal* bufferPtr = buffer.get();

    // Store in map and transfer ownership.  Buffer is empty after this, hence the bufferPtr
    (*bufferMap)[deviceAddress] = std::move(buffer);

    
    if (desc->persistentlyMapped && bufferPtr->allocationInfo.pMappedData) {
        bufferPtr->mappedData = bufferPtr->allocationInfo.pMappedData;
    }
    
    if (desc->initialData && desc->initialDataSize > 0) {
        VEResult uploadResult = veUpdateBuffer(device, deviceAddress, desc->initialData, 
                                              desc->initialDataSize, 0);
        if (uploadResult != VE_SUCCESS) {
            vmaDestroyBuffer(deviceInternal->allocator, bufferPtr->buffer, bufferPtr->allocation);
            return VE_INVALID_ADDRESS;
        }
    }
    
    return deviceAddress;
}

extern "C" void veDestroyBuffer(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) {
        return;
    }
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    BufferMap* bufferMap = getBufferMap(deviceInternal);
    
    if (!bufferMap) {
        return;
    }
    
    auto it = bufferMap->find(address);
    if (it == bufferMap->end()) {
        return; // Buffer not found
    }
    
    VEBufferInternal* buffer = it->second.get();
    if (!buffer || !buffer->isValid) {
        return;
    }
    
    if (buffer->mappedData && !buffer->persistentlyMapped) {
        vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
    }
    
    vmaDestroyBuffer(deviceInternal->allocator, buffer->buffer, buffer->allocation);
    
    // Remove from map (automatically deletes the buffer via unique_ptr)
    bufferMap->erase(it);
}

// =============================================================================
// Transfer Commands
// =============================================================================

static VECommandBuffer* veBeginTransferCommandBuffer(VEDeviceInternal* device) {
    if (!device) {
        veSetError("Device cannot be NULL");
        return NULL;
    }
    
    VECommandBufferInternal* cmd;
    VEResult result = veAllocateCommandBuffer(device, device->transferCommandPool, &cmd);
    if (result != VE_SUCCESS) {
        return NULL;
    }
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VkResult vkResult = vkBeginCommandBuffer(cmd->commandBuffer, &beginInfo);
    if (vkResult != VK_SUCCESS) {
        veSetError("Failed to begin transfer command buffer (VkResult: %d)", vkResult);
        veFreeCommandBuffer(cmd);
        return NULL;
    }
    
    cmd->isRecording = true;
    cmd->isOneTime = true;
    
    return (VECommandBuffer*)cmd;
}

static VEResult veSubmitTransferCommandBuffer(VECommandBuffer* cmd, bool waitForCompletion) {
    if (!cmd) {
        veSetError("Command buffer cannot be NULL");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VECommandBufferInternal* internalCmd = (VECommandBufferInternal*)cmd;
    
    if (!internalCmd->isRecording) {
        veSetError("Command buffer is not recording");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VkResult result = vkEndCommandBuffer(internalCmd->commandBuffer);
    if (result != VK_SUCCESS) {
        veSetError("Failed to end command buffer (VkResult: %d)", result);
        return VE_ERROR_UNKNOWN;
    }
    
    internalCmd->isRecording = false;
    
    // Submit command buffer using stored device reference
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &internalCmd->commandBuffer;
    
    VkFence fence = VK_NULL_HANDLE;
    if (waitForCompletion) {
        // Create fence for synchronization
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        
        result = vkCreateFence(internalCmd->device->device, &fenceInfo, NULL, &fence);
        if (result != VK_SUCCESS) {
            veSetError("Failed to create fence for transfer command buffer submission (VkResult: %d)", result);
            return VE_ERROR_OUT_OF_MEMORY;
        }
    }
    
    result = vkQueueSubmit(internalCmd->device->transferQueue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(internalCmd->device->device, fence, NULL);
        }
        veSetError("Failed to submit command buffer (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    if (waitForCompletion) {
        // Wait for completion and cleanup fence
        result = vkWaitForFences(internalCmd->device->device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(internalCmd->device->device, fence, NULL);
        
        if (result != VK_SUCCESS) {
            veSetError("Failed to wait for transfer command buffer completion (VkResult: %d)", result);
            return VE_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // Free the command buffer for reuse
    veFreeCommandBuffer(internalCmd);
    
    return VE_SUCCESS;
}

// =============================================================================
// Buffer Memory Operations
// =============================================================================

extern "C" VEResult veMapBuffer(VEDevice* device, VEBufferAddress address, void** mappedData) {
    if (!device || address == VE_INVALID_ADDRESS || !mappedData) {
        veSetError("Invalid parameters for buffer mapping");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    if (!buffer || !buffer->isValid) {
        veSetError("Invalid buffer address");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    if (buffer->persistentlyMapped && buffer->mappedData) {
        *mappedData = buffer->mappedData;
        return VE_SUCCESS;
    }
    
    VkResult result = vmaMapMemory(deviceInternal->allocator, buffer->allocation, mappedData);
    if (result != VK_SUCCESS) {
        veSetError("Failed to map buffer (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    buffer->mappedData = *mappedData;
    return VE_SUCCESS;
}

extern "C" void veUnmapBuffer(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) {
        return;
    }
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    if (!buffer || !buffer->isValid || buffer->persistentlyMapped) {
        return;
    }
    
    if (buffer->mappedData) {
        vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
        buffer->mappedData = nullptr;
    }
}

static VEResult veUpdateBufferWithStaging(VEDeviceInternal* device, VEBufferInternal* dstBuffer,
                                         const void* data, size_t size, size_t offset) {
    // Create staging buffer
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = size;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo stagingAllocInfo = {};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // Keep mapped
    
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VmaAllocationInfo stagingAllocationInfo;
    
    VkResult result = vmaCreateBuffer(device->allocator, &stagingBufferInfo, &stagingAllocInfo,
                                     &stagingBuffer, &stagingAllocation, &stagingAllocationInfo);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create staging buffer (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy data to staging buffer (it's already mapped)
    memcpy(stagingAllocationInfo.pMappedData, data, size);
    vmaFlushAllocation(device->allocator, stagingAllocation, 0, size);
    
    // Get command buffer for transfer
    VECommandBuffer* transferCmd = veBeginTransferCommandBuffer(device);
    VECommandBufferInternal* transferCmdIternal = (VECommandBufferInternal*)transferCmd;

    if (transferCmdIternal->commandBuffer == VK_NULL_HANDLE) {
        vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);
        veSetError("Failed to get transfer command buffer");
        return VE_ERROR_TRANSFER_FAILED;
    }
    
    // Record copy command
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = size;
    
    vkCmdCopyBuffer(transferCmdIternal->commandBuffer, stagingBuffer, dstBuffer->buffer, 1, &copyRegion);
    
    // Add memory barrier if destination buffer will be used in different stage
    if (dstBuffer->usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) 
    {
        VkMemoryBarrier2 memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        
        // Determine destination stage based on buffer usage
        memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
        
        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &memoryBarrier;
        
        vkCmdPipelineBarrier2(transferCmdIternal->commandBuffer, &dependencyInfo);
    }
    
    // Submit transfer command and wait for completion
    VEResult submitResult = veSubmitTransferCommandBuffer(transferCmd, true);

    vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);
    
    return submitResult;
}

extern "C" VEResult veUpdateBuffer(VEDevice* device, VEBufferAddress address, 
                                   const void* data, size_t size, size_t offset) {
    if (!device || address == VE_INVALID_ADDRESS || !data || size == 0) {
        veSetError("Invalid parameters for buffer update");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    if (!buffer || !buffer->isValid) {
        veSetError("Invalid buffer address");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    if (offset + size > buffer->size) {
        veSetError("Update size exceeds buffer bounds");
        return VE_ERROR_INVALID_PARAMETER;
    }

    // Check if buffer is persistently mapped or can be mapped
    if (buffer->mappedData) {
        // Buffer is persistently mapped - direct update
        memcpy(static_cast<char*>(buffer->mappedData) + offset, data, size);
        vmaFlushAllocation(deviceInternal->allocator, buffer->allocation, offset, size);
        return VE_SUCCESS;
    }

    if( buffer->persistentlyMapped)
    {
        // Try to map the buffer directly (for host-visible memory)
        void* mappedData = nullptr;
        VkResult result = vmaMapMemory(deviceInternal->allocator, buffer->allocation, &mappedData);
        if (result == VK_SUCCESS) {
            // Buffer can be mapped directly
            memcpy(static_cast<char*>(mappedData) + offset, data, size);
            vmaFlushAllocation(deviceInternal->allocator, buffer->allocation, offset, size);
            vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
            return VE_SUCCESS;
        }
        else
        {
            return VE_ERROR_TRANSFER_FAILED;
        }
    }
    
    // Buffer cannot be mapped (GPU-only memory) - use staging buffer approach
    return veUpdateBufferWithStaging(deviceInternal, buffer, data, size, offset);
}

// =============================================================================
// Buffer Property Queries
// =============================================================================

extern "C" size_t veGetBufferSize(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) return 0;
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    return (buffer && buffer->isValid) ? buffer->size : 0;
}

extern "C" VkBufferUsageFlags veGetBufferUsage(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) return static_cast<VkBufferUsageFlags>(0);
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    return (buffer && buffer->isValid) ? buffer->usage : static_cast<VkBufferUsageFlags>(0);
}

// =============================================================================
// Convenience Buffer Creation Functions
// =============================================================================

extern "C" VEBufferAddress veCreateVertexBuffer(VEDevice* device, const void* vertices, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .initialData = vertices,
        .initialDataSize = size,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "VertexBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

extern "C" VEBufferAddress veCreateIndexBuffer(VEDevice* device, const void* indices, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .initialData = indices,
        .initialDataSize = size,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "IndexBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

extern "C" VEBufferAddress veCreateUniformBuffer(VEDevice* device, size_t size, bool persistentlyMapped, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .initialData = nullptr,
        .initialDataSize = 0,
        .persistentlyMapped = persistentlyMapped,
        .debugName = debugName ? debugName : "UniformBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

extern "C" VEBufferAddress veCreateStorageBuffer(VEDevice* device, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .initialData = nullptr,
        .initialDataSize = 0,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "StorageBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

extern "C" VEBufferAddress veCreateIndirectBuffer(VEDevice* device, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .initialData = nullptr,
        .initialDataSize = 0,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "IndirectBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}
