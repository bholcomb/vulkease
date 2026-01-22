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

void VEDeferredDeletionQueue::enqueueBuffer(VEBufferAddress address)
{
   if (address == VE_INVALID_ADDRESS)
      return;

   std::lock_guard<std::mutex> lock(mutex);
   VEDeferredDeletion deletion{};
   deletion.type = VEDeferredResourceType::Buffer;
   deletion.frameIndex = currentFrame;
   deletion.bufferAddress = address;
   pending.push_back(deletion);
}

void VEDeferredDeletionQueue::enqueueTexture(VETextureIndex index)
{
   if (index == VE_INVALID_TEXTURE_INDEX)
      return;

   std::lock_guard<std::mutex> lock(mutex);
   VEDeferredDeletion deletion{};
   deletion.type = VEDeferredResourceType::Texture;
   deletion.frameIndex = currentFrame;
   deletion.textureIndex = index;
   pending.push_back(deletion);
}

void VEDeferredDeletionQueue::enqueueSampler(VESamplerIndex index)
{
   if (index == VE_INVALID_SAMPLER_INDEX)
      return;

   std::lock_guard<std::mutex> lock(mutex);
   VEDeferredDeletion deletion{};
   deletion.type = VEDeferredResourceType::Sampler;
   deletion.frameIndex = currentFrame;
   deletion.samplerIndex = index;
   pending.push_back(deletion);
}

void VEDeferredDeletionQueue::enqueueShader(VEShader *shader)
{
   if (!shader)
      return;

   std::lock_guard<std::mutex> lock(mutex);
   VEDeferredDeletion deletion{};
   deletion.type = VEDeferredResourceType::Shader;
   deletion.frameIndex = currentFrame;
   deletion.shader = shader;
   pending.push_back(deletion);
}

void VEDeferredDeletionQueue::advanceFrame()
{
   std::lock_guard<std::mutex> lock(mutex);
   currentFrame++;
}

void VEDeferredDeletionQueue::processPending(VEDeviceInternal *device)
{
   if (!device)
      return;

   std::vector<VEDeferredDeletion> toDelete;

   {
      std::lock_guard<std::mutex> lock(mutex);

      // Move items ready for deletion to a separate vector
      auto it = pending.begin();
      while (it != pending.end())
      {
         if (currentFrame - it->frameIndex >= kFrameDelay)
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

   // Delete resources outside the lock
   for (const auto &deletion : toDelete)
   {
      switch (deletion.type)
      {
      case VEDeferredResourceType::Buffer:
         veDestroyBufferImmediate(device, deletion.bufferAddress);
         break;

      case VEDeferredResourceType::Texture:
         veDestroyTextureImmediate(device, deletion.textureIndex);
         break;

      case VEDeferredResourceType::Sampler:
         veDestroySamplerImmediate(device, deletion.samplerIndex);
         break;

      case VEDeferredResourceType::Shader:
         veDestroyShaderImmediate(deletion.shader);
         break;
      }
   }
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
   {
      switch (deletion.type)
      {
      case VEDeferredResourceType::Buffer:
         veDestroyBufferImmediate(device, deletion.bufferAddress);
         break;

      case VEDeferredResourceType::Texture:
         veDestroyTextureImmediate(device, deletion.textureIndex);
         break;

      case VEDeferredResourceType::Sampler:
         veDestroySamplerImmediate(device, deletion.samplerIndex);
         break;

      case VEDeferredResourceType::Shader:
         veDestroyShaderImmediate(deletion.shader);
         break;
      }
   }
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

   queue->advanceFrame();
   queue->processPending(device);
}
