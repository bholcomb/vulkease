/**
 * @file ve_buffer.cpp  
 * @brief Buffer Management Implementation (C++ with STL for simplicity)
 */

#include "ve_internal.h"
#include <unordered_map>
#include <memory>

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
    
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = device->physicalDevice;
    allocatorInfo.device = device->device;
    allocatorInfo.instance = device->context->instance;
    allocatorInfo.vulkanApiVersion = device->deviceProperties.apiVersion;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    
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
    
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = device->queueFamilies.graphicsFamily;
    
    result = vkCreateCommandPool(device->device, &poolInfo, NULL, &device->commandPool);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create command pool (VkResult: %d)", result);
        veCleanupVMA(device);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    device->commandBuffers = static_cast<VECommandBufferInternal*>(
        calloc(VE_MAX_COMMAND_BUFFERS, sizeof(VECommandBufferInternal)));
    device->commandBufferInUse = static_cast<bool*>(
        calloc(VE_MAX_COMMAND_BUFFERS, sizeof(bool)));
    device->commandBufferCount = 0;
    
    if (!device->commandBuffers || !device->commandBufferInUse) {
        veCleanupVMA(device);
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
    
    if (device->commandBuffers) {
        free(device->commandBuffers);
        device->commandBuffers = nullptr;
    }
    
    if (device->commandBufferInUse) {
        free(device->commandBufferInUse);
        device->commandBufferInUse = nullptr;
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
    bufferInfo.usage = veBufferUsageToVk(desc->usage) | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo allocInfo = {};
    
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
        return VE_INVALID_ADDRESS;
    }
    
    // Get the device address (guaranteed to work since we require buffer device address)
    VkBufferDeviceAddressInfo addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer->buffer;
    
    VEBufferAddress deviceAddress = vkGetBufferDeviceAddress(deviceInternal->device, &addressInfo);
    buffer->deviceAddress = deviceAddress;
    
    if (desc->persistentlyMapped && buffer->allocationInfo.pMappedData) {
        buffer->mappedData = buffer->allocationInfo.pMappedData;
    }
    
    if (desc->initialData && desc->initialDataSize > 0) {
        VEResult uploadResult = veUpdateBuffer(device, deviceAddress, desc->initialData, 
                                              desc->initialDataSize, 0);
        if (uploadResult != VE_SUCCESS) {
            vmaDestroyBuffer(deviceInternal->allocator, buffer->buffer, buffer->allocation);
            return VE_INVALID_ADDRESS;
        }
    }
    
    if (deviceInternal->context->validationEnabled) {
        veSetObjectDebugName(deviceInternal, (uint64_t)buffer->buffer, VK_OBJECT_TYPE_BUFFER, buffer->debugName);
    }
    
    buffer->isValid = true;
    
    // Store in map and transfer ownership
    (*bufferMap)[deviceAddress] = std::move(buffer);
    
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
    
    memcpy(static_cast<char*>(mappedData) + offset, data, size);
    vmaFlushAllocation(deviceInternal->allocator, buffer->allocation, offset, size);
    
    if (needsUnmap) {
        vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
    }
    
    return VE_SUCCESS;
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

extern "C" VEBufferUsage veGetBufferUsage(VEDevice* device, VEBufferAddress address) {
    if (!device || address == VE_INVALID_ADDRESS) return static_cast<VEBufferUsage>(0);
    
    VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
    VEBufferInternal* buffer = veGetBufferFromAddress(deviceInternal, address);
    
    return (buffer && buffer->isValid) ? buffer->usage : static_cast<VEBufferUsage>(0);
}

// =============================================================================
// Convenience Buffer Creation Functions
// =============================================================================

extern "C" VEBufferAddress veCreateVertexBuffer(VEDevice* device, const void* vertices, size_t size, const char* debugName) {
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

extern "C" VEBufferAddress veCreateIndexBuffer(VEDevice* device, const void* indices, size_t size, const char* debugName) {
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

extern "C" VEBufferAddress veCreateUniformBuffer(VEDevice* device, size_t size, bool persistentlyMapped, const char* debugName) {
    VEBufferDesc desc = {
        .size = size,
        .usage = VE_BUFFER_USAGE_UNIFORM,
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
        .usage = VE_BUFFER_USAGE_STORAGE,
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
        .usage = static_cast<VEBufferUsage>(VE_BUFFER_USAGE_INDIRECT | VE_BUFFER_USAGE_STORAGE),
        .initialData = nullptr,
        .initialDataSize = 0,
        .persistentlyMapped = false,
        .debugName = debugName ? debugName : "IndirectBuffer"
    };
    
    return veCreateBuffer(device, &desc);
}

// =============================================================================
// Utility Functions  
// =============================================================================

extern "C" const char* veResultToString(VkResult result) {
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

extern "C" void vePrintVkResult(const char* operation, VkResult result) {
    if (result != VK_SUCCESS) {
        printf("VulkEase: %s failed with %s (%d)\n", operation, veResultToString(result), result);
    }
}

extern "C" void veSetObjectDebugName(VEDeviceInternal* device, uint64_t objectHandle, 
                                     VkObjectType objectType, const char* name) {
    if (!device->context->validationEnabled || !name) {
        return;
    }
    
    PFN_vkSetDebugUtilsObjectNameEXT setDebugName = (PFN_vkSetDebugUtilsObjectNameEXT)
        vkGetInstanceProcAddr(device->context->instance, "vkSetDebugUtilsObjectNameEXT");
    
    if (!setDebugName) {
        return;
    }
    
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name;
    
    setDebugName(device->device, &nameInfo);
}