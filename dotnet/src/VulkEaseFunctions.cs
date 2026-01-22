using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;

namespace VulkEase
{
    // =============================================================================
    // SECTION 1: VERSION & API CONSTANTS
    // =============================================================================

    public static class VEConstants
    {
        public static VEBufferAddress VE_INVALID_ADDRESS = new VEBufferAddress { native = 0UL };
        public static VETextureIndex VE_INVALID_TEXTURE_INDEX = new VETextureIndex { native = 0xFFFFFFFF };
        public static VESamplerIndex VE_INVALID_SAMPLER_INDEX = new VESamplerIndex { native = 0xFFFFFFFF };

        public const UInt32 VULKEASE_VERSION_MAJOR = 2;
        public const UInt32 VULKEASE_VERSION_MINOR = 0;
        public const UInt32 VULKEASE_VERSION_PATCH = 0;

        public static UInt32 VE_MAKE_VERSION(UInt32 major, UInt32 minor, UInt32 patch) =>
            (((major) << 22) | ((minor) << 12) | (patch));

        public static readonly UInt32 VULKEASE_API_VERSION_2_0 = VE_MAKE_VERSION(2, 0, 0);
    }

    /// <summary>
    /// VulkEase main API class - Modern Vulkan 1.4+ Bindless Graphics API
    /// </summary>
    public static class VulkEase
    {
        // =============================================================================
        // UTILITY METHODS
        // =============================================================================

        private static IntPtr StringToHGlobalAnsi(string str)
        {
            return string.IsNullOrEmpty(str) ? IntPtr.Zero : Marshal.StringToHGlobalAnsi(str);
        }

        private static string? PtrToStringAnsi(IntPtr ptr)
        {
            return ptr == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(ptr);
        }

        // =============================================================================
        // SECTION 17: CONTEXT & DEVICE FUNCTIONS
        // =============================================================================

        #region Context & Device Functions

        /// <summary>
        /// Get the VulkEase library version.
        /// </summary>
        /// <returns>Version number encoded as (major &lt;&lt; 22) | (minor &lt;&lt; 12) | patch.
        /// Use VULKEASE_VERSION_MAJOR, VULKEASE_VERSION_MINOR, VULKEASE_VERSION_PATCH to decode.</returns>
        public static UInt32 GetVersion()
        {
            return VulkEaseDll.veGetVersion();
        }

        /// <summary>
        /// Create a VulkEase context, initializing the Vulkan 1.4 instance.
        /// </summary>
        /// <param name="applicationName">Application name shown in debug tools and driver info. Can be null for a default name.</param>
        /// <param name="additionalInstanceExtensions">Optional array of additional instance extension names to enable. Can be null.</param>
        /// <returns>The created VEContext handle.</returns>
        /// <exception cref="InvalidOperationException">Thrown if context creation fails due to:
        /// VE_ERROR_OUT_OF_MEMORY if allocation failed, or
        /// VE_ERROR_FEATURE_NOT_SUPPORTED if required Vulkan version/extensions unavailable.</exception>
        /// <remarks>
        /// The context is the root object for VulkEase. It owns the VkInstance and must outlive all devices created from it.
        /// Call DestroyContext() when done, after destroying all child devices.
        /// </remarks>
        public static VEContext CreateContext(string applicationName, string[]? additionalInstanceExtensions = null)
        {
            IntPtr extensionsPtr = IntPtr.Zero;
            IntPtr[]? extensionPtrs = null;
            uint extensionCount = 0;

            try
            {
                if (additionalInstanceExtensions != null && additionalInstanceExtensions.Length > 0)
                {
                    extensionCount = (uint)additionalInstanceExtensions.Length;
                    extensionPtrs = new IntPtr[extensionCount];
                    for (int i = 0; i < extensionCount; i++)
                    {
                        extensionPtrs[i] = StringToHGlobalAnsi(additionalInstanceExtensions[i]);
                    }
                    extensionsPtr = Marshal.AllocHGlobal(IntPtr.Size * (int)extensionCount);
                    Marshal.Copy(extensionPtrs.Select(p => p.ToInt64()).ToArray(), 0, extensionsPtr, (int)extensionCount);
                }

                VEResult result = VulkEaseDll.veCreateContext(applicationName, extensionsPtr, extensionCount, out IntPtr native);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateContext failed: {result}");
                return new VEContext { native = native };
            }
            finally
            {
                if (extensionPtrs != null)
                {
                    foreach (var ptr in extensionPtrs)
                    {
                        if (ptr != IntPtr.Zero) Marshal.FreeHGlobal(ptr);
                    }
                }
                if (extensionsPtr != IntPtr.Zero) Marshal.FreeHGlobal(extensionsPtr);
            }
        }

        /// <summary>
        /// Destroy a VulkEase context and release all associated resources.
        /// </summary>
        /// <param name="context">Context handle to destroy. Can be default (no-op).</param>
        /// <remarks>All devices created from this context must be destroyed before calling this.</remarks>
        public static void DestroyContext(VEContext context)
        {
            _ = VulkEaseDll.veDestroyContext(context.native);
        }

        /// <summary>
        /// Enumerate the number of available physical devices (GPUs).
        /// </summary>
        /// <param name="context">Valid VulkEase context.</param>
        /// <param name="count">Receives the number of available physical devices.</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_INVALID_PARAMETER if context is invalid.</returns>
        /// <remarks>Call this before GetPhysicalDeviceInfo() to determine how many devices are available for selection.</remarks>
        public static VEResult EnumeratePhysicalDevices(VEContext context, out uint count)
        {
            return VulkEaseDll.veEnumeratePhysicalDevices(context.native, out count);
        }

        /// <summary>
        /// Get information about a specific physical device.
        /// </summary>
        /// <param name="context">Valid VulkEase context.</param>
        /// <param name="deviceIndex">Index of the device (0 to count-1 from EnumeratePhysicalDevices).</param>
        /// <param name="physicalDevice">Receives the VkPhysicalDevice handle.</param>
        /// <param name="deviceName">Receives the device name string.</param>
        /// <param name="deviceType">Receives the device type enum (Discrete, Integrated, etc.).</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_INVALID_PARAMETER if context is invalid or deviceIndex out of range.</returns>
        public static VEResult GetPhysicalDeviceInfo(VEContext context, uint deviceIndex, out IntPtr physicalDevice, out string? deviceName, out VkPhysicalDeviceType? deviceType)
        {
            IntPtr nameBuffer = Marshal.AllocHGlobal(256);
            IntPtr typeBuffer = Marshal.AllocHGlobal(sizeof(int));
            try
            {
                var result = VulkEaseDll.veGetPhysicalDeviceInfo(context.native, deviceIndex, out physicalDevice, nameBuffer, typeBuffer);
                deviceName = result == VEResult.VE_SUCCESS ? PtrToStringAnsi(nameBuffer) : null;
                deviceType = result == VEResult.VE_SUCCESS ? (VkPhysicalDeviceType)Marshal.ReadInt32(typeBuffer) : null;
                return result;
            }
            finally
            {
                Marshal.FreeHGlobal(nameBuffer);
                Marshal.FreeHGlobal(typeBuffer);
            }
        }

        /// <summary>
        /// Create a VulkEase device from the context.
        /// </summary>
        /// <param name="context">Valid VulkEase context.</param>
        /// <param name="preferredDevice">VkPhysicalDevice to use, or IntPtr.Zero for auto-selection (prefers discrete GPUs over integrated).</param>
        /// <param name="additionalDeviceExtensions">Optional array of additional device extension names to enable. Can be null.</param>
        /// <returns>The created VEDevice handle.</returns>
        /// <exception cref="InvalidOperationException">Thrown if device creation fails due to:
        /// VE_ERROR_OUT_OF_MEMORY if allocation failed, or
        /// VE_ERROR_FEATURE_NOT_SUPPORTED if required features unavailable.</exception>
        /// <remarks>Call DestroyDevice() when done, before destroying the context.</remarks>
        public static VEDevice CreateDevice(VEContext context, IntPtr preferredDevice = default, string[]? additionalDeviceExtensions = null)
        {
            IntPtr extensionsPtr = IntPtr.Zero;
            IntPtr[]? extensionPtrs = null;
            uint extensionCount = 0;

            try
            {
                if (additionalDeviceExtensions != null && additionalDeviceExtensions.Length > 0)
                {
                    extensionCount = (uint)additionalDeviceExtensions.Length;
                    extensionPtrs = new IntPtr[extensionCount];
                    for (int i = 0; i < extensionCount; i++)
                    {
                        extensionPtrs[i] = StringToHGlobalAnsi(additionalDeviceExtensions[i]);
                    }
                    extensionsPtr = Marshal.AllocHGlobal(IntPtr.Size * (int)extensionCount);
                    Marshal.Copy(extensionPtrs.Select(p => p.ToInt64()).ToArray(), 0, extensionsPtr, (int)extensionCount);
                }

                VEResult result = VulkEaseDll.veCreateDevice(context.native, preferredDevice, extensionsPtr, extensionCount, out IntPtr native);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateDevice failed: {result}");
                return new VEDevice { native = native };
            }
            finally
            {
                if (extensionPtrs != null)
                {
                    foreach (var ptr in extensionPtrs)
                    {
                        if (ptr != IntPtr.Zero) Marshal.FreeHGlobal(ptr);
                    }
                }
                if (extensionsPtr != IntPtr.Zero) Marshal.FreeHGlobal(extensionsPtr);
            }
        }

        /// <summary>
        /// Destroy a VulkEase device and release all associated resources.
        /// </summary>
        /// <param name="device">Device handle to destroy. Can be default (no-op).</param>
        /// <remarks>All resources created from this device (buffers, textures, shaders, etc.) should be destroyed before calling this.</remarks>
        public static void DestroyDevice(VEDevice device)
        {
            _ = VulkEaseDll.veDestroyDevice(device.native);
        }

        /// <summary>
        /// Check if a Vulkan instance extension is available.
        /// </summary>
        /// <param name="extensionName">Name of the extension to check (e.g., "VK_KHR_surface").</param>
        /// <returns>True if the extension is available, false otherwise.</returns>
        /// <remarks>Query availability before creating a context if you need specific extensions.</remarks>
        public static bool IsInstanceExtensionAvailable(string extensionName)
        {
            return VulkEaseDll.veIsInstanceExtensionAvailable(extensionName);
        }

        /// <summary>
        /// Check if a Vulkan device extension is available.
        /// </summary>
        /// <param name="context">Valid VulkEase context.</param>
        /// <param name="extensionName">Name of the extension to check (e.g., "VK_KHR_swapchain").</param>
        /// <returns>True if the extension is available, false otherwise or if context is invalid.</returns>
        /// <remarks>Query availability before creating a device if you need specific extensions.</remarks>
        public static bool IsDeviceExtensionAvailable(VEContext context, string extensionName)
        {
            return VulkEaseDll.veIsDeviceExtensionAvailable(context.native, extensionName);
        }

        /// <summary>
        /// Wait for the device to become idle.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>VE_SUCCESS on success, VE_ERROR_INVALID_PARAMETER if device is invalid, or VE_ERROR_DEVICE_LOST if the device was lost.</returns>
        /// <remarks>
        /// Blocks until all GPU operations on all queues have completed.
        /// Useful for cleanup or synchronization points.
        /// Warning: This is a heavy synchronization operation. Avoid in performance-critical paths.
        /// </remarks>
        public static VEResult DeviceWaitIdle(VEDevice device)
        {
            return VulkEaseDll.veDeviceWaitIdle(device.native);
        }

        /// <summary>
        /// Get the GPU device name as a human-readable string.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Device name string, or null if device is invalid. The string is owned by VulkEase and valid until the device is destroyed.</returns>
        public static string? GetDeviceName(VEDevice device)
        {
            return PtrToStringAnsi(VulkEaseDll.veGetDeviceName(device.native));
        }

        /// <summary>
        /// Get the GPU driver version as a human-readable string.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Driver version string, or null if device is invalid. The string is owned by VulkEase and valid until the device is destroyed.</returns>
        public static string? GetDriverVersion(VEDevice device)
        {
            return PtrToStringAnsi(VulkEaseDll.veGetDriverVersion(device.native));
        }

        /// <summary>
        /// Get the Vulkan API version supported by the device.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Vulkan version encoded as VK_MAKE_API_VERSION(), or 0 if device is invalid.</returns>
        public static UInt32 GetVulkanVersion(VEDevice device)
        {
            return VulkEaseDll.veGetVulkanVersion(device.native);
        }

        #endregion

        // =============================================================================
        // SECTION 18: VULKAN HANDLE ACCESS (Escape Hatches)
        // =============================================================================

        #region Vulkan Handle Access

        /// <summary>
        /// Get the underlying VkInstance handle.
        /// </summary>
        /// <param name="context">Valid VulkEase context.</param>
        /// <returns>VkInstance handle, or IntPtr.Zero if context is invalid.</returns>
        /// <remarks>
        /// Provides access to the raw Vulkan instance for custom extension use or interop with other Vulkan libraries.
        /// Warning: The returned handle is owned by VulkEase. Do not destroy it directly. Lifetime matches the owning VEContext.
        /// </remarks>
        public static IntPtr GetVkInstance(VEContext context)
        {
            return VulkEaseDll.veGetVkInstance(context.native);
        }

        /// <summary>
        /// Get the underlying VkPhysicalDevice handle.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>VkPhysicalDevice handle, or IntPtr.Zero if device is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.</remarks>
        public static IntPtr GetVkPhysicalDevice(VEDevice device)
        {
            return VulkEaseDll.veGetVkPhysicalDevice(device.native);
        }

        /// <summary>
        /// Get the underlying VkDevice handle.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>VkDevice handle, or IntPtr.Zero if device is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Do not destroy it directly. Lifetime matches the owning VEDevice.</remarks>
        public static IntPtr GetVkDevice(VEDevice device)
        {
            return VulkEaseDll.veGetVkDevice(device.native);
        }

        /// <summary>
        /// Get the graphics queue handle.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>VkQueue handle for graphics operations, or IntPtr.Zero if device is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.</remarks>
        public static IntPtr GetVkGraphicsQueue(VEDevice device)
        {
            return VulkEaseDll.veGetVkGraphicsQueue(device.native);
        }

        /// <summary>
        /// Get the compute queue handle.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>VkQueue handle for compute operations, or IntPtr.Zero if device is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.</remarks>
        public static IntPtr GetVkComputeQueue(VEDevice device)
        {
            return VulkEaseDll.veGetVkComputeQueue(device.native);
        }

        /// <summary>
        /// Get the transfer queue handle.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>VkQueue handle for transfer operations, or IntPtr.Zero if device is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the owning VEDevice.</remarks>
        public static IntPtr GetVkTransferQueue(VEDevice device)
        {
            return VulkEaseDll.veGetVkTransferQueue(device.native);
        }

        /// <summary>
        /// Get the fence associated with a command buffer.
        /// </summary>
        /// <param name="cmd">Valid VulkEase command buffer.</param>
        /// <returns>VkFence handle, or IntPtr.Zero if cmd is invalid.</returns>
        /// <remarks>
        /// Each command buffer has an associated fence for synchronization purposes.
        /// Warning: The returned handle is owned by VulkEase. Lifetime matches the owning VECommandBuffer.
        /// </remarks>
        public static IntPtr GetVkCommandBufferFence(VECommandBuffer cmd)
        {
            return VulkEaseDll.veGetVkCommandBufferFence(cmd.native);
        }

        /// <summary>
        /// Get the VkBuffer handle from a buffer device address.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Buffer device address obtained from CreateBuffer().</param>
        /// <returns>VkBuffer handle, or IntPtr.Zero if device is invalid or address is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the buffer.</remarks>
        public static IntPtr GetVkBufferFromAddress(VEDevice device, ulong address)
        {
            return VulkEaseDll.veGetVkBufferFromAddress(device.native, address);
        }

        /// <summary>
        /// Get the VkImage handle from a texture index.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture index obtained from CreateTexture().</param>
        /// <returns>VkImage handle, or IntPtr.Zero if device is invalid or index is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the texture.</remarks>
        public static IntPtr GetVkImageFromTexture(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseDll.veGetVkImageFromTexture(device.native, texture.native);
        }

        /// <summary>
        /// Get the VkImageView handle from a texture index.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture index obtained from CreateTexture().</param>
        /// <returns>VkImageView handle, or IntPtr.Zero if device is invalid or index is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the texture.</remarks>
        public static IntPtr GetVkImageViewFromTexture(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseDll.veGetVkImageViewFromTexture(device.native, texture.native);
        }

        /// <summary>
        /// Get the VkSampler handle from a sampler index.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="sampler">Sampler index obtained from CreateSampler().</param>
        /// <returns>VkSampler handle, or IntPtr.Zero if device is invalid or index is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the sampler.</remarks>
        public static IntPtr GetVkSamplerFromIndex(VEDevice device, VESamplerIndex sampler)
        {
            return VulkEaseDll.veGetVkSamplerFromIndex(device.native, sampler.native);
        }

        /// <summary>
        /// Get the VkSwapchainKHR handle from a swapchain.
        /// </summary>
        /// <param name="swapchain">Valid VulkEase swapchain.</param>
        /// <returns>VkSwapchainKHR handle, or IntPtr.Zero if swapchain is invalid.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the swapchain.</remarks>
        public static IntPtr GetVkSwapchain(VESwapchain swapchain)
        {
            return VulkEaseDll.veGetVkSwapchain(swapchain.native);
        }

        /// <summary>
        /// Get a swapchain image by index.
        /// </summary>
        /// <param name="swapchain">Valid VulkEase swapchain.</param>
        /// <param name="imageIndex">Index of the swapchain image (0 to image count - 1).</param>
        /// <returns>VkImage handle, or IntPtr.Zero if swapchain is invalid or index is out of range.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the swapchain.</remarks>
        public static IntPtr GetVkSwapchainImage(VESwapchain swapchain, uint imageIndex)
        {
            return VulkEaseDll.veGetVkSwapchainImage(swapchain.native, imageIndex);
        }

        /// <summary>
        /// Get a swapchain image view by index.
        /// </summary>
        /// <param name="swapchain">Valid VulkEase swapchain.</param>
        /// <param name="imageIndex">Index of the swapchain image (0 to image count - 1).</param>
        /// <returns>VkImageView handle, or IntPtr.Zero if swapchain is invalid or index is out of range.</returns>
        /// <remarks>Warning: The returned handle is owned by VulkEase. Lifetime matches the swapchain.</remarks>
        public static IntPtr GetVkSwapchainImageView(VESwapchain swapchain, uint imageIndex)
        {
            return VulkEaseDll.veGetVkSwapchainImageView(swapchain.native, imageIndex);
        }

        #endregion

        // =============================================================================
        // SECTION 19: MESSAGE CALLBACK FUNCTIONS
        // =============================================================================

        #region Message Callback Functions

        /// <summary>
        /// Delegate for receiving VulkEase diagnostic messages.
        /// </summary>
        /// <param name="severity">Severity level of the message.</param>
        /// <param name="message">The message text.</param>
        public delegate void MessageCallbackDelegate(VEMessageSeverity severity, string message);

        // Store the delegate to prevent garbage collection
        private static VulkEaseDll.VEMessageCallback? _nativeCallback;
        private static MessageCallbackDelegate? _userCallback;

        /// <summary>
        /// Set the global debug/diagnostic message callback.
        /// </summary>
        /// <param name="callback">Callback function to receive messages. Pass null to disable.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>
        /// The callback may be invoked from any thread. Ensure your callback
        /// implementation is thread-safe and non-blocking.
        /// </remarks>
        public static VEResult SetMessageCallback(MessageCallbackDelegate? callback)
        {
            if (callback == null)
            {
                _userCallback = null;
                _nativeCallback = null;
                var descNull = new VulkEaseDll.VEMessageCallbackDesc { callback = null!, userData = IntPtr.Zero };
                return VulkEaseDll.veSetMessageCallback(ref descNull);
            }

            _userCallback = callback;
            _nativeCallback = (severity, messagePtr, userData) =>
            {
                string? message = Marshal.PtrToStringAnsi(messagePtr);
                _userCallback?.Invoke(severity, message ?? string.Empty);
            };

            var desc = new VulkEaseDll.VEMessageCallbackDesc
            {
                callback = _nativeCallback,
                userData = IntPtr.Zero
            };
            return VulkEaseDll.veSetMessageCallback(ref desc);
        }

        /// <summary>
        /// Set the minimum message severity that will be emitted.
        /// </summary>
        /// <param name="minSeverity">Minimum severity level to emit. Messages below this are filtered out.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Default is VE_MESSAGE_SEVERITY_WARNING.</remarks>
        public static VEResult SetMinMessageSeverity(VEMessageSeverity minSeverity)
        {
            return VulkEaseDll.veSetMinMessageSeverity(minSeverity);
        }

        #endregion

        // =============================================================================
        // SECTION 20: FENCE FUNCTIONS
        // =============================================================================

        #region Fence Functions

        /// <summary>
        /// Create a fence for GPU-CPU synchronization.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="signaled">If true, the fence starts in signaled state. Set to true when you want to skip the first wait.</param>
        /// <param name="fence">Receives the VkFence handle on success.</param>
        /// <returns>VE_SUCCESS on success, VE_ERROR_INVALID_PARAMETER if device or fence is invalid, or VE_ERROR_OUT_OF_MEMORY if allocation failed.</returns>
        /// <remarks>
        /// Fences are used to synchronize the CPU with GPU operations.
        /// They can be passed to async transfer operations or used for custom synchronization.
        /// Call DestroyFence() when done.
        /// </remarks>
        public static VEResult CreateFence(VEDevice device, bool signaled, out IntPtr fence)
        {
            return VulkEaseDll.veCreateFence(device.native, signaled, out fence);
        }

        /// <summary>
        /// Destroy a fence.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="fence">Fence handle to destroy. Can be IntPtr.Zero (no-op).</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_INVALID_PARAMETER if device is invalid.</returns>
        public static VEResult DestroyFence(VEDevice device, IntPtr fence)
        {
            return VulkEaseDll.veDestroyFence(device.native, fence);
        }

        /// <summary>
        /// Wait for a fence to be signaled.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="fence">Fence to wait on.</param>
        /// <param name="timeout">Maximum time to wait in nanoseconds. Default is UInt64.MaxValue (infinite).</param>
        /// <returns>VE_SUCCESS if signaled, VE_TIMEOUT if timeout expired, or VE_ERROR_DEVICE_LOST if device was lost.</returns>
        /// <remarks>Blocks the calling thread until the fence is signaled or the timeout expires.</remarks>
        public static VEResult WaitFence(VEDevice device, IntPtr fence, UInt64 timeout = UInt64.MaxValue)
        {
            return VulkEaseDll.veWaitFence(device.native, fence, timeout);
        }

        /// <summary>
        /// Check if a fence is signaled without blocking.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="fence">Fence to check.</param>
        /// <param name="signaled">Receives true if the fence is signaled, false otherwise.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult GetFenceStatus(VEDevice device, IntPtr fence, out bool signaled)
        {
            return VulkEaseDll.veGetFenceStatus(device.native, fence, out signaled);
        }

        /// <summary>
        /// Reset a fence to unsignaled state.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="fence">Fence to reset.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Must be called before reusing a fence that has been signaled.</remarks>
        public static VEResult ResetFence(VEDevice device, IntPtr fence)
        {
            return VulkEaseDll.veResetFence(device.native, fence);
        }

        #endregion

        // =============================================================================
        // SECTION 21: BUFFER FUNCTIONS
        // =============================================================================

        #region Buffer Functions

        /// <summary>
        /// Create a buffer and return its GPU device address.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Buffer descriptor specifying size, usage, and optional initial data.</param>
        /// <returns>GPU device address of the created buffer. Pass directly to shaders via push constants.</returns>
        /// <exception cref="InvalidOperationException">Thrown if buffer creation fails due to invalid parameters or out of memory.</exception>
        /// <remarks>
        /// VulkEase buffers use buffer device addresses (BDA) instead of handles.
        /// The 64-bit address can be passed directly to shaders via push constants.
        /// Call DestroyBuffer() when done.
        /// </remarks>
        public static VEBufferAddress CreateBuffer(VEDevice device, VEBufferDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateBuffer(device.native, ref desc, out ulong address);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateBuffer failed: {result}");
            return new VEBufferAddress { native = address };
        }

        /// <summary>
        /// Destroy a buffer by its device address.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Buffer address to destroy. Can be VE_INVALID_ADDRESS (no-op).</param>
        public static void DestroyBuffer(VEDevice device, VEBufferAddress address)
        {
            _ = VulkEaseDll.veDestroyBuffer(device.native, address.native);
        }

        /// <summary>
        /// Map a buffer for CPU access.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Buffer to map.</param>
        /// <param name="mappedData">Receives pointer to the mapped memory.</param>
        /// <returns>VE_SUCCESS on success, VE_ERROR_MEMORY_MAP_FAILED if mapping failed.</returns>
        /// <remarks>The buffer must have been created with host-visible memory. Call UnmapBuffer() when done.</remarks>
        public static VEResult MapBuffer(VEDevice device, VEBufferAddress address, out IntPtr mappedData)
        {
            return VulkEaseDll.veMapBuffer(device.native, address.native, out mappedData);
        }

        /// <summary>
        /// Unmap a previously mapped buffer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Buffer to unmap.</param>
        public static void UnmapBuffer(VEDevice device, VEBufferAddress address)
        {
            _ = VulkEaseDll.veUnmapBuffer(device.native, address.native);
        }

        /// <summary>
        /// Update buffer data from the CPU.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Target buffer address.</param>
        /// <param name="data">Pointer to source data.</param>
        /// <param name="size">Number of bytes to copy.</param>
        /// <param name="offset">Byte offset into the buffer (default 0).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Uses persistent mapping or host transfer for efficient updates.</remarks>
        public static VEResult UpdateBuffer(VEDevice device, VEBufferAddress address, IntPtr data, UInt64 size, UInt64 offset = default)
        {
            return VulkEaseDll.veUpdateBuffer(device.native, address.native, data, size, offset);
        }

        /// <summary>
        /// Get the size of a buffer in bytes.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Buffer to query.</param>
        /// <returns>Size in bytes, or 0 if buffer is invalid.</returns>
        public static UIntPtr GetBufferSize(VEDevice device, VEBufferAddress address)
        {
            return new UIntPtr(VulkEaseDll.veGetBufferSize(device.native, address.native));
        }

        /// <summary>
        /// Get the usage flags of a buffer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Buffer to query.</param>
        /// <returns>VkBufferUsageFlags, or 0 if buffer is invalid.</returns>
        public static VkBufferUsageFlags GetBufferUsage(VEDevice device, VEBufferAddress address)
        {
            return VulkEaseDll.veGetBufferUsage(device.native, address.native);
        }

        /// <summary>
        /// Create a vertex buffer with initial data.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="vertices">Pointer to vertex data.</param>
        /// <param name="size">Size of vertex data in bytes.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>GPU device address of the created buffer.</returns>
        /// <exception cref="InvalidOperationException">Thrown if buffer creation fails.</exception>
        public static VEBufferAddress CreateVertexBuffer(VEDevice device, IntPtr vertices, UInt64 size, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateVertexBuffer(device.native, vertices, size, namePtr, out ulong address);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateVertexBuffer failed: {result}");
                return new VEBufferAddress { native = address };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create an index buffer with initial data.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="indices">Pointer to index data.</param>
        /// <param name="size">Size of index data in bytes.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>GPU device address of the created buffer.</returns>
        /// <exception cref="InvalidOperationException">Thrown if buffer creation fails.</exception>
        public static VEBufferAddress CreateIndexBuffer(VEDevice device, IntPtr indices, UInt64 size, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateIndexBuffer(device.native, indices, size, namePtr, out ulong address);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateIndexBuffer failed: {result}");
                return new VEBufferAddress { native = address };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create a uniform buffer for shader constants.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="size">Size of buffer in bytes.</param>
        /// <param name="persistentlyMapped">If true, the buffer remains mapped for easy CPU updates.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>GPU device address of the created buffer.</returns>
        /// <exception cref="InvalidOperationException">Thrown if buffer creation fails.</exception>
        /// <remarks>Uniform buffers are ideal for per-frame data like view/projection matrices.</remarks>
        public static VEBufferAddress CreateUniformBuffer(VEDevice device, UInt64 size, bool persistentlyMapped, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateUniformBuffer(device.native, size, persistentlyMapped, namePtr, out ulong address);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateUniformBuffer failed: {result}");
                return new VEBufferAddress { native = address };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create a storage buffer for large read/write data.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="size">Size of buffer in bytes.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>GPU device address of the created buffer.</returns>
        /// <exception cref="InvalidOperationException">Thrown if buffer creation fails.</exception>
        /// <remarks>Storage buffers (SSBOs) can be larger than uniform buffers and support read/write access from shaders.</remarks>
        public static VEBufferAddress CreateStorageBuffer(VEDevice device, UInt64 size, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateStorageBuffer(device.native, size, namePtr, out ulong address);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateStorageBuffer failed: {result}");
                return new VEBufferAddress { native = address };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create an indirect draw/dispatch buffer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="size">Size of buffer in bytes.</param>
        /// <param name="debugName">Optional debug name for the buffer. Can be null.</param>
        /// <returns>GPU device address of the created buffer.</returns>
        /// <exception cref="InvalidOperationException">Thrown if buffer creation fails.</exception>
        /// <remarks>Creates a buffer for indirect drawing or compute dispatch commands populated by the GPU.</remarks>
        public static VEBufferAddress CreateIndirectBuffer(VEDevice device, UInt64 size, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateIndirectBuffer(device.native, size, namePtr, out ulong address);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateIndirectBuffer failed: {result}");
                return new VEBufferAddress { native = address };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Copy buffer data on the GPU (deferred/command buffer version).
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="src">Source buffer address.</param>
        /// <param name="dst">Destination buffer address.</param>
        /// <param name="srcOffset">Byte offset into source buffer.</param>
        /// <param name="dstOffset">Byte offset into destination buffer.</param>
        /// <param name="size">Number of bytes to copy.</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_INVALID_PARAMETER if cmd is invalid or addresses are invalid.</returns>
        /// <remarks>Records a buffer copy operation into a command buffer for later execution.</remarks>
        public static VEResult CmdCopyBuffer(VECommandBuffer cmd, VEBufferAddress src, VEBufferAddress dst,
            UInt64 srcOffset, UInt64 dstOffset, UInt64 size)
        {
            return VulkEaseDll.veCmdCopyBuffer(cmd.native, src.native, dst.native, srcOffset, dstOffset, size);
        }

        /// <summary>
        /// Copy buffer data on the GPU (immediate version).
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="src">Source buffer address.</param>
        /// <param name="dst">Destination buffer address.</param>
        /// <param name="srcOffset">Byte offset into source buffer.</param>
        /// <param name="dstOffset">Byte offset into destination buffer.</param>
        /// <param name="size">Number of bytes to copy.</param>
        /// <param name="fence">Optional fence to signal when the copy completes. Can be IntPtr.Zero.</param>
        /// <returns>VE_SUCCESS on success, or an error code on failure.</returns>
        /// <remarks>Executes the copy immediately on a transfer queue. If fence is provided, signals it when complete.</remarks>
        public static VEResult CopyBuffer(VEDevice device, VEBufferAddress src, VEBufferAddress dst,
            UInt64 srcOffset, UInt64 dstOffset, UInt64 size, IntPtr fence = default)
        {
            return VulkEaseDll.veCopyBuffer(device.native, src.native, dst.native, srcOffset, dstOffset, size, fence);
        }

        /// <summary>
        /// Fill a buffer with a 32-bit value.
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="dst">Destination buffer address.</param>
        /// <param name="offset">Byte offset into the buffer (must be 4-byte aligned).</param>
        /// <param name="size">Number of bytes to fill (must be 4-byte aligned or VK_WHOLE_SIZE).</param>
        /// <param name="data">32-bit value to fill with.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdFillBuffer(VECommandBuffer cmd, VEBufferAddress dst, UInt64 offset, UInt64 size, UInt32 data)
        {
            return VulkEaseDll.veCmdFillBuffer(cmd.native, dst.native, offset, size, data);
        }

        /// <summary>
        /// Update buffer with inline data during command buffer recording.
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="dst">Destination buffer address.</param>
        /// <param name="offset">Byte offset into the buffer.</param>
        /// <param name="data">Data to copy into the buffer.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>For small updates (less than 65536 bytes). Larger updates should use staging buffers.</remarks>
        public static VEResult CmdUpdateBuffer(VECommandBuffer cmd, VEBufferAddress dst, UInt64 offset, byte[] data)
        {
            var handle = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                return VulkEaseDll.veCmdUpdateBuffer(cmd.native, dst.native, offset, (UInt64)data.Length, handle.AddrOfPinnedObject());
            }
            finally
            {
                handle.Free();
            }
        }

        #endregion

        // =============================================================================
        // SECTION 22: TEXTURE FUNCTIONS
        // =============================================================================

        #region Texture Functions

        /// <summary>
        /// Create a texture and return its bindless index.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Texture descriptor with dimensions, format, usage, and optional initial data.</param>
        /// <returns>32-bit bindless texture index. Pass to shaders via push constants.</returns>
        /// <exception cref="InvalidOperationException">Thrown if texture creation fails.</exception>
        /// <remarks>
        /// VulkEase textures use bindless indices for access from shaders.
        /// The returned index can be used to sample the texture via the global texture array.
        /// Call DestroyTexture() when done.
        /// </remarks>
        public static VETextureIndex CreateTexture(VEDevice device, VETextureDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateTexture(device.native, ref desc, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateTexture failed: {result}");
            return new VETextureIndex { native = index };
        }

        /// <summary>
        /// Destroy a texture by its bindless index.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="index">Texture index to destroy. Can be VE_INVALID_TEXTURE_INDEX (no-op).</param>
        public static void DestroyTexture(VEDevice device, VETextureIndex index)
        {
            _ = VulkEaseDll.veDestroyTexture(device.native, index.native);
        }

        /// <summary>
        /// Get the dimensions of a texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="index">Texture index to query.</param>
        /// <returns>VkExtent3D with width, height, and depth.</returns>
        public static VkExtent3D GetTextureSize(VEDevice device, VETextureIndex index)
        {
            return VulkEaseDll.veGetTextureSize(device.native, index.native);
        }

        /// <summary>
        /// Get the format of a texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="index">Texture index to query.</param>
        /// <returns>VkFormat of the texture, or VK_FORMAT_UNDEFINED if invalid.</returns>
        public static VkFormat GetTextureFormat(VEDevice device, VETextureIndex index)
        {
            return VulkEaseDll.veGetTextureFormat(device.native, index.native);
        }

        /// <summary>
        /// Create a 1D texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="width">Width in pixels.</param>
        /// <param name="format">Pixel format.</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if texture creation fails.</exception>
        public static VETextureIndex CreateTexture1D(VEDevice device, UInt32 width, VkFormat format, VkImageUsageFlags usage, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateTexture1D(device.native, width, format, usage, namePtr, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateTexture1D failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create a 2D texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="width">Width in pixels.</param>
        /// <param name="height">Height in pixels.</param>
        /// <param name="format">Pixel format.</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if texture creation fails.</exception>
        public static VETextureIndex CreateTexture2D(VEDevice device, UInt32 width, UInt32 height, VkFormat format, VkImageUsageFlags usage, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateTexture2D(device.native, width, height, format, usage, namePtr, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateTexture2D failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create a 3D (volume) texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="width">Width in pixels.</param>
        /// <param name="height">Height in pixels.</param>
        /// <param name="depth">Depth in pixels.</param>
        /// <param name="format">Pixel format.</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if texture creation fails.</exception>
        public static VETextureIndex CreateTexture3D(VEDevice device, UInt32 width, UInt32 height, UInt32 depth, VkFormat format, VkImageUsageFlags usage, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateTexture3D(device.native, width, height, depth, format, usage, namePtr, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateTexture3D failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create a 2D texture array.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="width">Width in pixels.</param>
        /// <param name="height">Height in pixels.</param>
        /// <param name="layers">Number of array layers.</param>
        /// <param name="format">Pixel format.</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if texture creation fails.</exception>
        public static VETextureIndex CreateTexture2DArray(VEDevice device, UInt32 width, UInt32 height, UInt32 layers, VkFormat format, VkImageUsageFlags usage, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateTexture2DArray(device.native, width, height, layers, format, usage, namePtr, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateTexture2DArray failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create a cube map texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="size">Width and height of each face in pixels (cube faces are square).</param>
        /// <param name="format">Pixel format.</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if texture creation fails.</exception>
        public static VETextureIndex CreateTextureCube(VEDevice device, UInt32 size, VkFormat format, VkImageUsageFlags usage, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateTextureCube(device.native, size, format, usage, namePtr, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateTextureCube failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Create a multisampled 2D texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="width">Width in pixels.</param>
        /// <param name="height">Height in pixels.</param>
        /// <param name="format">Pixel format.</param>
        /// <param name="sampleCount">Number of samples per pixel (e.g., VK_SAMPLE_COUNT_4_BIT).</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if texture creation fails.</exception>
        public static VETextureIndex CreateTexture2DMultisample(VEDevice device, UInt32 width, UInt32 height, VkFormat format, VkSampleCountFlags sampleCount, VkImageUsageFlags usage, string? debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veCreateTexture2DMultisample(device.native, width, height, format, sampleCount, usage, namePtr, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateTexture2DMultisample failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Load a texture from an image file.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="filename">Path to the image file (PNG, JPG, BMP, TGA, etc.).</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="generateMips">If true, automatically generates mipmaps.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if loading fails.</exception>
        public static VETextureIndex LoadTexture(VEDevice device, string filename, VkImageUsageFlags usage, bool generateMips)
        {
            var namePtr = StringToHGlobalAnsi(filename);
            try
            {
                VEResult result = VulkEaseDll.veLoadTexture(device.native, namePtr, usage, generateMips, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veLoadTexture failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Load an HDR texture from a file.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="filename">Path to the HDR image file.</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="generateMips">If true, automatically generates mipmaps.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if loading fails.</exception>
        public static VETextureIndex LoadHDRTexture(VEDevice device, string filename, VkImageUsageFlags usage, bool generateMips)
        {
            var namePtr = StringToHGlobalAnsi(filename);
            try
            {
                VEResult result = VulkEaseDll.veLoadHDRTexture(device.native, namePtr, usage, generateMips, out uint index);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veLoadHDRTexture failed: {result}");
                return new VETextureIndex { native = index };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Load a cube map texture from 6 image files.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="filenames">Array of 6 file paths for +X, -X, +Y, -Y, +Z, -Z faces.</param>
        /// <param name="usage">Vulkan image usage flags.</param>
        /// <param name="generateMips">If true, automatically generates mipmaps.</param>
        /// <returns>Bindless texture index.</returns>
        /// <exception cref="ArgumentException">Thrown if filenames is null or does not contain exactly 6 paths.</exception>
        /// <exception cref="InvalidOperationException">Thrown if loading fails.</exception>
        public static VETextureIndex LoadCubeTexture(VEDevice device, string[] filenames, VkImageUsageFlags usage, bool generateMips)
        {
            if (filenames == null || filenames.Length != 6)
                throw new ArgumentException("Cube texture requires exactly 6 filenames");

            IntPtr[] namePtrs = new IntPtr[6];
            try
            {
                for (int i = 0; i < 6; i++)
                {
                    namePtrs[i] = StringToHGlobalAnsi(filenames[i]);
                }

                IntPtr arrayPtr = Marshal.AllocHGlobal(IntPtr.Size * 6);
                try
                {
                    Marshal.Copy(namePtrs, 0, arrayPtr, 6);
                    VEResult result = VulkEaseDll.veLoadCubeTexture(device.native, arrayPtr, usage, generateMips, out uint index);
                    if (result != VEResult.VE_SUCCESS)
                        throw new InvalidOperationException($"veLoadCubeTexture failed: {result}");
                    return new VETextureIndex { native = index };
                }
                finally
                {
                    Marshal.FreeHGlobal(arrayPtr);
                }
            }
            finally
            {
                for (int i = 0; i < 6; i++)
                {
                    if (namePtrs[i] != IntPtr.Zero)
                        Marshal.FreeHGlobal(namePtrs[i]);
                }
            }
        }

        /// <summary>
        /// Write data to a texture region using host transfer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture to write to.</param>
        /// <param name="srcData">Pointer to source data.</param>
        /// <param name="dataSize">Size of the source data in bytes.</param>
        /// <param name="offsetX">X offset into the texture.</param>
        /// <param name="offsetY">Y offset into the texture.</param>
        /// <param name="offsetZ">Z offset into the texture.</param>
        /// <param name="width">Width of the region to write.</param>
        /// <param name="height">Height of the region to write.</param>
        /// <param name="depth">Depth of the region to write.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult HostWriteTextureRegion(VEDevice device, VETextureIndex texture, IntPtr srcData,
            UIntPtr dataSize, UInt32 offsetX, UInt32 offsetY, UInt32 offsetZ,
            UInt32 width, UInt32 height, UInt32 depth)
        {
            return VulkEaseDll.veHostWriteTextureRegion(device.native, texture.native, srcData, dataSize,
                offsetX, offsetY, offsetZ, width, height, depth);
        }

        /// <summary>
        /// Read data from a texture region using host transfer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture to read from.</param>
        /// <param name="dstData">Pointer to destination buffer.</param>
        /// <param name="dataSize">Size of the destination buffer in bytes.</param>
        /// <param name="offsetX">X offset into the texture.</param>
        /// <param name="offsetY">Y offset into the texture.</param>
        /// <param name="offsetZ">Z offset into the texture.</param>
        /// <param name="width">Width of the region to read.</param>
        /// <param name="height">Height of the region to read.</param>
        /// <param name="depth">Depth of the region to read.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult HostReadTextureRegion(VEDevice device, VETextureIndex texture, IntPtr dstData,
            UIntPtr dataSize, UInt32 offsetX, UInt32 offsetY, UInt32 offsetZ,
            UInt32 width, UInt32 height, UInt32 depth)
        {
            return VulkEaseDll.veHostReadTextureRegion(device.native, texture.native, dstData, dataSize,
                offsetX, offsetY, offsetZ, width, height, depth);
        }

        /// <summary>
        /// Write data to an entire texture using host transfer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture to write to.</param>
        /// <param name="srcData">Pointer to source data.</param>
        /// <param name="dataSize">Size of the source data in bytes.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult HostWriteTexture(VEDevice device, VETextureIndex texture, IntPtr srcData, UIntPtr dataSize)
        {
            return VulkEaseDll.veHostWriteTexture(device.native, texture.native, srcData, dataSize);
        }

        /// <summary>
        /// Read data from an entire texture using host transfer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture to read from.</param>
        /// <param name="dstData">Pointer to destination buffer.</param>
        /// <param name="dataSize">Size of the destination buffer in bytes.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult HostReadTexture(VEDevice device, VETextureIndex texture, IntPtr dstData, UIntPtr dataSize)
        {
            return VulkEaseDll.veHostReadTexture(device.native, texture.native, dstData, dataSize);
        }

        /// <summary>
        /// Generate mipmaps for a texture (command buffer version).
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="texture">Texture to generate mipmaps for.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Records mipmap generation commands into the command buffer for later execution.</remarks>
        public static VEResult CmdGenerateMipmaps(VECommandBuffer cmd, VETextureIndex texture)
        {
            return VulkEaseDll.veCmdGenerateMipmaps(cmd.native, texture.native);
        }

        /// <summary>
        /// Generate mipmaps for a texture (immediate/blocking version).
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture to generate mipmaps for.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Executes immediately and blocks until complete.</remarks>
        public static VEResult GenerateMipmaps(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseDll.veGenerateMipmaps(device.native, texture.native);
        }

        /// <summary>
        /// Save a texture to an image file.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture to save.</param>
        /// <param name="filename">Output file path. Format is determined by extension (PNG, JPG, BMP, TGA).</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_IO_FAILED if writing fails.</returns>
        public static VEResult SaveTexture(VEDevice device, VETextureIndex texture, string filename)
        {
            var namePtr = StringToHGlobalAnsi(filename);
            try
            {
                return VulkEaseDll.veSaveTexture(device.native, texture.native, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Copy texture data (command buffer version).
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="src">Source texture.</param>
        /// <param name="dst">Destination texture.</param>
        /// <param name="region">Optional region to copy. If null, copies entire texture.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdCopyTexture(VECommandBuffer cmd, VETextureIndex src, VETextureIndex dst, VETextureCopyRegion? region = null)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veCmdCopyTexture(cmd.native, src.native, dst.native, handle.AddrOfPinnedObject());
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veCmdCopyTexture(cmd.native, src.native, dst.native, IntPtr.Zero);
        }

        /// <summary>
        /// Copy texture data (immediate version).
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="src">Source texture.</param>
        /// <param name="dst">Destination texture.</param>
        /// <param name="region">Optional region to copy. If null, copies entire texture.</param>
        /// <param name="fence">Optional fence to signal when complete.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CopyTexture(VEDevice device, VETextureIndex src, VETextureIndex dst,
            VETextureCopyRegion? region = null, IntPtr fence = default)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veCopyTexture(device.native, src.native, dst.native, handle.AddrOfPinnedObject(), fence);
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veCopyTexture(device.native, src.native, dst.native, IntPtr.Zero, fence);
        }

        /// <summary>
        /// Blit (scaled copy) texture data (command buffer version).
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="src">Source texture.</param>
        /// <param name="dst">Destination texture.</param>
        /// <param name="region">Optional region to blit. If null, blits entire texture.</param>
        /// <param name="filter">Filtering mode for scaling (linear or nearest).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdBlitTexture(VECommandBuffer cmd, VETextureIndex src, VETextureIndex dst,
            VETextureBlitRegion? region = null, VkFilter filter = VkFilter.VK_FILTER_LINEAR)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veCmdBlitTexture(cmd.native, src.native, dst.native, handle.AddrOfPinnedObject(), filter);
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veCmdBlitTexture(cmd.native, src.native, dst.native, IntPtr.Zero, filter);
        }

        /// <summary>
        /// Blit (scaled copy) texture data (immediate version).
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="src">Source texture.</param>
        /// <param name="dst">Destination texture.</param>
        /// <param name="region">Optional region to blit. If null, blits entire texture.</param>
        /// <param name="filter">Filtering mode for scaling (linear or nearest).</param>
        /// <param name="fence">Optional fence to signal when complete.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult BlitTexture(VEDevice device, VETextureIndex src, VETextureIndex dst,
            VETextureBlitRegion? region = null, VkFilter filter = VkFilter.VK_FILTER_LINEAR, IntPtr fence = default)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veBlitTexture(device.native, src.native, dst.native, handle.AddrOfPinnedObject(), filter, fence);
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veBlitTexture(device.native, src.native, dst.native, IntPtr.Zero, filter, fence);
        }

        /// <summary>
        /// Copy buffer data to a texture (command buffer version).
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="src">Source buffer address.</param>
        /// <param name="dst">Destination texture.</param>
        /// <param name="region">Optional region specifying the copy parameters. If null, uses defaults.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdCopyBufferToTexture(VECommandBuffer cmd, VEBufferAddress src, VETextureIndex dst,
            VEBufferTextureCopyRegion? region = null)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veCmdCopyBufferToTexture(cmd.native, src.native, dst.native, handle.AddrOfPinnedObject());
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veCmdCopyBufferToTexture(cmd.native, src.native, dst.native, IntPtr.Zero);
        }

        /// <summary>
        /// Copy buffer data to a texture (immediate version).
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="src">Source buffer address.</param>
        /// <param name="dst">Destination texture.</param>
        /// <param name="region">Optional region specifying the copy parameters. If null, uses defaults.</param>
        /// <param name="fence">Optional fence to signal when complete.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CopyBufferToTexture(VEDevice device, VEBufferAddress src, VETextureIndex dst,
            VEBufferTextureCopyRegion? region = null, IntPtr fence = default)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veCopyBufferToTexture(device.native, src.native, dst.native, handle.AddrOfPinnedObject(), fence);
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veCopyBufferToTexture(device.native, src.native, dst.native, IntPtr.Zero, fence);
        }

        /// <summary>
        /// Copy texture data to a buffer (command buffer version).
        /// </summary>
        /// <param name="cmd">Valid command buffer in recording state.</param>
        /// <param name="src">Source texture.</param>
        /// <param name="dst">Destination buffer address.</param>
        /// <param name="region">Optional region specifying the copy parameters. If null, uses defaults.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdCopyTextureToBuffer(VECommandBuffer cmd, VETextureIndex src, VEBufferAddress dst,
            VEBufferTextureCopyRegion? region = null)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veCmdCopyTextureToBuffer(cmd.native, src.native, dst.native, handle.AddrOfPinnedObject());
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veCmdCopyTextureToBuffer(cmd.native, src.native, dst.native, IntPtr.Zero);
        }

        /// <summary>
        /// Copy texture data to a buffer (immediate version).
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="src">Source texture.</param>
        /// <param name="dst">Destination buffer address.</param>
        /// <param name="region">Optional region specifying the copy parameters. If null, uses defaults.</param>
        /// <param name="fence">Optional fence to signal when complete.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CopyTextureToBuffer(VEDevice device, VETextureIndex src, VEBufferAddress dst,
            VEBufferTextureCopyRegion? region = null, IntPtr fence = default)
        {
            if (region.HasValue)
            {
                var regionValue = region.Value;
                var handle = GCHandle.Alloc(regionValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veCopyTextureToBuffer(device.native, src.native, dst.native, handle.AddrOfPinnedObject(), fence);
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veCopyTextureToBuffer(device.native, src.native, dst.native, IntPtr.Zero, fence);
        }

        #endregion

        // =============================================================================
        // SECTION 23: SAMPLER FUNCTIONS
        // =============================================================================

        #region Sampler Functions

        /// <summary>
        /// Create a sampler with custom settings.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Sampler descriptor with filtering, addressing, and other settings.</param>
        /// <returns>Bindless sampler index. Pass to shaders via push constants.</returns>
        /// <exception cref="InvalidOperationException">Thrown if sampler creation fails.</exception>
        /// <remarks>Call DestroySampler() when done.</remarks>
        public static VESamplerIndex CreateSampler(VEDevice device, VESamplerDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateSampler(device.native, ref desc, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        /// <summary>
        /// Destroy a sampler by its bindless index.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="index">Sampler index to destroy. Can be VE_INVALID_SAMPLER_INDEX (no-op).</param>
        public static void DestroySampler(VEDevice device, VESamplerIndex index)
        {
            _ = VulkEaseDll.veDestroySampler(device.native, index.native);
        }

        /// <summary>
        /// Create a linear (bilinear) filtering sampler with clamp-to-edge addressing.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Bindless sampler index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if sampler creation fails.</exception>
        public static VESamplerIndex CreateLinearSampler(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateLinearSampler(device.native, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateLinearSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        /// <summary>
        /// Create a nearest (point) filtering sampler with clamp-to-edge addressing.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Bindless sampler index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if sampler creation fails.</exception>
        public static VESamplerIndex CreateNearestSampler(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateNearestSampler(device.native, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateNearestSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        /// <summary>
        /// Create an anisotropic filtering sampler.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="maxAnisotropy">Maximum anisotropy level (typically 1.0 to 16.0).</param>
        /// <returns>Bindless sampler index.</returns>
        /// <exception cref="InvalidOperationException">Thrown if sampler creation fails.</exception>
        public static VESamplerIndex CreateAnisotropicSampler(VEDevice device, float maxAnisotropy)
        {
            VEResult result = VulkEaseDll.veCreateAnisotropicSampler(device.native, maxAnisotropy, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateAnisotropicSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        /// <summary>
        /// Create a shadow map comparison sampler.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Bindless sampler index configured for depth comparison.</returns>
        /// <exception cref="InvalidOperationException">Thrown if sampler creation fails.</exception>
        /// <remarks>Uses compare-less-or-equal for standard shadow mapping.</remarks>
        public static VESamplerIndex CreateShadowSampler(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateShadowSampler(device.native, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateShadowSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        /// <summary>
        /// Get a built-in default sampler.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="sampler">Which default sampler to retrieve.</param>
        /// <returns>Bindless sampler index.</returns>
        public static VESamplerIndex GetDefaultSampler(VEDevice device, VEDefaultSampler sampler)
        {
            return new VESamplerIndex { native = VulkEaseDll.veGetDefaultSampler(device.native, sampler) };
        }

        #endregion

        // =============================================================================
        // SECTION 24: SHADER FUNCTIONS
        // =============================================================================

        #region Shader Functions

        /// <summary>
        /// Load a shader from SPIR-V bytecode in memory.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="buffer">SPIR-V bytecode.</param>
        /// <param name="stage">Shader stage (vertex, fragment, compute, etc.).</param>
        /// <param name="entryPoint">Entry point function name (typically "main").</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Shader object handle.</returns>
        /// <exception cref="ArgumentException">Thrown if buffer is null or empty.</exception>
        /// <exception cref="InvalidOperationException">Thrown if shader compilation fails.</exception>
        /// <remarks>
        /// VulkEase uses shader objects (VK_EXT_shader_object) for hot-reloadable shaders.
        /// Call DestroyShader() when done.
        /// </remarks>
        public static VEShader LoadShaderFromBuffer(VEDevice device, byte[] buffer, VkShaderStageFlags stage, string entryPoint, string? debugName = null)
        {
            if (buffer == null || buffer.Length == 0)
            {
                throw new ArgumentException("Shader buffer must not be null or empty.", nameof(buffer));
            }

            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            IntPtr bufferPtr = IntPtr.Zero;

            try
            {
                bufferPtr = Marshal.AllocHGlobal(buffer.Length);
                Marshal.Copy(buffer, 0, bufferPtr, buffer.Length);
                VEResult result = VulkEaseDll.veLoadShaderFromBuffer(
                    device.native,
                    stage,
                    bufferPtr,
                    new UIntPtr((ulong)buffer.Length),
                    entryPtr,
                    namePtr,
                    out IntPtr shaderPtr);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veLoadShaderFromBuffer failed: {result}");
                return new VEShader { native = shaderPtr };
            }
            finally
            {
                if (bufferPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(bufferPtr);
                if (entryPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(entryPtr);
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Load a shader from a SPIR-V file.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="filename">Path to the .spv file.</param>
        /// <param name="stage">Shader stage (vertex, fragment, compute, etc.).</param>
        /// <param name="entryPoint">Entry point function name (typically "main").</param>
        /// <param name="debugName">Optional debug name for tools like RenderDoc.</param>
        /// <returns>Shader object handle.</returns>
        /// <exception cref="InvalidOperationException">Thrown if file cannot be loaded or shader compilation fails.</exception>
        public static VEShader LoadShaderFromFile(VEDevice device, string filename, VkShaderStageFlags stage, string entryPoint, string? debugName = null)
        {
            var filenamePtr = StringToHGlobalAnsi(filename);
            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName ?? string.Empty);
            try
            {
                VEResult result = VulkEaseDll.veLoadShaderFromFile(device.native, filenamePtr, stage, entryPtr, namePtr, out IntPtr shaderPtr);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veLoadShaderFromFile failed: {result}");
                return new VEShader { native = shaderPtr };
            }
            finally
            {
                if (filenamePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(filenamePtr);
                if (entryPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(entryPtr);
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Destroy a shader object.
        /// </summary>
        /// <param name="shader">Shader handle to destroy. Can be default (no-op).</param>
        public static void DestroyShader(VEShader shader)
        {
            _ = VulkEaseDll.veDestroyShader(shader.native);
        }

        /// <summary>
        /// Enable or disable shader hot-reloading for the device.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="enable">True to enable hot-reloading, false to disable.</param>
        /// <remarks>When enabled, shaders loaded from files can be automatically reloaded when modified.</remarks>
        public static void SetShaderHotReloadEnabled(VEDevice device, bool enable)
        {
            VulkEaseDll.veSetShaderHotReloadEnabled(device.native, enable);
        }

        /// <summary>
        /// Check if a shader's source file has been modified and needs reloading.
        /// </summary>
        /// <param name="shader">Shader to check.</param>
        /// <returns>True if the shader needs reloading.</returns>
        public static bool ShaderNeedsReload(VEShader shader)
        {
            return VulkEaseDll.veShaderNeedsReload(shader.native);
        }

        /// <summary>
        /// Reload a shader from its source file.
        /// </summary>
        /// <param name="shader">Shader to reload.</param>
        /// <returns>VE_SUCCESS on success, or an error code if reloading failed.</returns>
        public static VEResult ReloadShader(VEShader shader)
        {
            return VulkEaseDll.veReloadShader(shader.native);
        }

        #endregion

        // =============================================================================
        // SECTION 25: GRAPHICS PIPELINE FUNCTIONS
        // =============================================================================

        #region Graphics Pipeline Functions

        /// <summary>
        /// Create a graphics pipeline with custom settings.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Pipeline descriptor with shaders, vertex input, and state.</param>
        /// <returns>Graphics pipeline handle.</returns>
        /// <exception cref="InvalidOperationException">Thrown if pipeline creation fails.</exception>
        /// <remarks>
        /// Combines shaders + vertex input + material state (depth, stencil, blend, rasterization).
        /// Use the convenience functions CreateOpaquePipeline, CreateTransparentPipeline, etc. for common cases.
        /// Call DestroyGraphicsPipeline() when done.
        /// </remarks>
        public static VEGraphicsPipeline CreateGraphicsPipeline(VEDevice device, VEGraphicsPipelineDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateGraphicsPipeline(device.native, ref desc, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateGraphicsPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Destroy a graphics pipeline.
        /// </summary>
        /// <param name="pipeline">Pipeline handle to destroy. Can be default (no-op).</param>
        public static void DestroyGraphicsPipeline(VEGraphicsPipeline pipeline)
        {
            _ = VulkEaseDll.veDestroyGraphicsPipeline(pipeline.native);
        }

        /// <summary>
        /// Create an opaque rendering pipeline.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="vertexShader">Vertex shader object.</param>
        /// <param name="fragmentShader">Fragment shader object.</param>
        /// <param name="debugName">Optional debug name.</param>
        /// <returns>Pipeline configured for opaque geometry (depth test on, depth write on, no blending).</returns>
        /// <exception cref="InvalidOperationException">Thrown if pipeline creation fails.</exception>
        public static VEGraphicsPipeline CreateOpaquePipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string? debugName = null)
        {
            VEResult result = VulkEaseDll.veCreateOpaquePipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName ?? string.Empty, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateOpaquePipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Create a transparent rendering pipeline with alpha blending.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="vertexShader">Vertex shader object.</param>
        /// <param name="fragmentShader">Fragment shader object.</param>
        /// <param name="debugName">Optional debug name.</param>
        /// <returns>Pipeline configured for transparent geometry (depth test on, depth write off, alpha blend).</returns>
        /// <exception cref="InvalidOperationException">Thrown if pipeline creation fails.</exception>
        public static VEGraphicsPipeline CreateTransparentPipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string? debugName = null)
        {
            VEResult result = VulkEaseDll.veCreateTransparentPipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName ?? string.Empty, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateTransparentPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Create an additive blending pipeline.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="vertexShader">Vertex shader object.</param>
        /// <param name="fragmentShader">Fragment shader object.</param>
        /// <param name="debugName">Optional debug name.</param>
        /// <returns>Pipeline configured for additive blending (depth test on, depth write off, additive blend).</returns>
        /// <exception cref="InvalidOperationException">Thrown if pipeline creation fails.</exception>
        public static VEGraphicsPipeline CreateAdditivePipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string? debugName = null)
        {
            VEResult result = VulkEaseDll.veCreateAdditivePipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName ?? string.Empty, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateAdditivePipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Create a shadow map rendering pipeline.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="vertexShader">Vertex shader object.</param>
        /// <param name="fragmentShader">Fragment shader object.</param>
        /// <param name="debugName">Optional debug name.</param>
        /// <returns>Pipeline configured for shadow map rendering (depth bias enabled, backface culling).</returns>
        /// <exception cref="InvalidOperationException">Thrown if pipeline creation fails.</exception>
        public static VEGraphicsPipeline CreateShadowPipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string? debugName = null)
        {
            VEResult result = VulkEaseDll.veCreateShadowPipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName ?? string.Empty, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateShadowPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Create a UI overlay rendering pipeline.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="vertexShader">Vertex shader object.</param>
        /// <param name="fragmentShader">Fragment shader object.</param>
        /// <param name="debugName">Optional debug name.</param>
        /// <returns>Pipeline configured for UI rendering (no depth, alpha blend, no culling).</returns>
        /// <exception cref="InvalidOperationException">Thrown if pipeline creation fails.</exception>
        public static VEGraphicsPipeline CreateUIOverlayPipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string? debugName = null)
        {
            VEResult result = VulkEaseDll.veCreateUIOverlayPipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName ?? string.Empty, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateUIOverlayPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        public static VEGraphicsPipelineDesc DefaultGraphicsPipelineDesc()
        {
            return VulkEaseDll.veDefaultGraphicsPipelineDesc();
        }

        #endregion

        // =============================================================================
        // SECTION 26: DRAW STATE FUNCTIONS
        // =============================================================================

        #region Draw State Functions

        /// <summary>
        /// Create a draw state with custom settings.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Draw state descriptor with rasterization, depth, stencil, and blend settings.</param>
        /// <returns>Draw state handle.</returns>
        /// <exception cref="InvalidOperationException">Thrown if creation fails.</exception>
        /// <remarks>
        /// Draw states capture per-draw dynamic state that can be changed without pipeline rebinds.
        /// Call DestroyDrawState() when done.
        /// </remarks>
        public static VEDrawState CreateDrawState(VEDevice device, VEDrawStateDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateDrawState(device.native, ref desc, out IntPtr drawStatePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateDrawState failed: {result}");
            return new VEDrawState { native = drawStatePtr };
        }

        /// <summary>
        /// Destroy a draw state.
        /// </summary>
        /// <param name="drawState">Draw state handle to destroy. Can be default (no-op).</param>
        public static void DestroyDrawState(VEDrawState drawState)
        {
            _ = VulkEaseDll.veDestroyDrawState(drawState.native);
        }

        /// <summary>
        /// Create a default draw state preset.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Draw state with default settings (fill mode, CCW front face, back culling, depth test on).</returns>
        /// <exception cref="InvalidOperationException">Thrown if creation fails.</exception>
        public static VEDrawState CreateDefaultDrawState(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateDefaultDrawState(device.native, out IntPtr drawStatePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateDefaultDrawState failed: {result}");
            return new VEDrawState { native = drawStatePtr };
        }

        /// <summary>
        /// Create a wireframe draw state preset.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Draw state configured for wireframe rendering (line mode, no culling).</returns>
        /// <exception cref="InvalidOperationException">Thrown if creation fails.</exception>
        public static VEDrawState CreateWireframeDrawState(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateWireframeDrawState(device.native, out IntPtr drawStatePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateWireframeDrawState failed: {result}");
            return new VEDrawState { native = drawStatePtr };
        }

        /// <summary>
        /// Create a draw state preset optimized for shadow map rendering.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Draw state configured for shadow rendering (depth bias enabled).</returns>
        /// <exception cref="InvalidOperationException">Thrown if creation fails.</exception>
        public static VEDrawState CreateShadowDrawState(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateShadowDrawState(device.native, out IntPtr drawStatePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateShadowDrawState failed: {result}");
            return new VEDrawState { native = drawStatePtr };
        }

        /// <summary>
        /// Get a default draw state descriptor.
        /// </summary>
        /// <returns>VEDrawStateDesc initialized with sensible defaults.</returns>
        public static VEDrawStateDesc DefaultDrawStateDesc()
        {
            return VulkEaseDll.veDefaultDrawStateDesc();
        }

        #endregion

        // =============================================================================
        // SECTION 27: COMMAND BUFFER FUNCTIONS
        // =============================================================================

        #region Command Buffer Functions

        /// <summary>
        /// Begin a primary command buffer for recording commands.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>Command buffer handle in recording state.</returns>
        /// <exception cref="InvalidOperationException">Thrown if command buffer allocation fails.</exception>
        /// <remarks>
        /// Command buffers record GPU commands for later submission.
        /// Call EndCommandBuffer() when done recording, then SubmitCommandBuffer() to execute.
        /// </remarks>
        public static VECommandBuffer BeginCommandBuffer(VEDevice device)
        {
            VEResult result = VulkEaseDll.veBeginCommandBuffer(device.native, out IntPtr cmdPtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veBeginCommandBuffer failed: {result}");
            return new VECommandBuffer { native = cmdPtr };
        }

        /// <summary>
        /// Submit a command buffer for execution.
        /// </summary>
        /// <param name="cmd">Command buffer to submit.</param>
        /// <param name="waitForCompletion">If true, blocks until execution completes.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SubmitCommandBuffer(VECommandBuffer cmd, bool waitForCompletion = false)
        {
            VulkEaseDll.VESubmitInfo submitInfo = new VulkEaseDll.VESubmitInfo
            {
                waitSemaphores = IntPtr.Zero,
                waitSemaphoreValues = IntPtr.Zero,
                waitStageMasks = IntPtr.Zero,
                waitSemaphoreCount = 0,
                signalSemaphores = IntPtr.Zero,
                signalSemaphoreValues = IntPtr.Zero,
                signalSemaphoreCount = 0,
                fence = IntPtr.Zero,
                waitForCompletion = waitForCompletion
            };
            return VulkEaseDll.veSubmitCommandBuffer(cmd.native, ref submitInfo);
        }

        /// <summary>
        /// End command buffer recording.
        /// </summary>
        /// <param name="cmd">Command buffer to end.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Must be called after recording and before submission.</remarks>
        public static VEResult EndCommandBuffer(VECommandBuffer cmd)
        {
            return VulkEaseDll.veEndCommandBuffer(cmd.native);
        }

        /// <summary>
        /// Reset a command buffer for reuse.
        /// </summary>
        /// <param name="cmd">Command buffer to reset.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Clears all recorded commands. The command buffer can be re-recorded after this.</remarks>
        public static VEResult ResetCommandBuffer(VECommandBuffer cmd)
        {
            return VulkEaseDll.veResetCommandBuffer(cmd.native);
        }

        /// <summary>
        /// Release a command buffer back to the pool.
        /// </summary>
        /// <param name="cmd">Command buffer to release.</param>
        /// <remarks>The command buffer must not be in use by the GPU when released.</remarks>
        public static void ReleaseCommandBuffer(VECommandBuffer cmd)
        {
            VulkEaseDll.veReleaseCommandBuffer(cmd.native);
        }

        /// <summary>
        /// Populate a secondary command buffer descriptor from render target.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="renderTarget">Render target to extract formats from.</param>
        /// <param name="desc">Descriptor to populate.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Use this when creating secondary command buffers that will be executed inside a render pass.</remarks>
        public static VEResult PopulateSecondaryDescFromRenderTarget(VEDevice device, VERenderTarget renderTarget, ref VESecondaryCommandBufferDesc desc)
        {
            if (desc.colorAttachmentFormats == null || desc.colorAttachmentFormats.Length != 8)
            {
                desc.colorAttachmentFormats = new VkFormat[8];
            }

            var internalTarget = renderTarget.ToInternal();
            return VulkEaseDll.vePopulateSecondaryDescFromRenderTarget(device.native, ref internalTarget, ref desc);
        }

        /// <summary>
        /// Begin a secondary command buffer for recording.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Descriptor with inheritance and format info.</param>
        /// <returns>Secondary command buffer handle in recording state.</returns>
        /// <exception cref="InvalidOperationException">Thrown if allocation fails.</exception>
        /// <remarks>Secondary command buffers can be executed from primary command buffers using ExecuteSecondaryCommandBuffers().</remarks>
        public static VECommandBuffer BeginSecondaryCommandBuffer(VEDevice device, VESecondaryCommandBufferDesc desc)
        {
            if (desc.colorAttachmentFormats == null || desc.colorAttachmentFormats.Length != 8)
            {
                desc.colorAttachmentFormats = new VkFormat[8];
            }

            IntPtr descPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VESecondaryCommandBufferDesc>());
            try
            {
                Marshal.StructureToPtr(desc, descPtr, false);
                VEResult result = VulkEaseDll.veBeginSecondaryCommandBuffer(device.native, descPtr, out IntPtr cmdPtr);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veBeginSecondaryCommandBuffer failed: {result}");
                return new VECommandBuffer { native = cmdPtr };
            }
            finally
            {
                Marshal.FreeHGlobal(descPtr);
            }
        }

        /// <summary>
        /// Begin recording on a secondary command buffer.
        /// </summary>
        /// <param name="cmd">Secondary command buffer to begin recording on.</param>
        /// <param name="desc">Descriptor with inheritance and format info.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult BeginSecondaryRecording(VECommandBuffer cmd, VESecondaryCommandBufferDesc desc)
        {
            if (desc.colorAttachmentFormats == null || desc.colorAttachmentFormats.Length != 8)
            {
                desc.colorAttachmentFormats = new VkFormat[8];
            }

            IntPtr descPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VESecondaryCommandBufferDesc>());
            try
            {
                Marshal.StructureToPtr(desc, descPtr, false);
                return VulkEaseDll.veBeginSecondaryRecording(cmd.native, descPtr);
            }
            finally
            {
                Marshal.FreeHGlobal(descPtr);
            }
        }

        /// <summary>
        /// Execute secondary command buffers from a primary command buffer.
        /// </summary>
        /// <param name="primary">Primary command buffer in recording state.</param>
        /// <param name="secondaryBuffers">List of secondary command buffers to execute.</param>
        /// <param name="releaseCommandBuffers">If true, releases the secondary buffers after execution.</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_INVALID_PARAMETER if list is null or empty.</returns>
        public static VEResult ExecuteSecondaryCommandBuffers(VECommandBuffer primary, IReadOnlyList<VECommandBuffer> secondaryBuffers, bool releaseCommandBuffers = true)
        {
            if (secondaryBuffers == null || secondaryBuffers.Count == 0)
            {
                return VEResult.VE_ERROR_INVALID_PARAMETER;
            }

            var native = new IntPtr[secondaryBuffers.Count];
            for (int i = 0; i < secondaryBuffers.Count; ++i)
            {
                native[i] = secondaryBuffers[i].native;
            }

            return VulkEaseDll.veExecuteSecondaryCommandBuffers(primary.native, (uint)native.Length, native, releaseCommandBuffers);
        }

        #endregion

        // =============================================================================
        // SECTION 28: RENDERING FUNCTIONS
        // =============================================================================

        #region Render Target Functions

        /// <summary>
        /// Create a render target structure for dynamic rendering.
        /// </summary>
        /// <param name="width">Render area width in pixels.</param>
        /// <param name="height">Render area height in pixels.</param>
        /// <returns>Initialized VERenderTarget ready for attachment configuration.</returns>
        public static VERenderTarget CreateRenderTarget(uint width, uint height)
        {
            return new VERenderTarget(width, height);
        }

        /// <summary>
        /// Create a render target structure with a custom offset.
        /// </summary>
        /// <param name="x">Render area X offset.</param>
        /// <param name="y">Render area Y offset.</param>
        /// <param name="width">Render area width in pixels.</param>
        /// <param name="height">Render area height in pixels.</param>
        /// <returns>Initialized VERenderTarget with offset.</returns>
        public static VERenderTarget CreateRenderTargetWithOffset(int x, int y, uint width, uint height)
        {
            return new VERenderTarget(x, y, width, height);
        }

        /// <summary>
        /// Add a color attachment to a render target.
        /// </summary>
        /// <param name="target">Render target to modify.</param>
        /// <param name="texture">Texture for the color attachment.</param>
        /// <param name="loadOp">Load operation (clear, load, or don't care).</param>
        /// <param name="clearColor">Clear color value if loadOp is CLEAR.</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_INVALID_PARAMETER if attachment limit exceeded.</returns>
        public static VEResult RenderTargetAddColorAttachment(ref VERenderTarget target, VETextureIndex texture,
            VkAttachmentLoadOp loadOp, VEColor clearColor)
        {
            var internalTarget = target.ToInternal();
            var result = VulkEaseDll.veRenderTargetAddColorAttachment(ref internalTarget, texture.native, loadOp, clearColor);
            target.FromInternal(internalTarget);
            return result;
        }

        /// <summary>
        /// Add a color attachment with MSAA resolve target.
        /// </summary>
        /// <param name="target">Render target to modify.</param>
        /// <param name="texture">Multisampled texture for rendering.</param>
        /// <param name="resolveTexture">Single-sampled texture to resolve into.</param>
        /// <param name="loadOp">Load operation.</param>
        /// <param name="clearColor">Clear color value.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult RenderTargetAddColorAttachmentResolve(ref VERenderTarget target, VETextureIndex texture,
            VETextureIndex resolveTexture, VkAttachmentLoadOp loadOp, VEColor clearColor)
        {
            var internalTarget = target.ToInternal();
            var result = VulkEaseDll.veRenderTargetAddColorAttachmentResolve(ref internalTarget, texture.native,
                resolveTexture.native, loadOp, clearColor);
            target.FromInternal(internalTarget);
            return result;
        }

        /// <summary>
        /// Set the depth attachment for rendering.
        /// </summary>
        /// <param name="target">Render target to modify.</param>
        /// <param name="texture">Texture for depth attachment (must have depth format).</param>
        /// <param name="loadOp">Load operation.</param>
        /// <param name="clearDepth">Clear depth value (typically 1.0 for reverse-Z, 0.0 otherwise).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult RenderTargetSetDepthAttachment(ref VERenderTarget target, VETextureIndex texture,
            VkAttachmentLoadOp loadOp, float clearDepth)
        {
            var internalTarget = target.ToInternal();
            var result = VulkEaseDll.veRenderTargetSetDepthAttachment(ref internalTarget, texture.native, loadOp, clearDepth);
            target.FromInternal(internalTarget);
            return result;
        }

        /// <summary>
        /// Set the stencil attachment for rendering.
        /// </summary>
        /// <param name="target">Render target to modify.</param>
        /// <param name="texture">Texture for stencil attachment.</param>
        /// <param name="loadOp">Load operation.</param>
        /// <param name="clearStencil">Clear stencil value.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult RenderTargetSetStencilAttachment(ref VERenderTarget target, VETextureIndex texture,
            VkAttachmentLoadOp loadOp, uint clearStencil)
        {
            var internalTarget = target.ToInternal();
            var result = VulkEaseDll.veRenderTargetSetStencilAttachment(ref internalTarget, texture.native, loadOp, clearStencil);
            target.FromInternal(internalTarget);
            return result;
        }

        /// <summary>
        /// Resize all textures attached to a render target.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="target">Render target to resize.</param>
        /// <param name="width">New width in pixels.</param>
        /// <param name="height">New height in pixels.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult ResizeRenderTarget(VEDevice device, ref VERenderTarget target, uint width, uint height)
        {
            var internalTarget = target.ToInternal();
            var result = VulkEaseDll.veResizeRenderTarget(device.native, ref internalTarget, width, height);
            target.FromInternal(internalTarget);
            return result;
        }

        /// <summary>
        /// Create a simple render target with color and optional depth.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="width">Width in pixels.</param>
        /// <param name="height">Height in pixels.</param>
        /// <param name="colorFormat">Color texture format.</param>
        /// <param name="depthFormat">Depth format (VK_FORMAT_UNDEFINED for no depth).</param>
        /// <param name="clearColor">Clear color value.</param>
        /// <param name="clearDepth">Clear depth value.</param>
        /// <param name="colorTexture">Receives the color texture index.</param>
        /// <param name="depthTexture">Receives the depth texture index (if depth format specified).</param>
        /// <returns>Configured VERenderTarget.</returns>
        public static VERenderTarget CreateSimpleRenderTarget(VEDevice device, uint width, uint height,
            VkFormat colorFormat, VkFormat depthFormat, VEColor clearColor, float clearDepth,
            out VETextureIndex colorTexture, out VETextureIndex depthTexture)
        {
            var internalTarget = VulkEaseDll.veCreateSimpleRenderTarget(device.native, width, height,
                colorFormat, depthFormat, clearColor, clearDepth, out uint colorTex, out uint depthTex);
            colorTexture = new VETextureIndex { native = colorTex };
            depthTexture = new VETextureIndex { native = depthTex };
            var target = new VERenderTarget();
            target.FromInternal(internalTarget);
            return target;
        }

        /// <summary>
        /// Blit a texture to a swapchain for presentation.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Source texture to blit.</param>
        /// <param name="swapchain">Destination swapchain.</param>
        /// <param name="filter">Filtering mode (NEAREST or LINEAR).</param>
        /// <returns>VE_SUCCESS on success, or VE_ERROR_SWAPCHAIN_OUT_OF_DATE if resize needed.</returns>
        public static VEResult BlitTextureToSwapchain(VECommandBuffer cmd, VETextureIndex texture,
            VESwapchain swapchain, VkFilter filter)
        {
            return VulkEaseDll.veBlitTextureToSwapchain(cmd.native, texture.native, swapchain.native, filter);
        }

        /// <summary>
        /// Begin dynamic rendering.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="renderTarget">Render target configuration with attachments.</param>
        /// <remarks>
        /// All draw commands must be issued between BeginRendering() and EndRendering().
        /// There are no render passes in VulkEase - just begin/end rendering blocks.
        /// </remarks>
        public static void BeginRendering(VECommandBuffer cmd, VERenderTarget renderTarget)
        {
            var internalTarget = renderTarget.ToInternal();
            VulkEaseDll.veBeginRendering(cmd.native, ref internalTarget);
        }

        /// <summary>
        /// End dynamic rendering.
        /// </summary>
        /// <param name="cmd">Command buffer with active rendering.</param>
        public static void EndRendering(VECommandBuffer cmd)
        {
            VulkEaseDll.veEndRendering(cmd.native);
        }

        #endregion

        // =============================================================================
        // SECTION 29: STATE BINDING FUNCTIONS
        // =============================================================================

        #region State Binding Functions

        /// <summary>
        /// Bind a graphics pipeline for subsequent draw commands.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="pipeline">Graphics pipeline to bind.</param>
        public static void BindGraphicsPipeline(VECommandBuffer cmd, VEGraphicsPipeline pipeline)
        {
            VulkEaseDll.veBindGraphicsPipeline(cmd.native, pipeline.native);
        }

        /// <summary>
        /// Apply a draw state's dynamic state settings.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="drawState">Draw state to apply.</param>
        public static void ApplyDrawState(VECommandBuffer cmd, VEDrawState drawState)
        {
            VulkEaseDll.veApplyDrawState(cmd.native, drawState.native);
        }

        /// <summary>
        /// Apply complete graphics state in a single call.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="pipeline">Graphics pipeline to bind.</param>
        /// <param name="drawState">Optional draw state to apply.</param>
        /// <param name="viewport">Optional viewport to set.</param>
        /// <param name="scissor">Optional scissor rect to set.</param>
        /// <exception cref="InvalidOperationException">Thrown if state application fails.</exception>
        /// <remarks>Convenience function that combines BindGraphicsPipeline, ApplyDrawState, SetViewport, and SetScissor.</remarks>
        public static void ApplyGraphicsState(VECommandBuffer cmd, VEGraphicsPipeline pipeline,
            VEDrawState? drawState = null, VEViewport? viewport = null, VERect2D? scissor = null)
        {
            IntPtr viewportPtr = IntPtr.Zero;
            IntPtr scissorPtr = IntPtr.Zero;

            try
            {
                if (viewport.HasValue)
                {
                    viewportPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VEViewport>());
                    Marshal.StructureToPtr(viewport.Value, viewportPtr, false);
                }
                if (scissor.HasValue)
                {
                    scissorPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VERect2D>());
                    Marshal.StructureToPtr(scissor.Value, scissorPtr, false);
                }

                VEResult result = VulkEaseDll.veApplyGraphicsState(cmd.native, pipeline.native,
                    drawState?.native ?? IntPtr.Zero, viewportPtr, scissorPtr);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veApplyGraphicsState failed: {result}");
            }
            finally
            {
                if (viewportPtr != IntPtr.Zero) Marshal.FreeHGlobal(viewportPtr);
                if (scissorPtr != IntPtr.Zero) Marshal.FreeHGlobal(scissorPtr);
            }
        }

        /// <summary>
        /// Bind a single shader.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="shader">Shader to bind.</param>
        public static void BindShader(VECommandBuffer cmd, VEShader shader)
        {
            VulkEaseDll.veBindShader(cmd.native, shader.native);
        }

        /// <summary>
        /// Bind multiple shaders at once.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="shaders">Array of shaders to bind.</param>
        public static void BindShaders(VECommandBuffer cmd, VEShader[] shaders)
        {
            IntPtr shadersPtr = IntPtr.Zero;
            try
            {
                if (shaders != null && shaders.Length > 0)
                {
                    shadersPtr = Marshal.AllocHGlobal(IntPtr.Size * shaders.Length);
                    IntPtr[] nativePtrs = new IntPtr[shaders.Length];
                    for (int i = 0; i < shaders.Length; i++)
                    {
                        nativePtrs[i] = shaders[i].native;
                    }
                    Marshal.Copy(nativePtrs, 0, shadersPtr, shaders.Length);
                }

                VulkEaseDll.veBindShaders(cmd.native, (UInt32)(shaders?.Length ?? 0), shadersPtr);
            }
            finally
            {
                if (shadersPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(shadersPtr);
            }
        }

        /// <summary>
        /// Unbind a shader stage.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="stage">Shader stage to unbind.</param>
        public static void UnbindShaderStage(VECommandBuffer cmd, VkShaderStageFlags stage)
        {
            VulkEaseDll.veUnbindShaderStage(cmd.native, stage);
        }

        #endregion

        // =============================================================================
        // SECTION 30: VIEWPORT & SCISSOR FUNCTIONS
        // =============================================================================

        #region Viewport & Scissor Functions

        /// <summary>
        /// Set the viewport for rendering.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="x">Viewport X offset.</param>
        /// <param name="y">Viewport Y offset.</param>
        /// <param name="width">Viewport width.</param>
        /// <param name="height">Viewport height.</param>
        /// <param name="minDepth">Minimum depth value (default 0.0).</param>
        /// <param name="maxDepth">Maximum depth value (default 1.0).</param>
        public static void SetViewport(VECommandBuffer cmd, float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f)
        {
            VulkEaseDll.veSetViewport(cmd.native, x, y, width, height, minDepth, maxDepth);
        }

        /// <summary>
        /// Set the scissor rectangle for rendering.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="x">Scissor X offset.</param>
        /// <param name="y">Scissor Y offset.</param>
        /// <param name="width">Scissor width.</param>
        /// <param name="height">Scissor height.</param>
        public static void SetScissor(VECommandBuffer cmd, int x, int y, UInt32 width, UInt32 height)
        {
            VulkEaseDll.veSetScissor(cmd.native, x, y, width, height);
        }

        #endregion

        // =============================================================================
        // SECTION 31: DYNAMIC STATE FUNCTIONS
        // =============================================================================

        #region Dynamic State Functions

        /// <summary>
        /// Set the primitive topology.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="topology">Primitive topology (triangles, lines, points, etc.).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetTopology(VECommandBuffer cmd, VkPrimitiveTopology topology)
        {
            return VulkEaseDll.veSetTopology(cmd.native, topology);
        }

        /// <summary>
        /// Enable or disable primitive restart.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to enable primitive restart.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetPrimitiveRestart(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetPrimitiveRestart(cmd.native, enable);
        }

        /// <summary>
        /// Set tessellation patch control points.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="controlPoints">Number of control points per patch.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetPatchControlPoints(VECommandBuffer cmd, UInt32 controlPoints)
        {
            return VulkEaseDll.veSetPatchControlPoints(cmd.native, controlPoints);
        }

        /// <summary>
        /// Set polygon rendering mode.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="mode">Polygon mode (fill, line, point).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetPolygonMode(VECommandBuffer cmd, VkPolygonMode mode)
        {
            return VulkEaseDll.veSetPolygonMode(cmd.native, mode);
        }

        /// <summary>
        /// Set line width for line primitives.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="width">Line width in pixels.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetLineWidth(VECommandBuffer cmd, float width)
        {
            return VulkEaseDll.veSetLineWidth(cmd.native, width);
        }

        /// <summary>
        /// Set face culling mode.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="cullMode">Culling mode (none, front, back, or both).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetCullMode(VECommandBuffer cmd, VkCullModeFlags cullMode)
        {
            return VulkEaseDll.veSetCullMode(cmd.native, cullMode);
        }

        /// <summary>
        /// Set front face winding order.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="frontFace">Front face winding (counter-clockwise or clockwise).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetFrontFace(VECommandBuffer cmd, VkFrontFace frontFace)
        {
            return VulkEaseDll.veSetFrontFace(cmd.native, frontFace);
        }

        /// <summary>
        /// Enable or disable rasterizer discard.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to discard all rasterized fragments.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetRasterizerDiscard(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetRasterizerDiscard(cmd.native, enable);
        }

        /// <summary>
        /// Set depth bias parameters.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to enable depth bias.</param>
        /// <param name="constantFactor">Constant depth bias factor.</param>
        /// <param name="clamp">Maximum depth bias clamp value.</param>
        /// <param name="slopeFactor">Slope-based depth bias factor.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Depth bias is useful for shadow mapping to avoid shadow acne.</remarks>
        public static VEResult SetDepthBias(VECommandBuffer cmd, bool enable, float constantFactor, float clamp, float slopeFactor)
        {
            return VulkEaseDll.veSetDepthBias(cmd.native, enable, constantFactor, clamp, slopeFactor);
        }

        /// <summary>
        /// Enable or disable depth clamping.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to clamp depth values instead of clipping.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetDepthClamp(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetDepthClamp(cmd.native, enable);
        }

        /// <summary>
        /// Enable or disable alpha-to-coverage.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to enable alpha-to-coverage.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetAlphaToCoverage(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetAlphaToCoverage(cmd.native, enable);
        }

        /// <summary>
        /// Enable or disable alpha-to-one.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to enable alpha-to-one.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetAlphaToOne(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetAlphaToOne(cmd.native, enable);
        }

        /// <summary>
        /// Enable or disable depth testing.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to enable depth testing.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetDepthTest(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetDepthTest(cmd.native, enable);
        }

        /// <summary>
        /// Enable or disable depth writing.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to enable depth writing.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetDepthWrite(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetDepthWrite(cmd.native, enable);
        }

        /// <summary>
        /// Set the depth comparison operation.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="op">Comparison operation (less, greater, equal, etc.).</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetDepthCompareOp(VECommandBuffer cmd, VkCompareOp op)
        {
            return VulkEaseDll.veSetDepthCompareOp(cmd.native, op);
        }

        /// <summary>
        /// Enable or disable stencil testing.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enable">True to enable stencil testing.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetStencilTest(VECommandBuffer cmd, bool enable)
        {
            return VulkEaseDll.veSetStencilTest(cmd.native, enable);
        }

        /// <summary>
        /// Set stencil operations.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="faceMask">Which faces to affect (VK_STENCIL_FACE_FRONT_BIT, etc.).</param>
        /// <param name="failOp">Operation on stencil test fail.</param>
        /// <param name="passOp">Operation on stencil and depth test pass.</param>
        /// <param name="depthFailOp">Operation on stencil pass but depth test fail.</param>
        /// <param name="compareOp">Stencil comparison operation.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetStencilOp(VECommandBuffer cmd, VkStencilFaceFlags faceMask,
            VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp)
        {
            return VulkEaseDll.veSetStencilOp(cmd.native, faceMask, failOp, passOp, depthFailOp, compareOp);
        }

        /// <summary>
        /// Set the stencil reference value.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="faceMask">Which faces to affect.</param>
        /// <param name="reference">Reference value for stencil comparison.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetStencilReference(VECommandBuffer cmd, VkStencilFaceFlags faceMask, uint reference)
        {
            return VulkEaseDll.veSetStencilReference(cmd.native, faceMask, reference);
        }

        /// <summary>
        /// Convenience function to enable or disable wireframe rendering.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enabled">True for wireframe, false for fill.</param>
        public static void SetWireframe(VECommandBuffer cmd, bool enabled)
        {
            VulkEaseDll.veSetWireframe(cmd.native, enabled);
        }

        /// <summary>
        /// Convenience function to configure depth testing.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="testEnabled">Whether depth testing is enabled.</param>
        /// <param name="writeEnabled">Whether depth writing is enabled.</param>
        public static void SetDepthTesting(VECommandBuffer cmd, bool testEnabled, bool writeEnabled)
        {
            VulkEaseDll.veSetDepthTesting(cmd.native, testEnabled, writeEnabled);
        }

        /// <summary>
        /// Override rasterization state.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="raster">Raster configuration to apply.</param>
        public static void OverrideRasterState(VECommandBuffer cmd, VERasterConfig raster)
        {
            VulkEaseDll.veOverrideRasterState(cmd.native, ref raster);
        }

        /// <summary>
        /// Override depth/stencil state.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="depth">Depth configuration to apply.</param>
        public static void OverrideDepthState(VECommandBuffer cmd, VEDepthConfig depth)
        {
            VulkEaseDll.veOverrideDepthState(cmd.native, ref depth);
        }

        /// <summary>
        /// Override blend state.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="blend">Blend configuration to apply.</param>
        public static void OverrideBlendState(VECommandBuffer cmd, VEBlendConfig blend)
        {
            VulkEaseDll.veOverrideBlendState(cmd.native, ref blend);
        }

        /// <summary>
        /// Enable or disable alpha blending.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="enabled">True to enable alpha blending.</param>
        public static void SetAlphaBlending(VECommandBuffer cmd, bool enabled)
        {
            VulkEaseDll.veSetAlphaBlending(cmd.native, enabled);
        }

        /// <summary>
        /// Set face culling mode.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="cullMode">Culling mode to apply.</param>
        public static void SetCulling(VECommandBuffer cmd, VkCullModeFlags cullMode)
        {
            VulkEaseDll.veSetCulling(cmd.native, cullMode);
        }

        #endregion

        // =============================================================================
        // SECTION 32: PUSH CONSTANTS & BUFFER BINDING
        // =============================================================================

        #region Push Constants & Buffer Binding

        /// <summary>
        /// Push constants to shaders.
        /// </summary>
        /// <typeparam name="T">Struct type to push.</typeparam>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="data">Data to push.</param>
        /// <param name="offset">Offset in push constant range (default 0).</param>
        /// <remarks>
        /// Push constants provide the fastest way to pass small amounts of data to shaders.
        /// Maximum size is 256 bytes (Vulkan spec minimum guarantee).
        /// </remarks>
        public static void PushConstants<T>(VECommandBuffer cmd, T data, UIntPtr offset = default) where T : struct
        {
            IntPtr dataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<T>());
            try
            {
                Marshal.StructureToPtr(data, dataPtr, false);
                VulkEaseDll.vePushConstants(cmd.native, dataPtr, (UIntPtr)Marshal.SizeOf<T>(), offset);
            }
            finally
            {
                Marshal.FreeHGlobal(dataPtr);
            }
        }

        /// <summary>
        /// Bind a vertex buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="binding">Binding index (typically 0).</param>
        /// <param name="vertexBuffer">Buffer containing vertex data.</param>
        /// <param name="offset">Byte offset into the buffer.</param>
        public static void BindVertexBuffer(VECommandBuffer cmd, UInt32 binding, VEBufferAddress vertexBuffer, UInt64 offset)
        {
            VulkEaseDll.veBindVertexBuffer(cmd.native, binding, vertexBuffer.native, offset);
        }

        /// <summary>
        /// Bind an index buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="indexBuffer">Buffer containing index data.</param>
        /// <param name="offset">Byte offset into the buffer.</param>
        /// <param name="format">Index format (UINT16 or UINT32).</param>
        public static void BindIndexBuffer(VECommandBuffer cmd, VEBufferAddress indexBuffer, UInt32 offset, VkIndexType format)
        {
            VulkEaseDll.veBindIndexBuffer(cmd.native, indexBuffer.native, offset, format);
        }

        #endregion

        // =============================================================================
        // SECTION 33: DRAW COMMANDS
        // =============================================================================

        #region Draw Commands

        /// <summary>
        /// Draw non-indexed primitives.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="vertexCount">Number of vertices to draw.</param>
        /// <param name="instanceCount">Number of instances to draw (default 1).</param>
        /// <param name="firstVertex">Index of the first vertex (default 0).</param>
        /// <param name="firstInstance">Index of the first instance (default 0).</param>
        public static void Draw(VECommandBuffer cmd, UInt32 vertexCount, UInt32 instanceCount = 1, UInt32 firstVertex = 0, UInt32 firstInstance = 0)
        {
            VulkEaseDll.veDraw(cmd.native, vertexCount, instanceCount, firstVertex, firstInstance);
        }

        /// <summary>
        /// Draw indexed primitives.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="indexCount">Number of indices to draw.</param>
        /// <param name="instanceCount">Number of instances to draw (default 1).</param>
        /// <param name="firstIndex">Index of the first index in the buffer (default 0).</param>
        /// <param name="vertexOffset">Value added to each index before fetching vertex data (default 0).</param>
        /// <param name="firstInstance">Index of the first instance (default 0).</param>
        public static void DrawIndexed(VECommandBuffer cmd, UInt32 indexCount, UInt32 instanceCount = 1, UInt32 firstIndex = 0, int vertexOffset = 0, UInt32 firstInstance = 0)
        {
            VulkEaseDll.veDrawIndexed(cmd.native, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }

        /// <summary>
        /// Draw primitives using parameters from an indirect buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="indirectBuffer">Buffer containing VkDrawIndirectCommand structures.</param>
        /// <param name="offset">Byte offset into the indirect buffer.</param>
        /// <param name="drawCount">Number of draws to execute.</param>
        /// <param name="stride">Stride between draw commands in the buffer.</param>
        public static void DrawIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset, UInt32 drawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndirect(cmd.native, indirectBuffer.native, offset, drawCount, stride);
        }

        /// <summary>
        /// Draw indexed primitives using parameters from an indirect buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="indirectBuffer">Buffer containing VkDrawIndexedIndirectCommand structures.</param>
        /// <param name="offset">Byte offset into the indirect buffer.</param>
        /// <param name="drawCount">Number of draws to execute.</param>
        /// <param name="stride">Stride between draw commands in the buffer.</param>
        public static void DrawIndexedIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset, UInt32 drawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndexedIndirect(cmd.native, indirectBuffer.native, offset, drawCount, stride);
        }

        /// <summary>
        /// Draw primitives with a GPU-determined draw count.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="indirectBuffer">Buffer containing draw commands.</param>
        /// <param name="indirectOffset">Byte offset into the indirect buffer.</param>
        /// <param name="countBuffer">Buffer containing the draw count.</param>
        /// <param name="countOffset">Byte offset into the count buffer.</param>
        /// <param name="maxDrawCount">Maximum number of draws to execute.</param>
        /// <param name="stride">Stride between draw commands.</param>
        public static void DrawIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr indirectOffset, VEBufferAddress countBuffer, UInt64 countOffset, UInt32 maxDrawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndirectCount(cmd.native, indirectBuffer.native, indirectOffset, countBuffer.native, countOffset, maxDrawCount, stride);
        }

        /// <summary>
        /// Draw indexed primitives with a GPU-determined draw count.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="indirectBuffer">Buffer containing draw commands.</param>
        /// <param name="indirectOffset">Byte offset into the indirect buffer.</param>
        /// <param name="countBuffer">Buffer containing the draw count.</param>
        /// <param name="countOffset">Byte offset into the count buffer.</param>
        /// <param name="maxDrawCount">Maximum number of draws to execute.</param>
        /// <param name="stride">Stride between draw commands.</param>
        public static void DrawIndexedIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr indirectOffset, VEBufferAddress countBuffer, UInt64 countOffset, UInt32 maxDrawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndexedIndirectCount(cmd.native, indirectBuffer.native, indirectOffset, countBuffer.native, countOffset, maxDrawCount, stride);
        }

        #endregion

        // =============================================================================
        // SECTION 34: CLEAR COMMANDS
        // =============================================================================

        #region Clear Commands

        /// <summary>
        /// Clear a color attachment during rendering.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state (inside a render pass).</param>
        /// <param name="attachmentIndex">Index of the color attachment to clear.</param>
        /// <param name="clearValue">Color to clear to.</param>
        /// <param name="rect">Optional rectangle to clear. If null, clears the entire attachment.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult ClearColorAttachment(VECommandBuffer cmd, UInt32 attachmentIndex, VEColor clearValue, VERect2D? rect = null)
        {
            if (rect.HasValue)
            {
                var rectValue = rect.Value;
                var handle = GCHandle.Alloc(rectValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veClearColorAttachment(cmd.native, attachmentIndex, clearValue, handle.AddrOfPinnedObject());
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veClearColorAttachment(cmd.native, attachmentIndex, clearValue, IntPtr.Zero);
        }

        /// <summary>
        /// Clear depth and/or stencil attachment during rendering.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state (inside a render pass).</param>
        /// <param name="depth">Depth value to clear to.</param>
        /// <param name="stencil">Stencil value to clear to.</param>
        /// <param name="aspectMask">Which aspects to clear (VK_IMAGE_ASPECT_DEPTH_BIT and/or VK_IMAGE_ASPECT_STENCIL_BIT).</param>
        /// <param name="rect">Optional rectangle to clear. If null, clears the entire attachment.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult ClearDepthStencilAttachment(VECommandBuffer cmd, float depth, UInt32 stencil,
            UInt32 aspectMask, VERect2D? rect = null)
        {
            if (rect.HasValue)
            {
                var rectValue = rect.Value;
                var handle = GCHandle.Alloc(rectValue, GCHandleType.Pinned);
                try
                {
                    return VulkEaseDll.veClearDepthStencilAttachment(cmd.native, depth, stencil, aspectMask, handle.AddrOfPinnedObject());
                }
                finally
                {
                    handle.Free();
                }
            }
            return VulkEaseDll.veClearDepthStencilAttachment(cmd.native, depth, stencil, aspectMask, IntPtr.Zero);
        }

        #endregion

        // =============================================================================
        // SECTION 35: QUERY FUNCTIONS
        // =============================================================================

        #region Query Functions

        /// <summary>
        /// Create a query pool for GPU queries.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Query pool descriptor with type and count.</param>
        /// <param name="pool">Receives the created query pool handle.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Query pools can be used for timestamp queries, occlusion queries, and pipeline statistics.</remarks>
        public static VEResult CreateQueryPool(VEDevice device, VEQueryPoolDesc desc, out VEQueryPool pool)
        {
            VEResult result = VulkEaseDll.veCreateQueryPool(device.native, ref desc, out IntPtr native);
            pool = new VEQueryPool { native = native };
            return result;
        }

        /// <summary>
        /// Destroy a query pool.
        /// </summary>
        /// <param name="pool">Query pool to destroy.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult DestroyQueryPool(VEQueryPool pool)
        {
            return VulkEaseDll.veDestroyQueryPool(pool.native);
        }

        /// <summary>
        /// Reset queries in a query pool.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="pool">Query pool containing the queries.</param>
        /// <param name="firstQuery">First query index to reset.</param>
        /// <param name="queryCount">Number of queries to reset.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>Queries must be reset before they can be used again.</remarks>
        public static VEResult CmdResetQueryPool(VECommandBuffer cmd, VEQueryPool pool, UInt32 firstQuery, UInt32 queryCount)
        {
            return VulkEaseDll.veCmdResetQueryPool(cmd.native, pool.native, firstQuery, queryCount);
        }

        /// <summary>
        /// Begin a query.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="pool">Query pool to use.</param>
        /// <param name="queryIndex">Index of the query to begin.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdBeginQuery(VECommandBuffer cmd, VEQueryPool pool, UInt32 queryIndex)
        {
            return VulkEaseDll.veCmdBeginQuery(cmd.native, pool.native, queryIndex);
        }

        /// <summary>
        /// End a query.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="pool">Query pool to use.</param>
        /// <param name="queryIndex">Index of the query to end.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdEndQuery(VECommandBuffer cmd, VEQueryPool pool, UInt32 queryIndex)
        {
            return VulkEaseDll.veCmdEndQuery(cmd.native, pool.native, queryIndex);
        }

        /// <summary>
        /// Write a GPU timestamp to a query.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="pool">Timestamp query pool.</param>
        /// <param name="queryIndex">Index of the query to write.</param>
        /// <param name="stage">Pipeline stage at which to record the timestamp.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult CmdWriteTimestamp(VECommandBuffer cmd, VEQueryPool pool, UInt32 queryIndex, VkPipelineStageFlags2 stage)
        {
            return VulkEaseDll.veCmdWriteTimestamp(cmd.native, pool.native, queryIndex, stage);
        }

        /// <summary>
        /// Retrieve query results from a query pool.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="pool">Query pool to read from.</param>
        /// <param name="firstQuery">First query index to read.</param>
        /// <param name="queryCount">Number of queries to read.</param>
        /// <param name="results">Array to receive the results.</param>
        /// <param name="flags">Query result flags (default: 64-bit results).</param>
        /// <returns>VE_SUCCESS on success, VE_NOT_READY if results not available yet.</returns>
        public static VEResult GetQueryResults(VEDevice device, VEQueryPool pool, UInt32 firstQuery, UInt32 queryCount,
            UInt64[] results, VkQueryResultFlags flags = VkQueryResultFlags.VK_QUERY_RESULT_64_BIT)
        {
            var handle = GCHandle.Alloc(results, GCHandleType.Pinned);
            try
            {
                return VulkEaseDll.veGetQueryResults(device.native, pool.native, firstQuery, queryCount,
                    handle.AddrOfPinnedObject(), (UInt64)(results.Length * sizeof(UInt64)), sizeof(UInt64), flags);
            }
            finally
            {
                handle.Free();
            }
        }

        #endregion

        // =============================================================================
        // SECTION 36: COMPUTE FUNCTIONS
        // =============================================================================

        #region Compute Functions

        /// <summary>
        /// Dispatch a compute shader.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="groupCountX">Number of workgroups in X dimension.</param>
        /// <param name="groupCountY">Number of workgroups in Y dimension.</param>
        /// <param name="groupCountZ">Number of workgroups in Z dimension.</param>
        public static void Dispatch(VECommandBuffer cmd, UInt32 groupCountX, UInt32 groupCountY, UInt32 groupCountZ)
        {
            VulkEaseDll.veDispatch(cmd.native, groupCountX, groupCountY, groupCountZ);
        }

        /// <summary>
        /// Dispatch a compute shader with parameters from an indirect buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="indirectBuffer">Buffer containing VkDispatchIndirectCommand.</param>
        /// <param name="offset">Byte offset into the indirect buffer.</param>
        public static void DispatchIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset)
        {
            VulkEaseDll.veDispatchIndirect(cmd.native, indirectBuffer.native, offset);
        }

        #endregion

        // =============================================================================
        // SECTION 37: SYNCHRONIZATION FUNCTIONS
        // =============================================================================

        #region Synchronization Functions

        /// <summary>
        /// Insert a barrier for vertex to fragment shader synchronization.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        public static void BarrierVertexToFragment(VECommandBuffer cmd) => VulkEaseDll.veBarrierVertexToFragment(cmd.native);

        /// <summary>
        /// Insert a barrier for compute to vertex shader synchronization.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        public static void BarrierComputeToVertex(VECommandBuffer cmd) => VulkEaseDll.veBarrierComputeToVertex(cmd.native);

        /// <summary>
        /// Insert a barrier for compute to compute shader synchronization.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        public static void BarrierComputeToCompute(VECommandBuffer cmd) => VulkEaseDll.veBarrierComputeToCompute(cmd.native);

        /// <summary>
        /// Insert a barrier for graphics to present synchronization.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        public static void BarrierGraphicsToPresent(VECommandBuffer cmd) => VulkEaseDll.veBarrierGraphicsToPresent(cmd.native);

        /// <summary>
        /// Transition a texture between image layouts.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        /// <param name="oldLayout">Current layout of the texture.</param>
        /// <param name="newLayout">Desired layout for the texture.</param>
        public static void TransitionTexture(VECommandBuffer cmd, VETextureIndex texture, VkImageLayout oldLayout, VkImageLayout newLayout)
        {
            VulkEaseDll.veTransitionTexture(cmd.native, texture.native, oldLayout, newLayout);
        }

        /// <summary>
        /// Transition a texture to shader read layout.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        public static void TransitionTextureForShaderRead(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForShaderRead(cmd.native, texture.native);

        /// <summary>
        /// Transition a texture to color attachment layout.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        public static void TransitionTextureForColorAttachment(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForColorAttachment(cmd.native, texture.native);

        /// <summary>
        /// Transition a texture to depth attachment layout.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        public static void TransitionTextureForDepthAttachment(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForDepthAttachment(cmd.native, texture.native);

        /// <summary>
        /// Transition a texture to transfer source layout.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        public static void TransitionTextureForTransferSrc(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForTransferSrc(cmd.native, texture.native);

        /// <summary>
        /// Transition a texture to transfer destination layout.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        public static void TransitionTextureForTransferDst(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForTransferDst(cmd.native, texture.native);

        /// <summary>
        /// Transition a texture to present layout for swapchain presentation.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        public static void TransitionTextureForPresent(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForPresent(cmd.native, texture.native);

        /// <summary>
        /// Transition a texture to a specific layout.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="texture">Texture to transition.</param>
        /// <param name="newLayout">Target layout for the texture.</param>
        public static void TransitionTextureToLayout(VECommandBuffer cmd, VETextureIndex texture, VkImageLayout newLayout)
        {
            VulkEaseDll.veTransitionTextureToLayout(cmd.native, texture.native, newLayout);
        }

        #endregion

        // =============================================================================
        // SECTION 38: SWAPCHAIN FUNCTIONS
        // =============================================================================

        #region Swapchain Functions

        /// <summary>
        /// Create a swapchain for window presentation.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="desc">Swapchain descriptor with surface, size, and format settings.</param>
        /// <returns>Swapchain handle.</returns>
        /// <exception cref="InvalidOperationException">Thrown if swapchain creation fails.</exception>
        /// <remarks>Call DestroySwapchain() when done.</remarks>
        public static VESwapchain CreateSwapchain(VEDevice device, VESwapchainDesc desc)
        {
            var namePtr = StringToHGlobalAnsi(desc.DebugName ?? string.Empty);
            try
            {
                var internalDesc = new VESwapchainDescInternal
                {
                    surface = ConvertSurfaceDesc(desc.Surface),
                    width = desc.Width,
                    height = desc.Height,
                    colorFormat = desc.ColorFormat,
                    vsync = desc.VSync,
                    debugName = namePtr
                };
                VEResult result = VulkEaseDll.veCreateSwapchain(device.native, ref internalDesc, out IntPtr native);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateSwapchain failed: {result}");
                return new VESwapchain { native = native };
            }
            finally
            {
                if (namePtr != IntPtr.Zero) Marshal.FreeHGlobal(namePtr);
            }
        }

        private static VESurfaceDescInternal ConvertSurfaceDesc(VESurfaceDesc desc)
        {
            var result = new VESurfaceDescInternal { type = desc.Type };
            switch (desc.Type)
            {
                case VESurfaceType.Win32:
                    result.win32_hwnd = desc.Handle1;
                    break;
                case VESurfaceType.Xlib:
                    result.xlib_display = desc.Handle1;
                    result.xlib_window = (ulong)desc.Handle2.ToInt64();
                    break;
                case VESurfaceType.Wayland:
                    result.wayland_display = desc.Handle1;
                    result.wayland_surface = desc.Handle2;
                    break;
                case VESurfaceType.Cocoa:
                    result.cocoa_window = desc.Handle1;
                    break;
                case VESurfaceType.Android:
                    result.android_nativeWindow = desc.Handle1;
                    break;
            }
            return result;
        }

        /// <summary>
        /// Destroy a swapchain.
        /// </summary>
        /// <param name="swapchain">Swapchain to destroy.</param>
        public static void DestroySwapchain(VESwapchain swapchain)
        {
            _ = VulkEaseDll.veDestroySwapchain(swapchain.native);
        }

        /// <summary>
        /// Present the rendered image to the swapchain.
        /// </summary>
        /// <param name="swapchain">Swapchain to present to.</param>
        /// <param name="cmd">Command buffer with rendering commands.</param>
        /// <param name="releaseCommandBuffer">If true, releases the command buffer after presentation.</param>
        /// <returns>VE_SUCCESS on success, VE_SUBOPTIMAL or VE_OUT_OF_DATE if swapchain needs recreation.</returns>
        public static VEResult PresentImage(VESwapchain swapchain, VECommandBuffer cmd, bool releaseCommandBuffer = true)
        {
            return VulkEaseDll.vePresentImage(swapchain.native, cmd.native, releaseCommandBuffer);
        }

        /// <summary>
        /// Resize a swapchain after window resize.
        /// </summary>
        /// <param name="swapchain">Swapchain to resize.</param>
        /// <param name="width">New width in pixels.</param>
        /// <param name="height">New height in pixels.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult ResizeSwapchain(VESwapchain swapchain, UInt32 width, UInt32 height)
        {
            return VulkEaseDll.veResizeSwapchain(swapchain.native, width, height);
        }

        /// <summary>
        /// Get the current size of a swapchain.
        /// </summary>
        /// <param name="swapchain">Swapchain to query.</param>
        /// <returns>Current swapchain dimensions.</returns>
        public static VkExtent2D GetSwapchainSize(VESwapchain swapchain)
        {
            return VulkEaseDll.veGetSwapchainSize(swapchain.native);
        }

        /// <summary>
        /// Get the color format of a swapchain.
        /// </summary>
        /// <param name="swapchain">Swapchain to query.</param>
        /// <returns>VkFormat of the swapchain images.</returns>
        public static VkFormat GetSwapchainFormat(VESwapchain swapchain)
        {
            return VulkEaseDll.veGetSwapchainFormat(swapchain.native);
        }

        #endregion

        // =============================================================================
        // SECTION 40: DEBUG & PROFILING FUNCTIONS
        // =============================================================================

        #region Debug & Profiling Functions

        /// <summary>
        /// Begin a debug label region in a command buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="label">Label text to display in debug tools.</param>
        /// <param name="color">Color for the label region.</param>
        /// <remarks>Visible in tools like RenderDoc. Must be balanced with EndDebugLabel.</remarks>
        public static void BeginDebugLabel(VECommandBuffer cmd, string label, VEColor color)
        {
            var labelPtr = StringToHGlobalAnsi(label);
            try
            {
                VulkEaseDll.veBeginDebugLabel(cmd.native, labelPtr, color);
            }
            finally
            {
                if (labelPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(labelPtr);
            }
        }

        /// <summary>
        /// End a debug label region in a command buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        public static void EndDebugLabel(VECommandBuffer cmd)
        {
            VulkEaseDll.veEndDebugLabel(cmd.native);
        }

        /// <summary>
        /// Insert a single debug label marker in a command buffer.
        /// </summary>
        /// <param name="cmd">Command buffer in recording state.</param>
        /// <param name="label">Label text to display.</param>
        /// <param name="color">Color for the marker.</param>
        public static void InsertDebugLabel(VECommandBuffer cmd, string label, VEColor color)
        {
            var labelPtr = StringToHGlobalAnsi(label);
            try
            {
                VulkEaseDll.veInsertDebugLabel(cmd.native, labelPtr, color);
            }
            finally
            {
                if (labelPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(labelPtr);
            }
        }

        /// <summary>
        /// Set a debug name for a buffer.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="address">Buffer address.</param>
        /// <param name="name">Debug name to set.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetBufferDebugName(VEDevice device, VEBufferAddress address, string name)
        {
            var namePtr = StringToHGlobalAnsi(name);
            try
            {
                return VulkEaseDll.veSetBufferDebugName(device.native, address.native, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Set a debug name for a texture.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="texture">Texture index.</param>
        /// <param name="name">Debug name to set.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetTextureDebugName(VEDevice device, VETextureIndex texture, string name)
        {
            var namePtr = StringToHGlobalAnsi(name);
            try
            {
                return VulkEaseDll.veSetTextureDebugName(device.native, texture.native, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Set a debug name for a sampler.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="sampler">Sampler index.</param>
        /// <param name="name">Debug name to set.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult SetSamplerDebugName(VEDevice device, VESamplerIndex sampler, string name)
        {
            var namePtr = StringToHGlobalAnsi(name);
            try
            {
                return VulkEaseDll.veSetSamplerDebugName(device.native, sampler.native, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Get performance statistics for the device.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="stats">Receives performance statistics.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult GetPerformanceStats(VEDevice device, out VEPerformanceStats stats)
        {
            return VulkEaseDll.veGetPerformanceStats(device.native, out stats);
        }

        /// <summary>
        /// Get memory usage statistics for the device.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="stats">Receives memory statistics.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult GetMemoryStats(VEDevice device, out VEMemoryStats stats)
        {
            return VulkEaseDll.veGetMemoryStats(device.native, out stats);
        }

        /// <summary>
        /// Get graphics pipeline statistics.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="stats">Receives pipeline statistics.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult GetGraphicsPipelineStats(VEDevice device, out VEGraphicsPipelineStats stats)
        {
            return VulkEaseDll.veGetGraphicsPipelineStats(device.native, out stats);
        }

        /// <summary>
        /// Print debug information about the device to the message callback.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        public static void PrintDebugInfo(VEDevice device)
        {
            VulkEaseDll.vePrintDebugInfo(device.native);
        }

        /// <summary>
        /// Print profiling information to the message callback.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        public static void PrintProfileInfo(VEDevice device)
        {
            VulkEaseDll.vePrintProfileInfo(device.native);
        }

        /// <summary>
        /// Get pipeline usage statistics.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="stats">Receives pipeline statistics.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult GetPipelineStats(VEDevice device, out VEPipelineStats stats)
        {
            return VulkEaseDll.veGetPipelineStats(device.native, out stats);
        }

        /// <summary>
        /// Print graphics pipeline configuration to the message callback.
        /// </summary>
        /// <param name="pipeline">Pipeline to print configuration for.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        public static VEResult PrintGraphicsPipeline(VEGraphicsPipeline pipeline)
        {
            return VulkEaseDll.vePrintGraphicsPipeline(pipeline.native);
        }

        /// <summary>
        /// Validate a graphics pipeline configuration for common errors.
        /// </summary>
        /// <param name="pipeline">Pipeline to validate.</param>
        /// <returns>VE_SUCCESS if valid, or an error code describing the issue.</returns>
        public static VEResult ValidateGraphicsPipeline(VEGraphicsPipeline pipeline)
        {
            return VulkEaseDll.veValidateGraphicsPipeline(pipeline.native);
        }

        #endregion

        // =============================================================================
        // SECTION 41: FRAME TIMING FUNCTIONS
        // =============================================================================

        #region Frame Timing Functions

        /// <summary>
        /// Begin a new frame for timing purposes.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>
        /// Call at the start of each frame to begin timing measurement.
        /// Must be paired with EndFrame().
        /// </remarks>
        public static VEResult BeginFrame(VEDevice device)
        {
            return VulkEaseDll.veBeginFrame(device.native);
        }

        /// <summary>
        /// End the current frame and retrieve timing information.
        /// </summary>
        /// <param name="device">Valid VulkEase device.</param>
        /// <param name="timing">Receives frame timing statistics.</param>
        /// <returns>VE_SUCCESS on success.</returns>
        /// <remarks>
        /// Call at the end of each frame after all GPU work is submitted.
        /// Returns CPU and GPU timing information for the completed frame.
        /// </remarks>
        public static VEResult EndFrame(VEDevice device, out VEFrameTimingInfo timing)
        {
            return VulkEaseDll.veEndFrame(device.native, out timing);
        }

        #endregion
    }
}
