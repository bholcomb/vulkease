#include "ve_internal.h"

#include <new>

std::mutex &VEDeviceQueueLocks::graphicsMutex() noexcept { return queue; }

std::mutex &VEDeviceQueueLocks::computeMutex() noexcept { return queue; }

std::mutex &VEDeviceQueueLocks::transferMutex() noexcept { return queue; }

void VEDeviceQueueLocks::lockGraphics() { queue.lock(); }

void VEDeviceQueueLocks::unlockGraphics() { queue.unlock(); }

void VEDeviceQueueLocks::lockCompute() { queue.lock(); }

void VEDeviceQueueLocks::unlockCompute() { queue.unlock(); }

void VEDeviceQueueLocks::lockTransfer() { queue.lock(); }

void VEDeviceQueueLocks::unlockTransfer() { queue.unlock(); }

static VEDeviceQueueLocks *ensureQueueLocks(VEDeviceInternal *device)
{
   if (!device)
   {
      veSetError("Device cannot be NULL when initializing queue locks");
      return nullptr;
   }

   if (!device->queueLocks)
   {
      device->queueLocks = std::make_unique<VEDeviceQueueLocks>();
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
