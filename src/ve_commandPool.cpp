#include "ve_internal.h"

#include <functional>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>
#include <vector>

void VECommandBufferInternal::clearFenceTracking()
{
   activeFence = VK_NULL_HANDLE;
   fenceActive = false;
}

void VECommandBufferInternal::markFenceActive(VkFence fence)
{
   activeFence = fence;
   fenceActive = (fence != VK_NULL_HANDLE);
}

VkFence VECommandBufferInternal::currentFence() const
{
   return activeFence != VK_NULL_HANDLE ? activeFence : inFlightFence;
}

void VECommandBufferInternal::resetState()
{
   isRecording = false;
   isOneTime = false;
   boundShaders = 0;
   clearFenceTracking();
   currentTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   currentPatchControlPoints = 0;
   executedSecondaries.clear();
}

VECommandPool::~VECommandPool() { destroy(); }

bool VECommandPool::initialize(VEDeviceInternal *device, uint32_t family)
{
   if (!device)
   {
      return false;
   }

   destroy();
   ownerDevice = device;

   commandBuffers =
       static_cast<VECommandBufferInternal *>(calloc(VE_MAX_COMMAND_BUFFERS, sizeof(VECommandBufferInternal)));
   commandBufferInUse = static_cast<bool *>(calloc(VE_MAX_COMMAND_BUFFERS, sizeof(bool)));
   commandBufferCount = 0;
   nextFreeCommandBuffer = 0;

   if (!commandBuffers || !commandBufferInUse)
   {
      destroy();
      return false;
   }

   allocationLock.reset(new (std::nothrow) std::mutex());
   if (!allocationLock)
   {
      destroy();
      return false;
   }

   queueFamily = family;

   VkCommandPoolCreateInfo poolInfo{};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   poolInfo.queueFamilyIndex = queueFamily;

   VkResult result = vkCreateCommandPool(ownerDevice->device, &poolInfo, NULL, &commandPool);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create command pool (VkResult: %d)", result);
      destroy();
      return false;
   }

   return true;
}

void VECommandPool::destroy()
{
   if (commandBuffers && ownerDevice && ownerDevice->device)
   {
      for (uint32_t i = 0; i < VE_MAX_COMMAND_BUFFERS; ++i)
      {
         if (commandBuffers[i].inFlightFence)
         {
            vkDestroyFence(ownerDevice->device, commandBuffers[i].inFlightFence, NULL);
            commandBuffers[i].inFlightFence = VK_NULL_HANDLE;
         }
      }
   }

   releaseCommandBufferResources();

   if (commandPool != VK_NULL_HANDLE && ownerDevice && ownerDevice->device)
   {
      vkDestroyCommandPool(ownerDevice->device, commandPool, NULL);
   }

   commandPool = VK_NULL_HANDLE;
   allocationLock.reset();
   commandBufferCount = 0;
   nextFreeCommandBuffer = 0;
   ownerDevice = nullptr;
}

VEResult VECommandPool::allocate(VECommandBufferInternal **outCmd)
{
   if (!outCmd)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!ownerDevice || !ownerDevice->device || !allocationLock)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   std::lock_guard<std::mutex> lock(*allocationLock);

   if (!commandBuffers || !commandBufferInUse)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   uint32_t checkCount = 0;
   uint32_t nextBuffer = nextFreeCommandBuffer;

   while (commandBufferInUse[nextBuffer])
   {
      VECommandBufferInternal *candidate = &commandBuffers[nextBuffer];

      if (candidate->fenceActive)
      {
         VkFence fenceToCheck = candidate->currentFence();
         if (fenceToCheck)
         {
            VkResult fenceStatus = vkGetFenceStatus(ownerDevice->device, fenceToCheck);
            if (fenceStatus == VK_SUCCESS)
            {
               candidate->clearFenceTracking();
               commandBufferInUse[nextBuffer] = false;
               if (commandBufferCount > 0)
               {
                  --commandBufferCount;
               }
               continue;
            }
            else if (fenceStatus != VK_NOT_READY)
            {
               veSetError("Failed to query command buffer fence status (VkResult: %d)", fenceStatus);
               return VE_ERROR_UNKNOWN;
            }
         }
      }
      else
      {
         // Marked in-use but no active fence: this buffer was returned without
         // being tracked in-flight. Surface this so we can find the culprit.
         veSetError("Command buffer reuse without fence tracking (cb=%p index=%u pool=%p)",
                    (void *)candidate->commandBuffer, candidate->index, (void *)this);
      }

      nextBuffer = (nextBuffer + 1) % VE_MAX_COMMAND_BUFFERS;
      checkCount++;
      if (checkCount > VE_MAX_COMMAND_BUFFERS)
      {
         veSetError("Failed to allocate command buffer");
         return VE_ERROR_OUT_OF_MEMORY;
      }
   }

   nextFreeCommandBuffer = (nextBuffer + 1) % VE_MAX_COMMAND_BUFFERS;

   VECommandBufferInternal &buffer = commandBuffers[nextBuffer];
   if (!buffer.commandBuffer)
   {
      VkCommandBufferAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.commandPool = commandPool;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandBufferCount = 1;

      VkResult result = vkAllocateCommandBuffers(ownerDevice->device, &allocInfo, &buffer.commandBuffer);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to allocate command buffer (VkResult: %d)", result);
         return VE_ERROR_OUT_OF_MEMORY;
      }

      buffer.commandPool = this;
      buffer.index = nextBuffer;
      buffer.device = ownerDevice;
      buffer.isSecondary = (allocInfo.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY);

      if (!buffer.inFlightFence)
      {
         VkFenceCreateInfo fenceInfo{};
         fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
         fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
         VkResult fenceResult = vkCreateFence(ownerDevice->device, &fenceInfo, NULL, &buffer.inFlightFence);
         if (fenceResult != VK_SUCCESS)
         {
            vkFreeCommandBuffers(ownerDevice->device, commandPool, 1, &buffer.commandBuffer);
            buffer.commandBuffer = VK_NULL_HANDLE;
            veSetError("Failed to create command buffer fence (VkResult: %d)", fenceResult);
            return VE_ERROR_OUT_OF_MEMORY;
         }
      }
   }

   commandBufferInUse[nextBuffer] = true;
   buffer.device = ownerDevice;
   buffer.resetState();

   *outCmd = &buffer;

   commandBufferCount++;
   return VE_SUCCESS;
}

void VECommandPool::release(VECommandBufferInternal &cmd)
{
   if (cmd.commandPool != this || !allocationLock)
   {
      return;
   }

   // Lock ordering: never hold allocationLock while taking the global
   // threadCommandPools mutex (trackInFlight). Take allocationLock first,
   // decide whether we need to defer, then drop the lock before tracking.
   std::unique_lock<std::mutex> lock(*allocationLock);

   if (cmd.index >= VE_MAX_COMMAND_BUFFERS || !commandBuffers || !commandBufferInUse)
   {
      return;
   }

   // If still in-flight, defer reclaim until fence is signaled.
   if (cmd.fenceActive)
   {
      VkFence fence = cmd.currentFence();
      if (fence != VK_NULL_HANDLE)
      {
         // Drop allocationLock before grabbing the global pool mutex to avoid
         // lock-order inversion with reclaimInFlight/forEach.
         lock.unlock();
         ownerDevice->threadCommandPools.trackInFlight(fence, &cmd);
         return;
      }
   }
   else if (commandBufferInUse[cmd.index])
   {
      VkFence fence = cmd.currentFence();
      if (fence != VK_NULL_HANDLE)
      {
         VkResult status = vkGetFenceStatus(ownerDevice->device, fence);
         if (status == VK_NOT_READY)
         {
            const size_t threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
            veSetError("Releasing buffer while fence is still pending (cb=%p index=%u pool=%p fence=%p thread=%zu)",
                       (void *)cmd.commandBuffer, cmd.index, (void *)this, (void *)fence, threadHash);
         }
      }
      else
      {
         const size_t threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
         veSetError("Releasing buffer without fence handle (cb=%p index=%u pool=%p thread=%zu)",
                    (void *)cmd.commandBuffer, cmd.index, (void *)this, threadHash);
      }
   }

   finishRelease(cmd);
}

void VECommandPool::finishRelease(VECommandBufferInternal &cmd)
{
   commandBufferInUse[cmd.index] = false;
   if (commandBufferCount > 0)
   {
      --commandBufferCount;
   }

   cmd.resetState();
}

void VECommandPool::notifyFenceSignaled(VkFence fence)
{
   if (fence == VK_NULL_HANDLE || !commandBuffers || !allocationLock || !ownerDevice || !ownerDevice->device)
   {
      return;
   }

   std::lock_guard<std::mutex> lock(*allocationLock);

   for (uint32_t i = 0; i < VE_MAX_COMMAND_BUFFERS; ++i)
   {
      VECommandBufferInternal &cmd = commandBuffers[i];
      if (cmd.activeFence == fence)
      {
         if (commandBufferInUse[i])
         {
            commandBufferInUse[i] = false;
            if (commandBufferCount > 0)
            {
               --commandBufferCount;
            }
         }

         cmd.clearFenceTracking();
      }
   }
}

void VECommandPool::releaseCommandBufferResources()
{
   if (commandBuffers)
   {
      free(commandBuffers);
      commandBuffers = nullptr;
   }

   if (commandBufferInUse)
   {
      free(commandBufferInUse);
      commandBufferInUse = nullptr;
   }
}

VECommandBufferInternal *veGetCommandBufferInternal(VECommandBuffer *cmd)
{
   return reinterpret_cast<VECommandBufferInternal *>(cmd);
}

VEResult veAllocateCommandBuffer(VEDeviceInternal *device, VECommandPool *pool, VECommandBufferInternal **outCmd)
{
   if (!pool || pool->ownerDevice != device)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   return pool->allocate(outCmd);
}

void veFreeCommandBuffer(VECommandBufferInternal *cmd)
{
   if (!cmd || !cmd->commandPool)
   {
      return;
   }

   cmd->commandPool->release(*cmd);
}

void veNotifyCommandBufferFenceSignaled(VEDeviceInternal *device, VkFence fence)
{
   if (!device || fence == VK_NULL_HANDLE)
   {
      return;
   }

   device->threadCommandPools.forEach([fence](VECommandPool &pool) { pool.notifyFenceSignaled(fence); });
}