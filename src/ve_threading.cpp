#include "ve_internal.h"

#include <new>

std::mutex &VEDeviceQueueLocks::graphicsMutex() noexcept { return graphics; }

std::mutex &VEDeviceQueueLocks::computeMutex() noexcept { return compute; }

std::mutex &VEDeviceQueueLocks::transferMutex() noexcept { return transfer; }

void VEDeviceQueueLocks::lockGraphics() { graphics.lock(); }

void VEDeviceQueueLocks::unlockGraphics() { graphics.unlock(); }

void VEDeviceQueueLocks::lockCompute() { compute.lock(); }

void VEDeviceQueueLocks::unlockCompute() { compute.unlock(); }

void VEDeviceQueueLocks::lockTransfer() { transfer.lock(); }

void VEDeviceQueueLocks::unlockTransfer() { transfer.unlock(); }

static VEDeviceQueueLocks *ensureQueueLocks(VEDeviceInternal *device)
{
   if (!device)
   {
      veSetError("Device cannot be NULL when initializing queue locks");
      return nullptr;
   }

   if (!device->queueLocks)
   {
      std::unique_ptr<VEDeviceQueueLocks> locks(new (std::nothrow) VEDeviceQueueLocks());
      if (!locks)
      {
         veSetError("Failed to allocate queue locks");
         return nullptr;
      }

      device->queueLocks.reset(locks.release());
   }

   return device->queueLocks.get();
}

void veInitializeQueueLocks(VEDeviceInternal *device) { (void)ensureQueueLocks(device); }

void veDestroyQueueLocks(VEDeviceInternal *device)
{
   if (!device)
   {
      return;
   }

   device->queueLocks.reset();
}
