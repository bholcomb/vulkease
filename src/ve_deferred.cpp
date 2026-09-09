/**
 * @file ve_deferred.cpp
 * @brief Deferred Deletion Queue Implementation
 *
 * Handles safe resource cleanup by deferring destruction until
 * the GPU is no longer using the resources.
 */

#include "ve_internal.h"

// =============================================================================
// Deferred Deletion Queue Implementation
// =============================================================================

namespace
{
uint64_t veRetireValue(const VEDeviceInternal *device)
{
   if (!device || !device->queueLocks)
      return 0;
   std::lock_guard<std::mutex> lock(device->queueLocks->graphicsMutex());
   return device->lastSubmittedSerial.load(std::memory_order_acquire);
}

void veProcessDeletion(VEDeviceInternal *device, const VEDeferredDeletion &deletion)
{
   switch (deletion.type)
   {
   case VEDeferredResourceType::Buffer:
      veDestroyBufferImmediate(device, deletion.bufferAddress);
      break;
   case VEDeferredResourceType::Texture:
      veDestroyTextureImmediate(device, deletion.textureIndex);
      break;
   case VEDeferredResourceType::TextureView:
      veDestroyTextureViewImmediate(device, deletion.textureView);
      break;
   case VEDeferredResourceType::Sampler:
      veDestroySamplerImmediate(device, deletion.samplerIndex);
      break;
   case VEDeferredResourceType::Shader:
      veDestroyShaderImmediate(deletion.shader);
      break;
   case VEDeferredResourceType::ShaderObject:
      if (deletion.shaderObject != VK_NULL_HANDLE)
         veFuncs.vkDestroyShaderEXT(device->device, deletion.shaderObject, nullptr);
      break;
   case VEDeferredResourceType::QueryPool:
      veDestroyQueryPoolImmediate(deletion.queryPool);
      break;
   }
}
} // namespace

void VEDeferredDeletionQueue::enqueueBuffer(VEDeviceInternal *device, VEBufferAddress address)
{
   if (address == VE_INVALID_ADDRESS)
      return;

   {
      std::lock_guard<std::mutex> lock(mutex);
      VEDeferredDeletion deletion{};
      deletion.type = VEDeferredResourceType::Buffer;
      deletion.retireValue = veRetireValue(device);
      deletion.bufferAddress = address;
      pending.push_back(deletion);
   }
   processPending(device);
}

void VEDeferredDeletionQueue::enqueueTexture(VEDeviceInternal *device, VETexture index)
{
   if (index == VE_INVALID_TEXTURE)
      return;

   {
      std::lock_guard<std::mutex> lock(mutex);
      VEDeferredDeletion deletion{};
      deletion.type = VEDeferredResourceType::Texture;
      deletion.retireValue = veRetireValue(device);
      deletion.textureIndex = index;
      pending.push_back(deletion);
   }
   processPending(device);
}

void VEDeferredDeletionQueue::enqueueTextureView(VEDeviceInternal *device, VETextureView view)
{
   if (view == VE_INVALID_TEXTURE_VIEW)
      return;

   {
      std::lock_guard<std::mutex> lock(mutex);
      VEDeferredDeletion deletion{};
      deletion.type = VEDeferredResourceType::TextureView;
      deletion.retireValue = veRetireValue(device);
      deletion.textureView = view;
      pending.push_back(deletion);
   }
   processPending(device);
}

void VEDeferredDeletionQueue::enqueueSampler(VEDeviceInternal *device, VESamplerIndex index)
{
   if (index == VE_INVALID_SAMPLER_INDEX)
      return;

   {
      std::lock_guard<std::mutex> lock(mutex);
      VEDeferredDeletion deletion{};
      deletion.type = VEDeferredResourceType::Sampler;
      deletion.retireValue = veRetireValue(device);
      deletion.samplerIndex = index;
      pending.push_back(deletion);
   }
   processPending(device);
}

void VEDeferredDeletionQueue::enqueueShader(VEDeviceInternal *device, VEShader *shader)
{
   if (!shader)
      return;

   {
      std::lock_guard<std::mutex> lock(mutex);
      VEDeferredDeletion deletion{};
      deletion.type = VEDeferredResourceType::Shader;
      deletion.retireValue = veRetireValue(device);
      deletion.shader = shader;
      pending.push_back(deletion);
   }
   processPending(device);
}

void VEDeferredDeletionQueue::enqueueQueryPool(VEDeviceInternal *device, VEQueryPool *pool)
{
   if (!pool)
      return;

   {
      std::lock_guard<std::mutex> lock(mutex);
      VEDeferredDeletion deletion{};
      deletion.type = VEDeferredResourceType::QueryPool;
      deletion.retireValue = veRetireValue(device);
      deletion.queryPool = pool;
      pending.push_back(deletion);
   }
   processPending(device);
}

void VEDeferredDeletionQueue::enqueueShaderObject(VEDeviceInternal *device, VkShaderEXT shaderObject)
{
   if (shaderObject == VK_NULL_HANDLE)
      return;

   {
      std::lock_guard<std::mutex> lock(mutex);
      VEDeferredDeletion deletion{};
      deletion.type = VEDeferredResourceType::ShaderObject;
      deletion.retireValue = veRetireValue(device);
      deletion.shaderObject = shaderObject;
      pending.push_back(deletion);
   }
   processPending(device);
}

void VEDeferredDeletionQueue::processPending(VEDeviceInternal *device)
{
   if (!device)
      return;

   uint64_t completedValue = 0;
   if (device->retirementTimeline != VK_NULL_HANDLE)
   {
      VkResult result = vkGetSemaphoreCounterValue(device->device, device->retirementTimeline, &completedValue);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to query deferred deletion timeline (VkResult: %d)", result);
         return;
      }
   }

   std::vector<VEDeferredDeletion> toDelete;

   {
      std::lock_guard<std::mutex> lock(mutex);

      // Move items ready for deletion to a separate vector
      auto it = pending.begin();
      while (it != pending.end())
      {
         if (it->retireValue <= completedValue)
         {
            toDelete.push_back(*it);
            it = pending.erase(it);
         }
         else
         {
            ++it;
         }
      }
   }

   // Delete resources outside the lock. The timeline value proves that every
   // earlier VulkEase queue submission has completed.
   for (const auto &deletion : toDelete)
      veProcessDeletion(device, deletion);
}

void VEDeferredDeletionQueue::flush(VEDeviceInternal *device)
{
   if (!device)
      return;

   // Wait for device to be idle before flushing all pending deletions
   if (device->device)
   {
      vkDeviceWaitIdle(device->device);
   }

   std::vector<VEDeferredDeletion> toDelete;

   {
      std::lock_guard<std::mutex> lock(mutex);
      toDelete = std::move(pending);
      pending.clear();
   }

   // Delete all remaining resources
   for (const auto &deletion : toDelete)
      veProcessDeletion(device, deletion);
}

// =============================================================================
// Internal helpers
// =============================================================================

void veAdvanceDeferredDeletions(VEDeviceInternal *device)
{
   if (!device)
      return;

   VEDeferredDeletionQueue *queue = device->deferredDeletionQueue.get();
   if (!queue)
      return;

   queue->processPending(device);
}
