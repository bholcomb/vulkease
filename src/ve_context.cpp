/**
 * @file ve_context.c
 * @brief Context and Device Management Implementation
 */

#include "ve_internal.h"

#include <new>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// =============================================================================
// Internal Constants and Limits
// =============================================================================

#define VE_MAX_PHYSICAL_DEVICES 8
#define VE_MAX_QUEUE_FAMILIES 16
#define VE_MAX_EXTENSIONS 64
#define VE_MAX_LAYERS 16

// Base required extensions for VulkEase 2.0
static const char *BASE_INSTANCE_EXTENSIONS[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                                 VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                                                 VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME};

// Platform-specific surface extensions (define constants if not available)
#ifndef VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME "VK_KHR_xlib_surface"
#endif
#ifndef VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#define VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME "VK_KHR_wayland_surface"
#endif
#ifndef VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#define VK_MVK_MACOS_SURFACE_EXTENSION_NAME "VK_MVK_macos_surface"
#endif

static const char *PLATFORM_SURFACE_EXTENSIONS[] = {
#ifdef _WIN32
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
//    VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
    VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#endif
};

static const char *REQUIRED_DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_SHADER_OBJECT_EXTENSION_NAME,              // Still required - not promoted to core
    VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,   // Still required - not promoted to core
    VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, // Still required - not promoted to core
    VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME             // Optional but highly beneficial
};

static const char *VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};

// global external functions struct
VEFuncs veFuncs;

// =============================================================================
// Error Handling
// =============================================================================

static char g_lastError[512] = {0};

void veSetError(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vsnprintf(g_lastError, sizeof(g_lastError), format, args);
   va_end(args);

   printf("VULKEASE ERROR: %s", g_lastError);
}

const char *veGetLastError(void) { return g_lastError[0] ? g_lastError : "No error"; }

const char *getMessageTypeString(VkDebugUtilsMessageTypeFlagsEXT messageType)
{
   switch (messageType)
   {
   case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
      return "General";
   case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
      return "Validation";
   case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
      return "Performance";
   case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:
      return "Device Address Binding";
   }

   return "Unknown";
}

// Debug messenger callback
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void *pUserData)
{
   (void)pUserData; // suppresses unused parameter warning

   const char *severity = "INFO";
   if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
   {
      severity = "ERROR";
   }
   else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
   {
      severity = "WARNING";
   }

   printf("[VulkEase %s]-%s: %s\n", severity, getMessageTypeString(messageType), pCallbackData->pMessage);
   return VK_FALSE;
}

const char *veResultToString(VkResult result)
{
   switch (result)
   {
   case VK_SUCCESS:
      return "VK_SUCCESS";
   case VK_NOT_READY:
      return "VK_NOT_READY";
   case VK_TIMEOUT:
      return "VK_TIMEOUT";
   case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "VK_ERROR_OUT_OF_HOST_MEMORY";
   case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
   case VK_ERROR_INITIALIZATION_FAILED:
      return "VK_ERROR_INITIALIZATION_FAILED";
   case VK_ERROR_DEVICE_LOST:
      return "VK_ERROR_DEVICE_LOST";
   case VK_ERROR_MEMORY_MAP_FAILED:
      return "VK_ERROR_MEMORY_MAP_FAILED";
   default:
      return "Unknown VkResult";
   }
}

void vePrintVkResult(const char *operation, VkResult result)
{
   if (result != VK_SUCCESS)
   {
      printf("VulkEase: %s failed with %s (%d)\n", operation, veResultToString(result), result);
   }
}

// =============================================================================
// Extension Support Checking
// =============================================================================

static bool checkInstanceExtensionSupport(const char **requiredExtensions, uint32_t requiredCount)
{
   uint32_t extensionCount;
   vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);

   std::vector<VkExtensionProperties> extensions(extensionCount);
   vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions.data());

   bool allSupported = true;
   for (uint32_t i = 0; i < requiredCount; i++)
   {
      bool found = false;
      for (uint32_t j = 0; j < extensionCount; j++)
      {
         if (strcmp(requiredExtensions[i], extensions[j].extensionName) == 0)
         {
            found = true;
            break;
         }
      }
      if (!found)
      {
         printf("CRITICAL: Required instance extension not available: %s\n", requiredExtensions[i]);
         printf("This indicates outdated GPU drivers or incompatible hardware.\n");
         allSupported = false;
      }
   }

   return allSupported;
}

#ifdef DEBUG
static bool checkValidationLayerSupport(void)
{
   uint32_t layerCount;
   vkEnumerateInstanceLayerProperties(&layerCount, NULL);

   std::vector<VkLayerProperties> layers(layerCount);
   vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

   bool found = false;
   for (uint32_t i = 0; i < layerCount; i++)
   {
      if (strcmp(VALIDATION_LAYERS[0], layers[i].layerName) == 0)
      {
         found = true;
         break;
      }
   }

   return found;
}
#endif

static bool checkDeviceExtensionSupport(VkPhysicalDevice device, const char **requiredExtensions,
                                        uint32_t requiredCount)
{
   uint32_t extensionCount;
   vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

   std::vector<VkExtensionProperties> extensions(extensionCount);
   vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, extensions.data());

   bool allSupported = true;
   for (uint32_t i = 0; i < requiredCount; i++)
   {
      bool found = false;
      for (uint32_t j = 0; j < extensionCount; j++)
      {
         if (strcmp(requiredExtensions[i], extensions[j].extensionName) == 0)
         {
            found = true;
            break;
         }
      }
      if (!found)
      {
         allSupported = false;
      }
   }

   return allSupported;
}

// =============================================================================
// GPU Capability Validation
// =============================================================================

static bool validateMinimumGPUCapabilities(VkInstance instance)
{
   uint32_t deviceCount = 0;
   vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

   if (deviceCount == 0)
   {
      return false;
   }

   std::vector<VkPhysicalDevice> devices(deviceCount);
   vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

   bool foundSuitableGPU = false;

   for (uint32_t i = 0; i < deviceCount; i++)
   {
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(devices[i], &properties);

      // Require Vulkan 1.4 API support on the GPU - no fallbacks
      if (properties.apiVersion < VK_API_VERSION_1_4)
      {
         continue;
      }

      // Check for critical extensions that are still required in 1.4
      const char *criticalExtensions[] = {VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
                                          VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
                                          VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME};

      bool hasAllExtensions = checkDeviceExtensionSupport(devices[i], criticalExtensions,
                                                          sizeof(criticalExtensions) / sizeof(criticalExtensions[0]));

      if (!hasAllExtensions)
      {
         continue;
      }

      // Check for Vulkan 1.4 mandatory features
      VkPhysicalDeviceVulkan14Features vulkan14Features{};
      vulkan14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

      VkPhysicalDeviceVulkan13Features vulkan13Features{};
      vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
      vulkan13Features.pNext = &vulkan14Features;

      VkPhysicalDeviceVulkan12Features vulkan12Features{};
      vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
      vulkan12Features.pNext = &vulkan13Features;

      VkPhysicalDeviceHostImageCopyFeatures hostImageCopyFeatures{};
      hostImageCopyFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES;
      hostImageCopyFeatures.pNext = &vulkan12Features;

      VkPhysicalDeviceFeatures2 features2{};
      features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      features2.pNext = &hostImageCopyFeatures;

      vkGetPhysicalDeviceFeatures2(devices[i], &features2);

      // Validate mandatory Vulkan 1.4 features
      if (vulkan12Features.bufferDeviceAddress && // Core since 1.2
          vulkan12Features.descriptorIndexing &&  // Core since 1.2
          vulkan12Features.scalarBlockLayout &&   // Mandatory in 1.4
          vulkan13Features.dynamicRendering &&    // Core since 1.3
          vulkan14Features.pushDescriptor)
      { // Mandatory in 1.4
         foundSuitableGPU = true;
         break;
      }
   }

   return foundSuitableGPU;
}

static void queryDevice14Features(VkPhysicalDevice physicalDevice, VEDeviceFeatures *features)
{
   // Host image copy features (optional but recommended)
   VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures{};
   hostImageCopyFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES;

   // Vulkan 1.4 core features
   VkPhysicalDeviceVulkan14Features vulkan14Features{};
   vulkan14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
   vulkan14Features.pNext = &hostImageCopyFeatures;

   // Vulkan 1.3 core features
   VkPhysicalDeviceVulkan13Features vulkan13Features{};
   vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   vulkan13Features.pNext = &vulkan14Features;

   // Vulkan 1.2 core features
   VkPhysicalDeviceVulkan12Features vulkan12Features{};
   vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   vulkan12Features.pNext = &vulkan13Features;

   // Extension features still required in 1.4
   VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extDynState3Features{};
   extDynState3Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
   extDynState3Features.pNext = &vulkan12Features;

   VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertexInputDynFeatures{};
   vertexInputDynFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
   vertexInputDynFeatures.pNext = &extDynState3Features;

   VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{};
   shaderObjectFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
   shaderObjectFeatures.pNext = &vertexInputDynFeatures;

   VkPhysicalDeviceFeatures2 features2{};
   features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   features2.pNext = &shaderObjectFeatures;

   vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

   // Map features to internal structure
   // Core Vulkan 1.2+ features (mandatory)
   features->bufferDeviceAddress = vulkan12Features.bufferDeviceAddress;
   features->descriptorIndexing = vulkan12Features.descriptorIndexing;
   features->scalarBlockLayout = vulkan12Features.scalarBlockLayout; // Mandatory in 1.4
   features->updateAfterBind = vulkan12Features.descriptorBindingSampledImageUpdateAfterBind &&
                               vulkan12Features.descriptorBindingVariableDescriptorCount &&
                               vulkan12Features.descriptorBindingPartiallyBound;

   // Core Vulkan 1.3+ features (mandatory)
   features->dynamicRendering = vulkan13Features.dynamicRendering;

   // Core Vulkan 1.4+ features (mandatory)
   features->pushDescriptor = vulkan14Features.pushDescriptor;                       // Mandatory in 1.4
   features->dynamicRenderingLocalRead = vulkan14Features.dynamicRenderingLocalRead; // Optional but recommended

   // Optional Vulkan 1.4+ features
   features->hostImageCopy = hostImageCopyFeatures.hostImageCopy;

   // Extension features (still required)
   features->extendedDynamicState3 = extDynState3Features.extendedDynamicState3PolygonMode &&
                                     extDynState3Features.extendedDynamicState3ColorBlendEnable;
   features->vertexInputDynamicState = vertexInputDynFeatures.vertexInputDynamicState;
   features->shaderObject = shaderObjectFeatures.shaderObject;

   // Basic features
   features->samplerAnisotropy = features2.features.samplerAnisotropy;
   features->fillModeNonSolid = features2.features.fillModeNonSolid;
   features->wideLines = features2.features.wideLines;
   features->depthClamp = features2.features.depthClamp;
}

// =============================================================================
// Device Selection
// =============================================================================

static int scorePhysicalDevice(VkPhysicalDevice device)
{
   VkPhysicalDeviceProperties properties;
   vkGetPhysicalDeviceProperties(device, &properties);

   // Require Vulkan 1.4+ - no fallbacks
   if (properties.apiVersion < VK_API_VERSION_1_4)
   {
      return -1;
   }

   if (!checkDeviceExtensionSupport(device, REQUIRED_DEVICE_EXTENSIONS,
                                    sizeof(REQUIRED_DEVICE_EXTENSIONS) / sizeof(REQUIRED_DEVICE_EXTENSIONS[0])))
   {
      return -1;
   }

   // Check queue families
   uint32_t queueFamilyCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

   std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
   vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

   bool hasGraphicsQueue = false;
   bool hasComputeQueue = false;

   for (uint32_t i = 0; i < queueFamilyCount; i++)
   {
      if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
         hasGraphicsQueue = true;
      }
      if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
      {
         hasComputeQueue = true;
      }
   }

   if (!hasGraphicsQueue || !hasComputeQueue)
   {
      return -1;
   }

   int score = 0;

   // Device type scoring
   switch (properties.deviceType)
   {
   case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      score += 1000;
      break;
   case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      score += 100;
      break;
   default:
      score += 10;
      break;
   }

   // Vulkan 1.4 bonus
   if (properties.apiVersion >= VK_API_VERSION_1_4)
      score += 200;

   // Check for optional features that improve performance
   VEDeviceFeatures features;
   queryDevice14Features(device, &features);
   if (features.hostImageCopy)
      score += 50;
   if (features.dynamicRenderingLocalRead)
      score += 50;

   return score;
}

static VkPhysicalDevice selectBestPhysicalDevice(VEContextInternal *context)
{
   uint32_t deviceCount = 0;
   vkEnumeratePhysicalDevices(context->instance, &deviceCount, NULL);

   if (deviceCount == 0)
   {
      veSetError("No Vulkan-capable GPUs found");
      return VK_NULL_HANDLE;
   }

   std::vector<VkPhysicalDevice> devices(deviceCount);
   vkEnumeratePhysicalDevices(context->instance, &deviceCount, devices.data());

   VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
   int bestScore = -1;

   for (uint32_t i = 0; i < deviceCount; i++)
   {
      int score = scorePhysicalDevice(devices[i]);
      if (score > bestScore)
      {
         bestScore = score;
         bestDevice = devices[i];
      }
   }

   if (bestDevice == VK_NULL_HANDLE)
   {
      veSetError("No suitable physical device found");
   }

   return bestDevice;
}

// =============================================================================
// Queue Family Finding
// =============================================================================

static bool findQueueFamilies(VkPhysicalDevice physicalDevice, VEQueueFamilies *queueFamilies)
{
   uint32_t queueFamilyCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

   std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
   vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.data());

   queueFamilies->graphicsFamily = UINT32_MAX;
   queueFamilies->computeFamily = UINT32_MAX;
   queueFamilies->transferFamily = UINT32_MAX;

   for (uint32_t i = 0; i < queueFamilyCount; i++)
   {
      VkQueueFlags flags = families[i].queueFlags;

      if ((flags & VK_QUEUE_GRAPHICS_BIT) && queueFamilies->graphicsFamily == UINT32_MAX)
      {
         queueFamilies->graphicsFamily = i;
      }

      if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) &&
          queueFamilies->computeFamily == UINT32_MAX)
      {
         queueFamilies->computeFamily = i;
      }

      if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) && !(flags & VK_QUEUE_COMPUTE_BIT) &&
          queueFamilies->transferFamily == UINT32_MAX)
      {
         queueFamilies->transferFamily = i;
      }
   }

   if (queueFamilies->computeFamily == UINT32_MAX)
   {
      queueFamilies->computeFamily = queueFamilies->graphicsFamily;
   }
   if (queueFamilies->transferFamily == UINT32_MAX)
   {
      queueFamilies->transferFamily = queueFamilies->graphicsFamily;
   }

   return queueFamilies->graphicsFamily != UINT32_MAX;
}

// =============================================================================
// Context Management Implementation
// =============================================================================

uint32_t veGetVersion(void) { return VULKEASE_API_VERSION_2_0; }

VEContext *veCreateContext(const char *applicationName)
{
   if (!applicationName)
   {
      veSetError("Application name cannot be NULL");
      return NULL;
   }

   VEContextInternal *context = static_cast<VEContextInternal *>(calloc(1, sizeof(VEContextInternal)));
   if (!context)
   {
      veSetError("Failed to allocate context memory");
      return NULL;
   }

   // Build extension list for Vulkan 1.4
   const char *allExtensions[VE_MAX_EXTENSIONS];
   uint32_t allExtensionCount = 0;

   // Add base extensions
   uint32_t baseCount = sizeof(BASE_INSTANCE_EXTENSIONS) / sizeof(BASE_INSTANCE_EXTENSIONS[0]);
   for (uint32_t i = 0; i < baseCount && allExtensionCount < VE_MAX_EXTENSIONS; i++)
   {
      allExtensions[allExtensionCount++] = BASE_INSTANCE_EXTENSIONS[i];
   }

   // Add platform-specific surface extensions
   uint32_t platformCount = sizeof(PLATFORM_SURFACE_EXTENSIONS) / sizeof(PLATFORM_SURFACE_EXTENSIONS[0]);
   for (uint32_t i = 0; i < platformCount && allExtensionCount < VE_MAX_EXTENSIONS; i++)
   {
      allExtensions[allExtensionCount++] = PLATFORM_SURFACE_EXTENSIONS[i];
   }

   if (!checkInstanceExtensionSupport(allExtensions, allExtensionCount))
   {
      veSetError("Missing required Vulkan instance extensions. Ensure your GPU drivers "
                 "support Vulkan 1.4+ and platform surface extensions are available.");
      free(context);
      return NULL;
   }

   bool validationEnabled = false;
#ifdef DEBUG
   if (checkValidationLayerSupport())
   {
      validationEnabled = true;
   }
#endif

   strcpy(context->applicationName, applicationName);
   context->validationEnabled = validationEnabled;

   VkApplicationInfo appInfo{};
   appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   appInfo.pApplicationName = applicationName;
   appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
   appInfo.pEngineName = "VulkEase 2.0";
   appInfo.engineVersion = VULKEASE_API_VERSION_2_0;
   appInfo.apiVersion = VK_API_VERSION_1_4; // Require Vulkan 1.4

   VkInstanceCreateInfo createInfo{};
   createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   createInfo.pApplicationInfo = &appInfo;

   // Reuse the extension list we already built and validated
   createInfo.enabledExtensionCount = allExtensionCount;
   createInfo.ppEnabledExtensionNames = allExtensions;

   if (validationEnabled)
   {
      createInfo.enabledLayerCount = sizeof(VALIDATION_LAYERS) / sizeof(VALIDATION_LAYERS[0]);
      createInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
   }

   VkResult result = vkCreateInstance(&createInfo, NULL, &context->instance);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create Vulkan instance (VkResult: %d)", result);
      free(context);
      return NULL;
   }

   // Validate that the instance actually supports Vulkan 1.4+ - no fallbacks
   uint32_t apiVersion;
   if (vkEnumerateInstanceVersion(&apiVersion) == VK_SUCCESS)
   {
      if (apiVersion < VK_API_VERSION_1_4)
      {
         veSetError("Vulkan 1.4 or later required, found version %d.%d.%d", VK_VERSION_MAJOR(apiVersion),
                    VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));
         vkDestroyInstance(context->instance, NULL);
         free(context);
         return NULL;
      }
   }

   // Validate that we have at least one compatible GPU with Vulkan 1.4 features
   if (!validateMinimumGPUCapabilities(context->instance))
   {
      veSetError("No compatible GPU found with required Vulkan 1.4 features "
                 "(buffer device address, descriptor indexing, dynamic rendering, push descriptors). "
                 "Ensure your GPU drivers support Vulkan 1.4.");
      vkDestroyInstance(context->instance, NULL);
      free(context);
      return NULL;
   }

   initializeInstanceFunctions(context->instance);

   if (validationEnabled)
   {
      VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
      debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debugCreateInfo.messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      debugCreateInfo.pfnUserCallback = debugCallback;

      veFuncs.vkCreateDebugUtilsMessengerEXT(context->instance, &debugCreateInfo, NULL, &context->debugMessenger);

      context->validationEnabled = true;
   }

   return (VEContext *)context;
}

void veDestroyContext(VEContext *context)
{
   if (!context)
      return;

   VEContextInternal *internal = (VEContextInternal *)context;

   if (internal->debugMessenger)
   {
      veFuncs.vkDestroyDebugUtilsMessengerEXT(internal->instance, internal->debugMessenger, NULL);
   }

   if (internal->instance)
   {
      vkDestroyInstance(internal->instance, NULL);
   }

   free(internal);
}

// =============================================================================
// Device Management Implementation
// =============================================================================

VEDevice *veCreateDevice(VEContext *context)
{
   if (!context)
   {
      veSetError("Context cannot be NULL");
      return NULL;
   }

   VEContextInternal *contextInternal = (VEContextInternal *)context;

   VEDeviceInternal *device = static_cast<VEDeviceInternal *>(calloc(1, sizeof(VEDeviceInternal)));
   if (!device)
   {
      veSetError("Failed to allocate device memory");
      return NULL;
   }

   device->context = contextInternal;

   // Select best physical device for Vulkan 1.4
   uint32_t deviceCount = 0;
   vkEnumeratePhysicalDevices(contextInternal->instance, &deviceCount, NULL);
   if (deviceCount == 0)
   {
      veSetError("No Vulkan-capable GPUs found");
      free(device);
      return NULL;
   }

   std::vector<VkPhysicalDevice> devices(deviceCount);
   vkEnumeratePhysicalDevices(contextInternal->instance, &deviceCount, devices.data());

   device->physicalDevice = selectBestPhysicalDevice(contextInternal);

   vkGetPhysicalDeviceProperties(device->physicalDevice, &device->deviceProperties);
   vkGetPhysicalDeviceMemoryProperties(device->physicalDevice, &device->memoryProperties);
   queryDevice14Features(device->physicalDevice, &device->features);

   if (!findQueueFamilies(device->physicalDevice, &device->queueFamilies))
   {
      veSetError("Failed to find suitable queue families");
      free(device);
      return NULL;
   }

   // Create queue create infos
   float queuePriority = 1.0f;

   uint32_t uniqueQueueFamilies[3];
   uint32_t uniqueCount = 0;

   uniqueQueueFamilies[uniqueCount++] = device->queueFamilies.graphicsFamily;

   if (device->queueFamilies.computeFamily != device->queueFamilies.graphicsFamily)
   {
      uniqueQueueFamilies[uniqueCount++] = device->queueFamilies.computeFamily;
   }

   if (device->queueFamilies.transferFamily != device->queueFamilies.graphicsFamily &&
       device->queueFamilies.transferFamily != device->queueFamilies.computeFamily)
   {
      uniqueQueueFamilies[uniqueCount++] = device->queueFamilies.transferFamily;
   }

   VkDeviceQueueCreateInfo queueCreateInfos[3] = {};
   for (uint32_t i = 0; i < uniqueCount; i++)
   {
      queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfos[i].queueFamilyIndex = uniqueQueueFamilies[i];
      queueCreateInfos[i].queueCount = 1;
      queueCreateInfos[i].pQueuePriorities = &queuePriority;
   }

   // Vulkan 1.4 core features (mandatory)
   VkPhysicalDeviceVulkan14Features vulkan14Features{};
   vulkan14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
   vulkan14Features.pushDescriptor = VK_TRUE; // Mandatory in 1.4
   vulkan14Features.hostImageCopy = VK_TRUE;
   vulkan14Features.dynamicRenderingLocalRead = device->features.dynamicRenderingLocalRead; // Optional

   // Vulkan 1.3 core features
   VkPhysicalDeviceVulkan13Features vulkan13Features{};
   vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   vulkan13Features.pNext = &vulkan14Features;
   vulkan13Features.dynamicRendering = VK_TRUE; // Core since 1.3
   vulkan13Features.synchronization2 = VK_TRUE;

   // Vulkan 1.2 core features (now mandatory in 1.4)
   VkPhysicalDeviceVulkan12Features vulkan12Features{};
   vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   vulkan12Features.pNext = &vulkan13Features;
   vulkan12Features.bufferDeviceAddress = VK_TRUE; // Core since 1.2
   vulkan12Features.descriptorIndexing = VK_TRUE;  // Core since 1.2
   vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
   vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
   vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
   vulkan12Features.runtimeDescriptorArray = VK_TRUE;
   vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
   vulkan12Features.scalarBlockLayout = VK_TRUE; // Mandatory in 1.4
   vulkan12Features.shaderInt8 = VK_TRUE;        // Mandatory in 1.4
   vulkan12Features.shaderFloat16 = VK_TRUE;     // Mandatory in 1.4

   // Extension features (still required in 1.4)
   VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extDynState3Features{};
   extDynState3Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
   extDynState3Features.pNext = &vulkan12Features;
   extDynState3Features.extendedDynamicState3PolygonMode = VK_TRUE;
   extDynState3Features.extendedDynamicState3ColorBlendEnable = VK_TRUE;
   extDynState3Features.extendedDynamicState3ColorBlendEquation = VK_TRUE;
   extDynState3Features.extendedDynamicState3ColorWriteMask = VK_TRUE;

   VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertexInputDynFeatures{};
   vertexInputDynFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
   vertexInputDynFeatures.pNext = &extDynState3Features;
   vertexInputDynFeatures.vertexInputDynamicState = VK_TRUE;

   VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{};
   shaderObjectFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
   shaderObjectFeatures.pNext = &vertexInputDynFeatures;
   shaderObjectFeatures.shaderObject = VK_TRUE;

   // Basic features
   VkPhysicalDeviceFeatures2 deviceFeatures2{};
   deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   deviceFeatures2.pNext = &shaderObjectFeatures;
   deviceFeatures2.features.samplerAnisotropy = device->features.samplerAnisotropy;
   deviceFeatures2.features.fillModeNonSolid = device->features.fillModeNonSolid;
   deviceFeatures2.features.wideLines = device->features.wideLines;
   deviceFeatures2.features.depthClamp = device->features.depthClamp;
   deviceFeatures2.features.shaderInt64 = VK_TRUE;

   VkDeviceCreateInfo deviceCreateInfo{};
   deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   deviceCreateInfo.pNext = &deviceFeatures2;
   deviceCreateInfo.queueCreateInfoCount = uniqueCount;
   deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;

   // Use updated extension list - many are now core in 1.4
   deviceCreateInfo.enabledExtensionCount = sizeof(REQUIRED_DEVICE_EXTENSIONS) / sizeof(REQUIRED_DEVICE_EXTENSIONS[0]);
   deviceCreateInfo.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS;

   if (contextInternal->validationEnabled)
   {
      deviceCreateInfo.enabledLayerCount = sizeof(VALIDATION_LAYERS) / sizeof(VALIDATION_LAYERS[0]);
      deviceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
   }

   VkResult result = vkCreateDevice(device->physicalDevice, &deviceCreateInfo, NULL, &device->device);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create Vulkan 1.4 logical device (VkResult: %d)", result);
      free(device);
      return NULL;
   }

   initializeDeviceFunctions(device->device);

   // Get queues
   vkGetDeviceQueue(device->device, device->queueFamilies.graphicsFamily, 0, &device->graphicsQueue);
   vkGetDeviceQueue(device->device, device->queueFamilies.computeFamily, 0, &device->computeQueue);
   vkGetDeviceQueue(device->device, device->queueFamilies.transferFamily, 0, &device->transferQueue);

   // Initialize VMA with Vulkan 1.4
   VEResult vmaResult = device->initializeVma();
   if (vmaResult != VE_SUCCESS)
   {
      vkDestroyDevice(device->device, NULL);
      free(device);
      return NULL;
   }

   // Initialize command pools
   device->graphicsCommandPool = new (std::nothrow) VECommandPool();
   if (!device->graphicsCommandPool ||
       !device->graphicsCommandPool->initialize(device, device->queueFamilies.graphicsFamily))
   {
      vkDestroyDevice(device->device, NULL);
      delete device->graphicsCommandPool;
      device->graphicsCommandPool = nullptr;
      free(device);
      return NULL;
   }

   if (device->computeQueue != device->graphicsQueue)
   {
      device->computeCommandPool = new (std::nothrow) VECommandPool();
      if (!device->computeCommandPool ||
          !device->computeCommandPool->initialize(device, device->queueFamilies.computeFamily))
      {
         vkDestroyDevice(device->device, NULL);
         delete device->computeCommandPool;
         device->computeCommandPool = nullptr;
         free(device);
         return NULL;
      }
   }
   else
   {
      device->computeCommandPool = device->graphicsCommandPool;
   }

   if (device->transferQueue != device->graphicsQueue)
   {
      device->transferCommandPool = new (std::nothrow) VECommandPool();
      if (!device->transferCommandPool ||
          !device->transferCommandPool->initialize(device, device->queueFamilies.transferFamily))
      {
         vkDestroyDevice(device->device, NULL);
         delete device->transferCommandPool;
         device->transferCommandPool = nullptr;
         free(device);
         return NULL;
      }
   }
   else
   {
      device->transferCommandPool = device->graphicsCommandPool;
   }

   veInitializeQueueLocks(device);
   if (!device->queueLocks)
   {
      veSetError("Failed to initialize device queue locks");
      if (device->transferCommandPool && device->transferCommandPool != device->graphicsCommandPool)
      {
         delete device->transferCommandPool;
         device->transferCommandPool = NULL;
      }
      if (device->computeCommandPool && device->computeCommandPool != device->graphicsCommandPool)
      {
         delete device->computeCommandPool;
         device->computeCommandPool = NULL;
      }
      if (device->graphicsCommandPool)
      {
         delete device->graphicsCommandPool;
         device->graphicsCommandPool = NULL;
      }
      vkDestroyDevice(device->device, NULL);
      free(device);
      return NULL;
   }

   // Initialize bindless descriptors
   VEResult bindlessResult = device->initializeBindlessDescriptors();
   if (bindlessResult != VE_SUCCESS)
   {
      device->cleanupVma();
      vkDestroyDevice(device->device, NULL);
      free(device);
      return NULL;
   }

   // Initialize config arrays
   device->maxRenderConfigs = VE_MAX_RENDER_CONFIGS;
   device->renderConfigs =
       static_cast<VERenderConfigInternal *>(calloc(VE_MAX_RENDER_CONFIGS, sizeof(VERenderConfigInternal)));
   device->renderConfigCount = 0;

   // initialize vertex configs
   device->maxVertexConfigs = VE_MAX_VERTEX_CONFIGS;
   device->vertexConfigs =
       static_cast<VEVertexConfigInternal *>(calloc(VE_MAX_VERTEX_CONFIGS, sizeof(VEVertexConfigInternal)));
   device->vertexConfigCount = 0;

   // initialize shader configs
   device->maxShaderConfigs = VE_MAX_SHADER_CONFIGS;
   device->shaderConfigs =
       static_cast<VEShaderConfigInternal *>(calloc(VE_MAX_SHADER_CONFIGS, sizeof(VEShaderConfigInternal)));
   device->shaderConfigCount = 0;

   device->maxShaders = VE_MAX_SHADERS;
   device->shaderCount = 0;

   // Create global pipeline layouts with Vulkan 1.4 enhanced push constants (256 bytes)
   VkDescriptorSetLayout setLayouts[2] = {
       device->textureDescriptorSetLayout, // set = 0 in shaders
       device->samplerDescriptorSetLayout  // set = 1 in shaders
   };

   // Graphics pipeline layout with 256-byte push constants
   VkPushConstantRange gfxPushConstantRange{};
   gfxPushConstantRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
   gfxPushConstantRange.offset = 0;
   gfxPushConstantRange.size = VE_MAX_PUSH_CONSTANT_BYTES; // Now 256 bytes in Vulkan 1.4

   VkPipelineLayoutCreateInfo gfxLayoutInfo{};
   gfxLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   gfxLayoutInfo.setLayoutCount = 2;
   gfxLayoutInfo.pSetLayouts = setLayouts;
   gfxLayoutInfo.pushConstantRangeCount = 1;
   gfxLayoutInfo.pPushConstantRanges = &gfxPushConstantRange;

   result = vkCreatePipelineLayout(device->device, &gfxLayoutInfo, NULL, &device->globalGraphicsPipelineLayout);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create graphics pipeline layout (VkResult: %d)", result);
      free(device);
      return NULL;
   }

   // Compute pipeline layout with 256-byte push constants
   VkPushConstantRange computePushConstantRange{};
   computePushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
   computePushConstantRange.offset = 0;
   computePushConstantRange.size = VE_MAX_PUSH_CONSTANT_BYTES; // Now 256 bytes in Vulkan 1.4

   VkPipelineLayoutCreateInfo computeLayoutInfo{};
   computeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   computeLayoutInfo.setLayoutCount = 2;
   computeLayoutInfo.pSetLayouts = setLayouts;
   computeLayoutInfo.pushConstantRangeCount = 1;
   computeLayoutInfo.pPushConstantRanges = &computePushConstantRange;

   result = vkCreatePipelineLayout(device->device, &computeLayoutInfo, NULL, &device->globalComputePipelineLayout);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create compute pipeline layout (VkResult: %d)", result);
      free(device);
      return NULL;
   }

   return (VEDevice *)device;
}

void veDestroyDevice(VEDevice *device)
{
   if (!device)
      return;

   VEDeviceInternal *internal = (VEDeviceInternal *)device;

   if (internal->device)
   {
      vkDeviceWaitIdle(internal->device);
   }

   // free internal config buffers
   free(internal->shaderConfigs);
   free(internal->vertexConfigs);
   free(internal->renderConfigs);

   // cleanup the pipeline layouts
   vkDestroyPipelineLayout(internal->device, internal->globalGraphicsPipelineLayout, NULL);
   vkDestroyPipelineLayout(internal->device, internal->globalComputePipelineLayout, NULL);

   if (internal->transferCommandPool && internal->transferCommandPool != internal->graphicsCommandPool)
   {
      delete internal->transferCommandPool;
      internal->transferCommandPool = NULL;
   }

   if (internal->computeCommandPool && internal->computeCommandPool != internal->graphicsCommandPool)
   {
      delete internal->computeCommandPool;
      internal->computeCommandPool = NULL;
   }

   if (internal->graphicsCommandPool)
   {
      delete internal->graphicsCommandPool;
      internal->graphicsCommandPool = NULL;
   }

   veDestroyQueueLocks(internal);

   internal->cleanupBindlessDescriptors();
   internal->cleanupVma();

   if (internal->device)
   {
      vkDestroyDevice(internal->device, NULL);
   }

   free(internal);
}

VEResult veDeviceWaitIdle(VEDevice *device)
{
   if (!device)
   {
      veSetError("Device cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *internal = (VEDeviceInternal *)device;
   VkResult result = vkDeviceWaitIdle(internal->device);

   if (result != VK_SUCCESS)
   {
      veSetError("Failed to wait for device idle (VkResult: %d)", result);
      return VE_ERROR_UNKNOWN;
   }

   return VE_SUCCESS;
}

// =============================================================================
// Feature Detection Implementation
// =============================================================================

// =============================================================================
// Device Information Implementation
// =============================================================================

const char *veGetDeviceName(VEDevice *device)
{
   if (!device)
      return "Unknown";
   VEDeviceInternal *internal = (VEDeviceInternal *)device;
   return internal->deviceProperties.deviceName;
}

const char *veGetDriverVersion(VEDevice *device)
{
   if (!device)
      return "Unknown";
   VEDeviceInternal *internal = (VEDeviceInternal *)device;

   static char versionString[64];
   snprintf(versionString, sizeof(versionString), "%u.%u.%u",
            VK_VERSION_MAJOR(internal->deviceProperties.driverVersion),
            VK_VERSION_MINOR(internal->deviceProperties.driverVersion),
            VK_VERSION_PATCH(internal->deviceProperties.driverVersion));

   return versionString;
}

uint32_t veGetVulkanVersion(VEDevice *device)
{
   if (!device)
      return 0;
   VEDeviceInternal *internal = (VEDeviceInternal *)device;
   return internal->deviceProperties.apiVersion;
}

#define GET_INSTANCE_FUNC(extName)                                                                                     \
   veFuncs.extName = (PFN_##extName)vkGetInstanceProcAddr(instance, #extName);                                         \
   if (veFuncs.extName == NULL)                                                                                        \
   {                                                                                                                   \
      printf("Failed to find instance function: %s\n", #extName);                                                      \
      return false;                                                                                                    \
   }

#define GET_DEVICE_FUNC(extName)                                                                                       \
   veFuncs.extName = (PFN_##extName)vkGetDeviceProcAddr(device, #extName);                                             \
   if (veFuncs.extName == NULL)                                                                                        \
   {                                                                                                                   \
      printf("Failed to find device function: %s\n", #extName);                                                        \
      return false;                                                                                                    \
   }

bool initializeInstanceFunctions(VkInstance instance)
{
   GET_INSTANCE_FUNC(vkSetDebugUtilsObjectNameEXT);
   GET_INSTANCE_FUNC(vkCmdBeginDebugUtilsLabelEXT);
   GET_INSTANCE_FUNC(vkSubmitDebugUtilsMessageEXT);
   GET_INSTANCE_FUNC(vkCmdInsertDebugUtilsLabelEXT);
   GET_INSTANCE_FUNC(vkCmdEndDebugUtilsLabelEXT);
   GET_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);
   GET_INSTANCE_FUNC(vkDestroyDebugUtilsMessengerEXT);

   return true;
}

bool initializeDeviceFunctions(VkDevice device)
{
   // Shader objects (still extension)
   GET_DEVICE_FUNC(vkCreateShadersEXT);
   GET_DEVICE_FUNC(vkCmdBindShadersEXT);
   GET_DEVICE_FUNC(vkGetShaderBinaryDataEXT);
   GET_DEVICE_FUNC(vkDestroyShaderEXT);

   // Extended dynamic state 3 (still extension)
   GET_DEVICE_FUNC(vkCmdSetPolygonModeEXT);
   GET_DEVICE_FUNC(vkCmdSetDepthClampEnableEXT);
   GET_DEVICE_FUNC(vkCmdSetColorBlendEnableEXT);
   GET_DEVICE_FUNC(vkCmdSetColorWriteMaskEXT);
   GET_DEVICE_FUNC(vkCmdSetColorBlendEquationEXT);
   GET_DEVICE_FUNC(vkCmdSetRasterizationSamplesEXT);
   GET_DEVICE_FUNC(vkCmdSetSampleMaskEXT);
   GET_DEVICE_FUNC(vkCmdSetAlphaToCoverageEnableEXT);
   GET_DEVICE_FUNC(vkCmdSetAlphaToOneEnableEXT);
   GET_DEVICE_FUNC(vkCmdSetPatchControlPointsEXT);
   GET_DEVICE_FUNC(vkCmdSetConservativeRasterizationModeEXT);
   GET_DEVICE_FUNC(vkCmdSetLineRasterizationModeEXT);
   GET_DEVICE_FUNC(vkCmdSetProvokingVertexModeEXT);

   // extended dynamic state 2 logic op
   GET_DEVICE_FUNC(vkCmdSetLogicOpEnableEXT);
   GET_DEVICE_FUNC(vkCmdSetLogicOpEXT);

   // Vertex input dynamic state (still extension)
   GET_DEVICE_FUNC(vkCmdSetVertexInputEXT);

   // Push Descriptors (Vulkan 1.4 core entry points)
   GET_DEVICE_FUNC(vkCmdPushDescriptorSet);
   GET_DEVICE_FUNC(vkCmdPushDescriptorSetWithTemplate);

   // Host Image Copy (optional extension)
   GET_DEVICE_FUNC(vkCopyMemoryToImageEXT);
   GET_DEVICE_FUNC(vkCopyImageToMemoryEXT);
   GET_DEVICE_FUNC(vkCopyImageToImageEXT);
   
   return true;
}
