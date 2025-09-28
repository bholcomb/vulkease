#include "ve_internal.h"

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
      free(pool->commandBuffers);
      pool->commandBuffers = nullptr;
   }

   if (pool->commandBufferInUse)
   {
      free(pool->commandBufferInUse);
      pool->commandBufferInUse = nullptr;
   }

   pool->commandBufferCount = 0;

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
   // Find free command buffer
   uint32_t checkCount = 0;
   uint32_t nextBuffer = pool->nextFreeCommandBuffer;
   while (pool->commandBufferInUse[nextBuffer])
   {
      nextBuffer++;
      nextBuffer = nextBuffer % VE_MAX_COMMAND_BUFFERS; // wrap around looking for a free one
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
   }

   pool->commandBufferInUse[nextBuffer] = true;
   pool->commandBuffers[nextBuffer].device = device; // Store device reference
   pool->commandBuffers[nextBuffer].isRecording = false;
   pool->commandBuffers[nextBuffer].isOneTime = false;

   *outCmd = &pool->commandBuffers[nextBuffer];

   pool->commandBufferCount++;
   return VE_SUCCESS;
}

extern "C" void veFreeCommandBuffer(VECommandBufferInternal *cmd)
{
   if (!cmd || cmd->index >= VE_MAX_COMMAND_BUFFERS)
      return;

   VECommandPool *pool = cmd->commandPool;
   pool->commandBufferInUse[cmd->index] = false;
   pool->commandBufferCount--;
   cmd->isRecording = false;
   cmd->isOneTime = false;
}