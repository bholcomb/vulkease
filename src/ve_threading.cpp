#include "ve_internal.h"

#include <new>

struct VEDeviceQueueLocks
{
   std::mutex graphics;
   std::mutex compute;
   std::mutex transfer;
};

static VEDeviceQueueLocks *ensureQueueLocks(VEDeviceInternal *device)
{
   if (!device)
   {
      return nullptr;
   }

   if (!device->queueLocks)
   {
      auto *locks = new (std::nothrow) VEDeviceQueueLocks();
      if (!locks)
      {
         veSetError("Failed to allocate queue locks");
         return nullptr;
      }

      device->queueLocks = locks;
   }

   return device->queueLocks;
}

extern "C" void veInitializeQueueLocks(VEDeviceInternal *device)
{
   (void)ensureQueueLocks(device);
}

extern "C" void veDestroyQueueLocks(VEDeviceInternal *device)
{
   if (!device || !device->queueLocks)
   {
      return;
   }

   auto *locks = static_cast<VEDeviceQueueLocks *>(device->queueLocks);
   delete locks;
   device->queueLocks = nullptr;
}

extern "C" void veLockGraphicsQueue(VEDeviceInternal *device)
{
   VEDeviceQueueLocks *locks = ensureQueueLocks(device);
   if (locks)
   {
      locks->graphics.lock();
   }
}

extern "C" void veUnlockGraphicsQueue(VEDeviceInternal *device)
{
   if (!device || !device->queueLocks)
   {
      return;
   }

   device->queueLocks->graphics.unlock();
}

extern "C" void veLockComputeQueue(VEDeviceInternal *device)
{
   VEDeviceQueueLocks *locks = ensureQueueLocks(device);
   if (locks)
   {
      locks->compute.lock();
   }
}

extern "C" void veUnlockComputeQueue(VEDeviceInternal *device)
{
   if (!device || !device->queueLocks)
   {
      return;
   }

   device->queueLocks->compute.unlock();
}

extern "C" void veLockTransferQueue(VEDeviceInternal *device)
{
   VEDeviceQueueLocks *locks = ensureQueueLocks(device);
   if (locks)
   {
      locks->transfer.lock();
   }
}

extern "C" void veUnlockTransferQueue(VEDeviceInternal *device)
{
   if (!device || !device->queueLocks)
   {
      return;
   }

   device->queueLocks->transfer.unlock();
}

static std::mutex &fallbackMutex()
{
   static std::mutex fallback;
   return fallback;
}

std::mutex &veGetGraphicsQueueMutex(VEDeviceInternal *device)
{
   VEDeviceQueueLocks *locks = ensureQueueLocks(device);
   return locks ? locks->graphics : fallbackMutex();
}

std::mutex &veGetComputeQueueMutex(VEDeviceInternal *device)
{
   VEDeviceQueueLocks *locks = ensureQueueLocks(device);
   return locks ? locks->compute : fallbackMutex();
}

std::mutex &veGetTransferQueueMutex(VEDeviceInternal *device)
{
   VEDeviceQueueLocks *locks = ensureQueueLocks(device);
   return locks ? locks->transfer : fallbackMutex();
}


