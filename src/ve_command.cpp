/**
 * @file ve_command.c
 * @brief Command Buffer and Rendering Implementation
 */

#include "ve_internal.h"

#include <functional>
#include <mutex>
#include <thread>
#include <vector>

VECommandPool *VEThreadCommandPools::acquire(VEDeviceInternal *device, VECommandPoolKind kind)
{
   if (!device)
   {
      veSetError("Device cannot be NULL when acquiring a command pool");
      return nullptr;
   }

   const std::thread::id threadId = std::this_thread::get_id();

   std::lock_guard<std::mutex> lock(mutex);
   auto &setPtr = perThread[threadId];
   if (!setPtr)
   {
      setPtr = std::make_unique<PoolSet>();
   }

   std::unique_ptr<VECommandPool> *slot = nullptr;
   uint32_t familyIndex = 0;

   switch (kind)
   {
   case VECommandPoolKind::Graphics:
      slot = &setPtr->graphics;
      familyIndex = device->queueFamilies.graphicsFamily;
      break;
   case VECommandPoolKind::Compute:
      slot = &setPtr->compute;
      familyIndex = device->queueFamilies.computeFamily;
      break;
   case VECommandPoolKind::Transfer:
      slot = &setPtr->transfer;
      familyIndex = device->queueFamilies.transferFamily;
      break;
   }

   if (!slot)
   {
      veSetError("Invalid command pool kind requested");
      return nullptr;
   }

   if (!slot->get())
   {
      std::unique_ptr<VECommandPool> newPool(new (std::nothrow) VECommandPool());
      if (!newPool)
      {
         veSetError("Failed to allocate command pool");
         return nullptr;
      }

      if (!newPool->initialize(device, familyIndex))
      {
         return nullptr;
      }

      *slot = std::move(newPool);
   }

   return slot->get();
}

void VEThreadCommandPools::destroyAll(VEDeviceInternal *device)
{
   (void)device;

   std::lock_guard<std::mutex> lock(mutex);
   perThread.clear();
   inFlight.clear();
}

void VEThreadCommandPools::trackInFlight(VkFence fence, VECommandBufferInternal *cmd)
{
   if (!fence || !cmd)
   {
      return;
   }

   std::lock_guard<std::mutex> lock(mutex);
   inFlight[fence].push_back(cmd);
}

void VEThreadCommandPools::reclaimInFlight(VEDeviceInternal *device)
{
   if (!device)
   {
      return;
   }

   // Move completed command buffers out of the shared map while holding the
   // mutex briefly, then do the heavier reset/release work without the mutex to
   // avoid blocking other threads (and to avoid lock-order inversions).
   std::vector<std::vector<VECommandBufferInternal *>> toReclaim;

   {
      std::lock_guard<std::mutex> lock(mutex);
      for (auto it = inFlight.begin(); it != inFlight.end();)
      {
         VkFence fence = it->first;
         VkResult status = vkGetFenceStatus(device->device, fence);
         if (status == VK_NOT_READY)
         {
            ++it;
            continue;
         }
         if (status != VK_SUCCESS)
         {
            ++it;
            continue;
         }

         toReclaim.push_back(std::move(it->second));
         it = inFlight.erase(it);
      }
   }

   for (auto &cmdList : toReclaim)
   {
      for (VECommandBufferInternal *cmd : cmdList)
      {
         if (!cmd || !cmd->commandPool)
            continue;
         // Take the pool's allocation lock before modifying command buffer state.
         // This prevents a race where allocate() sees fenceActive=false but
         // commandBufferInUse=true, then checks the wrong fence (inFlightFence
         // instead of the actual submission fence) and reallocates a buffer
         // that the GPU is still using.
         VECommandPool *pool = cmd->commandPool;
         if (pool->allocationLock)
         {
            std::lock_guard<std::mutex> poolLock(*pool->allocationLock);
            cmd->clearFenceTracking();
            pool->finishRelease(*cmd);
         }
         else
         {
            cmd->clearFenceTracking();
            pool->finishRelease(*cmd);
         }
      }
   }
}

namespace
{
VEResult veEnsureBufferIdle(VECommandBufferInternal *cmd)
{
   if (!cmd || !cmd->device)
   {
      veSetError("Invalid command buffer state");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkFence fence = cmd->currentFence();
   if (fence != VK_NULL_HANDLE)
   {
      VkResult status = vkGetFenceStatus(cmd->device->device, fence);
      if (status == VK_NOT_READY)
      {
         const size_t threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
         veSetError("Command buffer still in-flight (cb=%p index=%u pool=%p fence=%p thread=%zu)",
                    (void *)cmd->commandBuffer, cmd->index, (void *)cmd->commandPool, (void *)fence, threadHash);
         return VE_ERROR_INVALID_PARAMETER;
      }
      else if (status != VK_SUCCESS)
      {
         veSetError("Failed to query command buffer fence status (VkResult: %d)", status);
         return VE_ERROR_UNKNOWN;
      }
   }

   if (cmd->fenceActive)
   {
      cmd->clearFenceTracking();
   }

   return VE_SUCCESS;
}

VEResult veEnsureCommandBufferLevel(VECommandBufferInternal *cmd, VkCommandBufferLevel desiredLevel)
{
   if (!cmd || !cmd->commandPool || !cmd->device)
   {
      veSetError("Invalid command buffer state");
      return VE_ERROR_INVALID_PARAMETER;
   }

   const bool wantsSecondary = (desiredLevel == VK_COMMAND_BUFFER_LEVEL_SECONDARY);
   if (cmd->commandBuffer != VK_NULL_HANDLE && cmd->isSecondary == wantsSecondary)
   {
      return VE_SUCCESS;
   }

   if (cmd->commandBuffer != VK_NULL_HANDLE && cmd->fenceActive)
   {
      const size_t threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
      veSetError("Attempt to reallocate in-flight command buffer (cb=%p index=%u pool=%p fence=%p thread=%zu)",
                 (void *)cmd->commandBuffer, cmd->index, (void *)cmd->commandPool, (void *)cmd->currentFence(),
                 threadHash);
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = cmd->device;
   if (!deviceInternal->device || cmd->commandPool->commandPool == VK_NULL_HANDLE)
   {
      veSetError("Invalid device or command pool for command buffer allocation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (cmd->commandBuffer != VK_NULL_HANDLE)
   {
      vkFreeCommandBuffers(deviceInternal->device, cmd->commandPool->commandPool, 1, &cmd->commandBuffer);
      cmd->commandBuffer = VK_NULL_HANDLE;
   }

   VkCommandBufferAllocateInfo allocInfo{};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.commandPool = cmd->commandPool->commandPool;
   allocInfo.level = desiredLevel;
   allocInfo.commandBufferCount = 1;

   VkResult vkResult = vkAllocateCommandBuffers(deviceInternal->device, &allocInfo, &cmd->commandBuffer);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to allocate command buffer (VkResult: %d)", vkResult);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   cmd->isSecondary = wantsSecondary;
   return VE_SUCCESS;
}
} // namespace

// =============================================================================
// Command Buffer Operations
// =============================================================================

VEResult veBeginCommandBuffer(VEDevice *device, VECommandBuffer **outCmd)
{
   if (!outCmd)
   {
      veSetError("outCmd cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   VECommandBufferInternal *cmd;
   VECommandPool *pool = deviceInternal->threadCommandPools.acquire(deviceInternal, VECommandPoolKind::Graphics);
   if (!pool)
   {
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VEResult result = veAllocateCommandBuffer(deviceInternal, pool, &cmd);
   if (result != VE_SUCCESS)
   {
      return result;
   }

   if (veEnsureBufferIdle(cmd) != VE_SUCCESS)
   {
      veFreeCommandBuffer(cmd);
      return VE_ERROR_UNKNOWN;
   }

   VEResult levelResult = veEnsureCommandBufferLevel(cmd, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
   if (levelResult != VE_SUCCESS)
   {
      veFreeCommandBuffer(cmd);
      return levelResult;
   }

   VkResult resetResult = vkResetCommandBuffer(cmd->commandBuffer, 0);
   if (resetResult != VK_SUCCESS)
   {
      VkFence fence = cmd->currentFence();
      VkResult fenceStatus = fence ? vkGetFenceStatus(cmd->device->device, fence) : VK_SUCCESS;
      veSetError(
          "Failed to reset primary command buffer (VkResult: %d) cb=%p index=%u fence=%p fenceStatus=%d fenceActive=%d",
          resetResult, (void *)cmd->commandBuffer, cmd->index, (void *)fence, fenceStatus, cmd->fenceActive ? 1 : 0);
      veFreeCommandBuffer(cmd);
      return VE_ERROR_UNKNOWN;
   }

   VkCommandBufferBeginInfo beginInfo{};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   VkResult vkResult = vkBeginCommandBuffer(cmd->commandBuffer, &beginInfo);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to begin command buffer (VkResult: %d)", vkResult);
      veFreeCommandBuffer(cmd);
      return VE_ERROR_UNKNOWN;
   }

   cmd->isRecording = true;
   cmd->isOneTime = true;

   VkDescriptorSet descriptorSets[2] = {deviceInternal->textureDescriptorSet, deviceInternal->samplerDescriptorSet};
   vkCmdBindDescriptorSets(cmd->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           deviceInternal->globalGraphicsPipelineLayout, 0, 2, descriptorSets, 0, NULL);
   vkCmdBindDescriptorSets(cmd->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                           deviceInternal->globalComputePipelineLayout, 0, 2, descriptorSets, 0, NULL);
   deviceInternal->frameStats.descriptorBinds += 2;

   *outCmd = (VECommandBuffer *)cmd;
   return VE_SUCCESS;
}

VEResult veEndCommandBuffer(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("Command buffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   if (!internal->isRecording)
   {
      veSetError("Command buffer is not recording");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkResult vkResult = vkEndCommandBuffer(internal->commandBuffer);
   if (vkResult != VK_SUCCESS)
   {
      veSetError("Failed to end command buffer (VkResult: %d)", vkResult);
      return VE_ERROR_UNKNOWN;
   }

   internal->isRecording = false;
   return VE_SUCCESS;
}

VEResult veSubmitCommandBuffer(VECommandBuffer *cmd, const VESubmitInfo *submitInfo)
{
   if (!cmd)
   {
      veSetError("Command buffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (!internal->device)
   {
      veSetError("Command buffer device is not initialized");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VESubmitInfo info{};
   if (submitInfo)
   {
      info = *submitInfo;
   }

   if (internal->isRecording)
   {
      VEResult endResult = veEndCommandBuffer(cmd);
      if (endResult != VE_SUCCESS)
      {
         return endResult;
      }
   }

   if (info.waitSemaphoreCount > 0 && !info.waitSemaphores)
   {
      veSetError("Wait semaphore array cannot be NULL when waitSemaphoreCount > 0");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (info.signalSemaphoreCount > 0 && !info.signalSemaphores)
   {
      veSetError("Signal semaphore array cannot be NULL when signalSemaphoreCount > 0");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkFence fence = info.fence;
   bool usingInternalFence = (fence == VK_NULL_HANDLE);

   if (usingInternalFence)
   {
      fence = internal->inFlightFence;
      if (fence == VK_NULL_HANDLE)
      {
         VkFenceCreateInfo fenceInfo{};
         fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
         fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
         VkResult fenceResult = vkCreateFence(internal->device->device, &fenceInfo, NULL, &fence);
         if (fenceResult != VK_SUCCESS)
         {
            veSetError("Failed to create command buffer fence (VkResult: %d)", fenceResult);
            return VE_ERROR_OUT_OF_MEMORY;
         }
         internal->inFlightFence = fence;
      }
   }

   // Flush sparse bindings before command submission. Keep this blocking until
   // sparse completion is represented by per-submit timeline values; reusing
   // one binary semaphore across concurrent submissions is not safe.
   bool hasPendingSparseBinds = internal->device->hasPendingSparseBinds();
   if (hasPendingSparseBinds)
   {
      VEResult sparseFlushResult = internal->device->flushPendingSparseBinds(true);
      if (sparseFlushResult != VE_SUCCESS)
      {
         veSetError("Failed to flush pending sparse bindings");
         return sparseFlushResult;
      }
      hasPendingSparseBinds = false;
   }

   std::vector<VkSemaphoreSubmitInfo> waitInfos;

   // If we flushed sparse bindings, we need to wait on the sparse bind semaphore
   if (hasPendingSparseBinds && internal->device->sparseBindSemaphore != VK_NULL_HANDLE)
   {
      VkSemaphoreSubmitInfo sparseWaitInfo{};
      sparseWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      sparseWaitInfo.semaphore = internal->device->sparseBindSemaphore;
      sparseWaitInfo.value = 0;
      sparseWaitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      sparseWaitInfo.deviceIndex = 0;
      waitInfos.push_back(sparseWaitInfo);
   }

   if (info.waitSemaphoreCount > 0)
   {
      size_t offset = waitInfos.size();
      waitInfos.resize(offset + info.waitSemaphoreCount);
      for (uint32_t i = 0; i < info.waitSemaphoreCount; ++i)
      {
         if (info.waitSemaphores[i] == VK_NULL_HANDLE)
         {
            veSetError("Wait semaphore %u is NULL", i);
            return VE_ERROR_INVALID_PARAMETER;
         }

         VkSemaphoreSubmitInfo &waitInfo = waitInfos[offset + i];
         waitInfo = {};
         waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
         waitInfo.semaphore = info.waitSemaphores[i];
         waitInfo.value = info.waitSemaphoreValues ? info.waitSemaphoreValues[i] : 0;
         waitInfo.stageMask =
             info.waitStageMasks ? info.waitStageMasks[i] : (VkPipelineStageFlags2)VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
         waitInfo.deviceIndex = 0;
      }
   }

   std::vector<VkSemaphoreSubmitInfo> signalInfos;
   if (info.signalSemaphoreCount > 0)
   {
      signalInfos.resize(info.signalSemaphoreCount);
      for (uint32_t i = 0; i < info.signalSemaphoreCount; ++i)
      {
         if (info.signalSemaphores[i] == VK_NULL_HANDLE)
         {
            veSetError("Signal semaphore %u is NULL", i);
            return VE_ERROR_INVALID_PARAMETER;
         }

         VkSemaphoreSubmitInfo &signalInfo = signalInfos[i];
         signalInfo = {};
         signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
         signalInfo.semaphore = info.signalSemaphores[i];
         signalInfo.value = info.signalSemaphoreValues ? info.signalSemaphoreValues[i] : 0;
         signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
         signalInfo.deviceIndex = 0;
      }
   }

   if (internal->device->retirementTimeline != VK_NULL_HANDLE)
   {
      VkSemaphoreSubmitInfo retirementSignal{};
      retirementSignal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      retirementSignal.semaphore = internal->device->retirementTimeline;
      retirementSignal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      signalInfos.push_back(retirementSignal);
   }

   VkCommandBufferSubmitInfo commandBufferInfo{};
   commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
   commandBufferInfo.commandBuffer = internal->commandBuffer;
   commandBufferInfo.deviceMask = 0;

   VkSubmitInfo2 submitInfo2{};
   submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
   submitInfo2.commandBufferInfoCount = 1;
   submitInfo2.pCommandBufferInfos = &commandBufferInfo;
   submitInfo2.waitSemaphoreInfoCount = static_cast<uint32_t>(waitInfos.size());
   submitInfo2.pWaitSemaphoreInfos = waitInfos.empty() ? NULL : waitInfos.data();
   submitInfo2.signalSemaphoreInfoCount = static_cast<uint32_t>(signalInfos.size());
   submitInfo2.pSignalSemaphoreInfos = signalInfos.empty() ? NULL : signalInfos.data();

   VEDeviceQueueLocks *locks = internal->device->queueLocks.get();
   if (!locks)
   {
      veSetError("Device queue locks not initialized");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkResult submitResult = VK_SUCCESS;
   uint64_t submissionSerial = 0;
   {
      std::lock_guard<std::mutex> lock(locks->graphicsMutex());
      if (fence != VK_NULL_HANDLE)
      {
         VkResult fenceReset = vkResetFences(internal->device->device, 1, &fence);
         if (fenceReset != VK_SUCCESS)
         {
            veSetError("Failed to reset command buffer fence (VkResult: %d)", fenceReset);
            return VE_ERROR_UNKNOWN;
         }
      }
      if (internal->device->retirementTimeline != VK_NULL_HANDLE)
      {
         submissionSerial = ++internal->device->nextSubmissionSerial;
         signalInfos.back().value = submissionSerial;
      }
      submitResult = vkQueueSubmit2(internal->device->graphicsQueue, 1, &submitInfo2, fence);
      if (submitResult == VK_SUCCESS && submissionSerial != 0)
         internal->device->lastSubmittedSerial.store(submissionSerial, std::memory_order_release);
   }

   if (submitResult != VK_SUCCESS)
   {
      // A failed submit never signals its fence. Replace an internally-owned
      // fence with a signaled one so this command buffer cannot hang forever
      // when it is next reclaimed.
      if (usingInternalFence)
      {
         vkDestroyFence(internal->device->device, internal->inFlightFence, NULL);
         internal->inFlightFence = VK_NULL_HANDLE;
         VkFenceCreateInfo fenceInfo{};
         fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
         fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
         vkCreateFence(internal->device->device, &fenceInfo, NULL, &internal->inFlightFence);
      }
      veSetError("Failed to submit command buffer (VkResult: %d)", submitResult);
      return VE_ERROR_UNKNOWN;
   }

   if (info.waitForCompletion && fence != VK_NULL_HANDLE)
   {
      VkResult waitResult = vkWaitForFences(internal->device->device, 1, &fence, VK_TRUE, UINT64_MAX);
      if (waitResult != VK_SUCCESS)
      {
         veSetError("Failed to wait for command buffer completion (VkResult: %d)", waitResult);
         return VE_ERROR_OUT_OF_MEMORY;
      }

      internal->clearFenceTracking();
      veFreeCommandBuffer(internal);
      return VE_SUCCESS;
   }

   if (fence != VK_NULL_HANDLE)
   {
      internal->markFenceActive(fence);
      internal->device->threadCommandPools.trackInFlight(fence, internal);

      // Propagate the same fence to any secondary command buffers executed by
      // this primary so they are not reset/reused until the primary fence signals.
      for (VECommandBufferInternal *secondary : internal->executedSecondaries)
      {
         if (!secondary)
            continue;
         secondary->markFenceActive(fence);
         secondary->device->threadCommandPools.trackInFlight(fence, secondary);
      }
   }

   internal->device->threadCommandPools.reclaimInFlight(internal->device);
   veAdvanceDeferredDeletions(internal->device);
   internal->executedSecondaries.clear();

   return VE_SUCCESS;
}

VEResult veResetCommandBuffer(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("Command buffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (!internal->device)
   {
      veSetError("Command buffer device is not initialized");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (internal->isRecording)
   {
      veSetError("Cannot reset command buffer while recording");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (internal->fenceActive)
   {
      VkFence fence = internal->currentFence();
      if (fence != VK_NULL_HANDLE)
      {
         VkResult status = vkGetFenceStatus(internal->device->device, fence);
         if (status == VK_NOT_READY)
         {
            veSetError("Command buffer GPU work is still in-flight");
            return VE_ERROR_INVALID_PARAMETER;
         }
         else if (status != VK_SUCCESS)
         {
            veSetError("Failed to query command buffer fence status (VkResult: %d)", status);
            return VE_ERROR_UNKNOWN;
         }
      }
   }

   VkResult resetResult = vkResetCommandBuffer(internal->commandBuffer, 0);
   if (resetResult != VK_SUCCESS)
   {
      veSetError("Failed to reset command buffer (VkResult: %d)", resetResult);
      return VE_ERROR_UNKNOWN;
   }

   internal->resetState();
   return VE_SUCCESS;
}

extern "C" VEResult veReleaseCommandBuffer(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   veFreeCommandBuffer(internal);
   return VE_SUCCESS;
}

extern "C" VEResult vePopulateSecondaryDescFromRenderTarget(VEDevice *device, const VERenderTarget *renderTarget,
                                                            VESecondaryCommandBufferDesc *desc)
{
   if (!device || !renderTarget || !desc)
   {
      veSetError("vePopulateSecondaryDescFromRenderTarget: device, renderTarget, and desc must be non-null");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (renderTarget->colorAttachmentCount > 8)
   {
      veSetError("vePopulateSecondaryDescFromRenderTarget: colorAttachmentCount exceeds maximum of 8");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VkSampleCountFlags sampleMask = 0;

   auto setSamples = [&](VkSampleCountFlags samples) -> bool
   {
      VkSampleCountFlagBits resolved = samples ? static_cast<VkSampleCountFlagBits>(samples) : VK_SAMPLE_COUNT_1_BIT;
      if (sampleMask == 0)
      {
         sampleMask = resolved;
         return true;
      }
      if (sampleMask != resolved)
      {
         veSetError("vePopulateSecondaryDescFromRenderTarget: attachments must use a consistent sample count");
         return false;
      }
      return true;
   };

   auto formatHasStencil = [](VkFormat format) -> bool
   {
      return format == VK_FORMAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT ||
             format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
   };

   // Color attachments
   desc->colorAttachmentCount = renderTarget->colorAttachmentCount;
   for (uint32_t i = 0; i < renderTarget->colorAttachmentCount; ++i)
   {
      VETextureViewInternal *view = deviceInternal->getTextureView(renderTarget->attachments[i].view);
      VETextureInternal *tex = view ? deviceInternal->getTexture(view->texture) : nullptr;
      if (!view || !tex)
      {
         veSetError("vePopulateSecondaryDescFromRenderTarget: invalid texture index %u for color attachment %u",
                    renderTarget->attachments[i].view, i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      desc->colorAttachmentFormats[i] = view->format;
      if (!setSamples(tex->sampleCount))
      {
         return VE_ERROR_INVALID_PARAMETER;
      }
   }

   // Depth attachment
   desc->depthAttachmentFormat = VK_FORMAT_UNDEFINED;
   desc->stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
   if (renderTarget->hasDepthAttachment)
   {
      const VERenderTargetAttachment *depthAttachment = &renderTarget->attachments[VE_DEPTH_ATTACHMENT_INDEX];
      VETextureViewInternal *view = deviceInternal->getTextureView(depthAttachment->view);
      VETextureInternal *tex = view ? deviceInternal->getTexture(view->texture) : nullptr;
      if (!view || !tex)
      {
         veSetError("vePopulateSecondaryDescFromRenderTarget: invalid depth attachment texture index %u",
                    depthAttachment->view);
         return VE_ERROR_INVALID_PARAMETER;
      }

      desc->depthAttachmentFormat = view->format;
      if (!setSamples(tex->sampleCount))
      {
         return VE_ERROR_INVALID_PARAMETER;
      }

      if (formatHasStencil(view->format))
      {
         desc->stencilAttachmentFormat = view->format;
      }
   }

   // Stencil attachment (explicit)
   if (renderTarget->hasStencilAttachment)
   {
      const VERenderTargetAttachment *stencilAttachment = &renderTarget->attachments[VE_STENCIL_ATTACHMENT_INDEX];
      VETextureViewInternal *view = deviceInternal->getTextureView(stencilAttachment->view);
      VETextureInternal *tex = view ? deviceInternal->getTexture(view->texture) : nullptr;
      if (!view || !tex)
      {
         veSetError("vePopulateSecondaryDescFromRenderTarget: invalid stencil attachment texture index %u",
                    stencilAttachment->view);
         return VE_ERROR_INVALID_PARAMETER;
      }

      desc->stencilAttachmentFormat = view->format;
      if (!setSamples(tex->sampleCount))
      {
         return VE_ERROR_INVALID_PARAMETER;
      }
   }

   if (sampleMask == 0)
   {
      sampleMask = VK_SAMPLE_COUNT_1_BIT;
   }

   desc->rasterizationSamples = sampleMask;
   desc->renderingFlags = renderTarget->flags;

   if (desc->usageFlags == 0)
   {
      desc->usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
   }

   // Copy render area for veApplyRenderState defaults
   desc->renderAreaX = renderTarget->renderAreaX;
   desc->renderAreaY = renderTarget->renderAreaY;
   desc->renderAreaWidth = renderTarget->renderAreaWidth;
   desc->renderAreaHeight = renderTarget->renderAreaHeight;

   return VE_SUCCESS;
}

extern "C" VEResult veBeginSecondaryRecording(VECommandBuffer *cmd, const VESecondaryCommandBufferDesc *desc)
{
   if (!cmd)
   {
      veSetError("veBeginSecondaryRecording: cmd cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (!internal->device)
   {
      veSetError("veBeginSecondaryRecording: command buffer device is null");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!internal->isSecondary)
   {
      veSetError("veBeginSecondaryRecording: command buffer is not secondary");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (internal->isRecording)
   {
      veSetError("veBeginSecondaryRecording: command buffer is already recording");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Ensure GPU is not still using this buffer.
   if (veEnsureBufferIdle(internal) != VE_SUCCESS)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Ensure level is secondary (defensive).
   if (veEnsureCommandBufferLevel(internal, VK_COMMAND_BUFFER_LEVEL_SECONDARY) != VE_SUCCESS)
   {
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkCommandBufferUsageFlags usageFlags =
       (desc && desc->usageFlags != 0)
           ? desc->usageFlags
           : static_cast<VkCommandBufferUsageFlags>(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                                                    VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

   VkCommandBufferBeginInfo beginInfo{};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = usageFlags;

   const bool occlusionEnable = desc ? desc->occlusionQueryEnable : false;
   const VkQueryControlFlags occlusionFlags = desc ? desc->occlusionQueryFlags : 0;
   const uint32_t viewMask = desc ? desc->viewMask : 0;

   VkCommandBufferInheritanceInfo inheritanceInfo{};
   inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
   inheritanceInfo.renderPass = VK_NULL_HANDLE;
   inheritanceInfo.subpass = 0;
   inheritanceInfo.framebuffer = VK_NULL_HANDLE;
   inheritanceInfo.occlusionQueryEnable = occlusionEnable ? VK_TRUE : VK_FALSE;
   inheritanceInfo.queryFlags = occlusionEnable ? occlusionFlags : 0;
   inheritanceInfo.pipelineStatistics = 0;

   VkCommandBufferInheritanceRenderingInfo renderingInheritance{};
   renderingInheritance.flags = desc ? desc->renderingFlags : 0;
   std::vector<VkFormat> derivedColorFormats;
   uint32_t colorAttachmentCount = desc ? desc->colorAttachmentCount : 0;
   if (colorAttachmentCount > 8)
   {
      veSetError("veBeginSecondaryRecording: colorAttachmentCount exceeds maximum supported attachments (8)");
      return VE_ERROR_INVALID_PARAMETER;
   }

   const VkFormat *colorFormatsPtr = nullptr;
   if (colorAttachmentCount > 0)
   {
      if (!desc)
      {
         veSetError("veBeginSecondaryRecording: colorAttachmentFormats must be provided when colorAttachmentCount > 0");
         return VE_ERROR_INVALID_PARAMETER;
      }
      derivedColorFormats.assign(desc->colorAttachmentFormats, desc->colorAttachmentFormats + colorAttachmentCount);
      colorFormatsPtr = derivedColorFormats.data();
   }

   VkFormat depthFormat = desc ? desc->depthAttachmentFormat : VK_FORMAT_UNDEFINED;
   VkFormat stencilFormat = desc ? desc->stencilAttachmentFormat : VK_FORMAT_UNDEFINED;
   VkSampleCountFlags userSamples = desc ? desc->rasterizationSamples : 0;
   VkSampleCountFlagBits rasterizationSamples =
       userSamples ? static_cast<VkSampleCountFlagBits>(userSamples) : VK_SAMPLE_COUNT_1_BIT;

   renderingInheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
   renderingInheritance.viewMask = viewMask;
   renderingInheritance.colorAttachmentCount = colorAttachmentCount;
   renderingInheritance.pColorAttachmentFormats = colorFormatsPtr;
   renderingInheritance.depthAttachmentFormat = depthFormat;
   renderingInheritance.stencilAttachmentFormat = stencilFormat;
   renderingInheritance.rasterizationSamples = rasterizationSamples;
   inheritanceInfo.pNext = &renderingInheritance;

   beginInfo.pInheritanceInfo = &inheritanceInfo;

   VkResult resetResult = vkResetCommandBuffer(internal->commandBuffer, 0);
   if (resetResult != VK_SUCCESS)
   {
      VkFence fence = internal->currentFence();
      VkResult fenceStatus = fence ? vkGetFenceStatus(internal->device->device, fence) : VK_SUCCESS;
      veSetError("veBeginSecondaryRecording: failed to reset command buffer (VkResult: %d) cb=%p fence=%p "
                 "fenceStatus=%d fenceActive=%d",
                 resetResult, (void *)internal->commandBuffer, (void *)fence, fenceStatus,
                 internal->fenceActive ? 1 : 0);
      return VE_ERROR_UNKNOWN;
   }

   VkResult beginResult = vkBeginCommandBuffer(internal->commandBuffer, &beginInfo);
   if (beginResult != VK_SUCCESS)
   {
      veSetError("veBeginSecondaryRecording: failed to begin command buffer (VkResult: %d)", beginResult);
      return VE_ERROR_UNKNOWN;
   }

   internal->isRecording = true;
   internal->isOneTime = (usageFlags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != 0;
   internal->isSecondary = true;

   VkDescriptorSet descriptorSets[2] = {internal->device->textureDescriptorSet, internal->device->samplerDescriptorSet};
   vkCmdBindDescriptorSets(internal->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           internal->device->globalGraphicsPipelineLayout, 0, 2, descriptorSets, 0, nullptr);

   return VE_SUCCESS;
}

VEResult veBeginSecondaryCommandBuffer(VEDevice *device, const VESecondaryCommandBufferDesc *desc,
                                       VECommandBuffer **outCmd)
{
   if (!outCmd)
   {
      veSetError("outCmd cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkCommandBufferUsageFlags usageFlags =
       (desc && desc->usageFlags != 0)
           ? desc->usageFlags
           : static_cast<VkCommandBufferUsageFlags>(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                                                    VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

   const bool shouldBegin = !desc || desc->beginRecording;
   const bool occlusionEnable = desc ? desc->occlusionQueryEnable : false;
   const VkQueryControlFlags occlusionFlags = desc ? desc->occlusionQueryFlags : 0;
   const uint32_t viewMask = desc ? desc->viewMask : 0;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   VECommandBufferInternal *cmd = nullptr;
   VECommandPool *pool = deviceInternal->threadCommandPools.acquire(deviceInternal, VECommandPoolKind::Graphics);
   if (!pool)
   {
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VEResult allocResult = veAllocateCommandBuffer(deviceInternal, pool, &cmd);
   if (allocResult != VE_SUCCESS)
   {
      return allocResult;
   }

   if (veEnsureBufferIdle(cmd) != VE_SUCCESS)
   {
      veFreeCommandBuffer(cmd);
      return VE_ERROR_UNKNOWN;
   }

   VEResult levelResult = veEnsureCommandBufferLevel(cmd, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
   if (levelResult != VE_SUCCESS)
   {
      veFreeCommandBuffer(cmd);
      return levelResult;
   }

   VkCommandBufferBeginInfo beginInfo{};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = usageFlags;

   VkCommandBufferInheritanceInfo inheritanceInfo{};
   inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
   inheritanceInfo.renderPass = VK_NULL_HANDLE;
   inheritanceInfo.subpass = 0;
   inheritanceInfo.framebuffer = VK_NULL_HANDLE;
   inheritanceInfo.occlusionQueryEnable = occlusionEnable ? VK_TRUE : VK_FALSE;
   inheritanceInfo.queryFlags = occlusionEnable ? occlusionFlags : 0;
   inheritanceInfo.pipelineStatistics = 0;

   VkCommandBufferInheritanceRenderingInfo renderingInheritance{};
   renderingInheritance.flags = desc ? desc->renderingFlags : 0;
   std::vector<VkFormat> derivedColorFormats;
   uint32_t colorAttachmentCount = 0;
   VkFormat depthFormat = VK_FORMAT_UNDEFINED;
   VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
   VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   colorAttachmentCount = desc ? desc->colorAttachmentCount : 0;
   if (colorAttachmentCount > 8)
   {
      veSetError("colorAttachmentCount exceeds maximum supported attachments (8)");
      veFreeCommandBuffer(cmd);
      return VE_ERROR_INVALID_PARAMETER;
   }

   const VkFormat *colorFormatsPtr = nullptr;
   if (colorAttachmentCount > 0)
   {
      if (!desc)
      {
         veSetError("colorAttachmentFormats must be provided when colorAttachmentCount > 0");
         veFreeCommandBuffer(cmd);
         return VE_ERROR_INVALID_PARAMETER;
      }
      derivedColorFormats.assign(desc->colorAttachmentFormats, desc->colorAttachmentFormats + colorAttachmentCount);
      colorFormatsPtr = derivedColorFormats.data();
   }

   depthFormat = desc ? desc->depthAttachmentFormat : VK_FORMAT_UNDEFINED;
   stencilFormat = desc ? desc->stencilAttachmentFormat : VK_FORMAT_UNDEFINED;
   VkSampleCountFlags userSamples = desc ? desc->rasterizationSamples : 0;
   rasterizationSamples = userSamples ? static_cast<VkSampleCountFlagBits>(userSamples) : VK_SAMPLE_COUNT_1_BIT;

   renderingInheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
   renderingInheritance.viewMask = viewMask;
   renderingInheritance.colorAttachmentCount = colorAttachmentCount;
   renderingInheritance.pColorAttachmentFormats = colorFormatsPtr;
   renderingInheritance.depthAttachmentFormat = depthFormat;
   renderingInheritance.stencilAttachmentFormat = stencilFormat;
   renderingInheritance.rasterizationSamples = rasterizationSamples;
   inheritanceInfo.pNext = &renderingInheritance;

   beginInfo.pInheritanceInfo = &inheritanceInfo;

   if (shouldBegin)
   {
      VkResult resetResult = vkResetCommandBuffer(cmd->commandBuffer, 0);
      if (resetResult != VK_SUCCESS)
      {
         VkFence fence = cmd->currentFence();
         VkResult fenceStatus = fence ? vkGetFenceStatus(cmd->device->device, fence) : VK_SUCCESS;
         veSetError("Failed to reset secondary command buffer (VkResult: %d) cb=%p index=%u fence=%p fenceStatus=%d "
                    "fenceActive=%d",
                    resetResult, (void *)cmd->commandBuffer, cmd->index, (void *)fence, fenceStatus,
                    cmd->fenceActive ? 1 : 0);
         veFreeCommandBuffer(cmd);
         return VE_ERROR_UNKNOWN;
      }

      VkResult beginResult = vkBeginCommandBuffer(cmd->commandBuffer, &beginInfo);
      if (beginResult != VK_SUCCESS)
      {
         veSetError("Failed to begin secondary command buffer (VkResult: %d)", beginResult);
         veFreeCommandBuffer(cmd);
         return VE_ERROR_UNKNOWN;
      }

      cmd->isRecording = true;
      cmd->isOneTime = (usageFlags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != 0;

      VkDescriptorSet descriptorSets[2] = {deviceInternal->textureDescriptorSet, deviceInternal->samplerDescriptorSet};
      vkCmdBindDescriptorSets(cmd->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              deviceInternal->globalGraphicsPipelineLayout, 0, 2, descriptorSets, 0, nullptr);
   }
   else
   {
      cmd->isRecording = false;
      cmd->isOneTime = (usageFlags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != 0;
   }

   cmd->isSecondary = true;

   // Track render area from desc for veApplyRenderState defaults
   if (desc && (desc->renderAreaWidth > 0 || desc->renderAreaHeight > 0))
   {
      cmd->inRenderPass = true; // Secondary buffers will be executed inside a render pass
      cmd->renderAreaX = static_cast<uint32_t>(desc->renderAreaX >= 0 ? desc->renderAreaX : 0);
      cmd->renderAreaY = static_cast<uint32_t>(desc->renderAreaY >= 0 ? desc->renderAreaY : 0);
      cmd->renderAreaWidth = desc->renderAreaWidth;
      cmd->renderAreaHeight = desc->renderAreaHeight;
   }

   *outCmd = (VECommandBuffer *)cmd;
   return VE_SUCCESS;
}

VEResult veExecuteSecondaryCommandBuffers(VECommandBuffer *primaryCmd, uint32_t count,
                                          VECommandBuffer *const *secondaryCmds, bool releaseCommandBuffers)
{
   if (!primaryCmd || !secondaryCmds || count == 0)
   {
      veSetError("Invalid parameters for executing secondary command buffers");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *primaryInternal = (VECommandBufferInternal *)primaryCmd;
   if (!primaryInternal->isRecording)
   {
      veSetError("Primary command buffer must be recording to execute secondary buffers");
      return VE_ERROR_INVALID_PARAMETER;
   }
   if (primaryInternal->inRenderPass &&
       !(primaryInternal->currentRenderingFlags & VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT))
   {
      veSetError("The active rendering scope was not configured for secondary command buffers");
      return VE_ERROR_INVALID_PARAMETER;
   }

   std::vector<VkCommandBuffer> vkSecondaryBuffers(count);
   for (uint32_t i = 0; i < count; ++i)
   {
      if (!secondaryCmds[i])
      {
         veSetError("Secondary command buffer %u is NULL", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VECommandBufferInternal *secondaryInternal = (VECommandBufferInternal *)secondaryCmds[i];
      if (secondaryInternal->isRecording)
      {
         veSetError("Secondary command buffer %u is still recording", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      if (!secondaryInternal->isSecondary)
      {
         veSetError("Command buffer %u is not a secondary buffer", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      vkSecondaryBuffers[i] = secondaryInternal->commandBuffer;
      // Track on the primary so we can defer reuse until the primary fence signals.
      primaryInternal->executedSecondaries.push_back(secondaryInternal);
   }

   vkCmdExecuteCommands(primaryInternal->commandBuffer, count, vkSecondaryBuffers.data());

   // Release secondary command buffers if requested
   if (releaseCommandBuffers)
   {
      for (uint32_t i = 0; i < count; ++i)
      {
         VECommandBufferInternal *secondaryInternal = (VECommandBufferInternal *)secondaryCmds[i];
         veFreeCommandBuffer(secondaryInternal);
      }
      // Clear the tracking since we released them immediately
      primaryInternal->executedSecondaries.clear();
   }

   return VE_SUCCESS;
}

// =============================================================================
// Render Target Builder Functions
// =============================================================================

VERenderTarget veCreateRenderTarget(uint32_t width, uint32_t height)
{
   VERenderTarget target = {};
   target.renderAreaX = 0;
   target.renderAreaY = 0;
   target.renderAreaWidth = width;
   target.renderAreaHeight = height;
   target.colorAttachmentCount = 0;
   target.hasDepthAttachment = false;
   target.hasStencilAttachment = false;
   target.flags = 0;
   return target;
}

VERenderTarget veCreateRenderTargetWithOffset(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
   VERenderTarget target = {};
   target.renderAreaX = x;
   target.renderAreaY = y;
   target.renderAreaWidth = width;
   target.renderAreaHeight = height;
   target.colorAttachmentCount = 0;
   target.hasDepthAttachment = false;
   target.hasStencilAttachment = false;
   target.flags = 0;
   return target;
}

VEResult veRenderTargetAddColorAttachment(VERenderTarget *target, VETextureView view, VkAttachmentLoadOp loadOp,
                                          VEColor clearValue)
{
   if (!target || view == 0 || view == VE_INVALID_TEXTURE_VIEW ||
       target->colorAttachmentCount >= VE_MAX_COLOR_ATTACHMENTS)
   {
      veSetError("Invalid render target or too many color attachments");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderTargetAttachment *attachment = &target->attachments[target->colorAttachmentCount];
   attachment->view = view;
   attachment->loadOp = loadOp;
   attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   attachment->clearValue = clearValue;
   attachment->resolveView = VE_INVALID_TEXTURE_VIEW;

   target->colorAttachmentCount++;
   return VE_SUCCESS;
}

VEResult veRenderTargetAddColorAttachmentResolve(VERenderTarget *target, VETextureView view, VETextureView resolveView,
                                                 VkAttachmentLoadOp loadOp, VEColor clearValue)
{
   if (!target || view == 0 || view == VE_INVALID_TEXTURE_VIEW || resolveView == 0 ||
       resolveView == VE_INVALID_TEXTURE_VIEW || target->colorAttachmentCount >= VE_MAX_COLOR_ATTACHMENTS)
   {
      veSetError("Invalid render target or too many color attachments");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderTargetAttachment *attachment = &target->attachments[target->colorAttachmentCount];
   attachment->view = view;
   attachment->loadOp = loadOp;
   attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   attachment->clearValue = clearValue;
   attachment->resolveView = resolveView;

   target->colorAttachmentCount++;
   return VE_SUCCESS;
}

VEResult veRenderTargetSetDepthAttachment(VERenderTarget *target, VETextureView view, VkAttachmentLoadOp loadOp,
                                          float clearDepth)
{
   if (!target || view == 0 || view == VE_INVALID_TEXTURE_VIEW)
   {
      veSetError("Render target and depth view must be valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderTargetAttachment *attachment = &target->attachments[VE_DEPTH_ATTACHMENT_INDEX];
   attachment->view = view;
   attachment->loadOp = loadOp;
   // Store depth if we loaded it, otherwise don't care (cleared depth typically not needed after)
   attachment->storeOp =
       (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
   attachment->clearValue.r = clearDepth;
   attachment->clearValue.g = 0.0f;
   attachment->clearValue.b = 0.0f;
   attachment->clearValue.a = 0.0f;
   attachment->resolveView = VE_INVALID_TEXTURE_VIEW;

   target->hasDepthAttachment = true;
   return VE_SUCCESS;
}

VEResult veRenderTargetSetStencilAttachment(VERenderTarget *target, VETextureView view, VkAttachmentLoadOp loadOp,
                                            uint32_t clearStencil)
{
   if (!target || view == 0 || view == VE_INVALID_TEXTURE_VIEW)
   {
      veSetError("Render target and stencil view must be valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderTargetAttachment *attachment = &target->attachments[VE_STENCIL_ATTACHMENT_INDEX];
   attachment->view = view;
   attachment->loadOp = loadOp;
   attachment->storeOp =
       (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
   // Store stencil value in the clear color (will be interpreted correctly in veBeginRendering)
   // Use memcpy for type-safe bit-cast (avoids strict-aliasing violation)
   static_assert(sizeof(float) == sizeof(uint32_t), "float and uint32_t must be same size");
   memcpy(&attachment->clearValue.r, &clearStencil, sizeof(float));
   attachment->clearValue.g = 0.0f;
   attachment->clearValue.b = 0.0f;
   attachment->clearValue.a = 0.0f;
   attachment->resolveView = VE_INVALID_TEXTURE_VIEW;

   target->hasStencilAttachment = true;
   return VE_SUCCESS;
}

// =============================================================================
// Dynamic Rendering
// =============================================================================

VEResult veBeginRendering(VECommandBuffer *cmd, const VERenderTarget *renderTarget)
{
   if (!cmd || !renderTarget)
   {
      veSetError("Invalid parameters for begin rendering");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (renderTarget->colorAttachmentCount > VE_MAX_COLOR_ATTACHMENTS || renderTarget->renderAreaWidth == 0 ||
       renderTarget->renderAreaHeight == 0)
   {
      veSetError("Render target attachment count and extent must be valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (internal->isSecondary)
   {
      veSetError("veBeginRendering must be recorded in a primary command buffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   auto transitionView = [&](VETextureView view, bool depth) -> VEResult
   {
      VETextureViewInternal *textureView = internal->device->getTextureView(view);
      if (!textureView)
      {
         veSetError("Render target references invalid texture view %u", view);
         return VE_ERROR_NOT_FOUND;
      }
      return depth ? veTransitionTextureForDepthAttachment(cmd, textureView->texture)
                   : veTransitionTextureForColorAttachment(cmd, textureView->texture);
   };

   // Get pointers to attachments from the inline array
   const VERenderTargetAttachment *colorAttachments = renderTarget->attachments;
   const VERenderTargetAttachment *depthAttachment =
       renderTarget->hasDepthAttachment ? &renderTarget->attachments[VE_DEPTH_ATTACHMENT_INDEX] : nullptr;
   const VERenderTargetAttachment *stencilAttachment =
       renderTarget->hasStencilAttachment ? &renderTarget->attachments[VE_STENCIL_ATTACHMENT_INDEX] : nullptr;

   // Automatically transition all attachments to the correct layout
   for (uint32_t i = 0; i < renderTarget->colorAttachmentCount && i < VE_MAX_COLOR_ATTACHMENTS; i++)
   {
      VEResult transitionResult = transitionView(colorAttachments[i].view, false);
      if (transitionResult != VE_SUCCESS)
         return transitionResult;
      if (colorAttachments[i].resolveView != VE_INVALID_TEXTURE_VIEW)
      {
         transitionResult = transitionView(colorAttachments[i].resolveView, false);
         if (transitionResult != VE_SUCCESS)
            return transitionResult;
      }
   }
   if (depthAttachment)
   {
      VEResult transitionResult = transitionView(depthAttachment->view, true);
      if (transitionResult != VE_SUCCESS)
         return transitionResult;
   }
   if (stencilAttachment && stencilAttachment != depthAttachment)
   {
      VEResult transitionResult = transitionView(stencilAttachment->view, true);
      if (transitionResult != VE_SUCCESS)
         return transitionResult;
   }

   // Convert to Vulkan rendering info
   VkRenderingInfo vkRenderingInfo{};
   vkRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
   vkRenderingInfo.flags = renderTarget->flags;
   vkRenderingInfo.renderArea.offset.x = renderTarget->renderAreaX;
   vkRenderingInfo.renderArea.offset.y = renderTarget->renderAreaY;
   vkRenderingInfo.renderArea.extent.width = renderTarget->renderAreaWidth;
   vkRenderingInfo.renderArea.extent.height = renderTarget->renderAreaHeight;
   vkRenderingInfo.layerCount = 1;

   // Convert color attachments
   VkRenderingAttachmentInfo vkColorAttachments[8] = {};
   for (uint32_t i = 0; i < renderTarget->colorAttachmentCount && i < 8; i++)
   {
      vkColorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      VkImageView imageView = internal->device->getVkImageView(colorAttachments[i].view);
      if (imageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture view %u for color attachment %u", colorAttachments[i].view, i);
         return VE_ERROR_NOT_FOUND;
      }
      vkColorAttachments[i].imageView = imageView;

      vkColorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      vkColorAttachments[i].loadOp = (VkAttachmentLoadOp)colorAttachments[i].loadOp;
      vkColorAttachments[i].storeOp = (VkAttachmentStoreOp)colorAttachments[i].storeOp;
      vkColorAttachments[i].clearValue.color.float32[0] = colorAttachments[i].clearValue.r;
      vkColorAttachments[i].clearValue.color.float32[1] = colorAttachments[i].clearValue.g;
      vkColorAttachments[i].clearValue.color.float32[2] = colorAttachments[i].clearValue.b;
      vkColorAttachments[i].clearValue.color.float32[3] = colorAttachments[i].clearValue.a;

      // Handle resolve attachment if present
      if (colorAttachments[i].resolveView != VE_INVALID_TEXTURE_VIEW)
      {
         VkImageView resolveImageView = internal->device->getVkImageView(colorAttachments[i].resolveView);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for color attachment %u", colorAttachments[i].resolveView, i);
            return VE_ERROR_NOT_FOUND;
         }
         vkColorAttachments[i].resolveImageView = resolveImageView;
         vkColorAttachments[i].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
      }
   }

   vkRenderingInfo.colorAttachmentCount = renderTarget->colorAttachmentCount;
   vkRenderingInfo.pColorAttachments = vkColorAttachments;

   // Convert depth attachment
   VkRenderingAttachmentInfo vkDepthAttachment{};
   if (depthAttachment)
   {
      vkDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      VkImageView depthImageView = internal->device->getVkImageView(depthAttachment->view);
      if (depthImageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for depth attachment", depthAttachment->view);
         return VE_ERROR_NOT_FOUND;
      }
      vkDepthAttachment.imageView = depthImageView;

      vkDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      vkDepthAttachment.loadOp = (VkAttachmentLoadOp)depthAttachment->loadOp;
      vkDepthAttachment.storeOp = (VkAttachmentStoreOp)depthAttachment->storeOp;
      // Use depth clear value from the clear color (stored in r component by builder)
      vkDepthAttachment.clearValue.depthStencil.depth = depthAttachment->clearValue.r;
      vkDepthAttachment.clearValue.depthStencil.stencil = 0;

      // Handle resolve attachment if present
      if (depthAttachment->resolveView != VE_INVALID_TEXTURE_VIEW)
      {
         VkImageView resolveImageView = internal->device->getVkImageView(depthAttachment->resolveView);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for depth attachment", depthAttachment->resolveView);
            return VE_ERROR_NOT_FOUND;
         }
         vkDepthAttachment.resolveImageView = resolveImageView;
         vkDepthAttachment.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
      }

      vkRenderingInfo.pDepthAttachment = &vkDepthAttachment;
   }

   // Convert stencil attachment
   VkRenderingAttachmentInfo vkStencilAttachment{};
   if (stencilAttachment)
   {
      vkStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      VkImageView stencilImageView = internal->device->getVkImageView(stencilAttachment->view);
      if (stencilImageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for stencil attachment", stencilAttachment->view);
         return VE_ERROR_NOT_FOUND;
      }
      vkStencilAttachment.imageView = stencilImageView;

      vkStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      vkStencilAttachment.loadOp = (VkAttachmentLoadOp)stencilAttachment->loadOp;
      vkStencilAttachment.storeOp = (VkAttachmentStoreOp)stencilAttachment->storeOp;
      vkStencilAttachment.clearValue.depthStencil.depth = 1.0f;
      // Retrieve stencil value that was stored via memcpy in veRenderTargetSetStencilAttachment
      memcpy(&vkStencilAttachment.clearValue.depthStencil.stencil, &stencilAttachment->clearValue.r, sizeof(uint32_t));

      // Handle resolve attachment if present
      if (stencilAttachment->resolveView != VE_INVALID_TEXTURE_VIEW)
      {
         VkImageView resolveImageView = internal->device->getVkImageView(stencilAttachment->resolveView);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for stencil attachment", stencilAttachment->resolveView);
            return VE_ERROR_NOT_FOUND;
         }
         vkStencilAttachment.resolveImageView = resolveImageView;
         vkStencilAttachment.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
      }

      vkRenderingInfo.pStencilAttachment = &vkStencilAttachment;
   }

   vkCmdBeginRendering(internal->commandBuffer, &vkRenderingInfo);

   // Track render area for veApplyRenderState default viewport/scissor
   internal->inRenderPass = true;
   internal->currentRenderingFlags = renderTarget->flags;
   internal->renderAreaX = static_cast<uint32_t>(renderTarget->renderAreaX);
   internal->renderAreaY = static_cast<uint32_t>(renderTarget->renderAreaY);
   internal->renderAreaWidth = renderTarget->renderAreaWidth;
   internal->renderAreaHeight = renderTarget->renderAreaHeight;

   return VE_SUCCESS;
}

VEResult veEndRendering(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdEndRendering(internal->commandBuffer);

   // Clear render pass tracking
   internal->inRenderPass = false;
   internal->currentRenderingFlags = 0;
   return VE_SUCCESS;
}

// =============================================================================
// Shader Binding
// =============================================================================

VEResult veBindShader(VECommandBuffer *cmd, VEShader *shader)
{
   if (!cmd || !shader)
   {
      veSetError("Invalid parameters for veBindShader");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEShaderInternal *shaderInternal = (VEShaderInternal *)shader;
   if (!shaderInternal->isValid || shaderInternal->pendingDestroy || shaderInternal->device != internal->device)
   {
      veSetError("Shader is invalid, pending destruction, or belongs to another device");
      return VE_ERROR_INVALID_PARAMETER;
   }

   internal->boundShaders = internal->boundShaders | shaderInternal->stage;

   VkShaderStageFlagBits stageBit = static_cast<VkShaderStageFlagBits>(shaderInternal->stage);

   if (internal->device)
   {
      internal->device->frameStats.pipelineBinds += 1;
   }
   veFuncs.vkCmdBindShadersEXT(internal->commandBuffer, 1, &stageBit, &shaderInternal->shaderObject);
   return VE_SUCCESS;
}

VEResult veBindShaders(VECommandBuffer *cmd, uint32_t shaderCount, VEShader *const *shaders)
{
   if (!cmd || shaderCount == 0 || !shaders)
   {
      veSetError("Invalid parameters for veBindShaders");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkShaderStageFlagBits stageBits[16];
   VkShaderEXT shaderObjects[16];
   uint32_t validCount = 0;

   for (uint32_t i = 0; i < shaderCount && i < 16; i++)
   {
      if (shaders[i])
      {
         VEShaderInternal *shaderInternal = (VEShaderInternal *)shaders[i];
         VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
         if (!shaderInternal->isValid || shaderInternal->pendingDestroy || shaderInternal->device != internal->device)
         {
            veSetError("Shader %u is invalid, pending destruction, or belongs to another device", i);
            return VE_ERROR_INVALID_PARAMETER;
         }
         stageBits[validCount] = static_cast<VkShaderStageFlagBits>(shaderInternal->stage);
         shaderObjects[validCount] = shaderInternal->shaderObject;
         validCount++;
      }
   }

   if (validCount > 0)
   {
      VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
      if (internal->device)
      {
         internal->device->frameStats.pipelineBinds += validCount;
      }
      veFuncs.vkCmdBindShadersEXT(internal->commandBuffer, validCount, stageBits, shaderObjects);
   }
   return VE_SUCCESS;
}

VEResult veUnbindShaderStage(VECommandBuffer *cmd, VkShaderStageFlags stage)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkShaderEXT nullShader = VK_NULL_HANDLE;
   uint32_t remainingBits = static_cast<uint32_t>(stage);

   while (remainingBits)
   {
      uint32_t lowestBit = remainingBits & (~(remainingBits - 1u));
      VkShaderStageFlagBits stageBit = static_cast<VkShaderStageFlagBits>(lowestBit);
      veFuncs.vkCmdBindShadersEXT(internal->commandBuffer, 1, &stageBit, &nullShader);
      remainingBits &= ~lowestBit;
   }
   return VE_SUCCESS;
}
