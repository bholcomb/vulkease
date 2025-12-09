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

void VEDeferredDeletionQueue::enqueueShader(VEShader* shader)
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

void VEDeferredDeletionQueue::processPending(VEDeviceInternal* device)
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
   for (const auto& deletion : toDelete)
   {
      switch (deletion.type)
      {
         case VEDeferredResourceType::Buffer:
            veDestroyBuffer(reinterpret_cast<VEDevice*>(device), deletion.bufferAddress);
            break;
            
         case VEDeferredResourceType::Texture:
            veDestroyTexture(reinterpret_cast<VEDevice*>(device), deletion.textureIndex);
            break;
            
         case VEDeferredResourceType::Sampler:
            veDestroySampler(reinterpret_cast<VEDevice*>(device), deletion.samplerIndex);
            break;
            
         case VEDeferredResourceType::Shader:
            veDestroyShader(deletion.shader);
            break;
      }
   }
}

void VEDeferredDeletionQueue::flush(VEDeviceInternal* device)
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
   for (const auto& deletion : toDelete)
   {
      switch (deletion.type)
      {
         case VEDeferredResourceType::Buffer:
            veDestroyBuffer(reinterpret_cast<VEDevice*>(device), deletion.bufferAddress);
            break;
            
         case VEDeferredResourceType::Texture:
            veDestroyTexture(reinterpret_cast<VEDevice*>(device), deletion.textureIndex);
            break;
            
         case VEDeferredResourceType::Sampler:
            veDestroySampler(reinterpret_cast<VEDevice*>(device), deletion.samplerIndex);
            break;
            
         case VEDeferredResourceType::Shader:
            veDestroyShader(deletion.shader);
            break;
      }
   }
}

// =============================================================================
// API Functions for Deferred Deletion
// =============================================================================

extern "C" void veEnqueueBufferDeletion(VEDevice* device, VEBufferAddress address)
{
   if (!device || address == VE_INVALID_ADDRESS)
      return;
      
   VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
   if (deviceInternal->deferredDeletionQueue)
   {
      deviceInternal->deferredDeletionQueue->enqueueBuffer(address);
   }
   else
   {
      // Fallback to immediate deletion if queue not initialized
      veDestroyBuffer(device, address);
   }
}

extern "C" void veEnqueueTextureDeletion(VEDevice* device, VETextureIndex index)
{
   if (!device || index == VE_INVALID_TEXTURE_INDEX)
      return;
      
   VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
   if (deviceInternal->deferredDeletionQueue)
   {
      deviceInternal->deferredDeletionQueue->enqueueTexture(index);
   }
   else
   {
      veDestroyTexture(device, index);
   }
}

extern "C" void veEnqueueSamplerDeletion(VEDevice* device, VESamplerIndex index)
{
   if (!device || index == VE_INVALID_SAMPLER_INDEX)
      return;
      
   VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
   if (deviceInternal->deferredDeletionQueue)
   {
      deviceInternal->deferredDeletionQueue->enqueueSampler(index);
   }
   else
   {
      veDestroySampler(device, index);
   }
}

extern "C" void veEnqueueShaderDeletion(VEShader* shader)
{
   if (!shader)
      return;
   
   VEShaderInternal* shaderInternal = reinterpret_cast<VEShaderInternal*>(shader);
   VEDeviceInternal* deviceInternal = shaderInternal->device;
   
   if (deviceInternal && deviceInternal->deferredDeletionQueue)
   {
      deviceInternal->deferredDeletionQueue->enqueueShader(shader);
   }
   else
   {
      veDestroyShader(shader);
   }
}

extern "C" void veProcessDeferredDeletions(VEDevice* device)
{
   if (!device)
      return;
      
   VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
   if (deviceInternal->deferredDeletionQueue)
   {
      deviceInternal->deferredDeletionQueue->processPending(deviceInternal);
   }
}

extern "C" void veFlushDeferredDeletions(VEDevice* device)
{
   if (!device)
      return;
      
   VEDeviceInternal* deviceInternal = reinterpret_cast<VEDeviceInternal*>(device);
   if (deviceInternal->deferredDeletionQueue)
   {
      deviceInternal->deferredDeletionQueue->flush(deviceInternal);
   }
}


