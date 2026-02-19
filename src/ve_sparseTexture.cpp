#include "ve_internal.h"
#include <chrono>
#include <algorithm>

// =============================================================================
// Sparse Memory Pool Implementation
// =============================================================================

VmaAllocation VESparseMemoryPool::acquire(VmaAllocator allocator, VkMemoryRequirements memReqs, uint32_t memoryTypeIndex)
{
   std::lock_guard<std::mutex> lock(mutex);

   // Try to find a suitable free page
   for (auto it = freePages.begin(); it != freePages.end(); ++it)
   {
      // All pages in the pool should be the same size, just return the first one
      VmaAllocation alloc = it->allocation;
      freePages.erase(it);
      return alloc;
   }

   // No free pages, allocate a new one
   VmaAllocationCreateInfo allocInfo{};
   allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
   allocInfo.memoryTypeBits = (1u << memoryTypeIndex);

   VmaAllocation allocation;
   VkResult result = vmaAllocateMemoryPages(allocator, &memReqs, &allocInfo, 1, &allocation, nullptr);
   if (result != VK_SUCCESS)
   {
      return VK_NULL_HANDLE;
   }

   return allocation;
}

void VESparseMemoryPool::release(VmaAllocation alloc)
{
   std::lock_guard<std::mutex> lock(mutex);

   FreePage page;
   page.allocation = alloc;
   page.freedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
   freePages.push_back(page);
}

void VESparseMemoryPool::cleanup(VmaAllocator allocator, uint64_t currentTimeMs)
{
   std::lock_guard<std::mutex> lock(mutex);

   auto it = freePages.begin();
   while (it != freePages.end())
   {
      if (currentTimeMs - it->freedTime > recycleThresholdMs)
      {
         vmaFreeMemoryPages(allocator, 1, &it->allocation);
         it = freePages.erase(it);
      }
      else
      {
         ++it;
      }
   }
}

// =============================================================================
// Device Sparse Binding Support
// =============================================================================

VEResult VEDeviceInternal::initializeSparseBindingSupport()
{
   if (!features.sparseBinding)
   {
      return VE_SUCCESS; // Not an error, just not supported
   }

   sparseMemoryPool = std::make_unique<VESparseMemoryPool>();
   sparseBindMutex = std::make_unique<std::mutex>();

   // Find a queue that supports sparse binding
   uint32_t queueFamilyCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
   std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
   vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

   // Prefer the graphics queue if it supports sparse binding
   if (queueFamilies[this->queueFamilies.graphicsFamily].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
   {
      sparseBindingQueue = graphicsQueue;
      sparseBindingQueueFamily = this->queueFamilies.graphicsFamily;
   }
   else
   {
      // Look for any queue with sparse binding support
      for (uint32_t i = 0; i < queueFamilyCount; i++)
      {
         if (queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
         {
            // For simplicity, use the graphics queue family if possible
            // In a real implementation, you might want to get a dedicated sparse queue
            if (i == this->queueFamilies.graphicsFamily)
            {
               sparseBindingQueue = graphicsQueue;
            }
            else if (i == this->queueFamilies.computeFamily)
            {
               sparseBindingQueue = computeQueue;
            }
            else if (i == this->queueFamilies.transferFamily)
            {
               sparseBindingQueue = transferQueue;
            }
            sparseBindingQueueFamily = i;
            break;
         }
      }
   }

   if (sparseBindingQueue == VK_NULL_HANDLE)
   {
      veSetError("No queue with sparse binding support found");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   // Create fence and semaphore for sparse binding synchronization
   VkFenceCreateInfo fenceInfo{};
   fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   if (vkCreateFence(device, &fenceInfo, nullptr, &sparseBindFence) != VK_SUCCESS)
   {
      veSetError("Failed to create sparse bind fence");
      return VE_ERROR_UNKNOWN;
   }

   VkSemaphoreCreateInfo semaphoreInfo{};
   semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sparseBindSemaphore) != VK_SUCCESS)
   {
      vkDestroyFence(device, sparseBindFence, nullptr);
      sparseBindFence = VK_NULL_HANDLE;
      veSetError("Failed to create sparse bind semaphore");
      return VE_ERROR_UNKNOWN;
   }

   return VE_SUCCESS;
}

void VEDeviceInternal::cleanupSparseBindingSupport()
{
   if (sparseBindSemaphore != VK_NULL_HANDLE)
   {
      vkDestroySemaphore(device, sparseBindSemaphore, nullptr);
      sparseBindSemaphore = VK_NULL_HANDLE;
   }

   if (sparseBindFence != VK_NULL_HANDLE)
   {
      vkDestroyFence(device, sparseBindFence, nullptr);
      sparseBindFence = VK_NULL_HANDLE;
   }

   if (sparseMemoryPool)
   {
      // Free all remaining pages in the pool
      uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count();
      sparseMemoryPool->recycleThresholdMs = 0; // Force immediate cleanup
      sparseMemoryPool->cleanup(allocator, currentTime);
      sparseMemoryPool.reset();
   }

   sparseBindMutex.reset();
   pendingSparseBinds.clear();
}

bool VEDeviceInternal::hasPendingSparseBinds() const
{
   if (!sparseBindMutex)
      return false;
   std::lock_guard<std::mutex> lock(*sparseBindMutex);
   return !pendingSparseBinds.empty();
}

void VEDeviceInternal::queueSparseBind(VETextureIndex texture, const VkSparseImageMemoryBind &bind)
{
   if (!sparseBindMutex)
      return;

   std::lock_guard<std::mutex> lock(*sparseBindMutex);

   // Find or create pending bind for this texture
   for (auto &pending : pendingSparseBinds)
   {
      if (pending.texture == texture)
      {
         pending.imageBinds.push_back(bind);
         return;
      }
   }

   VESparsePendingBind newPending;
   newPending.texture = texture;
   newPending.imageBinds.push_back(bind);
   pendingSparseBinds.push_back(std::move(newPending));
}

void VEDeviceInternal::queueSparseUnbind(VETextureIndex texture, const VkSparseImageMemoryBind &bind)
{
   // Unbind is the same as bind but with memory = VK_NULL_HANDLE
   queueSparseBind(texture, bind);
}

VEResult VEDeviceInternal::flushPendingSparseBinds(bool blocking)
{
   if (!sparseBindMutex || sparseBindingQueue == VK_NULL_HANDLE)
      return VE_SUCCESS;

   std::vector<VESparsePendingBind> bindsToProcess;
   {
      std::lock_guard<std::mutex> lock(*sparseBindMutex);
      if (pendingSparseBinds.empty())
         return VE_SUCCESS;
      bindsToProcess = std::move(pendingSparseBinds);
      pendingSparseBinds.clear();
   }

   // Build sparse bind info structures
   std::vector<VkSparseImageMemoryBindInfo> imageBindInfos;
   imageBindInfos.reserve(bindsToProcess.size());

   for (auto &pending : bindsToProcess)
   {
      VETextureInternal *tex = getTexture(pending.texture);
      if (!tex || !tex->isSparse)
         continue;

      if (!pending.imageBinds.empty())
      {
         VkSparseImageMemoryBindInfo bindInfo{};
         bindInfo.image = tex->image;
         bindInfo.bindCount = static_cast<uint32_t>(pending.imageBinds.size());
         bindInfo.pBinds = pending.imageBinds.data();
         imageBindInfos.push_back(bindInfo);
      }
   }

   if (imageBindInfos.empty())
      return VE_SUCCESS;

   VkBindSparseInfo bindSparseInfo{};
   bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
   bindSparseInfo.imageBindCount = static_cast<uint32_t>(imageBindInfos.size());
   bindSparseInfo.pImageBinds = imageBindInfos.data();

   if (blocking)
   {
      bindSparseInfo.signalSemaphoreCount = 0;

      vkResetFences(device, 1, &sparseBindFence);

      VkResult result = vkQueueBindSparse(sparseBindingQueue, 1, &bindSparseInfo, sparseBindFence);
      if (result != VK_SUCCESS)
      {
         veSetError("vkQueueBindSparse failed: %s", veVkResultToString(result));
         return VE_ERROR_UNKNOWN;
      }

      result = vkWaitForFences(device, 1, &sparseBindFence, VK_TRUE, UINT64_MAX);
      if (result != VK_SUCCESS)
      {
         veSetError("vkWaitForFences failed for sparse binding: %s", veVkResultToString(result));
         return VE_ERROR_UNKNOWN;
      }
   }
   else
   {
      bindSparseInfo.signalSemaphoreCount = 1;
      bindSparseInfo.pSignalSemaphores = &sparseBindSemaphore;

      VkResult result = vkQueueBindSparse(sparseBindingQueue, 1, &bindSparseInfo, VK_NULL_HANDLE);
      if (result != VK_SUCCESS)
      {
         veSetError("vkQueueBindSparse failed: %s", veVkResultToString(result));
         return VE_ERROR_UNKNOWN;
      }
   }

   return VE_SUCCESS;
}

// =============================================================================
// Helper Functions
// =============================================================================

static uint64_t getCurrentTimeMs()
{
   return std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
       .count();
}

static uint32_t calculateLinearPageIndex(VESparseTextureData *sparseData, uint32_t mipLevel, uint32_t arrayLayer,
                                         uint32_t pageX, uint32_t pageY, uint32_t pageZ)
{
   // Calculate page offset for this mip level
   uint32_t mipPageOffset = 0;
   for (uint32_t m = 0; m < mipLevel && m < sparseData->mipTailFirstLod; m++)
   {
      uint32_t mipPagesX = std::max(1u, sparseData->pagesX >> m);
      uint32_t mipPagesY = std::max(1u, sparseData->pagesY >> m);
      uint32_t mipPagesZ = std::max(1u, sparseData->pagesZ >> m);
      mipPageOffset += mipPagesX * mipPagesY * mipPagesZ;
   }

   uint32_t mipPagesX = std::max(1u, sparseData->pagesX >> mipLevel);
   uint32_t mipPagesY = std::max(1u, sparseData->pagesY >> mipLevel);

   uint32_t pageIndex = mipPageOffset + pageZ * mipPagesX * mipPagesY + pageY * mipPagesX + pageX;

   // Account for array layers
   pageIndex += arrayLayer * sparseData->totalPageCount;

   return pageIndex;
}

// =============================================================================
// Public API Implementation
// =============================================================================

VULKEASE_API VEResult veGetSparseTextureInfo(VEDevice *device, VETextureIndex texture, VESparseTextureInfo *outInfo)
{
   if (!device || !outInfo)
   {
      veSetError("Device and outInfo cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *tex = deviceInternal->getTexture(texture);

   if (!tex || !tex->isValid || !tex->isSparse || !tex->sparseData)
   {
      veSetError("Invalid or non-sparse texture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VESparseTextureData *sparseData = tex->sparseData;
   outInfo->pageWidth = sparseData->pageWidth;
   outInfo->pageHeight = sparseData->pageHeight;
   outInfo->pageDepth = sparseData->pageDepth;
   outInfo->pagesX = sparseData->pagesX;
   outInfo->pagesY = sparseData->pagesY;
   outInfo->pagesZ = sparseData->pagesZ;
   outInfo->mipTailFirstLod = sparseData->mipTailFirstLod;
   outInfo->pageSize = sparseData->pageSize;

   return VE_SUCCESS;
}

VULKEASE_API VEResult veCommitSparsePages(VEDevice *device, VETextureIndex texture,
                                          const VESparsePageRegion *regions, uint32_t regionCount)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!regions || regionCount == 0)
   {
      veSetError("Regions cannot be NULL and regionCount must be > 0");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *tex = deviceInternal->getTexture(texture);

   if (!tex || !tex->isValid || !tex->isSparse || !tex->sparseData)
   {
      veSetError("Invalid or non-sparse texture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VESparseTextureData *sparseData = tex->sparseData;

   // Get memory requirements for the sparse image
   VkMemoryRequirements memReqs;
   vkGetImageMemoryRequirements(deviceInternal->device, tex->image, &memReqs);

   // Find memory type index
   uint32_t memoryTypeIndex = UINT32_MAX;
   for (uint32_t i = 0; i < deviceInternal->memoryProperties.memoryTypeCount; i++)
   {
      if ((memReqs.memoryTypeBits & (1 << i)) &&
          (deviceInternal->memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
         memoryTypeIndex = i;
         break;
      }
   }

   if (memoryTypeIndex == UINT32_MAX)
   {
      veSetError("No suitable memory type found for sparse texture");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   for (uint32_t r = 0; r < regionCount; r++)
   {
      const VESparsePageRegion &region = regions[r];

      // Validate mip level
      if (region.mipLevel >= sparseData->mipTailFirstLod)
      {
         veSetError("Cannot commit pages in mip tail (mip %u >= %u)", region.mipLevel, sparseData->mipTailFirstLod);
         return VE_ERROR_INVALID_PARAMETER;
      }

      uint32_t countX = region.pageCountX > 0 ? region.pageCountX : 1;
      uint32_t countY = region.pageCountY > 0 ? region.pageCountY : 1;
      uint32_t countZ = region.pageCountZ > 0 ? region.pageCountZ : 1;

      for (uint32_t z = 0; z < countZ; z++)
      {
         for (uint32_t y = 0; y < countY; y++)
         {
            for (uint32_t x = 0; x < countX; x++)
            {
               uint32_t pageX = region.pageX + x;
               uint32_t pageY = region.pageY + y;
               uint32_t pageZ = region.pageZ + z;

               uint32_t pageIndex =
                   calculateLinearPageIndex(sparseData, region.mipLevel, region.arrayLayer, pageX, pageY, pageZ);

               if (pageIndex >= sparseData->pages.size())
               {
                  veSetError("Page index out of range");
                  return VE_ERROR_INVALID_PARAMETER;
               }

               VESparsePageInfo &pageInfo = sparseData->pages[pageIndex];

               if (pageInfo.isCommitted)
               {
                  continue; // Already committed
               }

               // Allocate memory for this page
               VkMemoryRequirements pageMemReqs = memReqs;
               pageMemReqs.size = sparseData->pageSize;

               VmaAllocation allocation =
                   deviceInternal->sparseMemoryPool->acquire(deviceInternal->allocator, pageMemReqs, memoryTypeIndex);
               if (allocation == VK_NULL_HANDLE)
               {
                  veSetError("Failed to allocate memory for sparse page");
                  return VE_ERROR_OUT_OF_MEMORY;
               }

               VmaAllocationInfo allocInfo;
               vmaGetAllocationInfo(deviceInternal->allocator, allocation, &allocInfo);

               // Create sparse image memory bind
               VkSparseImageMemoryBind bind{};
               bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
               bind.subresource.mipLevel = region.mipLevel;
               bind.subresource.arrayLayer = region.arrayLayer;
               bind.offset.x = pageX * sparseData->pageWidth;
               bind.offset.y = pageY * sparseData->pageHeight;
               bind.offset.z = pageZ * sparseData->pageDepth;
               bind.extent.width = sparseData->pageWidth;
               bind.extent.height = sparseData->pageHeight;
               bind.extent.depth = sparseData->pageDepth;
               bind.memory = allocInfo.deviceMemory;
               bind.memoryOffset = allocInfo.offset;

               // Queue the bind operation
               deviceInternal->queueSparseBind(texture, bind);

               // Update page info
               pageInfo.allocation = allocation;
               pageInfo.isCommitted = true;
               pageInfo.uncommitTime = 0;
               sparseData->committedPageCount++;
            }
         }
      }
   }

   return VE_SUCCESS;
}

VULKEASE_API VEResult veUncommitSparsePages(VEDevice *device, VETextureIndex texture,
                                            const VESparsePageRegion *regions, uint32_t regionCount)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!regions || regionCount == 0)
   {
      veSetError("Regions cannot be NULL and regionCount must be > 0");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *tex = deviceInternal->getTexture(texture);

   if (!tex || !tex->isValid || !tex->isSparse || !tex->sparseData)
   {
      veSetError("Invalid or non-sparse texture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VESparseTextureData *sparseData = tex->sparseData;

   for (uint32_t r = 0; r < regionCount; r++)
   {
      const VESparsePageRegion &region = regions[r];

      if (region.mipLevel >= sparseData->mipTailFirstLod)
      {
         continue; // Cannot uncommit mip tail
      }

      uint32_t countX = region.pageCountX > 0 ? region.pageCountX : 1;
      uint32_t countY = region.pageCountY > 0 ? region.pageCountY : 1;
      uint32_t countZ = region.pageCountZ > 0 ? region.pageCountZ : 1;

      for (uint32_t z = 0; z < countZ; z++)
      {
         for (uint32_t y = 0; y < countY; y++)
         {
            for (uint32_t x = 0; x < countX; x++)
            {
               uint32_t pageX = region.pageX + x;
               uint32_t pageY = region.pageY + y;
               uint32_t pageZ = region.pageZ + z;

               uint32_t pageIndex =
                   calculateLinearPageIndex(sparseData, region.mipLevel, region.arrayLayer, pageX, pageY, pageZ);

               if (pageIndex >= sparseData->pages.size())
               {
                  continue;
               }

               VESparsePageInfo &pageInfo = sparseData->pages[pageIndex];

               if (!pageInfo.isCommitted)
               {
                  continue; // Already uncommitted
               }

               // Create unbind operation (memory = VK_NULL_HANDLE)
               VkSparseImageMemoryBind bind{};
               bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
               bind.subresource.mipLevel = region.mipLevel;
               bind.subresource.arrayLayer = region.arrayLayer;
               bind.offset.x = pageX * sparseData->pageWidth;
               bind.offset.y = pageY * sparseData->pageHeight;
               bind.offset.z = pageZ * sparseData->pageDepth;
               bind.extent.width = sparseData->pageWidth;
               bind.extent.height = sparseData->pageHeight;
               bind.extent.depth = sparseData->pageDepth;
               bind.memory = VK_NULL_HANDLE;
               bind.memoryOffset = 0;

               deviceInternal->queueSparseUnbind(texture, bind);

               // Release memory back to pool
               if (pageInfo.allocation != VK_NULL_HANDLE)
               {
                  deviceInternal->sparseMemoryPool->release(pageInfo.allocation);
                  pageInfo.allocation = VK_NULL_HANDLE;
               }

               pageInfo.isCommitted = false;
               pageInfo.uncommitTime = getCurrentTimeMs();
               sparseData->committedPageCount--;
            }
         }
      }
   }

   return VE_SUCCESS;
}

VULKEASE_API VEResult veFlushSparseBindings(VEDevice *device)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   if (!deviceInternal->supportsSparseBinding())
   {
      return VE_SUCCESS; // Nothing to flush
   }

   return deviceInternal->flushPendingSparseBinds(true); // Blocking
}

VULKEASE_API bool veIsSparsePagesCommitted(VEDevice *device, VETextureIndex texture, uint32_t mipLevel,
                                           uint32_t arrayLayer, uint32_t pageX, uint32_t pageY, uint32_t pageZ)
{
   if (!device)
      return false;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *tex = deviceInternal->getTexture(texture);

   if (!tex || !tex->isValid || !tex->isSparse || !tex->sparseData)
      return false;

   VESparseTextureData *sparseData = tex->sparseData;

   // Mip tail is always committed
   if (mipLevel >= sparseData->mipTailFirstLod)
      return true;

   uint32_t pageIndex = calculateLinearPageIndex(sparseData, mipLevel, arrayLayer, pageX, pageY, pageZ);

   if (pageIndex >= sparseData->pages.size())
      return false;

   return sparseData->pages[pageIndex].isCommitted;
}

VULKEASE_API uint32_t veGetSparseCommittedPageCount(VEDevice *device, VETextureIndex texture)
{
   if (!device)
      return 0;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *tex = deviceInternal->getTexture(texture);

   if (!tex || !tex->isValid || !tex->isSparse || !tex->sparseData)
      return 0;

   return tex->sparseData->committedPageCount;
}

VULKEASE_API uint32_t veGetSparseTotalPageCount(VEDevice *device, VETextureIndex texture)
{
   if (!device)
      return 0;

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VETextureInternal *tex = deviceInternal->getTexture(texture);

   if (!tex || !tex->isValid || !tex->isSparse || !tex->sparseData)
      return 0;

   return tex->sparseData->totalPageCount;
}
