/**
 * @file ve_swapchain.c
 * @brief Swapchain and Presentation Implementation
 *
 * Platform-specific surface creation for Windows, Linux, and macOS.
 *
 * Window handle format by platform:
 * - Windows: windowHandle = HWND (window handle)
 * - Linux X11: windowHandle = uintptr_t[2] where [0] = Display*, [1] = Window
 * - Linux Wayland: windowHandle = uintptr_t[2] where [0] = wl_display*, [1] =
 * wl_surface*
 * - macOS: windowHandle = NSView* (Metal view)
 */

#include "ve_internal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib.h>
#include <wayland-client.h>
#elif defined(__APPLE__)
#include <Cocoa/Cocoa.h>
#include <vulkan/vulkan_macos.h>
#endif

VESwapchainInternal::~VESwapchainInternal() { destroy(); }

void VESwapchainInternal::attachDevice(VEDeviceInternal *deviceInternal) noexcept { device = deviceInternal; }

void VESwapchainInternal::markForResize(uint32_t newWidth, uint32_t newHeight) noexcept
{
   width = newWidth;
   height = newHeight;
   requestRecreation(true);
}

VETextureIndex VESwapchainInternal::acquireNextImage()
{
   if (!device)
   {
      veSetError("Cannot acquire image - device reference not available");
      return VE_INVALID_TEXTURE_INDEX;
   }

   if (waitForCurrentFrameFence() != VE_SUCCESS)
   {
      return VE_INVALID_TEXTURE_INDEX;
   }

   VkSemaphore acquireSemaphore = imageAvailableSemaphores[currentFrame];

   uint32_t imageIndex = 0;
   VkResult result =
       vkAcquireNextImageKHR(device->device, swapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, &imageIndex);

   if (result == VK_ERROR_OUT_OF_DATE_KHR)
   {
      requestRecreation();
      return VE_INVALID_TEXTURE_INDEX;
   }
   else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
   {
      veSetError("Failed to acquire swapchain image (VkResult: %d)", result);
      return VE_INVALID_TEXTURE_INDEX;
   }

   vkResetFences(device->device, 1, &inFlightFences[currentFrame]);
   currentImageIndex = imageIndex;

   return textureIndices[imageIndex];
}

VEResult VESwapchainInternal::present(VECommandBufferInternal &cmd)
{
   if (!device)
   {
      veSetError("Cannot present image - device reference not available");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (currentImageIndex == UINT32_MAX)
   {
      veSetError("No image acquired for presentation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (cmd.isRecording)
   {
      VkResult result = vkEndCommandBuffer(cmd.commandBuffer);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to end command buffer (VkResult: %d)", result);
         return VE_ERROR_UNKNOWN;
      }
      cmd.isRecording = false;
   }
   else
   {
      printf("Could not transition the image because the command buffer was not recording\n");
   }

   VkSemaphore acquireSemaphore = imageAvailableSemaphores[currentFrame];
   VkSemaphore renderSemaphore = renderFinishedSemaphores[currentImageIndex];
   VkFence frameFence = inFlightFences[currentFrame];

   VEDeviceQueueLocks *locks = device->queueLocks.get();
   if (!locks)
   {
      veSetError("Device queue locks not initialized");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkSubmitInfo submitInfo{};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
   submitInfo.waitSemaphoreCount = 1;
   submitInfo.pWaitSemaphores = &acquireSemaphore;
   submitInfo.pWaitDstStageMask = waitStages;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &cmd.commandBuffer;
   submitInfo.signalSemaphoreCount = 1;
   submitInfo.pSignalSemaphores = &renderSemaphore;

   std::unique_lock<std::mutex> queueLock(locks->graphicsMutex());

   VkResult submitResult = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, frameFence);
   if (submitResult != VK_SUCCESS)
   {
      veSetError("Failed to submit draw command buffer (VkResult: %d)", submitResult);
      return VE_ERROR_UNKNOWN;
   }

   VkPresentInfoKHR presentInfo{};
   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   presentInfo.waitSemaphoreCount = 1;
   presentInfo.pWaitSemaphores = &renderSemaphore;
   presentInfo.swapchainCount = 1;
   presentInfo.pSwapchains = &swapchain;
   presentInfo.pImageIndices = &currentImageIndex;

   cmd.markFenceActive(frameFence);

   VkResult presentResult = vkQueuePresentKHR(device->graphicsQueue, &presentInfo);

   if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
   {
      requestRecreation();
      currentImageIndex = UINT32_MAX;
      currentFrame = (currentFrame + 1) % maxFramesInFlight;
      return VE_ERROR_SWAPCHAIN_OUT_OF_DATE;
   }
   else if (presentResult != VK_SUCCESS)
   {
      veSetError("Failed to present swapchain image (VkResult: %d)", presentResult);
      currentImageIndex = UINT32_MAX;
      currentFrame = (currentFrame + 1) % maxFramesInFlight;
      return VE_ERROR_UNKNOWN;
   }

   currentImageIndex = UINT32_MAX;
   currentFrame = (currentFrame + 1) % maxFramesInFlight;

  if (device)
  {
     using namespace std::chrono;
     const double nowSeconds = duration<double>(steady_clock::now().time_since_epoch()).count();

     if (device->lastFrameTimestampSeconds > 0.0)
     {
        double delta = nowSeconds - device->lastFrameTimestampSeconds;
        if (delta >= 0.0)
        {
           device->frameStats.frameTime = static_cast<uint64_t>(delta * 1'000'000'000.0);
        }
     }

     device->lastFrameTimestampSeconds = nowSeconds;
     device->performanceStats = device->frameStats;
     device->frameStats = {};
  }

   return VE_SUCCESS;
}

void VESwapchainInternal::querySize(uint32_t *outWidth, uint32_t *outHeight) const noexcept
{
   if (outWidth)
   {
      *outWidth = width;
   }
   if (outHeight)
   {
      *outHeight = height;
   }
}

VkFormat VESwapchainInternal::currentFormat() const noexcept { return format; }

void VESwapchainInternal::requestRecreation(bool value) noexcept { needsRecreation = value; }

bool VESwapchainInternal::hasDevice() const noexcept { return device != nullptr; }

void VESwapchainInternal::destroy()
{
   if (!device)
   {
      imageCount = 0;
      currentImageIndex = UINT32_MAX;
      width = 0;
      height = 0;
      requestRecreation(false);
      return;
   }

   if (device->device)
   {
      vkDeviceWaitIdle(device->device);
   }

   releaseTextureIndices();
   destroySyncObjects();
   destroyImageViews();
   destroySurfaceAndSwapchain();

   imageCount = 0;
   currentImageIndex = UINT32_MAX;
   width = 0;
   height = 0;
   requestRecreation(false);
   device = nullptr;
}

VEResult VESwapchainInternal::waitForCurrentFrameFence()
{
   if (!device)
   {
      veSetError("Cannot acquire image - device reference not available");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkFence currentFrameFence = inFlightFences[currentFrame];
   VkResult status = vkGetFenceStatus(device->device, currentFrameFence);

   if (status != VK_SUCCESS && status != VK_NOT_READY)
   {
      veSetError("Failed to get fence status (VkResult: %d)", status);
      return VE_ERROR_UNKNOWN;
   }

   if (status == VK_NOT_READY)
   {
      vkWaitForFences(device->device, 1, &currentFrameFence, VK_TRUE, UINT64_MAX);
   }

   veNotifyCommandBufferFenceSignaled(device, currentFrameFence);
   return VE_SUCCESS;
}

void VESwapchainInternal::releaseTextureIndices()
{
   if (!device)
   {
      return;
   }

   for (uint32_t i = 0; i < imageCount; ++i)
   {
      if (textureIndices[i] != VE_INVALID_TEXTURE_INDEX)
      {
         device->freeTextureIndex(textureIndices[i]);
         memset(&device->textures[textureIndices[i]], 0, sizeof(VETextureInternal));
         textureIndices[i] = VE_INVALID_TEXTURE_INDEX;
      }
   }
}

void VESwapchainInternal::destroySyncObjects()
{
   if (!device || !device->device)
   {
      return;
   }

   for (uint32_t i = 0; i < VE_MAX_FRAMES_IN_FLIGHT; ++i)
   {
      if (imageAvailableSemaphores[i])
      {
         vkDestroySemaphore(device->device, imageAvailableSemaphores[i], NULL);
         imageAvailableSemaphores[i] = VK_NULL_HANDLE;
      }
      if (inFlightFences[i])
      {
         vkDestroyFence(device->device, inFlightFences[i], NULL);
         inFlightFences[i] = VK_NULL_HANDLE;
      }
   }

   for (uint32_t i = 0; i < imageCount; ++i)
   {
      if (renderFinishedSemaphores[i])
      {
         vkDestroySemaphore(device->device, renderFinishedSemaphores[i], NULL);
         renderFinishedSemaphores[i] = VK_NULL_HANDLE;
      }
   }
}

void VESwapchainInternal::destroyImageViews()
{
   if (!device || !device->device)
   {
      return;
   }

   for (uint32_t i = 0; i < imageCount; ++i)
   {
      if (imageViews[i])
      {
         vkDestroyImageView(device->device, imageViews[i], NULL);
         imageViews[i] = VK_NULL_HANDLE;
      }
   }
}

void VESwapchainInternal::destroySurfaceAndSwapchain()
{
   if (device && device->device && swapchain)
   {
      vkDestroySwapchainKHR(device->device, swapchain, NULL);
      swapchain = VK_NULL_HANDLE;
   }

   if (device && device->context && surface)
   {
      vkDestroySurfaceKHR(device->context->instance, surface, NULL);
      surface = VK_NULL_HANDLE;
   }
}

// =============================================================================
// Platform-Specific Surface Creation
// =============================================================================

VkResult veCreateSurface(VEContextInternal *context, void *windowHandle, VkSurfaceKHR *surface)
{
   if (!context || !windowHandle || !surface)
   {
      return VK_ERROR_INITIALIZATION_FAILED;
   }

#ifdef _WIN32
   // Windows surface creation
   VkWin32SurfaceCreateInfoKHR createInfo{};
   createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
   createInfo.hinstance = GetModuleHandle(NULL);
   createInfo.hwnd = reinterpret_cast<HWND>(windowHandle);

   PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR =
       (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(context->instance, "vkCreateWin32SurfaceKHR");

   if (!vkCreateWin32SurfaceKHR)
   {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
   }

   return vkCreateWin32SurfaceKHR(context->instance, &createInfo, NULL, surface);

#elif defined(__linux__)
   // Linux surface creation - try X11 first, then Wayland
   // Note: In a real implementation, you would detect the windowing system at
   // runtime

   // Try X11 surface creation
   PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR =
       (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(context->instance, "vkCreateXlibSurfaceKHR");

   if (vkCreateXlibSurfaceKHR)
   {
      VkXlibSurfaceCreateInfoKHR createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
      auto handles = reinterpret_cast<uintptr_t *>(windowHandle);
      createInfo.dpy = reinterpret_cast<Display *>(handles[0]);
      createInfo.window = static_cast<Window>(handles[1]);

      return vkCreateXlibSurfaceKHR(context->instance, &createInfo, NULL, surface);
   }

   // Try Wayland surface creation if X11 failed
   PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR =
       (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(context->instance, "vkCreateWaylandSurfaceKHR");

   if (vkCreateWaylandSurfaceKHR)
   {
      VkWaylandSurfaceCreateInfoKHR createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
      auto handles = reinterpret_cast<uintptr_t *>(windowHandle);
      createInfo.display = reinterpret_cast<struct wl_display *>(handles[0]);
      createInfo.surface = reinterpret_cast<struct wl_surface *>(handles[1]);

      return vkCreateWaylandSurfaceKHR(context->instance, &createInfo, NULL, surface);
   }

   return VK_ERROR_EXTENSION_NOT_PRESENT;

#elif defined(__APPLE__)
   // macOS surface creation
   VkMacOSSurfaceCreateInfoMVK createInfo{};
   createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
   createInfo.pView = windowHandle;

   PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK =
       (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(context->instance, "vkCreateMacOSSurfaceMVK");

   if (!vkCreateMacOSSurfaceMVK)
   {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
   }

   return vkCreateMacOSSurfaceMVK(context->instance, &createInfo, NULL, surface);

#else
   // Unsupported platform
   (void)context;
   (void)windowHandle;
   (void)surface;
   return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

// =============================================================================
// Swapchain Support Detection
// =============================================================================

static bool checkSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
   uint32_t formatCount;
   vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);

   uint32_t presentModeCount;
   vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);

   return formatCount > 0 && presentModeCount > 0;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkPhysicalDevice device, VkSurfaceKHR surface,
                                                  VkFormat preferredFormat)
{
   uint32_t formatCount;
   vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);

   std::vector<VkSurfaceFormatKHR> availableFormats(formatCount);
   vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, availableFormats.data());

   // Look for preferred format
   for (uint32_t i = 0; i < formatCount; i++)
   {
      if (availableFormats[i].format == preferredFormat &&
          availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      {
         return availableFormats[i];
      }
   }

   // Fallback to first available format
   return availableFormats.empty() ? VkSurfaceFormatKHR{} : availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(VkPhysicalDevice device, VkSurfaceKHR surface, bool vsync)
{
   if (!vsync)
   {
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
   }

   uint32_t presentModeCount;
   vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);

   std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
   vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, availablePresentModes.data());

   VkPresentModeKHR selectedMode = VK_PRESENT_MODE_FIFO_KHR;
   // Prefer mailbox mode for low latency
   for (uint32_t i = 0; i < presentModeCount; i++)
   {
      if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
      {
         selectedMode = VK_PRESENT_MODE_MAILBOX_KHR;
         break;
      }
   }

   return selectedMode;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR *capabilities, uint32_t width, uint32_t height)
{
   if (capabilities->currentExtent.width != UINT32_MAX)
   {
      return capabilities->currentExtent;
   }

   VkExtent2D actualExtent{};
   actualExtent.width = width;
   actualExtent.height = height;

   actualExtent.width =
       std::clamp(actualExtent.width, capabilities->minImageExtent.width, capabilities->maxImageExtent.width);
   actualExtent.height =
       std::clamp(actualExtent.height, capabilities->minImageExtent.height, capabilities->maxImageExtent.height);

   return actualExtent;
}

// =============================================================================
// Swapchain Implementation
// =============================================================================

VESwapchain *veCreateSwapchain(VEDevice *device, void *windowHandle, uint32_t width, uint32_t height, VkFormat format,
                               bool vsync)
{
   if (!device || !windowHandle || width == 0 || height == 0)
   {
      veSetError("Invalid parameters for swapchain creation");
      return NULL;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   std::unique_ptr<VESwapchainInternal> swapchain(new (std::nothrow) VESwapchainInternal());
   if (!swapchain)
   {
      veSetError("Failed to allocate swapchain memory");
      return NULL;
   }

   swapchain->attachDevice(deviceInternal);
   swapchain->format = format;
   swapchain->width = width;
   swapchain->height = height;
   swapchain->maxFramesInFlight = VE_MAX_FRAMES_IN_FLIGHT;
   swapchain->currentFrame = 0;
   swapchain->currentImageIndex = UINT32_MAX;
   std::fill_n(&swapchain->textureIndices[0], VE_MAX_SWAPCHAIN_IMAGES, VE_INVALID_TEXTURE_INDEX);

   // Create surface
   VkResult result = veCreateSurface(deviceInternal->context, windowHandle, &swapchain->surface);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create surface (VkResult: %d)", result);
      return NULL;
   }

   // Check swapchain support
   if (!checkSwapchainSupport(deviceInternal->physicalDevice, swapchain->surface))
   {
      veSetError("Swapchain not supported on this device");
      return NULL;
   }

   // Get surface capabilities
   VkSurfaceCapabilitiesKHR capabilities;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deviceInternal->physicalDevice, swapchain->surface, &capabilities);

   // Choose surface format
   VkSurfaceFormatKHR surfaceFormat =
       chooseSwapSurfaceFormat(deviceInternal->physicalDevice, swapchain->surface, format);
   swapchain->format = surfaceFormat.format;

   // Choose present mode
   VkPresentModeKHR presentMode = chooseSwapPresentMode(deviceInternal->physicalDevice, swapchain->surface, vsync);

   // Choose extent
   VkExtent2D extent = chooseSwapExtent(&capabilities, width, height);
   swapchain->width = extent.width;
   swapchain->height = extent.height;

   // Choose image count
   uint32_t imageCount = capabilities.minImageCount + 1;
   if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
   {
      imageCount = capabilities.maxImageCount;
   }

   if (imageCount > VE_MAX_SWAPCHAIN_IMAGES)
   {
      imageCount = VE_MAX_SWAPCHAIN_IMAGES;
   }

   // Create swapchain
   VkSwapchainCreateInfoKHR createInfo{};
   createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   createInfo.surface = swapchain->surface;
   createInfo.minImageCount = imageCount;
   createInfo.imageFormat = surfaceFormat.format;
   createInfo.imageColorSpace = surfaceFormat.colorSpace;
   createInfo.imageExtent = extent;
   createInfo.imageArrayLayers = 1;
   createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

   // Handle queue families
   uint32_t queueFamilyIndices[] = {
       deviceInternal->queueFamilies.graphicsFamily // Assume graphics queue can present
   };

   createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
   createInfo.queueFamilyIndexCount = 1;
   createInfo.pQueueFamilyIndices = queueFamilyIndices;

   createInfo.preTransform = capabilities.currentTransform;
   createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   createInfo.presentMode = presentMode;
   createInfo.clipped = VK_TRUE;
   createInfo.oldSwapchain = VK_NULL_HANDLE;

   result = vkCreateSwapchainKHR(deviceInternal->device, &createInfo, NULL, &swapchain->swapchain);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create swapchain (VkResult: %d)", result);
      return NULL;
   }

   // Get swapchain images
   vkGetSwapchainImagesKHR(deviceInternal->device, swapchain->swapchain, &swapchain->imageCount, NULL);
   vkGetSwapchainImagesKHR(deviceInternal->device, swapchain->swapchain, &swapchain->imageCount, swapchain->images);

   // Create image views
   for (uint32_t i = 0; i < swapchain->imageCount; i++)
   {
      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image = swapchain->images[i];
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = surfaceFormat.format;
      viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel = 0;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount = 1;

      result = vkCreateImageView(deviceInternal->device, &viewInfo, NULL, &swapchain->imageViews[i]);
      if (result != VK_SUCCESS)
      {
         veSetError("Failed to create image view %u (VkResult: %d)", i, result);
         return NULL;
      }

      // Create texture index for bindless access
      uint32_t textureIndex = deviceInternal->allocateTextureIndex();
      if (textureIndex != VE_INVALID_TEXTURE_INDEX)
      {
         VETextureInternal *texture = &deviceInternal->textures[textureIndex];
         texture->image = swapchain->images[i];
         texture->imageView = swapchain->imageViews[i];
         texture->width = swapchain->width;
         texture->height = swapchain->height;
         texture->depth = 1;
         texture->mipLevels = 1;
         texture->arrayLayers = 1;
         texture->format = swapchain->format;
         texture->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
         texture->sampleCount = VK_SAMPLE_COUNT_1_BIT;
         texture->index = textureIndex;
         texture->isValid = true;
         snprintf(texture->debugName, sizeof(texture->debugName), "SwapchainImage_%u", i);

         swapchain->textureIndices[i] = textureIndex;
         deviceInternal->updateTextureDescriptor(textureIndex);
      }
      else
      {
         swapchain->textureIndices[i] = VE_INVALID_TEXTURE_INDEX;
      }
   }

   // Create synchronization objects
   swapchain->maxFramesInFlight = VE_MAX_FRAMES_IN_FLIGHT;
   swapchain->currentFrame = 0;

   // Create per-frame synchronization objects
   for (uint32_t i = 0; i < VE_MAX_FRAMES_IN_FLIGHT; i++)
   {
      VkSemaphoreCreateInfo semInfo{};
      semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled

      if (vkCreateSemaphore(deviceInternal->device, &semInfo, NULL, &swapchain->imageAvailableSemaphores[i]) !=
              VK_SUCCESS ||
          vkCreateFence(deviceInternal->device, &fenceInfo, NULL, &swapchain->inFlightFences[i]) != VK_SUCCESS)
      {
         veSetError("Cannot create sync objects");
         return NULL;
      }
   }

   // Create per-swapchain-image semaphores
   for (uint32_t i = 0; i < swapchain->imageCount; i++)
   {
      VkSemaphoreCreateInfo semInfo{};
      semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      if (vkCreateSemaphore(deviceInternal->device, &semInfo, NULL, &swapchain->renderFinishedSemaphores[i]) !=
          VK_SUCCESS)
      {
         veSetError("Cannot create sync objects");
         return NULL;
      }
   }

   swapchain->currentImageIndex = UINT32_MAX;
   swapchain->needsRecreation = false;

   return reinterpret_cast<VESwapchain *>(swapchain.release());
}

void veDestroySwapchain(VESwapchain *swapchain)
{
   if (!swapchain)
      return;

   delete reinterpret_cast<VESwapchainInternal *>(swapchain);
}

VETextureIndex veAcquireNextImage(VESwapchain *swapchain)
{
   if (!swapchain)
   {
      veSetError("Swapchain cannot be NULL");
      return VE_INVALID_TEXTURE_INDEX;
   }

   VESwapchainInternal *internal = reinterpret_cast<VESwapchainInternal *>(swapchain);
   return internal->acquireNextImage();
}

VEResult vePresentImage(VESwapchain *swapchain, VECommandBuffer *cmd)
{
   if (!swapchain || !cmd)
   {
      veSetError("Invalid parameters for image presentation");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VESwapchainInternal *internal = reinterpret_cast<VESwapchainInternal *>(swapchain);
   VECommandBufferInternal *cmdInternal = reinterpret_cast<VECommandBufferInternal *>(cmd);
   return internal->present(*cmdInternal);
}

VEResult veResizeSwapchain(VESwapchain *swapchain, uint32_t width, uint32_t height)
{
   if (!swapchain || width == 0 || height == 0)
   {
      veSetError("Invalid parameters for swapchain resize");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VESwapchainInternal *internal = reinterpret_cast<VESwapchainInternal *>(swapchain);
   internal->markForResize(width, height);

   return VE_SUCCESS;
}

VEResult veGetSwapchainSize(VESwapchain *swapchain, uint32_t *width, uint32_t *height)
{
   if (!swapchain)
   {
      veSetError("Swapchain cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VESwapchainInternal *internal = reinterpret_cast<VESwapchainInternal *>(swapchain);
   internal->querySize(width, height);

   return VE_SUCCESS;
}

VkFormat veGetSwapchainFormat(VESwapchain *swapchain)
{
   if (!swapchain)
      return VK_FORMAT_R8G8B8_UNORM;

   VESwapchainInternal *internal = reinterpret_cast<VESwapchainInternal *>(swapchain);
   return internal->currentFormat();
}
