using System;
using System.ComponentModel;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace VulkEase
{
    /// <summary>
    /// Dynamic library loader for VulkEase native libraries
    /// Automatically handles x86/x64 platform detection and loading
    /// </summary>
    public static class VulkEaseDll
    {
        static class Constants
        {
            public const string LibraryName = "NativeVulkEaseLib";
            public const string WindowsLibraryName = "vulkease.dll";
            public const string LinuxLibraryName = "libvulkease.so";
            public const string MacOsLibraryName = "vulkease.dylib";
        }

        static IntPtr DllImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            string baseDirectory = AppDomain.CurrentDomain.BaseDirectory;
            //string baseDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string libraryDirectory = Path.Combine(baseDirectory, "runtimes");
            string libraryPath = "";

            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                libraryPath = Path.Combine(libraryDirectory, "win-x64", "native", Constants.WindowsLibraryName);
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                libraryPath = Path.Combine(libraryDirectory, "linux-x64", "native", Constants.LinuxLibraryName);
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                libraryPath = Path.Combine(libraryDirectory, "osx-x64", "native", Constants.MacOsLibraryName);
            }

            IntPtr handle;
            NativeLibrary.TryLoad(libraryPath, assembly, searchPath, out handle);
            return handle;
        }

        static VulkEaseDll()
        {
            NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), DllImportResolver);
        }


        #region Context and Device Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 veGetVersion();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateContext([MarshalAs(UnmanagedType.LPStr)] string applicationName,
            IntPtr additionalInstanceExtensions, UInt32 additionalInstanceExtensionCount, out IntPtr outContext);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyContext(IntPtr context);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEnumeratePhysicalDevices(IntPtr context, out UInt32 count);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetPhysicalDeviceInfo(IntPtr context, UInt32 deviceIndex,
            out IntPtr physicalDevice, IntPtr deviceName, IntPtr deviceType);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateDevice(IntPtr context, IntPtr preferredDevice,
            IntPtr additionalDeviceExtensions, UInt32 additionalDeviceExtensionCount, out IntPtr outDevice);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyDevice(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool veIsInstanceExtensionAvailable([MarshalAs(UnmanagedType.LPStr)] string extensionName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool veIsDeviceExtensionAvailable(IntPtr context, [MarshalAs(UnmanagedType.LPStr)] string extensionName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDeviceWaitIdle(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetDeviceName(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetDriverVersion(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 veGetVulkanVersion(IntPtr device);

        // Debug message callback / severity
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate void VEMessageCallback(VEMessageSeverity severity, IntPtr message, IntPtr userData);

        [StructLayout(LayoutKind.Sequential)]
        internal struct VEMessageCallbackDesc
        {
            public VEMessageCallback callback;
            public IntPtr userData;
        }

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetMessageCallback(ref VEMessageCallbackDesc desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetMinMessageSeverity(VEMessageSeverity minSeverity);

        // Vulkan handle accessors
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkInstance(IntPtr context);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkPhysicalDevice(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkDevice(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkGraphicsQueue(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkComputeQueue(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkTransferQueue(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkCommandBufferFence(IntPtr cmd);
        #endregion
        

        #region Buffer Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateBuffer(IntPtr device, ref VEBufferDesc desc, out UInt64 outAddress);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyBuffer(IntPtr device, UInt64 address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt64 veGetBufferSize(IntPtr device, UInt64 address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkBufferUsageFlags veGetBufferUsage(IntPtr device, UInt64 address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veMapBuffer(IntPtr device, UInt64 address, out IntPtr mappedData);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veUnmapBuffer(IntPtr device, UInt64 address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veUpdateBuffer(IntPtr device, UInt64 address, IntPtr data, UInt64 size, UInt64 offset);

        // NOTE: value-returning overloads removed; use the VEResult + out versions above.
        #endregion


        #region Convenience buffer functions
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateVertexBuffer(IntPtr device, IntPtr vertices, UInt64 size, IntPtr debugName, out UInt64 outAddress);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateIndexBuffer(IntPtr device, IntPtr indices, UInt64 size, IntPtr debugName, out UInt64 outAddress);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateUniformBuffer(IntPtr device, UInt64 size, [MarshalAs(UnmanagedType.I1)] bool persistentlyMapped,
            IntPtr debugName, out UInt64 outAddress);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateStorageBuffer(IntPtr device, UInt64 size, IntPtr debugName, out UInt64 outAddress);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateIndirectBuffer(IntPtr device, UInt64 size, IntPtr debugName, out UInt64 outAddress);
        #endregion


        #region Texture Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTexture(IntPtr device, ref VETextureDesc desc, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyTexture(IntPtr device, UInt32 index);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkExtent3D veGetTextureSize(IntPtr device, UInt32 index);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetTextureFormat(IntPtr device, UInt32 index);

        // Convenience texture functions
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTexture1D(IntPtr device, UInt32 width, VkFormat format, VkImageUsageFlags usage, IntPtr debugName, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTexture2D(IntPtr device, UInt32 width, UInt32 height, VkFormat format, VkImageUsageFlags usage, IntPtr debugName, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTexture3D(IntPtr device, UInt32 width, UInt32 height, UInt32 depth, VkFormat format, VkImageUsageFlags usage, IntPtr debugName, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTexture2DArray(IntPtr device, UInt32 width, UInt32 height, UInt32 layers, VkFormat format, VkImageUsageFlags usage, IntPtr debugName, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTextureCube(IntPtr device, UInt32 size, VkFormat format, VkImageUsageFlags usage, IntPtr debugName, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTexture2DMultisample(IntPtr device, UInt32 width, UInt32 height, VkFormat format, VkSampleCountFlags sampleCount, VkImageUsageFlags usage, IntPtr debugName, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veLoadTexture(IntPtr device, IntPtr filename, VkImageUsageFlags usage, bool generateMips, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veLoadHDRTexture(IntPtr device, IntPtr filename, VkImageUsageFlags usage, bool generateMips, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veLoadCubeTexture(IntPtr device, IntPtr filenames, VkImageUsageFlags usage, bool generateMips, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGenerateMipmaps(IntPtr device, IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGenerateMipmapsImmediate(IntPtr device, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSaveTexture(IntPtr device, UInt32 texture, IntPtr filename);

        // Host image copy functions
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veHostWriteTextureRegion(IntPtr device, UInt32 textureIndex, IntPtr srcData,
            UIntPtr dataSize, UInt32 offsetX, UInt32 offsetY, UInt32 offsetZ,
            UInt32 width, UInt32 height, UInt32 depth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veHostReadTextureRegion(IntPtr device, UInt32 textureIndex, IntPtr dstData,
            UIntPtr dataSize, UInt32 offsetX, UInt32 offsetY, UInt32 offsetZ,
            UInt32 width, UInt32 height, UInt32 depth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veHostWriteTexture(IntPtr device, UInt32 textureIndex, IntPtr srcData,
            UIntPtr dataSize);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veHostReadTexture(IntPtr device, UInt32 textureIndex, IntPtr dstData,
            UIntPtr dataSize);
        #endregion

        #region Sampler Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateSampler(IntPtr device, ref VESamplerDesc desc, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroySampler(IntPtr device, UInt32 index);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateLinearSampler(IntPtr device, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateNearestSampler(IntPtr device, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateAnisotropicSampler(IntPtr device, float maxAnisotropy, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateShadowSampler(IntPtr device, out UInt32 outIndex);
        #endregion

        #region Shader Objects
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veLoadShaderFromBuffer(IntPtr device, VkShaderStageFlags stage, IntPtr code, UIntPtr codeSize, IntPtr entryPoint, IntPtr debugName, out IntPtr outShader);
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veLoadShaderFromFile(IntPtr device, IntPtr filename, VkShaderStageFlags stage, IntPtr entryPoint, IntPtr debugName, out IntPtr outShader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyShader(IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetShaderHotReloadEnabled(IntPtr device, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool veShaderNeedsReload(IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veReloadShader(IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateShaderConfig(IntPtr device, ref VEShaderConfigDesc desc, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyShaderConfig(IntPtr config);
        #endregion

        #region Render Configuration Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateRenderConfig(IntPtr device, ref VERenderConfigDesc desc, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyRenderConfig(IntPtr config);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateVertexConfig(IntPtr device, UInt32 bindingCount, IntPtr bindings, UInt32 attributeCount, IntPtr attributes, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyVertexConfig(IntPtr config);

        // Default configurations
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VERasterConfig veDefaultRasterConfig();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEDepthConfig veDefaultDepthConfig();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEBlendConfig veDefaultOpaqueBlendConfig();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEBlendConfig veDefaultAlphaBlendConfig();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEBlendConfig veDefaultAdditiveBlendConfig();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEMultisampleConfig veDefaultMultisampleConfig();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEVertexInputConfig veDefaultVertexInputConfig();

        // Common render configurations
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateOpaqueRenderConfig(IntPtr device, IntPtr debugName, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateTransparentRenderConfig(IntPtr device, IntPtr debugName, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateWireframeRenderConfig(IntPtr device, IntPtr debugName, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateShadowRenderConfig(IntPtr device, IntPtr debugName, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateUIRenderConfig(IntPtr device, IntPtr debugName, out IntPtr outConfig);

        // Configuration composition
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateConfigVariant(IntPtr baseConfig, ref VERenderConfigDesc overrides, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veMergeRenderConfigs(IntPtr device, UInt32 configCount, IntPtr configs, IntPtr debugName, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCloneRenderConfig(IntPtr config, IntPtr debugName, out IntPtr outConfig);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VERenderingInfoInternal veCreateRenderingInfo(UInt32 width, UInt32 height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VERenderingInfoInternal veCreateRenderingInfoWithOffset(Int32 x, Int32 y, UInt32 width, UInt32 height);
        #endregion

        #region Command Buffer and Rendering
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginCommandBuffer(IntPtr device, out IntPtr outCmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginSecondaryCommandBuffer(IntPtr device, IntPtr desc, out IntPtr outCmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePopulateSecondaryDescFromRenderingInfo(IntPtr device,
            ref VERenderingInfoInternal renderingInfo, ref VESecondaryCommandBufferDesc desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginSecondaryRecording(IntPtr cmd, IntPtr desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEndCommandBuffer(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veExecuteSecondaryCommandBuffers(IntPtr primaryCmd, UInt32 count, IntPtr[] secondaryCmds, [MarshalAs(UnmanagedType.I1)] bool releaseCommandBuffers);

        [StructLayout(LayoutKind.Sequential)]
        internal struct VESubmitInfo
        {
            public IntPtr waitSemaphores;
            public IntPtr waitSemaphoreValues;
            public IntPtr waitStageMasks;
            public UInt32 waitSemaphoreCount;

            public IntPtr signalSemaphores;
            public IntPtr signalSemaphoreValues;
            public UInt32 signalSemaphoreCount;

            public IntPtr fence;
            [MarshalAs(UnmanagedType.I1)]
            public bool waitForCompletion;
        }

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSubmitCommandBuffer(IntPtr cmd, ref VESubmitInfo submitInfo);

        // Escape hatch getters
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkBufferFromAddress(IntPtr device, UInt64 address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkImageFromTexture(IntPtr device, UInt32 textureIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkImageViewFromTexture(IntPtr device, UInt32 textureIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkSamplerFromIndex(IntPtr device, UInt32 samplerIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkSwapchain(IntPtr swapchain);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkSwapchainImage(IntPtr swapchain, UInt32 imageIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetVkSwapchainImageView(IntPtr swapchain, UInt32 imageIndex);


        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veResetCommandBuffer(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veReleaseCommandBuffer(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginRendering(IntPtr cmd, ref VERenderingInfoInternal renderingInfo);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEndRendering(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veApplyRenderConfig(IntPtr cmd, IntPtr config);

        [StructLayout(LayoutKind.Sequential)]
        internal struct VERenderState
        {
            public IntPtr shaderConfig;
            public IntPtr renderConfig;
            public IntPtr viewport;
            public IntPtr scissor;
        }

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veApplyRenderState(IntPtr cmd, ref VERenderState state);
        #endregion

        #region Shader binding
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindShader(IntPtr cmd, IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindShaders(IntPtr cmd, UInt32 shaderCount, IntPtr shaders);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindShaderConfig(IntPtr cmd, IntPtr config);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veUnbindShaderStage(IntPtr cmd, VkShaderStageFlags stage);
        #endregion

        #region Dynamic state
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetViewport(IntPtr cmd, float x, float y, float width, float height, float minDepth, float maxDepth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetScissor(IntPtr cmd, int x, int y, UInt32 width, UInt32 height);

        // State overrides
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veOverrideRasterState(IntPtr cmd, ref VERasterConfig raster);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veOverrideDepthState(IntPtr cmd, ref VEDepthConfig depth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veOverrideBlendState(IntPtr cmd, ref VEBlendConfig blend);
        #endregion

        #region Quick toggles
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetWireframe(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetAlphaBlending(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetDepthTesting(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool testEnabled, [MarshalAs(UnmanagedType.I1)] bool writeEnabled);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetCulling(IntPtr cmd, VkCullModeFlags cullMode);
        #endregion

        #region Push constants and binding
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePushConstants(IntPtr cmd, IntPtr data, UIntPtr size, UIntPtr offset);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindIndexBuffer(IntPtr cmd, UInt64 indexBuffer, UInt64 offset, VkIndexType format);
        #endregion

        #region Drawing
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDraw(IntPtr cmd, UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawIndexed(IntPtr cmd, UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, int vertexOffset, UInt32 firstInstance);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawIndirect(IntPtr cmd, UInt64 indirectBuffer, UInt64 offset, UInt32 drawCount, UInt32 stride);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawIndexedIndirect(IntPtr cmd, UInt64 indirectBuffer, UInt64 offset, UInt32 drawCount, UInt32 stride);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawIndirectCount(IntPtr cmd, UInt64 indirectBuffer, UInt64 indirectOffset, UInt64 countBuffer, UInt64 countOffset, UInt32 maxDrawCount, UInt32 stride);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawIndexedIndirectCount(IntPtr cmd, UInt64 indirectBuffer, UInt64 indirectOffset, UInt64 countBuffer, UInt64 countOffset, UInt32 maxDrawCount, UInt32 stride);
        #endregion

        #region Compute
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDispatch(IntPtr cmd, UInt32 groupCountX, UInt32 groupCountY, UInt32 groupCountZ);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDispatchIndirect(IntPtr cmd, UInt64 indirectBuffer, UInt64 offset);
        #endregion

        #region Barriers
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBarrierVertexToFragment(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBarrierComputeToVertex(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBarrierComputeToCompute(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBarrierGraphicsToPresent(IntPtr cmd);
        #endregion

        #region Texture transitions
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTexture(IntPtr cmd, UInt32 texture, VkImageLayout oldLayout, VkImageLayout newLayout);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTextureForShaderRead(IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTextureForColorAttachment(IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTextureForDepthAttachment(IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTextureForTransferSrc(IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTextureForTransferDst(IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTextureForPresent(IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veTransitionTextureToLayout(IntPtr cmd, UInt32 texture, VkImageLayout newLayout);
        #endregion

        #region Swapchain
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateSwapchain(IntPtr device, ref VESwapchainDescInternal desc, out IntPtr outSwapchain);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroySwapchain(IntPtr swapchain);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePresentImage(IntPtr swapchain, IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool releaseCommandBuffer);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veResizeSwapchain(IntPtr swapchain, UInt32 width, UInt32 height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkExtent2D veGetSwapchainSize(IntPtr swapchain);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetSwapchainFormat(IntPtr swapchain);
        #endregion

        #region Render Targets
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateRenderTarget(IntPtr device, ref VERenderTargetDescInternal desc, out IntPtr outRenderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyRenderTarget(IntPtr renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veResizeRenderTarget(IntPtr renderTarget, UInt32 width, UInt32 height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 veGetRenderTargetColorTexture(IntPtr renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 veGetRenderTargetDepthTexture(IntPtr renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 veGetRenderTargetResolveTexture(IntPtr renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkExtent2D veGetRenderTargetSize(IntPtr renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetRenderTargetColorFormat(IntPtr renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetRenderTargetDepthFormat(IntPtr renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBlitToSwapchain(IntPtr cmd, IntPtr renderTarget, IntPtr swapchain, VkFilter filter);
        #endregion

        #region Debug and Profiling
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginDebugLabel(IntPtr cmd, IntPtr label, VEColor color);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEndDebugLabel(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veInsertDebugLabel(IntPtr cmd, IntPtr label, VEColor color);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetBufferDebugName(IntPtr device, UInt64 address, IntPtr name);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetTextureDebugName(IntPtr device, UInt32 texture, IntPtr name);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetSamplerDebugName(IntPtr device, UInt32 sampler, IntPtr name);
        #endregion

        #region Statistics
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetPerformanceStats(IntPtr device, out VEPerformanceStats stats);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetMemoryStats(IntPtr device, out VEMemoryStats stats);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetRenderConfigStats(IntPtr device, out VERenderConfigStats stats);
        #endregion

        #region Debug information
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePrintDebugInfo(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePrintProfileInfo(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePrintRenderConfig(IntPtr config);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veValidateRenderConfig(IntPtr config);
        #endregion
    }
}