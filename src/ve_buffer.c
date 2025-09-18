/**
 * @file ve_buffer.c  
 * @brief Buffer Management Implementation
 */

#include "ve_internal.h"

// =============================================================================
// VMA Integration
// =============================================================================

VEResult veInitializeVMA(VEDeviceInternal* device) {
    VmaAllocatorCreateInfo allocatorInfo = {0};
    allocatorInfo.physicalDevice = device->physicalDevice;
    allocatorInfo.device = device->device;
    allocatorInfo.instance = device->context->instance;
    allocatorInfo.vulkanApiVersion = device->deviceProperties.apiVersion;
    
    if (device->features.bufferDeviceAddress) {
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }
    
    VkResult result = vmaCreateAllocator(&allocatorInfo, &device->allocator);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create VMA allocator (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    device->maxBuffers = VE_MAX_BUFFERS;
    device->buffers = calloc(device->maxBuffers, sizeof(VEBufferInternal));
    device->freeBufferIndices = calloc(device->maxBuffers, sizeof(uint32_t));
    
    if (!device->buffers || !device->freeBufferIndices) {
        veSetError("Failed to allocate buffer management memory");
        vmaDestroyAllocator(device->allocator);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    for (uint32_t i = 0; i < device->maxBuffers - 1; i++) {
        device->freeBufferIndices[i] = i + 1;
    }
    device->freeBufferCount = device->maxBuffers - 1;
    device->bufferCount = 0;
    
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = device->queueFamilies.graphicsFamily;
    
    result = vkCreateCommandPool(device->device, &poolInfo, NULL, &device->commandPool);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create command pool (VkResult: %d)", result);
        veCleanupVMA(device);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    device->commandBuffers = calloc(VE_MAX_COMMAND_BUFFERS, sizeof(VECommandBufferInternal));
    device->commandBufferInUse = calloc(VE_MAX_COMMAND_BUFFERS, sizeof(bool));
    device->commandBufferCount = 0;
    
    if (!device->commandBuffers || !device->commandBufferInUse) {
        veCleanupVMA(device);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    return VE_SUCCESS;
}

void veCleanupVMA(VEDeviceInternal* device) {
    if (!device) return;
    
    if (device->device) {
        vkDeviceWaitIdle(device->device);
    }
    
    if (device->buffers) {
        for (uint32_t i = 0; i < device->maxBuffers; i++) {
            if (device->buffers[i].isValid) {
                vmaDestroyBuffer(device->allocator, device->buffers[i].buffer, device->buffers[i].allocation);
            }
        }
        free(device->buffers);
        device->buffers = NULL;
    }
    
    if (device->freeBufferIndices) {
        free(device->freeBufferIndices);
        device->freeBufferIndices = NULL;
    }
    
    if (device->commandBuffers) {
        free(device->commandBuffers);
        device->commandBuffers = NULL;
    }
    
    if (device->commandBufferInUse) {
        free(device->commandBufferInUse);
        device->commandBufferInUse = NULL;
    }
    
    if (device->commandPool) {
        vkDestroyCommandPool(device->device, device->commandPool, NULL);
        device->commandPool = VK_NULL_HANDLE;
    }
    
    if (device->allocator) {
        vmaDestroyAllocator(device->allocator);
        device->allocator = VK_NULL_HANDLE;
    }
}

// =============================================================================
// Buffer Index Management
// =============================================================================

uint32_t veAllocateBufferIndex(VEDeviceInternal* device) {
    if (device->freeBufferCount == 0) {
        veSetError("No free buffer indices available (max: %u)", device->maxBuffers);
        return 0;
    }
    
    device->freeBufferCount--;
    uint32_t index = device->freeBufferIndices[device->freeBufferCount];
    device->bufferCount++;
    
    return index;
}

void veFreeBufferIndex(VEDeviceInternal* device, uint32_t index) {
    if (index == 0 || index >= device->maxBuffers) {
        return;
    }
    
    device->freeBufferIndices[device->freeBufferCount] = index;
    device->freeBufferCount++;
    device->bufferCount--;
}

VEBufferInternal* veGetBufferFromAddress(VEDeviceInternal* device, VEBufferAddress address) {
    if (address == VE_INVALID_ADDRESS) {
        return NULL;
    }
    
    for (uint32_t i = 1; i < device->maxBuffers; i++) {
        if (device->buffers[i].isValid && device->buffers[i].deviceAddress == address) {
            return &device->buffers[i];
        }
    }
    
    return NULL;
}

bool veValidateBufferAddress(VEDeviceInternal* device, VEBufferAddress address) {
    return veGetBufferFromAddress(device, address) != NULL;
}

// =============================================================================
// Buffer Creation
// =============================================================================

static VkBufferUsageFlags veBufferUsageToVk(VEBufferUsage usage) {
    VkBufferUsageFlags vkUsage = 0;
    
    if (usage & VE_BUFFER_USAGE_VERTEX) vkUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_INDEX) vkUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_UNIFORM) vkUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_STORAGE) vkUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_INDIRECT) vkUsage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (usage & VE_BUFFER_USAGE_TRANSFER_SRC) vkUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage & VE_BUFFER_USAGE_TRANSFER_DST) vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (usage & VE_BUFFER_USAGE_CONDITIONAL_RENDERING) vkUsage |= VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    
    return vkUsage;
}

VEBufferAddress veCreateBuffer(VEDevice* device, const VEBufferDesc* desc) {
    if (!device || !desc || desc->size == 0) {
        veSetError("Invalid parameters for buffer creation");
        return VE_INVALID_ADDRESS;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    uint32_t index = veAllocateBufferIndex(deviceInternal);
    if (index == 0) {
        return VE_INVALID_ADDRESS;
    }
    
    VEBufferInternal* buffer = &deviceInternal->buffers[index];
    memset(buffer, 0, sizeof(VEBufferInternal));
    
    buffer->size = desc->size;
    buffer->usage = desc->usage;
    buffer->persistentlyMapped = desc->persistentlyMapped;
    
    if (desc->debugName) {
        strncpy(buffer->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
    } else {
        snprintf(buffer->debugName, VE_MAX_DEBUG_NAME_LENGTH, "Buffer_%u", index);
    }
    
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc->size;
    bufferInfo.usage = veBufferUsageToVk(desc->usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (deviceInternal->features.bufferDeviceAddress) {
        bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    
    bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo allocInfo = {0};
    
    if (desc->usage & VE_BUFFER_USAGE_UNIFORM) {
        allocInfo.usage = desc->persistentlyMapped ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_CPU_TO_GPU;
    } else {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    }
    
    if (desc->persistentlyMapped) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    
    VkResult result = vmaCreateBuffer(deviceInternal->allocator, &bufferInfo, &allocInfo,
                                     &buffer->buffer, &buffer->allocation, &buffer->allocationInfo);
    
    if (result != VK_SUCCESS) {
        veSetError("Failed to create buffer (VkResult: %d)", result);
        veFreeBufferIndex(deviceInternal, index);
        return VE_INVALID_ADDRESS;
    }
    
    if (deviceInternal->features.bufferDeviceAddress) {
        VkBufferDeviceAddressInfo addressInfo = {0};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer->buffer;
        
        buffer->deviceAddress = vkGetBufferDeviceAddress(deviceInternal->device, &addressInfo);
    } else {
        buffer->deviceAddress = (VkDeviceAddress)index;
    }
    
    if (desc->persistentlyMapped && buffer->allocationInfo.pMappedData) {
        buffer->mappedData = buffer->allocationInfo.pMappedData;
    }
    
    if (desc->initialData && desc->initialDataSize > 0) {
        VEResult uploadResult = veUpdateBuffer(device, buffer->deviceAddress, desc->initialData, 
                                              desc->initialDataSize, 0);
        if (uploadResult != VE_SUCCESS) {
            vmaDestroyBuffer(deviceInternal->allocator, buffer->buffer, buffer->allocation);
            veFreeBufferIndex(deviceInternal, index);
            return VE_INVALID_ADDRESS;
        }
    }
    
    if (deviceInternal->context->validationEnabled) {
        veSetObjectDebugName(deviceInternal, (uint64_t)buffer->buffer, VK_OBJECT_TYPE_BUFFER, buffer->debugName);
    }
    
    buffer->isValid = true;
    
    return buffer->deviceAddress;
}

void veDestroyBuffer(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) {
        return;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    if (!buffer || !buffer->isValid) {
        return;
    }
    
    if (buffer->mappedData && !buffer->persistentlyMapped) {
        vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
    }
    
    vmaDestroyBuffer(deviceInternal->allocator, buffer->buffer, buffer->allocation);
    
    uint32_t index = (uint32_t)(uintptr_t)(buffer - deviceInternal->buffers);
    if (index > 0 && index < deviceInternal->maxBuffers) {
        veFreeBufferIndex(deviceInternal, index);
    }
    
    memset(buffer, 0, sizeof(VEBufferInternal));
}

// =============================================================================
// Buffer Memory Operations
// =============================================================================

VEResult veMapBuffer(VEDevice* device, VEBufferAddress address, void** mappedData) {
    if (!device || address == VE_INVALID_ADDRESS || !mappedData) {
        veSetError("Invalid parameters for buffer mapping");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
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

void veUnmapBuffer(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) {
        return;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    if (!buffer || !buffer->isValid || buffer->persistentlyMapped) {
        return;
    }
    
    if (buffer->mappedData) {
        vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
        buffer->mappedData = NULL;
    }
}

VEResult veUpdateBuffer(VEDevice* device, VEBufferAddress address, 
                       const void* data, size_t size, size_t offset) {
    if (!device || address == VE_INVALID_ADDRESS || !data || size == 0) {
        veSetError("Invalid parameters for buffer update");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    if (!buffer || !buffer->isValid) {
        veSetError("Invalid buffer address");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    if (offset + size > buffer->size) {
        veSetError("Update size exceeds buffer bounds");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    void* mappedData = buffer->mappedData;
    bool needsUnmap = false;
    
    if (!mappedData) {
        VkResult result = vmaMapMemory(deviceInternal->allocator, buffer->allocation, &mappedData);
        if (result != VK_SUCCESS) {
            veSetError("Failed to map buffer for update (VkResult: %d)", result);
            return VE_ERROR_OUT_OF_MEMORY;
        }
        needsUnmap = true;
    }
    
    memcpy((char*)mappedData + offset, data, size);
    vmaFlushAllocation(deviceInternal->allocator, buffer->allocation, offset, size);
    
    if (needsUnmap) {
        vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
    }
    
    return VE_SUCCESS;
}

// =============================================================================
// Buffer Property Queries
// =============================================================================

size_t veGetBufferSize(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) return 0;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    return (buffer && buffer->isValid) ? buffer->size : 0;
}

VEBufferUsage veGetBufferUsage(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) return 0;
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    return (buffer && buffer->isValid) ? buffer->usage : 0;
}

// =============================================================================
// Convenience Buffer Creation Functions
// =============================================================================

VEBufferAddress veCreateVertexBuffer(VEDevice* device, const void* vertices, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VE_BUFFER_USAGE_VERTEX,
        .initialData = vertices,
        .initialDataSize = size,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "VertexBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

VEBufferAddress veCreateIndexBuffer(VEDevice* device, const void* indices, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VE_BUFFER_USAGE_INDEX,
        .initialData = indices,
        .initialDataSize = size,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "IndexBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

VEBufferAddress veCreateUniformBuffer(VEDevice* device, size_t size, bool persistentlyMapped, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VE_BUFFER_USAGE_UNIFORM,
        .initialData = NULL,
        .initialDataSize = 0,
        .persistentlyMapped = persistentlyMapped,
        .debugName = debugName ? debugName : "UniformBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

VEBufferAddress veCreateStorageBuffer(VEDevice* device, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VE_BUFFER_USAGE_STORAGE,
        .initialData = NULL,
        .initialDataSize = 0,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "StorageBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

VEBufferAddress veCreateIndirectBuffer(VEDevice* device, size_t size, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VE_BUFFER_USAGE_INDIRECT | VE_BUFFER_USAGE_STORAGE,
        .initialData = NULL,
        .initialDataSize = 0,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "IndirectBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

// =============================================================================
// Utility Functions  
// =============================================================================

const char* veResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        default: return "Unknown VkResult";
    }
}

void vePrintVkResult(const char* operation, VkResult result) {
    if (result != VK_SUCCESS) {
        printf("VulkEase: %s failed with %s (%d)\\n", operation, veResultToString(result), result);
    }
}

void veSetObjectDebugName(VEDeviceInternal* device, uint64_t objectHandle, 
                         VkObjectType objectType, const char* name) {
    if (!device->context->validationEnabled || !name) {
        return;
    }
    
    PFN_vkSetDebugUtilsObjectNameEXT setDebugName = (PFN_vkSetDebugUtilsObjectNameEXT)
        vkGetInstanceProcAddr(device->context->instance, "vkSetDebugUtilsObjectNameEXT");
    
    if (!setDebugName) {
        return;
    }
    
    VkDebugUtilsObjectNameInfoEXT nameInfo = {0};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name;
    
    setDebugName(device->device, &nameInfo);
}