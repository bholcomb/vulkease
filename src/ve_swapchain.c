/**
 * @file ve_swapchain.c
 * @brief Swapchain and Presentation Implementation
 * 
 * Platform-specific surface creation for Windows, Linux, and macOS.
 * 
 * Window handle format by platform:
 * - Windows: windowHandle = HWND (window handle)
 * - Linux X11: windowHandle = uintptr_t[2] where [0] = Display*, [1] = Window  
 * - Linux Wayland: windowHandle = uintptr_t[2] where [0] = wl_display*, [1] = wl_surface*
 * - macOS: windowHandle = NSView* (Metal view)
 */

#include "ve_internal.h"
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <wayland-client.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#elif defined(__APPLE__)
#include <Cocoa/Cocoa.h>
#include <vulkan/vulkan_macos.h>
#endif

// =============================================================================
// Platform-Specific Surface Creation
// =============================================================================

VkResult veCreateSurface(VEContextInternal* context, void* windowHandle, VkSurfaceKHR* surface) {
    if (!context || !windowHandle || !surface) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

#ifdef _WIN32
    // Windows surface creation
    VkWin32SurfaceCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(NULL);
    createInfo.hwnd = (HWND)windowHandle;

    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = 
        (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(context->instance, "vkCreateWin32SurfaceKHR");
    
    if (!vkCreateWin32SurfaceKHR) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    
    return vkCreateWin32SurfaceKHR(context->instance, &createInfo, NULL, surface);

#elif defined(__linux__)
    // Linux surface creation - try X11 first, then Wayland
    // Note: In a real implementation, you would detect the windowing system at runtime
    
    // Try X11 surface creation
    PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR = 
        (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(context->instance, "vkCreateXlibSurfaceKHR");
    
    if (vkCreateXlibSurfaceKHR) {
        VkXlibSurfaceCreateInfoKHR createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        createInfo.dpy = (Display*)((uintptr_t*)windowHandle)[0];  // Display* passed as first element
        createInfo.window = (Window)((uintptr_t*)windowHandle)[1]; // Window passed as second element
        
        return vkCreateXlibSurfaceKHR(context->instance, &createInfo, NULL, surface);
    }
    
    // Try Wayland surface creation if X11 failed
    PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR = 
        (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(context->instance, "vkCreateWaylandSurfaceKHR");
    
    if (vkCreateWaylandSurfaceKHR) {
        VkWaylandSurfaceCreateInfoKHR createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        createInfo.display = (struct wl_display*)((uintptr_t*)windowHandle)[0];
        createInfo.surface = (struct wl_surface*)((uintptr_t*)windowHandle)[1];
        
        return vkCreateWaylandSurfaceKHR(context->instance, &createInfo, NULL, surface);
    }
    
    return VK_ERROR_EXTENSION_NOT_PRESENT;

#elif defined(__APPLE__)
    // macOS surface creation
    VkMacOSSurfaceCreateInfoMVK createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    createInfo.pView = windowHandle;

    PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK = 
        (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(context->instance, "vkCreateMacOSSurfaceMVK");
    
    if (!vkCreateMacOSSurfaceMVK) {
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

static bool checkSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);

    return formatCount > 0 && presentModeCount > 0;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkPhysicalDevice device, VkSurfaceKHR surface, VkFormat preferredFormat) {
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);

    VkSurfaceFormatKHR* availableFormats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, availableFormats);

    // Look for preferred format
    for (uint32_t i = 0; i < formatCount; i++) {
        if (availableFormats[i].format == preferredFormat && 
            availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            VkSurfaceFormatKHR result = availableFormats[i];
            free(availableFormats);
            return result;
        }
    }

    // Fallback to first available format
    VkSurfaceFormatKHR result = availableFormats[0];
    free(availableFormats);
    return result;
}

static VkPresentModeKHR chooseSwapPresentMode(VkPhysicalDevice device, VkSurfaceKHR surface, bool vsync) {
    if(!vsync)
    {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);

    VkPresentModeKHR* availablePresentModes = malloc(presentModeCount * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, availablePresentModes);

    VkPresentModeKHR ret = 0;
    // Prefer mailbox mode for low latency
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            ret = VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    if(ret == 0)
    {
        // Fallback to FIFO (always supported)
        ret = VK_PRESENT_MODE_FIFO_KHR;
    }

    free(availablePresentModes);
    return ret;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities, uint32_t width, uint32_t height) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    }

    VkExtent2D actualExtent = {width, height};

    actualExtent.width = fmax(capabilities->minImageExtent.width, 
                             fmin(capabilities->maxImageExtent.width, actualExtent.width));
    actualExtent.height = fmax(capabilities->minImageExtent.height,
                              fmin(capabilities->maxImageExtent.height, actualExtent.height));

    return actualExtent;
}

// =============================================================================
// Swapchain Implementation
// =============================================================================

VESwapchain* veCreateSwapchain(VEDevice* device, void* windowHandle,
                              uint32_t width, uint32_t height, VkFormat format, bool vsync) {
    if (!device || !windowHandle || width == 0 || height == 0) {
        veSetError("Invalid parameters for swapchain creation");
        return NULL;
    }

    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;

    VESwapchainInternal* swapchain = calloc(1, sizeof(VESwapchainInternal));
    if (!swapchain) {
        veSetError("Failed to allocate swapchain memory");
        return NULL;
    }

    swapchain->format = format;
    swapchain->width = width;
    swapchain->height = height;
    swapchain->device = device;

    // Create surface
    VkResult result = veCreateSurface(deviceInternal->context, windowHandle, &swapchain->surface);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create surface (VkResult: %d)", result);
        free(swapchain);
        return NULL;
    }

    // Check swapchain support
    if (!checkSwapchainSupport(deviceInternal->physicalDevice, swapchain->surface)) {
        veSetError("Swapchain not supported on this device");
        vkDestroySurfaceKHR(deviceInternal->context->instance, swapchain->surface, NULL);
        free(swapchain);
        return NULL;
    }

    // Get surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deviceInternal->physicalDevice, swapchain->surface, &capabilities);

    // Choose surface format
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(deviceInternal->physicalDevice, swapchain->surface, format);
    swapchain->format = surfaceFormat.format;

    // Choose present mode
    VkPresentModeKHR presentMode = chooseSwapPresentMode(deviceInternal->physicalDevice, swapchain->surface, vsync);

    // Choose extent
    VkExtent2D extent = chooseSwapExtent(&capabilities, width, height);
    swapchain->width = extent.width;
    swapchain->height = extent.height;

    // Choose image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    if (imageCount > VE_MAX_SWAPCHAIN_IMAGES) {
        imageCount = VE_MAX_SWAPCHAIN_IMAGES;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {0};
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
        deviceInternal->queueFamilies.graphicsFamily  // Assume graphics queue can present
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
    if (result != VK_SUCCESS) {
        veSetError("Failed to create swapchain (VkResult: %d)", result);
        vkDestroySurfaceKHR(deviceInternal->context->instance, swapchain->surface, NULL);
        free(swapchain);
        return NULL;
    }

    // Get swapchain images
    vkGetSwapchainImagesKHR(deviceInternal->device, swapchain->swapchain, &swapchain->imageCount, NULL);
    vkGetSwapchainImagesKHR(deviceInternal->device, swapchain->swapchain, &swapchain->imageCount, swapchain->images);

    // Create image views
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {0};
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
        if (result != VK_SUCCESS) {
            veSetError("Failed to create image view %u (VkResult: %d)", i, result);
            veDestroySwapchain((VESwapchain*)swapchain);
            return NULL;
        }

        // Create texture index for bindless access
        uint32_t textureIndex = veAllocateTextureIndex(deviceInternal);
        if (textureIndex != VE_INVALID_TEXTURE_INDEX) {
            VETextureInternal* texture = &deviceInternal->textures[textureIndex];
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
            veUpdateTextureDescriptor(deviceInternal, textureIndex);
        } else {
            swapchain->textureIndices[i] = VE_INVALID_TEXTURE_INDEX;
        }
    }

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    result = vkCreateSemaphore(deviceInternal->device, &semaphoreInfo, NULL, &swapchain->imageAvailableSemaphore);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create image available semaphore (VkResult: %d)", result);
        veDestroySwapchain((VESwapchain*)swapchain);
        return NULL;
    }

    result = vkCreateSemaphore(deviceInternal->device, &semaphoreInfo, NULL, &swapchain->renderFinishedSemaphore);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create render finished semaphore (VkResult: %d)", result);
        veDestroySwapchain((VESwapchain*)swapchain);
        return NULL;
    }

    result = vkCreateFence(deviceInternal->device, &fenceInfo, NULL, &swapchain->inFlightFence);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create in-flight fence (VkResult: %d)", result);
        veDestroySwapchain((VESwapchain*)swapchain);
        return NULL;
    }

    swapchain->currentImageIndex = UINT32_MAX;
    swapchain->needsRecreation = false;

    return (VESwapchain*)swapchain;
}

void veDestroySwapchain(VESwapchain* swapchain) {
    if (!swapchain) return;

    VESwapchainInternal* internal = (VESwapchainInternal*)swapchain;
    
    // Get device from context (limitation of current design)
    VEDeviceInternal* device = internal->device;
    
    if (device && device->device) {
        vkDeviceWaitIdle(device->device);

        // Clean up texture indices
        for (uint32_t i = 0; i < internal->imageCount; i++) {
            if (internal->textureIndices[i] != VE_INVALID_TEXTURE_INDEX) {
                veFreeTextureIndex(device, internal->textureIndices[i]);
                memset(&device->textures[internal->textureIndices[i]], 0, sizeof(VETextureInternal));
            }
        }

        // Destroy synchronization objects
        if (internal->inFlightFence) {
            vkDestroyFence(device->device, internal->inFlightFence, NULL);
        }
        if (internal->renderFinishedSemaphore) {
            vkDestroySemaphore(device->device, internal->renderFinishedSemaphore, NULL);
        }
        if (internal->imageAvailableSemaphore) {
            vkDestroySemaphore(device->device, internal->imageAvailableSemaphore, NULL);
        }

        // Destroy image views
        for (uint32_t i = 0; i < internal->imageCount; i++) {
            if (internal->imageViews[i]) {
                vkDestroyImageView(device->device, internal->imageViews[i], NULL);
            }
        }

        // Destroy swapchain
        if (internal->swapchain) {
            vkDestroySwapchainKHR(device->device, internal->swapchain, NULL);
        }

        // Destroy surface
        if (internal->surface) {
            vkDestroySurfaceKHR(device->context->instance, internal->surface, NULL);
        }
    }

    free(internal);
}

VETextureIndex veAcquireNextImage(VESwapchain* swapchain) {
    if (!swapchain) {
        veSetError("Swapchain cannot be NULL");
        return VE_INVALID_TEXTURE_INDEX;
    }

    VESwapchainInternal* internal = (VESwapchainInternal*)swapchain;
    
    // Get device from context (limitation of current design)
    VEDeviceInternal* device = internal->device;
    if (!device) {
        veSetError("Cannot acquire image - device reference not available");
        return VE_INVALID_TEXTURE_INDEX;
    }

    // Wait for previous frame
    VkResult status = vkGetFenceStatus(device->device, internal->inFlightFence);
    if(status != VK_SUCCESS && status != VK_NOT_READY)
    {
        veSetError("Failed to get swapchain fence status (VkResult: %d)", status);
        exit(-1);
    }
    if(status == VK_NOT_READY)
    {
        vkWaitForFences(device->device, 1, &internal->inFlightFence, VK_TRUE, UINT64_MAX);
    }

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device->device, internal->swapchain, UINT64_MAX,
                                          internal->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        internal->needsRecreation = true;
        return VE_INVALID_TEXTURE_INDEX;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        veSetError("Failed to acquire swapchain image (VkResult: %d)", result);
        return VE_INVALID_TEXTURE_INDEX;
    }

    vkResetFences(device->device, 1, &internal->inFlightFence);

    internal->currentImageIndex = imageIndex;
    return internal->textureIndices[imageIndex];
}

VEResult vePresentImage(VESwapchain* swapchain, VECommandBuffer* cmd) {
    if (!swapchain || !cmd) {
        veSetError("Invalid parameters for image presentation");
        return VE_ERROR_INVALID_PARAMETER;
    }

    VESwapchainInternal* internal = (VESwapchainInternal*)swapchain;
    VECommandBufferInternal* cmdInternal = (VECommandBufferInternal*)cmd;
    
    if (internal->currentImageIndex == UINT32_MAX) {
        veSetError("No image acquired for presentation");
        return VE_ERROR_INVALID_PARAMETER;
    }

    // Get device from context (limitation of current design)
    VEDeviceInternal* device = internal->device;
    if (!device) {
        veSetError("Cannot present image - device reference not available");
        return VE_ERROR_INVALID_PARAMETER;
    }

    // End command buffer if still recording
    if (cmdInternal->isRecording) {
        VkResult result = vkEndCommandBuffer(cmdInternal->commandBuffer);
        if (result != VK_SUCCESS) {
            veSetError("Failed to end command buffer (VkResult: %d)", result);
            return VE_ERROR_UNKNOWN;
        }
        cmdInternal->isRecording = false;
    }
    else
    {
        printf("Could not transition the image because the command buffer was not recording\n");
    }

    // Submit command buffer
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {internal->imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdInternal->commandBuffer;

    VkSemaphore signalSemaphores[] = {internal->renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult result = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, internal->inFlightFence);
    if (result != VK_SUCCESS) {
        veSetError("Failed to submit draw command buffer (VkResult: %d)", result);
        return VE_ERROR_UNKNOWN;
    }

    // Present
    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {internal->swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &internal->currentImageIndex;
    presentInfo.pResults = NULL;

    result = vkQueuePresentKHR(device->graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        internal->needsRecreation = true;
        return VE_ERROR_SWAPCHAIN_OUT_OF_DATE;
    } else if (result != VK_SUCCESS) {
        veSetError("Failed to present swapchain image (VkResult: %d)", result);
        return VE_ERROR_UNKNOWN;
    }

    internal->currentImageIndex = UINT32_MAX;

    veFreeCommandBuffer(cmdInternal);

    return VE_SUCCESS;
}

VEResult veResizeSwapchain(VESwapchain* swapchain, uint32_t width, uint32_t height) {
    if (!swapchain || width == 0 || height == 0) {
        veSetError("Invalid parameters for swapchain resize");
        return VE_ERROR_INVALID_PARAMETER;
    }

    VESwapchainInternal* internal = (VESwapchainInternal*)swapchain;
    internal->width = width;
    internal->height = height;
    internal->needsRecreation = true;

    // In a full implementation, we would recreate the swapchain here
    // For now, just mark it as needing recreation
    return VE_SUCCESS;
}

VEResult veGetSwapchainSize(VESwapchain* swapchain, uint32_t* width, uint32_t* height) {
    if (!swapchain) {
        veSetError("Swapchain cannot be NULL");
        return VE_ERROR_INVALID_PARAMETER;
    }

    VESwapchainInternal* internal = (VESwapchainInternal*)swapchain;
    
    if (width) *width = internal->width;
    if (height) *height = internal->height;

    return VE_SUCCESS;
}

VkFormat veGetSwapchainFormat(VESwapchain* swapchain) {
    if (!swapchain) return VK_FORMAT_R8G8B8_UNORM;
    
    VESwapchainInternal* internal = (VESwapchainInternal*)swapchain;
    return internal->format;
}
