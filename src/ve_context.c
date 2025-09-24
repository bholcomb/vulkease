/**
 * @file ve_context.c
 * @brief Context and Device Management Implementation
 */

#include "ve_internal.h"

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
static const char* BASE_INSTANCE_EXTENSIONS[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME  // For advanced surface queries
};

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

static const char* PLATFORM_SURFACE_EXTENSIONS[] = {
#ifdef _WIN32
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
//    VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
    VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#endif
};

static const char* REQUIRED_DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
    VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
    VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

static const char* VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};

// global external functions struct
VEFuncs veFuncs;

// =============================================================================
// Error Handling
// =============================================================================

static char g_lastError[512] = {0};

void veSetError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_lastError, sizeof(g_lastError), format, args);
    va_end(args);


    printf("VULKEASE ERROR: %s", g_lastError);
}

const char* veGetLastError(void) {
    return g_lastError[0] ? g_lastError : "No error";
}

// Debug messenger callback
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    const char* severity = "INFO";
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity = "ERROR";
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity = "WARNING";
    }
    
    printf("[VulkEase %s] %s\n", severity, pCallbackData->pMessage);
    return VK_FALSE;
}

// =============================================================================
// Extension Support Checking
// =============================================================================

static bool checkInstanceExtensionSupport(const char** requiredExtensions, uint32_t requiredCount) {
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    
    VkExtensionProperties* extensions = malloc(extensionCount * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions);
    
    bool allSupported = true;
    for (uint32_t i = 0; i < requiredCount; i++) {
        bool found = false;
        for (uint32_t j = 0; j < extensionCount; j++) {
            if (strcmp(requiredExtensions[i], extensions[j].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            printf("CRITICAL: Required instance extension not available: %s\n", requiredExtensions[i]);
            printf("This indicates outdated GPU drivers or incompatible hardware.\n");
            allSupported = false;
        }
    }
    
    free(extensions);
    return allSupported;
}

static bool checkValidationLayerSupport(void) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    
    VkLayerProperties* layers = malloc(layerCount * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount, layers);
    
    bool found = false;
    for (uint32_t i = 0; i < layerCount; i++) {
        if (strcmp(VALIDATION_LAYERS[0], layers[i].layerName) == 0) {
            found = true;
            break;
        }
    }
    
    free(layers);
    return found;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice device, const char** requiredExtensions, uint32_t requiredCount) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
    
    VkExtensionProperties* extensions = malloc(extensionCount * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, extensions);
    
    bool allSupported = true;
    for (uint32_t i = 0; i < requiredCount; i++) {
        bool found = false;
        for (uint32_t j = 0; j < extensionCount; j++) {
            if (strcmp(requiredExtensions[i], extensions[j].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            allSupported = false;
        }
    }
    
    free(extensions);
    return allSupported;
}

// =============================================================================
// GPU Capability Validation
// =============================================================================

static bool validateMinimumGPUCapabilities(VkInstance instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
    
    if (deviceCount == 0) {
        return false;
    }
    
    VkPhysicalDevice* devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
    
    bool foundSuitableGPU = false;
    
    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        
        // Require Vulkan 1.3+ API support on the GPU
        if (properties.apiVersion < VK_API_VERSION_1_3) {
            continue;
        }
        
        // Check for bindless-critical extensions
        const char* criticalExtensions[] = {
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
        };
        
        bool hasAllExtensions = checkDeviceExtensionSupport(devices[i], criticalExtensions, 
                                                           sizeof(criticalExtensions) / sizeof(criticalExtensions[0]));
        
        if (!hasAllExtensions) {
            continue;
        }
        
        // Check for bindless-critical features
        VkPhysicalDeviceVulkan12Features vulkan12Features = {0};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        
        VkPhysicalDeviceVulkan13Features vulkan13Features = {0};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.pNext = &vulkan12Features;
        
        VkPhysicalDeviceFeatures2 features2 = {0};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &vulkan13Features;
        
        vkGetPhysicalDeviceFeatures2(devices[i], &features2);
        
        if (vulkan12Features.bufferDeviceAddress && 
            vulkan12Features.descriptorIndexing && 
            vulkan13Features.dynamicRendering) {
            foundSuitableGPU = true;
            break;
        }
    }
    
    free(devices);
    return foundSuitableGPU;
}

// =============================================================================
// Device Selection
// =============================================================================

static int scorePhysicalDevice(VkPhysicalDevice device, VEContextInternal* context) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);
    
    if (!checkDeviceExtensionSupport(device, REQUIRED_DEVICE_EXTENSIONS, 
                                    sizeof(REQUIRED_DEVICE_EXTENSIONS) / sizeof(REQUIRED_DEVICE_EXTENSIONS[0]))) {
        return -1;
    }
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    
    VkQueueFamilyProperties* queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);
    
    bool hasGraphicsQueue = false;
    bool hasComputeQueue = false;
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            hasGraphicsQueue = true;
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            hasComputeQueue = true;
        }
    }
    
    free(queueFamilies);
    
    if (!hasGraphicsQueue || !hasComputeQueue) {
        return -1;
    }
    
    int score = 0;
    
    switch (properties.deviceType) {
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
    
    if (properties.apiVersion >= VK_API_VERSION_1_3) {
        score += 100;
    } else if (properties.apiVersion >= VK_API_VERSION_1_2) {
        score += 50;
    }
    
    return score;
}

static VkPhysicalDevice selectBestPhysicalDevice(VEContextInternal* context) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(context->instance, &deviceCount, NULL);
    
    if (deviceCount == 0) {
        veSetError("No Vulkan-capable GPUs found");
        return VK_NULL_HANDLE;
    }
    
    VkPhysicalDevice* devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(context->instance, &deviceCount, devices);
    
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = -1;
    
    for (uint32_t i = 0; i < deviceCount; i++) {
        int score = scorePhysicalDevice(devices[i], context);
        if (score > bestScore) {
            bestScore = score;
            bestDevice = devices[i];
        }
    }
    
    free(devices);
    
    if (bestDevice == VK_NULL_HANDLE) {
        veSetError("No suitable physical device found");
    }
    
    return bestDevice;
}

// =============================================================================
// Queue Family Finding
// =============================================================================

static bool findQueueFamilies(VkPhysicalDevice physicalDevice, VEQueueFamilies* queueFamilies) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    
    VkQueueFamilyProperties* families = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families);
    
    queueFamilies->graphicsFamily = UINT32_MAX;
    queueFamilies->computeFamily = UINT32_MAX;
    queueFamilies->transferFamily = UINT32_MAX;
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkQueueFlags flags = families[i].queueFlags;
        
        if ((flags & VK_QUEUE_GRAPHICS_BIT) && queueFamilies->graphicsFamily == UINT32_MAX) {
            queueFamilies->graphicsFamily = i;
        }
        
        if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) && 
            queueFamilies->computeFamily == UINT32_MAX) {
            queueFamilies->computeFamily = i;
        }
        
        if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) && 
            !(flags & VK_QUEUE_COMPUTE_BIT) && queueFamilies->transferFamily == UINT32_MAX) {
            queueFamilies->transferFamily = i;
        }
    }
    
    if (queueFamilies->computeFamily == UINT32_MAX) {
        queueFamilies->computeFamily = queueFamilies->graphicsFamily;
    }
    if (queueFamilies->transferFamily == UINT32_MAX) {
        queueFamilies->transferFamily = queueFamilies->graphicsFamily;
    }
    
    free(families);
    
    return queueFamilies->graphicsFamily != UINT32_MAX;
}

// =============================================================================
// Feature Detection
// =============================================================================

static void queryDeviceFeatures(VkPhysicalDevice physicalDevice, VEDeviceFeatures* features) {
    VkPhysicalDeviceVulkan12Features vulkan12Features = {0};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    
    VkPhysicalDeviceVulkan13Features vulkan13Features = {0};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &vulkan12Features;
    
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extDynState3Features = {0};
    extDynState3Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
    extDynState3Features.pNext = &vulkan13Features;
    
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertexInputDynFeatures = {0};
    vertexInputDynFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
    vertexInputDynFeatures.pNext = &extDynState3Features;
    
    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = {0};
    shaderObjectFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
    shaderObjectFeatures.pNext = &vertexInputDynFeatures;
    
    VkPhysicalDeviceFeatures2 features2 = {0};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &shaderObjectFeatures;

    
    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
    
    features->bufferDeviceAddress = vulkan12Features.bufferDeviceAddress;
    features->descriptorIndexing = vulkan12Features.descriptorIndexing;
    features->dynamicRendering = vulkan13Features.dynamicRendering;
    features->extendedDynamicState3 = extDynState3Features.extendedDynamicState3PolygonMode && 
                                      extDynState3Features.extendedDynamicState3ColorBlendEnable;
    features->updateAfterBind = vulkan12Features.descriptorBindingSampledImageUpdateAfterBind && 
                                vulkan12Features.descriptorBindingVariableDescriptorCount &&
                                vulkan12Features.descriptorBindingPartiallyBound;

    features->vertexInputDynamicState = vertexInputDynFeatures.vertexInputDynamicState;
    features->shaderObject = shaderObjectFeatures.shaderObject;
    
    features->samplerAnisotropy = features2.features.samplerAnisotropy;
    features->fillModeNonSolid = features2.features.fillModeNonSolid;
    features->wideLines = features2.features.wideLines;
    features->depthClamp = features2.features.depthClamp;
}

// =============================================================================
// Context Management Implementation
// =============================================================================

uint32_t veGetVersion(void) {
    return VULKEASE_API_VERSION_2_0;
}

VEContext* veCreateContext(const char* applicationName) {
    if (!applicationName) {
        veSetError("Application name cannot be NULL");
        return NULL;
    }
    
    VEContextInternal* context = calloc(1, sizeof(VEContextInternal));
    if (!context) {
        veSetError("Failed to allocate context memory");
        return NULL;
    }
    
    // Build the same extension list we'll use for instance creation to check support
    const char* allExtensions[VE_MAX_EXTENSIONS];
    uint32_t allExtensionCount = 0;
    
    // Add base extensions
    uint32_t baseCount = sizeof(BASE_INSTANCE_EXTENSIONS) / sizeof(BASE_INSTANCE_EXTENSIONS[0]);
    for (uint32_t i = 0; i < baseCount && allExtensionCount < VE_MAX_EXTENSIONS; i++) {
        allExtensions[allExtensionCount++] = BASE_INSTANCE_EXTENSIONS[i];
    }
    
    // Add platform-specific surface extensions
    uint32_t platformCount = sizeof(PLATFORM_SURFACE_EXTENSIONS) / sizeof(PLATFORM_SURFACE_EXTENSIONS[0]);
    for (uint32_t i = 0; i < platformCount && allExtensionCount < VE_MAX_EXTENSIONS; i++) {
        allExtensions[allExtensionCount++] = PLATFORM_SURFACE_EXTENSIONS[i];
    }
    
    if (!checkInstanceExtensionSupport(allExtensions, allExtensionCount)) {
        veSetError("Missing required Vulkan instance extensions. Ensure your GPU drivers support Vulkan 1.3+ and platform surface extensions are available.");
        free(context);
        return NULL;
    }
    
    bool validationEnabled = false;
#ifdef DEBUG
    if (checkValidationLayerSupport()) {
        validationEnabled = true;
    }
#endif
    
    strcpy(context->applicationName, applicationName);
    context->validationEnabled = validationEnabled;
    
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = applicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkEase 2.0";
    appInfo.engineVersion = VULKEASE_API_VERSION_2_0;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    
    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    // Reuse the extension list we already built and validated
    createInfo.enabledExtensionCount = allExtensionCount;
    createInfo.ppEnabledExtensionNames = allExtensions;
    
    if (validationEnabled) {
        createInfo.enabledLayerCount = sizeof(VALIDATION_LAYERS) / sizeof(VALIDATION_LAYERS[0]);
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
    }
    
    VkResult result = vkCreateInstance(&createInfo, NULL, &context->instance);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create Vulkan instance (VkResult: %d)", result);
        free(context);
        return NULL;
    }
    
    // Validate that the instance actually supports Vulkan 1.3+
    uint32_t apiVersion;
    if (vkEnumerateInstanceVersion(&apiVersion) == VK_SUCCESS) {
        if (apiVersion < VK_API_VERSION_1_3) {
            veSetError("Vulkan 1.3 or later required, found version %d.%d.%d", 
                       VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));
            vkDestroyInstance(context->instance, NULL);
            free(context);
            return NULL;
        }
    }
    
    // Validate that we have at least one compatible GPU with bindless support
    if (!validateMinimumGPUCapabilities(context->instance)) {
        veSetError("No compatible GPU found with required Vulkan 1.3+ features (buffer device address, descriptor indexing, dynamic rendering). Ensure your GPU drivers support modern Vulkan features.");
        vkDestroyInstance(context->instance, NULL);
        free(context);
        return NULL;
    }

    initializeInstanceFunctions(context->instance);
    
    if (validationEnabled) {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {0};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        
        veFuncs.vkCreateDebugUtilsMessengerEXT(context->instance, &debugCreateInfo, NULL, &context->debugMessenger);
               
        context->validationEnabled = true;
    }
    
    return (VEContext*)context;
}

void veDestroyContext(VEContext* context) {
    if (!context) return;
    
    VEContextInternal* internal = (VEContextInternal*)context;
    
    if (internal->debugMessenger) {
        veFuncs.vkDestroyDebugUtilsMessengerEXT(internal->instance, internal->debugMessenger, NULL);
    }
    
    if (internal->instance) {
        vkDestroyInstance(internal->instance, NULL);
    }
    
    free(internal);
}

// =============================================================================
// Device Management Implementation
// =============================================================================

VEDevice* veCreateDevice(VEContext* context) {
    if (!context) {
        veSetError("Context cannot be NULL");
        return NULL;
    }
    
    VEContextInternal* contextInternal = (VEContextInternal*)context;
    
    VEDeviceInternal* device = calloc(1, sizeof(VEDeviceInternal));
    if (!device) {
        veSetError("Failed to allocate device memory");
        return NULL;
    }
    
    device->context = contextInternal;
    device->physicalDevice = selectBestPhysicalDevice(contextInternal);
    if (device->physicalDevice == VK_NULL_HANDLE) {
        free(device);
        return NULL;
    }
    
    vkGetPhysicalDeviceProperties(device->physicalDevice, &device->deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(device->physicalDevice, &device->memoryProperties);
    queryDeviceFeatures(device->physicalDevice, &device->features);
    
    if (!findQueueFamilies(device->physicalDevice, &device->queueFamilies)) {
        veSetError("Failed to find suitable queue families");
        free(device);
        return NULL;
    }
    
    float queuePriority = 1.0f;
    
    uint32_t uniqueQueueFamilies[3];
    uint32_t uniqueCount = 0;
    
    uniqueQueueFamilies[uniqueCount++] = device->queueFamilies.graphicsFamily;
    
    if (device->queueFamilies.computeFamily != device->queueFamilies.graphicsFamily) {
        uniqueQueueFamilies[uniqueCount++] = device->queueFamilies.computeFamily;
    }
    
    if (device->queueFamilies.transferFamily != device->queueFamilies.graphicsFamily &&
        device->queueFamilies.transferFamily != device->queueFamilies.computeFamily) {
        uniqueQueueFamilies[uniqueCount++] = device->queueFamilies.transferFamily;
    }
    
    VkDeviceQueueCreateInfo queueCreateInfos[3] = {0};
    for (uint32_t i = 0; i < uniqueCount; i++) {
        queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[i].queueFamilyIndex = uniqueQueueFamilies[i];
        queueCreateInfos[i].queueCount = 1;
        queueCreateInfos[i].pQueuePriorities = &queuePriority;
    }
    
    VkPhysicalDeviceVulkan12Features vulkan12Features = {0};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.bufferDeviceAddress = device->features.bufferDeviceAddress;
    vulkan12Features.descriptorIndexing = device->features.descriptorIndexing;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = device->features.updateAfterBind;
    vulkan12Features.descriptorBindingPartiallyBound = device->features.updateAfterBind;
    vulkan12Features.descriptorBindingVariableDescriptorCount = device->features.updateAfterBind;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.scalarBlockLayout = VK_TRUE;
    
    VkPhysicalDeviceVulkan13Features vulkan13Features = {0};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &vulkan12Features;
    vulkan13Features.dynamicRendering = device->features.dynamicRendering;
    vulkan13Features.synchronization2 = VK_TRUE;
    
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extDynState3Features = {0};
    extDynState3Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
    extDynState3Features.pNext = &vulkan13Features;
    extDynState3Features.extendedDynamicState3PolygonMode = VK_TRUE;
    extDynState3Features.extendedDynamicState3ColorBlendEnable = VK_TRUE;
    extDynState3Features.extendedDynamicState3ColorBlendEquation = VK_TRUE;
    extDynState3Features.extendedDynamicState3ColorWriteMask = VK_TRUE;
    
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertexInputDynFeatures = {0};
    vertexInputDynFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
    vertexInputDynFeatures.pNext = &extDynState3Features;
    vertexInputDynFeatures.vertexInputDynamicState = device->features.vertexInputDynamicState;
    
    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = {0};
    shaderObjectFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
    shaderObjectFeatures.pNext = &vertexInputDynFeatures;
    shaderObjectFeatures.shaderObject = device->features.shaderObject;
    
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {0};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &shaderObjectFeatures;
    deviceFeatures2.features.samplerAnisotropy = device->features.samplerAnisotropy;
    deviceFeatures2.features.fillModeNonSolid = device->features.fillModeNonSolid;
    deviceFeatures2.features.wideLines = device->features.wideLines;
    deviceFeatures2.features.depthClamp = device->features.depthClamp;
    deviceFeatures2.features.shaderInt64 = VK_TRUE;
    
    VkDeviceCreateInfo deviceCreateInfo = {0};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &deviceFeatures2;
    deviceCreateInfo.queueCreateInfoCount = uniqueCount;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    
    deviceCreateInfo.enabledExtensionCount = sizeof(REQUIRED_DEVICE_EXTENSIONS) / sizeof(REQUIRED_DEVICE_EXTENSIONS[0]);
    deviceCreateInfo.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS;
    
    if (contextInternal->validationEnabled) {
        deviceCreateInfo.enabledLayerCount = sizeof(VALIDATION_LAYERS) / sizeof(VALIDATION_LAYERS[0]);
        deviceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
    }
    
    VkResult result = vkCreateDevice(device->physicalDevice, &deviceCreateInfo, NULL, &device->device);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create logical device (VkResult: %d)", result);
        free(device);
        return NULL;
    }

    initializeDeviceFunctions(device->device);
    
    vkGetDeviceQueue(device->device, device->queueFamilies.graphicsFamily, 0, &device->graphicsQueue);
    vkGetDeviceQueue(device->device, device->queueFamilies.computeFamily, 0, &device->computeQueue);
    vkGetDeviceQueue(device->device, device->queueFamilies.transferFamily, 0, &device->transferQueue);
    
    VEResult vmaResult = veInitializeVMA(device);
    if (vmaResult != VE_SUCCESS) {
        vkDestroyDevice(device->device, NULL);
        free(device);
        return NULL;
    }

    // Initialize command pools
    device->graphicsCommandPool = calloc(1, sizeof(VECommandPool));
    if(veInitCommandPool(device, device->queueFamilies.graphicsFamily, device->graphicsCommandPool) == false)
    {
        vkDestroyDevice(device->device, NULL);
        free(device);
        return NULL;
    }

    if(device->computeQueue != device->graphicsQueue)
    {
        device->computeCommandPool = calloc(1, sizeof(VECommandPool));
        if(veInitCommandPool(device, device->queueFamilies.computeFamily, device->computeCommandPool) == false)
        {
            vkDestroyDevice(device->device, NULL);
            free(device);
            return NULL;
        }
    }
    else
    {
        device->computeCommandPool = device->graphicsCommandPool;
    }

    if(device->transferQueue != device->graphicsQueue)
    {
        device->transferCommandPool = calloc(1, sizeof(VECommandPool));
        if(veInitCommandPool(device, device->queueFamilies.transferFamily, device->transferCommandPool) == false)
        {
            vkDestroyDevice(device->device, NULL);
            free(device);
            return NULL;
        }
    }
    else
    {
        device->transferCommandPool = device->graphicsCommandPool;
    }
    
    VEResult bindlessResult = veInitializeBindlessDescriptors(device);
    if (bindlessResult != VE_SUCCESS) {
        veCleanupVMA(device);
        vkDestroyDevice(device->device, NULL);
        free(device);
        return NULL;
    }

    //initialize render configs
    device->maxRenderConfigs = VE_MAX_RENDER_CONFIGS;
    device->renderConfigs = calloc(VE_MAX_RENDER_CONFIGS, sizeof(VERenderConfigInternal));
    device->renderConfigCount = 0;

    //initialize vertex configs
    device->maxVertexConfigs = VE_MAX_VERTEX_CONFIGS;
    device->vertexConfigs = calloc(VE_MAX_VERTEX_CONFIGS, sizeof(VEVertexConfigInternal));
    device->vertexConfigCount = 0;

    //initialize shader configs
    device->maxShaderConfigs = VE_MAX_SHADER_CONFIGS;
    device->shaderConfigs = calloc(VE_MAX_SHADER_CONFIGS, sizeof(VEShaderConfigInternal));
    device->shaderConfigCount = 0;
    
    //descriptor set layouts
    VkDescriptorSetLayout setLayouts[2] = {
        device->textureDescriptorSetLayout,  // will be set = 0 in shaders
        device->samplerDescriptorSetLayout   // will be set = 1 in shaders
    };

    //create a global push constant layout
    VkPushConstantRange gfxPushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,  // All stages can access
        .offset = 0,
        .size = 128  // VulkEase standard: 128 bytes  TODO:  investigate if 256 is better and more supported
    };

    VkPipelineLayoutCreateInfo gfxLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &gfxPushConstantRange
    };

    result = vkCreatePipelineLayout(device->device, &gfxLayoutInfo, NULL, &device->globalGraphicsPipelineLayout);
    if(result != VK_SUCCESS)
    {
        veSetError("Failed to create graphics pipeline layout (VkResult: %d)", result);
        free(device);
        return NULL;
    }

    VkPushConstantRange computePushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,  // Compute only
        .offset = 0,
        .size = 128  // VulkEase standard: 128 bytes  TODO:  investigate if 256 is better and more supported
    };

    VkPipelineLayoutCreateInfo computeLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &computePushConstantRange
    };

    result = vkCreatePipelineLayout(device->device, &computeLayoutInfo, NULL, &device->globalComputePipelineLayout);
    if(result != VK_SUCCESS)
    {
        veSetError("Failed to create compute pipeline layout (VkResult: %d)", result);
        free(device);
        return NULL;
    }

    return (VEDevice*)device;
}

void veDestroyDevice(VEDevice* device) {
    if (!device) return;
    
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    
    if (internal->device) {
        vkDeviceWaitIdle(internal->device);
    }

    // free internal config buffers
    free(internal->shaderConfigs);
    free(internal->vertexConfigs);
    free(internal->renderConfigs);

    //cleanup the pipeline layouts
    vkDestroyPipelineLayout(internal->device, internal->globalGraphicsPipelineLayout, NULL);
    vkDestroyPipelineLayout(internal->device, internal->globalComputePipelineLayout, NULL);

    if(internal->transferCommandPool && internal->transferCommandPool != internal->graphicsCommandPool)
    {
        veDestroyCommandPool(internal, internal->transferCommandPool);
        free(internal->transferCommandPool);
        internal->transferCommandPool = NULL;
    }

    if(internal->computeCommandPool && internal->computeCommandPool != internal->graphicsCommandPool)
    {
        veDestroyCommandPool(internal, internal->computeCommandPool);
        free(internal->computeCommandPool);
        internal->computeCommandPool = NULL;
    }

    if(internal->graphicsCommandPool)
    {
        veDestroyCommandPool(internal, internal->graphicsCommandPool);
        free(internal->graphicsCommandPool);
        internal->graphicsCommandPool = NULL;
    }
    
    veCleanupBindlessDescriptors(internal);
    veCleanupVMA(internal);
    
    if (internal->device) {
        vkDestroyDevice(internal->device, NULL);
    }
    
    free(internal);
}

VEResult veDeviceWaitIdle(VEDevice* device) {
    if (!device) {
        veSetError("Device cannot be NULL");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    VkResult result = vkDeviceWaitIdle(internal->device);
    
    if (result != VK_SUCCESS) {
        veSetError("Failed to wait for device idle (VkResult: %d)", result);
        return VE_ERROR_UNKNOWN;
    }
    
    return VE_SUCCESS;
}

// =============================================================================
// Feature Detection Implementation
// =============================================================================

bool veSupportsBufferDeviceAddress(VEDevice* device) {
    if (!device) return false;
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    return internal->features.bufferDeviceAddress;
}

bool veSupportsDescriptorIndexing(VEDevice* device) {
    if (!device) return false;
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    return internal->features.descriptorIndexing;
}

bool veSupportsShaderObjects(VEDevice* device) {
    if (!device) return false;
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    return internal->features.shaderObject;
}

bool veSupportsExtendedDynamicState3(VEDevice* device) {
    if (!device) return false;
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    return internal->features.extendedDynamicState3;
}

bool veSupportsVertexInputDynamicState(VEDevice* device) {
    if (!device) return false;
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    return internal->features.vertexInputDynamicState;
}

// =============================================================================
// Device Information Implementation
// =============================================================================

const char* veGetDeviceName(VEDevice* device) {
    if (!device) return "Unknown";
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    return internal->deviceProperties.deviceName;
}

const char* veGetDriverVersion(VEDevice* device) {
    if (!device) return "Unknown";
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    
    static char versionString[64];
    snprintf(versionString, sizeof(versionString), "%u.%u.%u",
             VK_VERSION_MAJOR(internal->deviceProperties.driverVersion),
             VK_VERSION_MINOR(internal->deviceProperties.driverVersion),
             VK_VERSION_PATCH(internal->deviceProperties.driverVersion));
    
    return versionString;
}

uint32_t veGetVulkanVersion(VEDevice* device) {
    if (!device) return 0;
    VEDeviceInternal* internal = (VEDeviceInternal*)device;
    return internal->deviceProperties.apiVersion;
}

#define GET_INSTANCE_FUNC(extName) \
    veFuncs.extName = (PFN_##extName)vkGetInstanceProcAddr(instance, #extName);\
    if(veFuncs.extName == NULL) { printf("Failed to find instance function: %s\n", #extName); return false; }

#define GET_DEVICE_FUNC(extName) \
    veFuncs.extName = (PFN_##extName)vkGetDeviceProcAddr(device, #extName); \
    if(veFuncs.extName == NULL) { printf("Failed to find device function: %s\n", #extName); return false; }

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
    GET_DEVICE_FUNC(vkCreateShadersEXT);
    GET_DEVICE_FUNC(vkCmdBindShadersEXT);
    GET_DEVICE_FUNC(vkGetShaderBinaryDataEXT);
    GET_DEVICE_FUNC(vkDestroyShaderEXT);

    GET_DEVICE_FUNC(vkCmdSetPolygonModeEXT);
    GET_DEVICE_FUNC(vkCmdSetDepthClampEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetColorBlendEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetColorWriteMaskEXT);
    GET_DEVICE_FUNC(vkCmdSetColorBlendEquationEXT);
    GET_DEVICE_FUNC(vkCmdSetVertexInputEXT);
    GET_DEVICE_FUNC(vkCmdSetRasterizationSamplesEXT);
    GET_DEVICE_FUNC(vkCmdSetSampleMaskEXT);
    GET_DEVICE_FUNC(vkCmdSetAlphaToCoverageEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetAlphaToOneEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetPatchControlPointsEXT);

    GET_DEVICE_FUNC(vkCmdSetConservativeRasterizationModeEXT);
    GET_DEVICE_FUNC(vkCmdSetLineRasterizationModeEXT);
    GET_DEVICE_FUNC(vkCmdSetProvokingVertexModeEXT);

    GET_DEVICE_FUNC(vkCmdSetLogicOpEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetLogicOpEXT);


    return true;
}
