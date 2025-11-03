#include "ve_internal.h"

#include <mutex>
#include <new>

extern "C" bool veInitCommandPool(VEDeviceInternal *device, uint32_t queueFamily, VECommandPool *pool)
{
   if (pool == nullptr)
   {
      return false;
   }

   pool->commandBuffers =
       static_cast<VECommandBufferInternal *>(calloc(VE_MAX_COMMAND_BUFFERS, sizeof(VECommandBufferInternal)));
   pool->commandBufferInUse = static_cast<bool *>(calloc(VE_MAX_COMMAND_BUFFERS, sizeof(bool)));
   pool->commandBufferCount = 0;

   if (!pool->commandBuffers || !pool->commandBufferInUse)
   {
      return false;
   }

   pool->allocationLock = new (std::nothrow) std::mutex();
   if (!pool->allocationLock)
   {
      free(pool->commandBuffers);
      pool->commandBuffers = nullptr;
      free(pool->commandBufferInUse);
      pool->commandBufferInUse = nullptr;
      return false;
   }

   pool->queueFamily = queueFamily;

   VkCommandPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   poolInfo.queueFamilyIndex = pool->queueFamily;

   VkResult result = vkCreateCommandPool(device->device, &poolInfo, NULL, &pool->commandPool);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create command pool (VkResult: %d)", result);
      return false;
   }

   return true;
}

extern "C" bool veDestroyCommandPool(VEDeviceInternal *device, VECommandPool *pool)
{
   if (pool == nullptr)
   {
      return false;
   }

   if (pool->commandBuffers)
   {
      for (uint32_t i = 0; i < VE_MAX_COMMAND_BUFFERS; ++i)
      {
         if (pool->commandBuffers[i].inFlightFence)
         {
            vkDestroyFence(device->device, pool->commandBuffers[i].inFlightFence, NULL);
            pool->commandBuffers[i].inFlightFence = VK_NULL_HANDLE;
         }
      }
      free(pool->commandBuffers);
      pool->commandBuffers = nullptr;
   }

   if (pool->commandBufferInUse)
   {
      free(pool->commandBufferInUse);
      pool->commandBufferInUse = nullptr;
   }

   pool->commandBufferCount = 0;

   if (pool->allocationLock)
   {
      delete static_cast<std::mutex *>(pool->allocationLock);
      pool->allocationLock = nullptr;
   }

   if (pool->commandPool)
   {
      vkDestroyCommandPool(device->device, pool->commandPool, NULL);
      pool->commandPool = VK_NULL_HANDLE;
   }

   return true;
}

extern "C" VECommandBufferInternal *veGetCommandBufferInternal(VECommandBuffer *cmd)
{
   return (VECommandBufferInternal *)cmd;
}

extern "C" VEResult veAllocateCommandBuffer(VEDeviceInternal *device, VECommandPool *pool,
                                            VECommandBufferInternal **outCmd)
{
   std::lock_guard<std::mutex> lock(*static_cast<std::mutex *>(pool->allocationLock));

   // Find free command buffer
   uint32_t checkCount = 0;
   uint32_t nextBuffer = pool->nextFreeCommandBuffer;
   while (pool->commandBufferInUse[nextBuffer])
   {
      VECommandBufferInternal *candidate = &pool->commandBuffers[nextBuffer];

      if (candidate->fenceActive)
      {
         VkFence fenceToCheck = candidate->activeFence ? candidate->activeFence : candidate->inFlightFence;
         if (fenceToCheck)
         {
            VkResult fenceStatus = vkGetFenceStatus(device->device, fenceToCheck);
            if (fenceStatus == VK_SUCCESS)
            {
               if (candidate->commandBuffer)
               {
                  vkResetCommandBuffer(candidate->commandBuffer, 0);
               }

               candidate->fenceActive = false;
               candidate->activeFence = VK_NULL_HANDLE;
               pool->commandBufferInUse[nextBuffer] = false;
               if (pool->commandBufferCount > 0)
               {
                  pool->commandBufferCount--;
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

      nextBuffer = (nextBuffer + 1) % VE_MAX_COMMAND_BUFFERS; // wrap around looking for a free one
      checkCount++;
      if (checkCount > VE_MAX_COMMAND_BUFFERS)
      {
         veSetError("Failed to allocate command buffer");
         return VE_ERROR_OUT_OF_MEMORY;
      }
   }

   // next free buffer is the next one
   pool->nextFreeCommandBuffer = (nextBuffer + 1) % VE_MAX_COMMAND_BUFFERS; // wrap around

   if (!pool->commandBuffers[nextBuffer].commandBuffer)
   {
      // Allocate new command buffer
      VkCommandBufferAllocateInfo allocInfo = {};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.commandPool = pool->commandPool;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandBufferCount = 1;

      VkResult result =
          vkAllocateCommandBuffers(device->device, &allocInfo, &pool->commandBuffers[nextBuffer].commandBuffer);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to allocate command buffer (VkResult: %d)", result);
         return VE_ERROR_OUT_OF_MEMORY;
      }

      pool->commandBuffers[nextBuffer].commandPool = pool;
      pool->commandBuffers[nextBuffer].index = nextBuffer;

      if (!pool->commandBuffers[nextBuffer].inFlightFence)
      {
         VkFenceCreateInfo fenceInfo = {};
         fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
         fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
         VkResult fenceResult =
             vkCreateFence(device->device, &fenceInfo, NULL, &pool->commandBuffers[nextBuffer].inFlightFence);
         if (fenceResult != VK_SUCCESS)
         {
            vkFreeCommandBuffers(device->device, pool->commandPool, 1, &pool->commandBuffers[nextBuffer].commandBuffer);
            pool->commandBuffers[nextBuffer].commandBuffer = VK_NULL_HANDLE;
            veSetError("Failed to create command buffer fence (VkResult: %d)", fenceResult);
            return VE_ERROR_OUT_OF_MEMORY;
         }
      }
   }

   pool->commandBufferInUse[nextBuffer] = true;
   pool->commandBuffers[nextBuffer].device = device; // Store device reference
   pool->commandBuffers[nextBuffer].isRecording = false;
   pool->commandBuffers[nextBuffer].isOneTime = false;
   pool->commandBuffers[nextBuffer].fenceActive = false;
   pool->commandBuffers[nextBuffer].activeFence = VK_NULL_HANDLE;

   *outCmd = &pool->commandBuffers[nextBuffer];

   pool->commandBufferCount++;
   return VE_SUCCESS;
}

extern "C" void veFreeCommandBuffer(VECommandBufferInternal *cmd)
{
   if (!cmd || cmd->index >= VE_MAX_COMMAND_BUFFERS)
      return;

   VECommandPool *pool = cmd->commandPool;

   std::lock_guard<std::mutex> lock(*static_cast<std::mutex *>(pool->allocationLock));

   pool->commandBufferInUse[cmd->index] = false;
   if (pool->commandBufferCount > 0)
   {
      pool->commandBufferCount--;
   }
   cmd->isRecording = false;
   cmd->isOneTime = false;
   cmd->fenceActive = false;
   cmd->activeFence = VK_NULL_HANDLE;

   if (cmd->commandBuffer)
   {
      vkResetCommandBuffer(cmd->commandBuffer, 0);
   }
}

static void veNotifyFenceInPool(VEDeviceInternal *device, VECommandPool *pool, VkFence fence)
{
   if (!device || !pool || fence == VK_NULL_HANDLE || !pool->commandBuffers)
   {
      return;
   }

   std::lock_guard<std::mutex> lock(*static_cast<std::mutex *>(pool->allocationLock));

   for (uint32_t i = 0; i < VE_MAX_COMMAND_BUFFERS; ++i)
   {
      VECommandBufferInternal *cmd = &pool->commandBuffers[i];
      if (cmd->activeFence == fence)
      {
         if (pool->commandBufferInUse[i])
         {
            pool->commandBufferInUse[i] = false;
            if (pool->commandBufferCount > 0)
            {
               pool->commandBufferCount--;
            }
         }

         cmd->fenceActive = false;
         cmd->activeFence = VK_NULL_HANDLE;

         if (cmd->commandBuffer)
         {
            vkResetCommandBuffer(cmd->commandBuffer, 0);
         }
      }
   }
}

extern "C" void veNotifyCommandBufferFenceSignaled(VEDeviceInternal *device, VkFence fence)
{
   if (!device || fence == VK_NULL_HANDLE)
   {
      return;
   }

   veNotifyFenceInPool(device, device->graphicsCommandPool, fence);
   if (device->computeCommandPool != device->graphicsCommandPool)
   {
      veNotifyFenceInPool(device, device->computeCommandPool, fence);
   }
   if (device->transferCommandPool != device->graphicsCommandPool &&
       device->transferCommandPool != device->computeCommandPool)
   {
      veNotifyFenceInPool(device, device->transferCommandPool, fence);
   }
}