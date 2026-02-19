/**
 * @file ve_buffer.cpp
 * @brief Buffer Management Implementation (C++ with STL for simplicity)
 */

#include "ve_internal.h"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

// Type aliases for cleaner code
using BufferMap = std::unordered_map<VEBufferAddress, std::unique_ptr<VEBufferInternal>>;

// Helper to get the buffer map from the opaque pointer
static BufferMap *getBufferMap(VEDeviceInternal *device) { return static_cast<BufferMap *>(device->bufferMap); }

// =============================================================================
// VMA Integration
// =============================================================================

VEResult VEDeviceInternal::initializeVma()
{
   if (!features.bufferDeviceAddress)
   {
      veSetError("Buffer device address support is required - no fallback strategy");
      return VE_ERROR_UNSUPPORTED;
   }

   VmaVulkanFunctions vulkanFunctions{};
   vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
   vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

   VmaAllocatorCreateInfo allocatorInfo{};
   allocatorInfo.physicalDevice = physicalDevice;
   allocatorInfo.device = device;
   allocatorInfo.instance = context->instance;
   // allocatorInfo.vulkanApiVersion = deviceProperties.apiVersion;
   allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
   allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
   allocatorInfo.pVulkanFunctions = &vulkanFunctions;

   VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create VMA allocator (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   bufferMap = new (std::nothrow) BufferMap();
   if (!bufferMap)
   {
      veSetError("Failed to allocate buffer map");
      vmaDestroyAllocator(allocator);
      allocator = VK_NULL_HANDLE;
      return VE_ERROR_OUT_OF_MEMORY;
   }

   bufferMapMutex = std::make_unique<std::mutex>();
   if (!bufferMapMutex)
   {
      veSetError("Failed to allocate buffer map mutex");
      delete static_cast<BufferMap *>(bufferMap);
      bufferMap = nullptr;
      vmaDestroyAllocator(allocator);
      allocator = VK_NULL_HANDLE;
      return VE_ERROR_OUT_OF_MEMORY;
   }

   return VE_SUCCESS;
}

void VEDeviceInternal::cleanupVma()
{
   if (device)
   {
      vkDeviceWaitIdle(device);
   }

   if (bufferMap)
   {
      // Lock while cleaning up buffer map
      if (bufferMapMutex)
      {
         std::lock_guard<std::mutex> lock(*bufferMapMutex);
         BufferMap *map = getBufferMap(this);
         for (auto &pair : *map)
         {
            auto &buffer = pair.second;
            if (buffer && buffer->isValid)
            {
               vmaDestroyBuffer(allocator, buffer->buffer, buffer->allocation);
            }
         }
         delete map;
         bufferMap = nullptr;
      }
      else
      {
         BufferMap *map = getBufferMap(this);
         for (auto &pair : *map)
         {
            auto &buffer = pair.second;
            if (buffer && buffer->isValid)
            {
               vmaDestroyBuffer(allocator, buffer->buffer, buffer->allocation);
            }
         }
         delete map;
         bufferMap = nullptr;
      }
   }

   bufferMapMutex.reset();

   if (allocator)
   {
      vmaDestroyAllocator(allocator);
      allocator = VK_NULL_HANDLE;
   }
}

// =============================================================================
// Buffer Address Management (Simplified with STL)
// =============================================================================

// Get buffer from address using O(1) hash map lookup
// Note: Caller must hold bufferMapMutex or call the thread-safe variant
VEBufferInternal *VEDeviceInternal::getBufferFromAddress(VEBufferAddress address)
{
   if (address == VE_INVALID_ADDRESS || !bufferMap || !bufferMapMutex)
   {
      return nullptr;
   }

   std::lock_guard<std::mutex> lock(*bufferMapMutex);
   BufferMap *map = getBufferMap(this);
   auto it = map->find(address);
   return (it != map->end()) ? it->second.get() : nullptr;
}

bool VEDeviceInternal::validateBufferAddress(VEBufferAddress address) const
{
   if (address == VE_INVALID_ADDRESS || !bufferMap || !bufferMapMutex)
   {
      return false;
   }

   std::lock_guard<std::mutex> lock(*bufferMapMutex);
   BufferMap *map = getBufferMap(const_cast<VEDeviceInternal *>(this));
   auto it = map->find(address);
   return it != map->end() && it->second && it->second->isValid;
}

VkBuffer VEDeviceInternal::getVkBufferFromAddress(VEBufferAddress address) const
{
   if (address == VE_INVALID_ADDRESS || !bufferMap || !bufferMapMutex)
   {
      return VK_NULL_HANDLE;
   }

   std::lock_guard<std::mutex> lock(*bufferMapMutex);
   BufferMap *map = getBufferMap(const_cast<VEDeviceInternal *>(this));
   auto it = map->find(address);
   if (it != map->end() && it->second && it->second->isValid)
   {
      return it->second->buffer;
   }
   return VK_NULL_HANDLE;
}

VECommandBufferInternal *VEDeviceInternal::beginTransferCommandBuffer()
{
   VECommandBufferInternal *cmd = nullptr;
   VECommandPool *pool = threadCommandPools.acquire(this, VECommandPoolKind::Transfer);
   if (!pool)
   {
      return nullptr;
   }

   if (pool->allocate(&cmd) != VE_SUCCESS || cmd == nullptr)
   {
      veSetError("Failed to allocate transfer command buffer");
      return nullptr;
   }

   if (cmd->fenceActive)
   {
      VkFence fence = cmd->currentFence();
      if (fence != VK_NULL_HANDLE)
      {
         VkResult status = vkGetFenceStatus(device, fence);
         if (status == VK_NOT_READY)
         {
            veSetError("Transfer command buffer GPU work still in-flight (cb=%p index=%u pool=%p fence=%p)",
                       (void *)cmd->commandBuffer, cmd->index, (void *)cmd->commandPool, (void *)fence);
            pool->release(*cmd);
            return nullptr;
         }
         else if (status != VK_SUCCESS)
         {
            veSetError("Failed to query transfer command buffer fence status (VkResult: %d)", status);
            pool->release(*cmd);
            return nullptr;
         }
      }

      cmd->clearFenceTracking();
   }

   VkResult resetResult = vkResetCommandBuffer(cmd->commandBuffer, 0);
   if (resetResult != VK_SUCCESS)
   {
      VkFence fence = cmd->currentFence();
      VkResult fenceStatus = fence ? vkGetFenceStatus(device, fence) : VK_SUCCESS;
      veSetError("Failed to reset transfer command buffer (VkResult: %d) cb=%p index=%u fence=%p fenceStatus=%d "
                 "fenceActive=%d",
                 resetResult, (void *)cmd->commandBuffer, cmd->index, (void *)fence, fenceStatus,
                 cmd->fenceActive ? 1 : 0);
      pool->release(*cmd);
      return nullptr;
   }

   VkCommandBufferBeginInfo beginInfo{};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   VkResult vkResult = vkBeginCommandBuffer(cmd->commandBuffer, &beginInfo);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to begin transfer command buffer (VkResult: %d)", vkResult);
      pool->release(*cmd);
      return nullptr;
   }

   cmd->isRecording = true;
   cmd->isOneTime = true;
   return cmd;
}

VEResult VEDeviceInternal::submitTransferCommandBuffer(VECommandBufferInternal *cmd, bool waitForCompletion)
{
   if (!cmd)
   {
      veSetError("Command buffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!cmd->isRecording)
   {
      veSetError("Command buffer is not recording");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkResult result = vkEndCommandBuffer(cmd->commandBuffer);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to end command buffer (VkResult: %d)", result);
      return VE_ERROR_UNKNOWN;
   }

   cmd->isRecording = false;

   VkFence fence = cmd->inFlightFence;
   if (fence == VK_NULL_HANDLE)
   {
      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      VkResult fenceResult = vkCreateFence(device, &fenceInfo, NULL, &fence);
      if (fenceResult != VK_SUCCESS)
      {
         veSetError("Failed to create transfer fence (VkResult: %d)", fenceResult);
         return VE_ERROR_OUT_OF_MEMORY;
      }
      cmd->inFlightFence = fence;
   }

   result = vkResetFences(device, 1, &fence);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to reset transfer fence (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VEDeviceQueueLocks *locks = queueLocks.get();
   if (!locks)
   {
      veSetError("Device queue locks not initialized");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkSubmitInfo submitInfo{};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &cmd->commandBuffer;

   // Track fence even on synchronous submissions so idle checks see pending work.
   cmd->markFenceActive(fence);

   {
      std::lock_guard<std::mutex> lock(locks->transferMutex());
      result = vkQueueSubmit(transferQueue, 1, &submitInfo, fence);
   }

   if (result != VK_SUCCESS)
   {
      veSetError("Failed to submit command buffer (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   if (waitForCompletion)
   {
      result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to wait for transfer command buffer completion (VkResult: %d)", result);
         return VE_ERROR_OUT_OF_MEMORY;
      }

      cmd->clearFenceTracking();
      veFreeCommandBuffer(cmd);
      // Nothing was tracked for async reclaim on this path.
      return VE_SUCCESS;
   }

   // Note: markFenceActive was already called above before submission
   // Track for async reclaim using 'this' (we are a VEDeviceInternal member function)
   this->threadCommandPools.trackInFlight(fence, cmd);
   this->threadCommandPools.reclaimInFlight(this);

   return VE_SUCCESS;
}

// =============================================================================
// Buffer Creation
// =============================================================================

extern "C" VEResult veCreateBuffer(VEDevice *device, const VEBufferDesc *desc, VEBufferAddress *outAddress)
{
   if (!outAddress)
   {
      veSetError("outAddress cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device || !desc || desc->size == 0)
   {
      veSetError("Invalid parameters for buffer creation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);

   // Buffer device address is required - no fallback
   if (!deviceInternal->features.bufferDeviceAddress)
   {
      veSetError("Buffer device address support is required");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   BufferMap *bufferMap = getBufferMap(deviceInternal);
   if (!bufferMap || !deviceInternal->bufferMapMutex)
   {
      veSetError("Buffer map not initialized");
      return VE_ERROR_NOT_INITIALIZED;
   }

   // Create buffer object using smart pointer for automatic cleanup
   auto buffer = std::make_unique<VEBufferInternal>();
   memset(buffer.get(), 0, sizeof(VEBufferInternal));

   buffer->size = desc->size;
   buffer->usage = desc->usage;
   buffer->persistentlyMapped = desc->persistentlyMapped;

   if (desc->debugName)
   {
      strncpy(buffer->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
   }
   else
   {
      snprintf(buffer->debugName, VE_MAX_DEBUG_NAME_LENGTH, "Buffer_%p", buffer.get());
   }

   VkBufferCreateInfo bufferInfo = {};
   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size = desc->size;
   bufferInfo.usage = desc->usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VmaAllocationCreateInfo allocInfo = {};
   allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

   if (desc->persistentlyMapped)
   {
      allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
   }
   else
   {
      // Prefer GPU memory for better performance, but allow fallback
      allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
   }

   VkResult result = vmaCreateBuffer(deviceInternal->allocator, &bufferInfo, &allocInfo, &buffer->buffer,
                                     &buffer->allocation, &buffer->allocationInfo);

   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create buffer (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Get the device address (guaranteed to work since we require buffer device
   // address)
   VkBufferDeviceAddressInfo addressInfo = {};
   addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
   addressInfo.buffer = buffer->buffer;

   VEBufferAddress deviceAddress = vkGetBufferDeviceAddress(deviceInternal->device, &addressInfo);
   buffer->deviceAddress = deviceAddress;

   if (deviceInternal->context->validationEnabled)
   {
      veSetObjectDebugName(deviceInternal, (uint64_t)buffer->buffer, VK_OBJECT_TYPE_BUFFER, buffer->debugName);
   }

   buffer->isValid = true;

   // hold on to the raw pointer for initializing
   VEBufferInternal *bufferPtr = buffer.get();

   // Store in map and transfer ownership under lock
   {
      std::lock_guard<std::mutex> lock(*deviceInternal->bufferMapMutex);
      (*bufferMap)[deviceAddress] = std::move(buffer);
   }

   if (desc->persistentlyMapped && bufferPtr->allocationInfo.pMappedData)
   {
      bufferPtr->mappedData = bufferPtr->allocationInfo.pMappedData;
   }

   if (desc->initialData && desc->initialDataSize > 0)
   {
      VEResult uploadResult = veUpdateBuffer(device, deviceAddress, desc->initialData, desc->initialDataSize, 0);
      if (uploadResult != VE_SUCCESS)
      {
         // Fix: Remove from map before destroying to prevent dangling entry
         {
            std::lock_guard<std::mutex> lock(*deviceInternal->bufferMapMutex);
            bufferMap->erase(deviceAddress);
         }
         vmaDestroyBuffer(deviceInternal->allocator, bufferPtr->buffer, bufferPtr->allocation);
         return uploadResult;
      }
   }

   *outAddress = deviceAddress;
   return VE_SUCCESS;
}

void veDestroyBufferImmediate(VEDeviceInternal *deviceInternal, VEBufferAddress address)
{
   if (!deviceInternal || address == VE_INVALID_ADDRESS)
   {
      return;
   }

   BufferMap *bufferMap = getBufferMap(deviceInternal);

   if (!bufferMap || !deviceInternal->bufferMapMutex)
   {
      return;
   }

   std::unique_ptr<VEBufferInternal> bufferToDestroy;

   {
      std::lock_guard<std::mutex> lock(*deviceInternal->bufferMapMutex);

      auto it = bufferMap->find(address);
      if (it == bufferMap->end())
      {
         return; // Buffer not found
      }

      VEBufferInternal *buffer = it->second.get();
      if (!buffer || !buffer->isValid)
      {
         return;
      }

      // Move ownership out of the map while holding the lock
      bufferToDestroy = std::move(it->second);
      bufferMap->erase(it);
   }

   // Destroy outside the lock to minimize contention
   if (bufferToDestroy)
   {
      if (bufferToDestroy->mappedData && !bufferToDestroy->persistentlyMapped)
      {
         vmaUnmapMemory(deviceInternal->allocator, bufferToDestroy->allocation);
      }
      vmaDestroyBuffer(deviceInternal->allocator, bufferToDestroy->buffer, bufferToDestroy->allocation);
   }
}

extern "C" VEResult veDestroyBuffer(VEDevice *device, VEBufferAddress address)
{
   if (!device || address == VE_INVALID_ADDRESS)
   {
      veSetError("Invalid parameters for buffer destruction");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);
   if (!deviceInternal->deferredDeletionQueue)
   {
      veSetError("Deferred deletion queue not initialized");
      return VE_ERROR_NOT_INITIALIZED;
   }
   deviceInternal->deferredDeletionQueue->enqueueBuffer(address);
   return VE_SUCCESS;
}

extern "C" VEResult veMapBuffer(VEDevice *device, VEBufferAddress address, void **mappedData)
{
   if (!mappedData)
   {
      veSetError("mappedData cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }
   if (!device || address == VE_INVALID_ADDRESS)
   {
      veSetError("Invalid parameters for buffer mapping");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);
   VEBufferInternal *buffer = deviceInternal->getBufferFromAddress(address);
   if (!buffer || !buffer->isValid)
   {
      veSetError("Buffer not found");
      return VE_ERROR_NOT_FOUND;
   }

   // If persistently mapped, return the existing pointer (if available).
   if (buffer->persistentlyMapped && buffer->mappedData)
   {
      *mappedData = buffer->mappedData;
      return VE_SUCCESS;
   }

   void *data = nullptr;
   VkResult result = vmaMapMemory(deviceInternal->allocator, buffer->allocation, &data);
   if (result != VK_SUCCESS || !data)
   {
      veSetError("Failed to map buffer memory (VkResult: %d)", result);
      return VE_ERROR_TRANSFER_FAILED;
   }

   buffer->mappedData = data;
   *mappedData = data;
   return VE_SUCCESS;
}

extern "C" VEResult veUnmapBuffer(VEDevice *device, VEBufferAddress address)
{
   if (!device || address == VE_INVALID_ADDRESS)
   {
      veSetError("Invalid parameters for buffer unmapping");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);
   VEBufferInternal *buffer = deviceInternal->getBufferFromAddress(address);
   if (!buffer || !buffer->isValid)
   {
      veSetError("Buffer not found");
      return VE_ERROR_NOT_FOUND;
   }

   // Persistently mapped buffers remain mapped for their lifetime.
   if (buffer->persistentlyMapped)
   {
      return VE_SUCCESS;
   }

   if (buffer->mappedData)
   {
      vmaUnmapMemory(deviceInternal->allocator, buffer->allocation);
      buffer->mappedData = nullptr;
   }

   return VE_SUCCESS;
}

// =============================================================================
// Transfer Commands
// =============================================================================

static VEResult veUpdateBufferWithStaging(VEDeviceInternal *device, VEBufferInternal *dstBuffer, const void *data,
                                          uint64_t size, uint64_t offset)
{
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

   VkResult result = vmaCreateBuffer(device->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer,
                                     &stagingAllocation, &stagingAllocationInfo);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create staging buffer (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   // Copy data to staging buffer (it's already mapped)
   memcpy(stagingAllocationInfo.pMappedData, data, size);
   vmaFlushAllocation(device->allocator, stagingAllocation, 0, size);

   // Get command buffer for transfer
   VECommandBufferInternal *transferCmd = device->beginTransferCommandBuffer();
   if (transferCmd == nullptr)
   {
      vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);
      veSetError("Failed to get transfer command buffer");
      return VE_ERROR_TRANSFER_FAILED;
   }

   // Record copy command
   VkBufferCopy copyRegion = {};
   copyRegion.srcOffset = 0;
   copyRegion.dstOffset = offset;
   copyRegion.size = size;

   vkCmdCopyBuffer(transferCmd->commandBuffer, stagingBuffer, dstBuffer->buffer, 1, &copyRegion);

   // Add memory barrier if destination buffer will be used in different stage
   if (dstBuffer->usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
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

      vkCmdPipelineBarrier2(transferCmd->commandBuffer, &dependencyInfo);
   }

   // Submit transfer command and wait for completion
   VEResult submitResult = device->submitTransferCommandBuffer(transferCmd, true);

   vmaDestroyBuffer(device->allocator, stagingBuffer, stagingAllocation);

   return submitResult;
}

extern "C" VEResult veUpdateBuffer(VEDevice *device, VEBufferAddress address, const void *data, uint64_t size,
                                   uint64_t offset)
{
   if (!device || address == VE_INVALID_ADDRESS || !data || size == 0)
   {
      veSetError("Invalid parameters for buffer update");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);
   VEBufferInternal *buffer = deviceInternal->getBufferFromAddress(address);

   if (!buffer || !buffer->isValid)
   {
      veSetError("Invalid buffer address");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (offset + size > buffer->size)
   {
      veSetError("Update size exceeds buffer bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Check if buffer is persistently mapped or can be mapped
   if (buffer->mappedData)
   {
      // Buffer is persistently mapped - direct update
      memcpy(static_cast<char *>(buffer->mappedData) + offset, data, size);
      vmaFlushAllocation(deviceInternal->allocator, buffer->allocation, offset, size);
      return VE_SUCCESS;
   }

   if (buffer->persistentlyMapped)
   {
      // Try to map the buffer directly (for host-visible memory)
      void *mappedData = nullptr;
      VkResult result = vmaMapMemory(deviceInternal->allocator, buffer->allocation, &mappedData);
      if (result == VK_SUCCESS)
      {
         // Buffer can be mapped directly
         memcpy(static_cast<char *>(mappedData) + offset, data, size);
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

extern "C" uint64_t veGetBufferSize(VEDevice *device, VEBufferAddress address)
{
   if (!device || address == VE_INVALID_ADDRESS)
   {
      veSetError("veGetBufferSize: invalid device or address");
      return 0;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);
   VEBufferInternal *buffer = deviceInternal->getBufferFromAddress(address);

   if (!buffer || !buffer->isValid)
   {
      veSetError("veGetBufferSize: buffer not found for address=%llu", (unsigned long long)address);
      return 0;
   }

   return buffer->size;
}

extern "C" VkBufferUsageFlags veGetBufferUsage(VEDevice *device, VEBufferAddress address)
{
   if (!device || address == VE_INVALID_ADDRESS)
   {
      veSetError("veGetBufferUsage: invalid device or address");
      return 0;
   }

   VEDeviceInternal *deviceInternal = reinterpret_cast<VEDeviceInternal *>(device);
   VEBufferInternal *buffer = deviceInternal->getBufferFromAddress(address);

   if (!buffer || !buffer->isValid)
   {
      veSetError("veGetBufferUsage: buffer not found for address=%llu", (unsigned long long)address);
      return 0;
   }

   return buffer->usage;
}

// =============================================================================
// Convenience Buffer Creation Functions
// =============================================================================

extern "C" VEResult veCreateVertexBuffer(VEDevice *device, const void *vertices, uint64_t size, const char *debugName,
                                         VEBufferAddress *outAddress)
{
   VEBufferDesc desc = {};
   desc.size = size;
   desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
   desc.initialData = vertices;
   desc.initialDataSize = size;
   desc.persistentlyMapped = false;
   desc.debugName = debugName ? debugName : "VertexBuffer";

   return veCreateBuffer(device, &desc, outAddress);
}

extern "C" VEResult veCreateIndexBuffer(VEDevice *device, const void *indices, uint64_t size, const char *debugName,
                                        VEBufferAddress *outAddress)
{
   VEBufferDesc desc = {};
   desc.size = size;
   desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
   desc.initialData = indices;
   desc.initialDataSize = size;
   desc.persistentlyMapped = false;
   desc.debugName = debugName ? debugName : "IndexBuffer";

   return veCreateBuffer(device, &desc, outAddress);
}

extern "C" VEResult veCreateUniformBuffer(VEDevice *device, uint64_t size, bool persistentlyMapped,
                                          const char *debugName, VEBufferAddress *outAddress)
{
   VEBufferDesc desc = {};
   desc.size = size;
   desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
   desc.initialData = nullptr;
   desc.initialDataSize = 0;
   desc.persistentlyMapped = persistentlyMapped;
   desc.debugName = debugName ? debugName : "UniformBuffer";

   return veCreateBuffer(device, &desc, outAddress);
}

extern "C" VEResult veCreateStorageBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                          VEBufferAddress *outAddress)
{
   VEBufferDesc desc = {};
   desc.size = size;
   desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
   desc.initialData = nullptr;
   desc.initialDataSize = 0;
   desc.persistentlyMapped = false;
   desc.debugName = debugName ? debugName : "StorageBuffer";

   return veCreateBuffer(device, &desc, outAddress);
}

extern "C" VEResult veCreateIndirectBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                           VEBufferAddress *outAddress)
{
   VEBufferDesc desc = {};
   desc.size = size;
   desc.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
   desc.initialData = nullptr;
   desc.initialDataSize = 0;
   desc.persistentlyMapped = false;
   desc.debugName = debugName ? debugName : "IndirectBuffer";

   return veCreateBuffer(device, &desc, outAddress);
}
