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

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        internal static extern bool veIsMeshShaderSupported(IntPtr device);

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
        internal static extern VEResult veImportExternalTexture(IntPtr device, ref VEExternalTextureDesc desc, out UInt32 outIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veReleaseExternalTexture(IntPtr device, UInt32 index);

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
        internal static extern VEResult veCmdGenerateMipmaps(IntPtr cmd, UInt32 texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGenerateMipmaps(IntPtr device, UInt32 texture);

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

        #region Buffer and Texture Transfer Operations
        // Buffer copy operations
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdCopyBuffer(IntPtr cmd, UInt64 src, UInt64 dst,
            UInt64 srcOffset, UInt64 dstOffset, UInt64 size);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCopyBuffer(IntPtr device, UInt64 src, UInt64 dst,
            UInt64 srcOffset, UInt64 dstOffset, UInt64 size, IntPtr fence);

        // Buffer fill and update
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdFillBuffer(IntPtr cmd, UInt64 dst, UInt64 offset, UInt64 size, UInt32 data);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdUpdateBuffer(IntPtr cmd, UInt64 dst, UInt64 offset, UInt64 size, IntPtr data);

        // Texture copy operations
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdCopyTexture(IntPtr cmd, UInt32 src, UInt32 dst, IntPtr region);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCopyTexture(IntPtr device, UInt32 src, UInt32 dst, IntPtr region, IntPtr fence);

        // Texture blit operations
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdBlitTexture(IntPtr cmd, UInt32 src, UInt32 dst, IntPtr region, VkFilter filter);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBlitTexture(IntPtr device, UInt32 src, UInt32 dst, IntPtr region, VkFilter filter, IntPtr fence);

        // Buffer-to-texture copy
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdCopyBufferToTexture(IntPtr cmd, UInt64 src, UInt32 dst, IntPtr region);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCopyBufferToTexture(IntPtr device, UInt64 src, UInt32 dst, IntPtr region, IntPtr fence);

        // Texture-to-buffer copy
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdCopyTextureToBuffer(IntPtr cmd, UInt32 src, UInt64 dst, IntPtr region);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCopyTextureToBuffer(IntPtr device, UInt32 src, UInt64 dst, IntPtr region, IntPtr fence);
        #endregion

        #region Query Pool Operations
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateQueryPool(IntPtr device, ref VEQueryPoolDesc desc, out IntPtr outPool);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyQueryPool(IntPtr pool);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdResetQueryPool(IntPtr cmd, IntPtr pool, UInt32 firstQuery, UInt32 queryCount);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdBeginQuery(IntPtr cmd, IntPtr pool, UInt32 queryIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdEndQuery(IntPtr cmd, IntPtr pool, UInt32 queryIndex);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCmdWriteTimestamp(IntPtr cmd, IntPtr pool, UInt32 queryIndex, VkPipelineStageFlags2 stage);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetQueryResults(IntPtr device, IntPtr pool, UInt32 firstQuery,
            UInt32 queryCount, IntPtr data, UInt64 dataSize, UInt64 stride, VkQueryResultFlags flags);
        #endregion

        #region Fence Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateFence(IntPtr device, [MarshalAs(UnmanagedType.I1)] bool signaled, out IntPtr outFence);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyFence(IntPtr device, IntPtr fence);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veWaitFence(IntPtr device, IntPtr fence, UInt64 timeout);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetFenceStatus(IntPtr device, IntPtr fence, [MarshalAs(UnmanagedType.I1)] out bool signaled);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veResetFence(IntPtr device, IntPtr fence);
        #endregion

        #region Frame Timing
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginFrame(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEndFrame(IntPtr device, out VEFrameTimingInfo outTiming);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 veGetDefaultSampler(IntPtr device, VEDefaultSampler sampler);
        #endregion

        #region Additional Dynamic State
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetTopology(IntPtr cmd, VkPrimitiveTopology topology);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetPrimitiveRestart(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetPatchControlPoints(IntPtr cmd, UInt32 controlPoints);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetPolygonMode(IntPtr cmd, VkPolygonMode mode);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetLineWidth(IntPtr cmd, float width);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetCullMode(IntPtr cmd, VkCullModeFlags cullMode);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetFrontFace(IntPtr cmd, VkFrontFace frontFace);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetRasterizerDiscard(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetDepthBias(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable, 
            float constantFactor, float clamp, float slopeFactor);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetDepthClamp(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetAlphaToCoverage(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetAlphaToOne(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetDepthTest(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetDepthWrite(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetDepthCompareOp(IntPtr cmd, VkCompareOp op);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetStencilTest(IntPtr cmd, [MarshalAs(UnmanagedType.I1)] bool enable);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetStencilOp(IntPtr cmd, VkStencilFaceFlags faceMask,
            VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetStencilReference(IntPtr cmd, VkStencilFaceFlags faceMask, UInt32 reference);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindVertexBuffer(IntPtr cmd, UInt32 binding, UInt64 vertexBuffer, UInt64 offset);
        #endregion

        #region Clear Commands
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veClearColorAttachment(IntPtr cmd, UInt32 attachmentIndex, VEColor clearValue, IntPtr rect);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veClearDepthStencilAttachment(IntPtr cmd, float depth, UInt32 stencil,
            UInt32 aspectMask, IntPtr rect);
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

        #endregion

        #region Graphics Pipeline Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateGraphicsPipeline(IntPtr device, ref VEGraphicsPipelineDesc desc, out IntPtr outPipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyGraphicsPipeline(IntPtr pipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEGraphicsPipelineDesc veDefaultGraphicsPipelineDesc();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern VEResult veCreateOpaquePipeline(IntPtr device, IntPtr vertexShader, IntPtr fragmentShader,
            IntPtr bindings, UInt32 bindingCount, IntPtr attrs, UInt32 attrCount,
            [MarshalAs(UnmanagedType.LPStr)] string debugName, out IntPtr outPipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern VEResult veCreateTransparentPipeline(IntPtr device, IntPtr vertexShader, IntPtr fragmentShader,
            IntPtr bindings, UInt32 bindingCount, IntPtr attrs, UInt32 attrCount,
            [MarshalAs(UnmanagedType.LPStr)] string debugName, out IntPtr outPipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern VEResult veCreateAdditivePipeline(IntPtr device, IntPtr vertexShader, IntPtr fragmentShader,
            IntPtr bindings, UInt32 bindingCount, IntPtr attrs, UInt32 attrCount,
            [MarshalAs(UnmanagedType.LPStr)] string debugName, out IntPtr outPipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern VEResult veCreateShadowPipeline(IntPtr device, IntPtr vertexShader, IntPtr fragmentShader,
            IntPtr bindings, UInt32 bindingCount, IntPtr attrs, UInt32 attrCount,
            [MarshalAs(UnmanagedType.LPStr)] string debugName, out IntPtr outPipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern VEResult veCreateUIOverlayPipeline(IntPtr device, IntPtr vertexShader, IntPtr fragmentShader,
            IntPtr bindings, UInt32 bindingCount, IntPtr attrs, UInt32 attrCount,
            [MarshalAs(UnmanagedType.LPStr)] string debugName, out IntPtr outPipeline);

        // Draw State Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateDrawState(IntPtr device, ref VEDrawStateDesc desc, out IntPtr outDrawState);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDestroyDrawState(IntPtr drawState);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateDefaultDrawState(IntPtr device, out IntPtr outDrawState);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateWireframeDrawState(IntPtr device, out IntPtr outDrawState);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veCreateShadowDrawState(IntPtr device, out IntPtr outDrawState);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEDrawStateDesc veDefaultDrawStateDesc();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VERenderTargetInternal veCreateRenderTarget(UInt32 width, UInt32 height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VERenderTargetInternal veCreateRenderTargetWithOffset(Int32 x, Int32 y, UInt32 width, UInt32 height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veRenderTargetAddColorAttachment(ref VERenderTargetInternal target, UInt32 texture,
            VkAttachmentLoadOp loadOp, VEColor clearValue);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veRenderTargetAddColorAttachmentResolve(ref VERenderTargetInternal target, UInt32 texture,
            UInt32 resolveTexture, VkAttachmentLoadOp loadOp, VEColor clearValue);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veRenderTargetSetDepthAttachment(ref VERenderTargetInternal target, UInt32 texture,
            VkAttachmentLoadOp loadOp, float clearDepth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veRenderTargetSetStencilAttachment(ref VERenderTargetInternal target, UInt32 texture,
            VkAttachmentLoadOp loadOp, UInt32 clearStencil);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veResizeRenderTarget(IntPtr device, ref VERenderTargetInternal target,
            UInt32 width, UInt32 height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VERenderTargetInternal veCreateSimpleRenderTarget(IntPtr device, UInt32 width, UInt32 height,
            VkFormat colorFormat, VkFormat depthFormat, VEColor clearColor, float clearDepth,
            out UInt32 outColorTexture, out UInt32 outDepthTexture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBlitTextureToSwapchain(IntPtr cmd, UInt32 texture,
            IntPtr swapchain, VkFilter filter);
        #endregion

        #region Command Buffer and Rendering
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginCommandBuffer(IntPtr device, out IntPtr outCmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBeginSecondaryCommandBuffer(IntPtr device, IntPtr desc, out IntPtr outCmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePopulateSecondaryDescFromRenderTarget(IntPtr device,
            ref VERenderTargetInternal renderTarget, ref VESecondaryCommandBufferDesc desc);

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
        internal static extern VEResult veBeginRendering(IntPtr cmd, ref VERenderTargetInternal renderTarget);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEndRendering(IntPtr cmd);

        // Graphics pipeline and draw state binding
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindGraphicsPipeline(IntPtr cmd, IntPtr pipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veApplyDrawState(IntPtr cmd, IntPtr drawState);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veApplyGraphicsState(IntPtr cmd, IntPtr pipeline, IntPtr drawState, IntPtr viewport, IntPtr scissor);
        #endregion

        #region Shader binding
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindShader(IntPtr cmd, IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veBindShaders(IntPtr cmd, UInt32 shaderCount, IntPtr shaders);

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

        // Mesh shader drawing commands
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawMeshTasks(IntPtr cmd, UInt32 groupCountX, UInt32 groupCountY, UInt32 groupCountZ);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawMeshTasksIndirect(IntPtr cmd, UInt64 indirectBuffer, UInt64 offset, UInt32 drawCount, UInt32 stride);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDrawMeshTasksIndirectCount(IntPtr cmd, UInt64 indirectBuffer, UInt64 indirectOffset, UInt64 countBuffer, UInt64 countOffset, UInt32 maxDrawCount, UInt32 stride);
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
        internal static extern VEResult veGetGraphicsPipelineStats(IntPtr device, out VEGraphicsPipelineStats stats);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetPipelineStats(IntPtr device, out VEPipelineStats stats);
        #endregion

        #region Debug information
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePrintDebugInfo(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePrintProfileInfo(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePrintGraphicsPipeline(IntPtr pipeline);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veValidateGraphicsPipeline(IntPtr pipeline);
        #endregion
    }
}