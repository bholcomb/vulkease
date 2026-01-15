/**
 * @file ve_command.c
 * @brief Command Buffer and Rendering Implementation
 */

#include "ve_internal.h"

#include <mutex>
#include <vector>
#include <thread>
#include <functional>

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
                 (void *)cmd->commandBuffer, cmd->index, (void *)cmd->commandPool,
                 (void *)cmd->currentFence(), threadHash);
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
      veSetError("Failed to reset primary command buffer (VkResult: %d) cb=%p index=%u fence=%p fenceStatus=%d fenceActive=%d",
                 resetResult, (void *)cmd->commandBuffer, cmd->index, (void *)fence, fenceStatus,
                 cmd->fenceActive ? 1 : 0);
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

   if (fence != VK_NULL_HANDLE)
   {
      VkResult fenceReset = vkResetFences(internal->device->device, 1, &fence);
      if (fenceReset != VK_SUCCESS)
      {
         veSetError("Failed to reset command buffer fence (VkResult: %d)", fenceReset);
         return VE_ERROR_OUT_OF_MEMORY;
      }
   }

   std::vector<VkSemaphoreSubmitInfo> waitInfos;
   if (info.waitSemaphoreCount > 0)
   {
      waitInfos.resize(info.waitSemaphoreCount);
      for (uint32_t i = 0; i < info.waitSemaphoreCount; ++i)
      {
         if (info.waitSemaphores[i] == VK_NULL_HANDLE)
         {
            veSetError("Wait semaphore %u is NULL", i);
            return VE_ERROR_INVALID_PARAMETER;
         }

         VkSemaphoreSubmitInfo &waitInfo = waitInfos[i];
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
   {
      std::lock_guard<std::mutex> lock(locks->graphicsMutex());
      submitResult = vkQueueSubmit2(internal->device->graphicsQueue, 1, &submitInfo2, fence);
   }

   if (submitResult != VK_SUCCESS)
   {
      veSetError("Failed to submit command buffer (VkResult: %d)", submitResult);
      return VE_ERROR_OUT_OF_MEMORY;
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

extern "C" VEResult vePopulateSecondaryDescFromRenderingInfo(VEDevice *device,
                                                             const VERenderingInfo *renderingInfo,
                                                             VESecondaryCommandBufferDesc *desc)
{
   if (!device || !renderingInfo || !desc)
   {
      veSetError("vePopulateSecondaryDescFromRenderingInfo: device, renderingInfo, and desc must be non-null");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (renderingInfo->colorAttachmentCount > 8)
   {
      veSetError("vePopulateSecondaryDescFromRenderingInfo: colorAttachmentCount exceeds maximum of 8");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VkSampleCountFlags sampleMask = 0;

   auto setSamples = [&](VkSampleCountFlags samples) -> bool {
      VkSampleCountFlagBits resolved =
          samples ? static_cast<VkSampleCountFlagBits>(samples) : VK_SAMPLE_COUNT_1_BIT;
      if (sampleMask == 0)
      {
         sampleMask = resolved;
         return true;
      }
      if (sampleMask != resolved)
      {
         veSetError("vePopulateSecondaryDescFromRenderingInfo: attachments must use a consistent sample count");
         return false;
      }
      return true;
   };

   auto formatHasStencil = [](VkFormat format) -> bool {
      return format == VK_FORMAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT ||
             format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
   };

   // Color attachments
   desc->colorAttachmentCount = renderingInfo->colorAttachmentCount;
   for (uint32_t i = 0; i < renderingInfo->colorAttachmentCount; ++i)
   {
      VETextureInternal *tex = deviceInternal->getTexture(renderingInfo->attachments[i].texture);
      if (!tex)
      {
         veSetError("vePopulateSecondaryDescFromRenderingInfo: invalid texture index %u for color attachment %u",
                    renderingInfo->attachments[i].texture, i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      desc->colorAttachmentFormats[i] = tex->format;
      if (!setSamples(tex->sampleCount))
      {
         return VE_ERROR_INVALID_PARAMETER;
      }
   }

   // Depth attachment
   desc->depthAttachmentFormat = VK_FORMAT_UNDEFINED;
   desc->stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
   if (renderingInfo->hasDepthAttachment)
   {
      const VERenderingAttachment *depthAttachment = &renderingInfo->attachments[VE_DEPTH_ATTACHMENT_INDEX];
      VETextureInternal *tex = deviceInternal->getTexture(depthAttachment->texture);
      if (!tex)
      {
         veSetError("vePopulateSecondaryDescFromRenderingInfo: invalid depth attachment texture index %u",
                    depthAttachment->texture);
         return VE_ERROR_INVALID_PARAMETER;
      }

      desc->depthAttachmentFormat = tex->format;
      if (!setSamples(tex->sampleCount))
      {
         return VE_ERROR_INVALID_PARAMETER;
      }

      if (formatHasStencil(tex->format))
      {
         desc->stencilAttachmentFormat = tex->format;
      }
   }

   // Stencil attachment (explicit)
   if (renderingInfo->hasStencilAttachment)
   {
      const VERenderingAttachment *stencilAttachment = &renderingInfo->attachments[VE_STENCIL_ATTACHMENT_INDEX];
      VETextureInternal *tex = deviceInternal->getTexture(stencilAttachment->texture);
      if (!tex)
      {
         veSetError("vePopulateSecondaryDescFromRenderingInfo: invalid stencil attachment texture index %u",
                    stencilAttachment->texture);
         return VE_ERROR_INVALID_PARAMETER;
      }

      desc->stencilAttachmentFormat = tex->format;
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

   if (desc->usageFlags == 0)
   {
      desc->usageFlags =
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
   }

   // Copy render area for veApplyRenderState defaults
   desc->renderAreaX = renderingInfo->renderAreaX;
   desc->renderAreaY = renderingInfo->renderAreaY;
   desc->renderAreaWidth = renderingInfo->renderAreaWidth;
   desc->renderAreaHeight = renderingInfo->renderAreaHeight;

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
      derivedColorFormats.assign(desc->colorAttachmentFormats,
                                 desc->colorAttachmentFormats + colorAttachmentCount);
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
      veSetError("veBeginSecondaryRecording: failed to reset command buffer (VkResult: %d) cb=%p fence=%p fenceStatus=%d fenceActive=%d",
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

   return VE_SUCCESS;
}

VEResult veBeginSecondaryCommandBuffer(VEDevice *device, const VESecondaryCommandBufferDesc *desc, VECommandBuffer **outCmd)
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
      derivedColorFormats.assign(desc->colorAttachmentFormats,
                                 desc->colorAttachmentFormats + colorAttachmentCount);
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
         veSetError("Failed to reset secondary command buffer (VkResult: %d) cb=%p index=%u fence=%p fenceStatus=%d fenceActive=%d",
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
      cmd->inRenderPass = true;  // Secondary buffers will be executed inside a render pass
      cmd->renderAreaX = static_cast<uint32_t>(desc->renderAreaX >= 0 ? desc->renderAreaX : 0);
      cmd->renderAreaY = static_cast<uint32_t>(desc->renderAreaY >= 0 ? desc->renderAreaY : 0);
      cmd->renderAreaWidth = desc->renderAreaWidth;
      cmd->renderAreaHeight = desc->renderAreaHeight;
   }

   *outCmd = (VECommandBuffer *)cmd;
   return VE_SUCCESS;
}

VEResult veExecuteSecondaryCommandBuffers(VECommandBuffer *primaryCmd, uint32_t count,
                                          VECommandBuffer *const *secondaryCmds,
                                          bool releaseCommandBuffers)
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
// Rendering Info Builder Functions
// =============================================================================

VERenderingInfo veCreateRenderingInfo(uint32_t width, uint32_t height)
{
   VERenderingInfo info = {};
   info.renderAreaX = 0;
   info.renderAreaY = 0;
   info.renderAreaWidth = width;
   info.renderAreaHeight = height;
   info.colorAttachmentCount = 0;
   info.hasDepthAttachment = false;
   info.hasStencilAttachment = false;
   return info;
}

VERenderingInfo veCreateRenderingInfoWithOffset(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
   VERenderingInfo info = {};
   info.renderAreaX = x;
   info.renderAreaY = y;
   info.renderAreaWidth = width;
   info.renderAreaHeight = height;
   info.colorAttachmentCount = 0;
   info.hasDepthAttachment = false;
   info.hasStencilAttachment = false;
   return info;
}

VEResult veRenderingAddColorAttachment(VERenderingInfo *info, VETextureIndex texture, VkAttachmentLoadOp loadOp,
                                      VEColor clearValue)
{
   if (!info || info->colorAttachmentCount >= VE_MAX_COLOR_ATTACHMENTS)
   {
      veSetError("Invalid rendering info or too many color attachments");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderingAttachment *attachment = &info->attachments[info->colorAttachmentCount];
   attachment->texture = texture;
   attachment->loadOp = loadOp;
   attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   attachment->clearValue = clearValue;
   attachment->resolveTexture = VE_INVALID_TEXTURE_INDEX;

   info->colorAttachmentCount++;
   return VE_SUCCESS;
}

VEResult veRenderingAddColorAttachmentResolve(VERenderingInfo *info, VETextureIndex texture, VETextureIndex resolveTexture,
                                             VkAttachmentLoadOp loadOp, VEColor clearValue)
{
   if (!info || info->colorAttachmentCount >= VE_MAX_COLOR_ATTACHMENTS)
   {
      veSetError("Invalid rendering info or too many color attachments");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderingAttachment *attachment = &info->attachments[info->colorAttachmentCount];
   attachment->texture = texture;
   attachment->loadOp = loadOp;
   attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   attachment->clearValue = clearValue;
   attachment->resolveTexture = resolveTexture;

   info->colorAttachmentCount++;
   return VE_SUCCESS;
}

VEResult veRenderingSetDepthAttachment(VERenderingInfo *info, VETextureIndex texture, VkAttachmentLoadOp loadOp,
                                      float clearDepth)
{
   if (!info)
   {
      veSetError("Rendering info cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderingAttachment *attachment = &info->attachments[VE_DEPTH_ATTACHMENT_INDEX];
   attachment->texture = texture;
   attachment->loadOp = loadOp;
   // Store depth if we loaded it, otherwise don't care (cleared depth typically not needed after)
   attachment->storeOp =
       (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
   attachment->clearValue.r = clearDepth;
   attachment->clearValue.g = 0.0f;
   attachment->clearValue.b = 0.0f;
   attachment->clearValue.a = 0.0f;
   attachment->resolveTexture = VE_INVALID_TEXTURE_INDEX;

   info->hasDepthAttachment = true;
   return VE_SUCCESS;
}

VEResult veRenderingSetStencilAttachment(VERenderingInfo *info, VETextureIndex texture, VkAttachmentLoadOp loadOp,
                                        uint32_t clearStencil)
{
   if (!info)
   {
      veSetError("Rendering info cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderingAttachment *attachment = &info->attachments[VE_STENCIL_ATTACHMENT_INDEX];
   attachment->texture = texture;
   attachment->loadOp = loadOp;
   attachment->storeOp =
       (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
   // Store stencil value in the clear color (will be interpreted correctly in veBeginRendering)
   attachment->clearValue.r = *(float *)&clearStencil; // Bit-cast to float for storage
   attachment->clearValue.g = 0.0f;
   attachment->clearValue.b = 0.0f;
   attachment->clearValue.a = 0.0f;
   attachment->resolveTexture = VE_INVALID_TEXTURE_INDEX;

   info->hasStencilAttachment = true;
   return VE_SUCCESS;
}

// =============================================================================
// Dynamic Rendering
// =============================================================================

VEResult veBeginRendering(VECommandBuffer *cmd, const VERenderingInfo *renderingInfo)
{
   if (!cmd || !renderingInfo)
   {
      veSetError("Invalid parameters for begin rendering");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // Get pointers to attachments from the inline array
   const VERenderingAttachment *colorAttachments = renderingInfo->attachments;
   const VERenderingAttachment *depthAttachment =
       renderingInfo->hasDepthAttachment ? &renderingInfo->attachments[VE_DEPTH_ATTACHMENT_INDEX] : nullptr;
   const VERenderingAttachment *stencilAttachment =
       renderingInfo->hasStencilAttachment ? &renderingInfo->attachments[VE_STENCIL_ATTACHMENT_INDEX] : nullptr;

   // Automatically transition all attachments to the correct layout
   for (uint32_t i = 0; i < renderingInfo->colorAttachmentCount && i < VE_MAX_COLOR_ATTACHMENTS; i++)
   {
      veTransitionTextureForColorAttachment(cmd, colorAttachments[i].texture);
      if (colorAttachments[i].resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         veTransitionTextureForColorAttachment(cmd, colorAttachments[i].resolveTexture);
      }
   }
   if (depthAttachment)
   {
      veTransitionTextureForDepthAttachment(cmd, depthAttachment->texture);
   }
   if (stencilAttachment && stencilAttachment != depthAttachment)
   {
      veTransitionTextureForDepthAttachment(cmd, stencilAttachment->texture);
   }

   // Convert to Vulkan rendering info
   VkRenderingInfo vkRenderingInfo{};
   vkRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
   vkRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
   vkRenderingInfo.renderArea.offset.x = renderingInfo->renderAreaX;
   vkRenderingInfo.renderArea.offset.y = renderingInfo->renderAreaY;
   vkRenderingInfo.renderArea.extent.width = renderingInfo->renderAreaWidth;
   vkRenderingInfo.renderArea.extent.height = renderingInfo->renderAreaHeight;
   vkRenderingInfo.layerCount = 1;

   // Convert color attachments
   VkRenderingAttachmentInfo vkColorAttachments[8] = {};
   for (uint32_t i = 0; i < renderingInfo->colorAttachmentCount && i < 8; i++)
   {
      vkColorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      // Get image view from texture index
      VkImageView imageView = internal->device->getImageViewFromTexture(colorAttachments[i].texture);
      if (imageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for color attachment %u", colorAttachments[i].texture, i);
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
      if (colorAttachments[i].resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         VkImageView resolveImageView =
             internal->device->getImageViewFromTexture(colorAttachments[i].resolveTexture);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for color attachment %u",
                       colorAttachments[i].resolveTexture, i);
            return VE_ERROR_NOT_FOUND;
         }
         vkColorAttachments[i].resolveImageView = resolveImageView;
         vkColorAttachments[i].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
      }
   }

   vkRenderingInfo.colorAttachmentCount = renderingInfo->colorAttachmentCount;
   vkRenderingInfo.pColorAttachments = vkColorAttachments;

   // Convert depth attachment
   VkRenderingAttachmentInfo vkDepthAttachment{};
   if (depthAttachment)
   {
      vkDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

      // Get image view from texture index
      VkImageView depthImageView = internal->device->getImageViewFromTexture(depthAttachment->texture);
      if (depthImageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for depth attachment", depthAttachment->texture);
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
      if (depthAttachment->resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         VkImageView resolveImageView =
             internal->device->getImageViewFromTexture(depthAttachment->resolveTexture);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for depth attachment",
                       depthAttachment->resolveTexture);
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

      // Get image view from texture index
      VkImageView stencilImageView =
          internal->device->getImageViewFromTexture(stencilAttachment->texture);
      if (stencilImageView == VK_NULL_HANDLE)
      {
         veSetError("Invalid texture index %u for stencil attachment", stencilAttachment->texture);
         return VE_ERROR_NOT_FOUND;
      }
      vkStencilAttachment.imageView = stencilImageView;

      vkStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      vkStencilAttachment.loadOp = (VkAttachmentLoadOp)stencilAttachment->loadOp;
      vkStencilAttachment.storeOp = (VkAttachmentStoreOp)stencilAttachment->storeOp;
      vkStencilAttachment.clearValue.depthStencil.depth = 1.0f;
      vkStencilAttachment.clearValue.depthStencil.stencil = 0;

      // Handle resolve attachment if present
      if (stencilAttachment->resolveTexture != VE_INVALID_TEXTURE_INDEX)
      {
         VkImageView resolveImageView =
             internal->device->getImageViewFromTexture(stencilAttachment->resolveTexture);
         if (resolveImageView == VK_NULL_HANDLE)
         {
            veSetError("Invalid resolve texture index %u for stencil attachment",
                       stencilAttachment->resolveTexture);
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
   internal->renderAreaX = static_cast<uint32_t>(renderingInfo->renderAreaX);
   internal->renderAreaY = static_cast<uint32_t>(renderingInfo->renderAreaY);
   internal->renderAreaWidth = renderingInfo->renderAreaWidth;
   internal->renderAreaHeight = renderingInfo->renderAreaHeight;

   // Bind the descriptors once per begin rendering call
   VkDescriptorSet descriptorSets[2] = {internal->device->textureDescriptorSet, internal->device->samplerDescriptorSet};

   vkCmdBindDescriptorSets(internal->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           internal->device->globalGraphicsPipelineLayout,
                           0,              // first set
                           2,              // set count
                           descriptorSets, // sets
                           0, NULL);       // dynamic offsset
   if (internal->device)
   {
      internal->device->frameStats.descriptorBinds += 1;
   }
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
   return VE_SUCCESS;
}

// =============================================================================
// Render Configuration Application
// =============================================================================

VEResult veApplyRenderConfig(VECommandBuffer *cmd, VERenderConfig *config)
{
   if (!cmd || !config)
   {
      veSetError("veApplyRenderConfig: Invalid parameters (cmd=%p, config=%p)", cmd, config);
      return VE_ERROR_INVALID_PARAMETER;
   }

   VERenderConfigInternal *configInternal = (VERenderConfigInternal *)config;
   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   
   // Validate config
   if (!configInternal->isValid)
   {
      veSetError("veApplyRenderConfig: Render config is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }
   
   // Validate command buffer state
   if (!internal->isRecording)
   {
      veSetError("veApplyRenderConfig: Command buffer is not recording");
      return VE_ERROR_INVALID_PARAMETER;
   }
   
   if (!internal->commandBuffer)
   {
      veSetError("veApplyRenderConfig: Command buffer handle is null");
      return VE_ERROR_INVALID_PARAMETER;
   }
   
   VkCommandBuffer vkCmd = internal->commandBuffer;

   // ==========================================================================
   // REQUIRED DYNAMIC STATE FOR SHADER OBJECTS (Vulkan 1.3 Core)
   // ==========================================================================

   // 1. VIEWPORT AND SCISSOR (REQUIRED - must use WithCount variants)
   // This is MANDATORY when shader objects are bound
   if (configInternal->configTypes & VE_CONFIG_TYPE_VIEWPORT)
   {
      VkViewport viewport{};
      viewport.x = configInternal->viewport.x;
      viewport.y = configInternal->viewport.y;
      viewport.width = configInternal->viewport.width;
      viewport.height = configInternal->viewport.height;
      viewport.minDepth = configInternal->viewport.minDepth;
      viewport.maxDepth = configInternal->viewport.maxDepth;
      vkCmdSetViewportWithCount(vkCmd, 1, &viewport);
   }

   if (configInternal->configTypes & VE_CONFIG_TYPE_SCISSOR)
   {
      VkRect2D scissor{};
      scissor.offset.x = configInternal->scissor.x;
      scissor.offset.y = configInternal->scissor.y;
      scissor.extent.width = configInternal->scissor.width;
      scissor.extent.height = configInternal->scissor.height;

      vkCmdSetScissorWithCount(vkCmd, 1, &scissor);
   }

   // 2. PRIMITIVE TOPOLOGY (REQUIRED)
   VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      topology = configInternal->vertexInputConfig.topology;
   }
   vkCmdSetPrimitiveTopology(vkCmd, topology);
   internal->currentTopology = topology;

   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      internal->currentPatchControlPoints = configInternal->vertexInputConfig.patchControlPoints;
   }
   else
   {
      internal->currentPatchControlPoints = 0;
   }

   // 3. PRIMITIVE RESTART (REQUIRED)
   VkBool32 primitiveRestart = VK_FALSE;
   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      primitiveRestart = configInternal->vertexInputConfig.primitiveRestartEnable ? VK_TRUE : VK_FALSE;
   }
   vkCmdSetPrimitiveRestartEnable(vkCmd, primitiveRestart);

   // ==========================================================================
   // RASTERIZATION STATE (Core + VK_EXT_extended_dynamic_state3)
   // ==========================================================================

   if (configInternal->configTypes & VE_CONFIG_TYPE_RASTERIZATION)
   {
      const VERasterConfig *raster = &configInternal->rasterConfig;

      // CORE DYNAMIC STATE (Vulkan 1.3)
      vkCmdSetCullMode(vkCmd, raster->cullMode);
      vkCmdSetFrontFace(vkCmd, raster->frontFace);
      vkCmdSetLineWidth(vkCmd, raster->lineWidth);
      vkCmdSetRasterizerDiscardEnable(vkCmd, raster->rasterizerDiscardEnable ? VK_TRUE : VK_FALSE);
      vkCmdSetDepthBiasEnable(vkCmd, raster->depthBiasEnable ? VK_TRUE : VK_FALSE);

      if (raster->depthBiasEnable)
      {
         vkCmdSetDepthBias(vkCmd, raster->depthBiasConstantFactor, raster->depthBiasClamp,
                           raster->depthBiasSlopeFactor);
      }

      // EXTENDED DYNAMIC STATE 3 (when available)
      // Polygon mode (VK_EXT_extended_dynamic_state3)
      veFuncs.vkCmdSetPolygonModeEXT(vkCmd, raster->polygonMode);

      // Depth clamp (VK_EXT_extended_dynamic_state3)
      veFuncs.vkCmdSetDepthClampEnableEXT(vkCmd, raster->depthClampEnable ? VK_TRUE : VK_FALSE);
   }
   else
   {
      // Set safe defaults when raster config not provided
      vkCmdSetCullMode(vkCmd, VK_CULL_MODE_BACK_BIT);
      vkCmdSetFrontFace(vkCmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
      vkCmdSetLineWidth(vkCmd, 1.0f);
      vkCmdSetRasterizerDiscardEnable(vkCmd, VK_FALSE);
      vkCmdSetDepthBiasEnable(vkCmd, VK_FALSE);

      veFuncs.vkCmdSetPolygonModeEXT(vkCmd, VK_POLYGON_MODE_FILL);
      veFuncs.vkCmdSetDepthClampEnableEXT(vkCmd, VK_FALSE);
   }

   // ==========================================================================
   // DEPTH/STENCIL STATE (Vulkan 1.3 Core)
   // ==========================================================================
   if (configInternal->configTypes & VE_CONFIG_TYPE_DEPTH_STENCIL)
   {
      const VEDepthConfig *depth = &configInternal->depthConfig;
      // Depth test state
      vkCmdSetDepthTestEnable(internal->commandBuffer, depth->depthTestEnable);
      vkCmdSetDepthWriteEnable(internal->commandBuffer, depth->depthWriteEnable);
      vkCmdSetDepthCompareOp(internal->commandBuffer, (VkCompareOp)depth->depthCompareOp);

      // Depth bounds test
      vkCmdSetDepthBoundsTestEnable(vkCmd, depth->depthBoundsTestEnable ? VK_TRUE : VK_FALSE);
      if (depth->depthBoundsTestEnable)
      {
         vkCmdSetDepthBounds(internal->commandBuffer, depth->minDepthBounds, depth->maxDepthBounds);
      }

      // Stencil test state
      vkCmdSetStencilTestEnable(vkCmd, depth->stencilTestEnable ? VK_TRUE : VK_FALSE);

      if (depth->stencilTestEnable)
      {
         // Front face stencil operations
         vkCmdSetStencilOp(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontFailOp, depth->frontPassOp,
                           depth->frontDepthFailOp, depth->frontCompareOp);

         vkCmdSetStencilCompareMask(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontCompareMask);
         vkCmdSetStencilWriteMask(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontWriteMask);
         vkCmdSetStencilReference(vkCmd, VK_STENCIL_FACE_FRONT_BIT, depth->frontReference);

         // Back face stencil operations
         vkCmdSetStencilOp(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backFailOp, depth->backPassOp,
                           depth->backDepthFailOp, depth->backCompareOp);

         vkCmdSetStencilCompareMask(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backCompareMask);
         vkCmdSetStencilWriteMask(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backWriteMask);
         vkCmdSetStencilReference(vkCmd, VK_STENCIL_FACE_BACK_BIT, depth->backReference);
      }
   }
   else
   {
      // Set safe defaults when depth config not provided
      vkCmdSetDepthTestEnable(vkCmd, VK_TRUE);
      vkCmdSetDepthWriteEnable(vkCmd, VK_TRUE);
      vkCmdSetDepthCompareOp(vkCmd, VK_COMPARE_OP_LESS);
      vkCmdSetDepthBoundsTestEnable(vkCmd, VK_FALSE);
      vkCmdSetStencilTestEnable(vkCmd, VK_FALSE);
   }

   // ==========================================================================
   // COLOR BLENDING STATE (VK_EXT_extended_dynamic_state3)
   // ==========================================================================

   uint32_t colorAttachmentCount = 1; // Default assumption
   if (configInternal->configTypes & VE_CONFIG_TYPE_COLOR_BLEND)
   {
      const VEBlendConfig *blend = &configInternal->blendConfig;
      colorAttachmentCount = blend->attachmentCount;

      // Set blend equations for ALL attachments at once
      // Per-attachment blend enables and color write masks
      VkBool32 blendEnables[VE_MAX_COLOR_ATTACHMENTS];
      VkColorComponentFlags colorWriteMasks[VE_MAX_COLOR_ATTACHMENTS];
      VkColorBlendEquationEXT blendEquations[VE_MAX_COLOR_ATTACHMENTS];

      for (uint32_t i = 0; i < colorAttachmentCount; i++)
      {
         blendEnables[i] = blend->attachments[i].blendEnable ? VK_TRUE : VK_FALSE;
         colorWriteMasks[i] = blend->attachments[i].colorWriteMask;

         // Set blend equation for this attachment (even if blending is disabled)
         VkColorBlendEquationEXT equation{};
         equation.srcColorBlendFactor = blend->attachments[i].srcColorBlendFactor;
         equation.dstColorBlendFactor = blend->attachments[i].dstColorBlendFactor;
         equation.colorBlendOp = blend->attachments[i].colorBlendOp;
         equation.srcAlphaBlendFactor = blend->attachments[i].srcAlphaBlendFactor;
         equation.dstAlphaBlendFactor = blend->attachments[i].dstAlphaBlendFactor;
         equation.alphaBlendOp = blend->attachments[i].alphaBlendOp;
         blendEquations[i] = equation;
      }

      // Set all blend state at once
      veFuncs.vkCmdSetColorBlendEnableEXT(vkCmd, 0, colorAttachmentCount, blendEnables);
      veFuncs.vkCmdSetColorWriteMaskEXT(vkCmd, 0, colorAttachmentCount, colorWriteMasks);
      veFuncs.vkCmdSetColorBlendEquationEXT(vkCmd, 0, colorAttachmentCount, blendEquations);

      // Logic operations (if supported)
      veFuncs.vkCmdSetLogicOpEnableEXT(vkCmd, blend->logicOpEnable ? VK_TRUE : VK_FALSE);
      if (blend->logicOpEnable)
      {
         veFuncs.vkCmdSetLogicOpEXT(vkCmd, blend->logicOp);
      }

      // Blend constants (always available in core)
      vkCmdSetBlendConstants(vkCmd, blend->blendConstants);
   }
   else
   {
      // Set safe defaults for color blending
      VkBool32 blendEnable = VK_FALSE;
      VkColorComponentFlags colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

      veFuncs.vkCmdSetColorBlendEnableEXT(vkCmd, 0, 1, &blendEnable);
      veFuncs.vkCmdSetColorWriteMaskEXT(vkCmd, 0, 1, &colorWriteMask);
      veFuncs.vkCmdSetLogicOpEnableEXT(vkCmd, VK_FALSE);

      // Default blend constants
      float blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      vkCmdSetBlendConstants(vkCmd, blendConstants);
   }

   // ==========================================================================
   // VERTEX INPUT STATE (VK_EXT_shader_object)
   // ==========================================================================

   if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
   {
      const VEVertexInputConfig *vertexInput = &configInternal->vertexInputConfig;

      // Convert VulkEase vertex input to Vulkan EXT format
      VkVertexInputBindingDescription2EXT bindings[VE_MAX_VERTEX_BINDINGS];
      VkVertexInputAttributeDescription2EXT attributes[VE_MAX_VERTEX_ATTRIBUTES];

      // Convert bindings
      for (uint32_t i = 0; i < vertexInput->bindingCount; i++)
      {
         VkVertexInputBindingDescription2EXT binding{};
         binding.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
         binding.binding = vertexInput->bindings[i].binding;
         binding.stride = vertexInput->bindings[i].stride;
         binding.inputRate = vertexInput->bindings[i].inputRate;
         binding.divisor = vertexInput->bindings[i].divisor;
         bindings[i] = binding;
      }

      // Convert attributes
      for (uint32_t i = 0; i < vertexInput->attributeCount; i++)
      {
         VkVertexInputAttributeDescription2EXT attribute{};
         attribute.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
         attribute.location = vertexInput->attributes[i].location;
         attribute.binding = vertexInput->attributes[i].binding;
         attribute.format = vertexInput->attributes[i].format;
         attribute.offset = vertexInput->attributes[i].offset;
         attributes[i] = attribute;
      }

      // Set vertex input layout dynamically
      veFuncs.vkCmdSetVertexInputEXT(vkCmd, vertexInput->bindingCount, bindings, vertexInput->attributeCount,
                                     attributes);
   }
   else
   {
      // No vertex input when not specified (e.g., fullscreen triangle)
      veFuncs.vkCmdSetVertexInputEXT(vkCmd, 0, NULL, 0, NULL);
   }

   // ==========================================================================
   // MULTISAMPLE STATE (VK_EXT_extended_dynamic_state3)
   // ==========================================================================

   if (configInternal->configTypes & VE_CONFIG_TYPE_MULTISAMPLE)
   {
      const VEMultisampleConfig *msaa = &configInternal->multisampleConfig;

      VkSampleCountFlagBits sampleCountBits = static_cast<VkSampleCountFlagBits>(msaa->rasterizationSamples);
      veFuncs.vkCmdSetRasterizationSamplesEXT(vkCmd, sampleCountBits);
      veFuncs.vkCmdSetAlphaToCoverageEnableEXT(vkCmd, msaa->alphaToCoverageEnable ? VK_TRUE : VK_FALSE);
      veFuncs.vkCmdSetAlphaToOneEnableEXT(vkCmd, msaa->alphaToOneEnable ? VK_TRUE : VK_FALSE);

      // Generate default sample mask (all samples enabled)
      uint32_t sampleCount = static_cast<uint32_t>(msaa->rasterizationSamples);
      uint32_t maskWords = (sampleCount + 31) / 32; // Calculate number of 32-bit words needed
      VkSampleMask sampleMask[16];                  // Maximum reasonable sample count (512 samples =
                                                    // 16 words)

      for (uint32_t i = 0; i < maskWords; i++)
      {
         sampleMask[i] = 0xFFFFFFFF; // All bits set = all samples enabled
      }

      veFuncs.vkCmdSetSampleMaskEXT(vkCmd, sampleCountBits, sampleMask);
   }
   else
   {
      // Default MSAA settings
      veFuncs.vkCmdSetRasterizationSamplesEXT(vkCmd, VK_SAMPLE_COUNT_1_BIT);
      veFuncs.vkCmdSetAlphaToCoverageEnableEXT(vkCmd, VK_FALSE);
      veFuncs.vkCmdSetAlphaToOneEnableEXT(vkCmd, VK_FALSE);

      VkSampleMask sampleMask = 0xFFFFFFFF;
      veFuncs.vkCmdSetSampleMaskEXT(vkCmd, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
   }

   // ==========================================================================
   // TESSELLATION STATE (VK_EXT_extended_dynamic_state2/3)
   // ==========================================================================

   if (topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
   {
      uint32_t patchControlPoints = 3; // Default
      if (configInternal->configTypes & VE_CONFIG_TYPE_VERTEX_INPUT)
      {
         patchControlPoints = configInternal->vertexInputConfig.patchControlPoints;
      }

      veFuncs.vkCmdSetPatchControlPointsEXT(vkCmd, patchControlPoints);
   }

   // ==========================================================================
   // ADDITIONAL REQUIRED STATE
   // ==========================================================================

   // Conservative rasterization (VK_EXT_extended_dynamic_state3)
   veFuncs.vkCmdSetConservativeRasterizationModeEXT(vkCmd, VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT);

   // Line rasterization (VK_EXT_extended_dynamic_state3)
   veFuncs.vkCmdSetLineRasterizationModeEXT(vkCmd, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT);

   veFuncs.vkCmdSetProvokingVertexModeEXT(vkCmd, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);

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

VEResult veBindShaderConfig(VECommandBuffer *cmd, VEShaderConfig *config)
{
   if (!cmd || !config)
   {
      veSetError("Invalid parameters for veBindShaderConfig");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEShaderConfigInternal *configInternal = (VEShaderConfigInternal *)config;
   VECommandBufferInternal *cmdInternal = (VECommandBufferInternal *)cmd;

   VEShader *shaders[6];
   uint32_t shaderCount = 0;

   if (configInternal->vertexShader)
   {
      shaders[shaderCount++] = configInternal->vertexShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_VERTEX_BIT;
   }
   if (configInternal->fragmentShader)
   {
      shaders[shaderCount++] = configInternal->fragmentShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_FRAGMENT_BIT;
   }
   if (configInternal->geometryShader)
   {
      shaders[shaderCount++] = configInternal->geometryShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_GEOMETRY_BIT;
   }
   if (configInternal->tessControlShader)
   {
      shaders[shaderCount++] = configInternal->tessControlShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
   }
   if (configInternal->tessEvalShader)
   {
      shaders[shaderCount++] = configInternal->tessEvalShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
   }
   if (configInternal->computeShader)
   {
      shaders[shaderCount++] = configInternal->computeShader;
      cmdInternal->boundShaders |= VK_SHADER_STAGE_COMPUTE_BIT;
   }

   if (shaderCount > 0)
   {
      return veBindShaders(cmd, shaderCount, shaders);
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