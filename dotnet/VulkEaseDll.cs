using System;
using System.ComponentModel;
using System.IO;
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
        internal static extern uint veGetVersion();

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateContext([MarshalAs(UnmanagedType.LPStr)] string applicationName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyContext(IntPtr context);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateDevice(IntPtr context);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyDevice(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDeviceWaitIdle(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetDeviceName(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetDriverVersion(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veGetVulkanVersion(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetLastError();
        #endregion
        

        #region Buffer Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateBuffer(IntPtr device, ref VEBufferDesc desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyBuffer(IntPtr device, ulong address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veMapBuffer(IntPtr device, ulong address, out IntPtr mappedData);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veUnmapBuffer(IntPtr device, ulong address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veUpdateBuffer(IntPtr device, ulong address, IntPtr data, UInt64 size, UInt64 offset);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UIntPtr veGetBufferSize(IntPtr device, ulong address);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkBufferUsageFlags veGetBufferUsage(IntPtr device, ulong address);
        #endregion


        #region Convenience buffer functions
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateVertexBuffer(IntPtr device, IntPtr vertices, UInt64 size, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateIndexBuffer(IntPtr device, IntPtr indices, UInt64 size, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateUniformBuffer(IntPtr device, UInt64 size, bool persistentlyMapped, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateStorageBuffer(IntPtr device, UInt64 size, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateIndirectBuffer(IntPtr device, UInt64 size, IntPtr debugName);
        #endregion


        #region Texture Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture(IntPtr device, ref VETextureDesc desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyTexture(IntPtr device, uint index);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetTextureSize(IntPtr device, uint index, out uint width, out uint height, out uint depth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetTextureFormat(IntPtr device, uint index);

        // Convenience texture functions
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture1D(IntPtr device, uint width, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture2D(IntPtr device, uint width, uint height, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture3D(IntPtr device, uint width, uint height, uint depth, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture2DArray(IntPtr device, uint width, uint height, uint layers, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTextureCube(IntPtr device, uint size, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture2DMultisample(IntPtr device, uint width, uint height, VkFormat format, VkSampleCountFlags sampleCount, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veLoadTexture(IntPtr device, IntPtr filename, VkImageUsageFlags usage, bool generateMips);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veLoadHDRTexture(IntPtr device, IntPtr filename, VkImageUsageFlags usage, bool generateMips);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veLoadCubeTexture(IntPtr device, IntPtr filenames, VkImageUsageFlags usage, bool generateMips);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGenerateMipmaps(IntPtr device, IntPtr cmd, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGenerateMipmapsImmediate(IntPtr device, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSaveTexture(IntPtr device, uint texture, IntPtr filename);
        #endregion

        #region Sampler Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateSampler(IntPtr device, ref VESamplerDesc desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroySampler(IntPtr device, uint index);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateLinearSampler(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateNearestSampler(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateAnisotropicSampler(IntPtr device, float maxAnisotropy);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateShadowSampler(IntPtr device);
        #endregion

        #region Shader Objects
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShaderFromSPIRV(IntPtr device, VkShaderStageFlags stage, IntPtr code, UIntPtr codeSize, IntPtr entryPoint, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShaderFromGLSL(IntPtr device, VkShaderStageFlags stage, IntPtr source, IntPtr entryPoint, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veLoadShader(IntPtr device, IntPtr filename, VkShaderStageFlags stage, IntPtr entryPoint, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyShader(IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEnableShaderHotReload(IntPtr shader, IntPtr sourceFile);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veReloadShader(IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShaderConfig(IntPtr device, ref VEShaderConfigDesc desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyShaderConfig(IntPtr config);
        #endregion

        #region Render Configuration Management
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateRenderConfig(IntPtr device, ref VERenderConfigDesc desc);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyRenderConfig(IntPtr config);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateVertexConfig(IntPtr device, uint bindingCount, IntPtr bindings, uint attributeCount, IntPtr attributes);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyVertexConfig(IntPtr config);

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
        internal static extern IntPtr veCreateOpaqueRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateTransparentRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateWireframeRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShadowRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateUIRenderConfig(IntPtr device, IntPtr debugName);

        // Configuration composition
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateConfigVariant(IntPtr baseConfig, ref VERenderConfigDesc overrides);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veMergeRenderConfigs(IntPtr device, uint configCount, IntPtr configs, IntPtr debugName);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCloneRenderConfig(IntPtr config, IntPtr debugName);
        #endregion

        #region Command Buffer and Rendering
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veBeginCommandBuffer(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSubmitCommandBuffer(IntPtr cmd, bool waitForCompletion);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBeginRendering(IntPtr cmd, ref VERenderingInfo renderingInfo);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veEndRendering(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veApplyRenderConfig(IntPtr cmd, IntPtr config);
        #endregion

        #region Shader binding
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindShader(IntPtr cmd, IntPtr shader);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindShaders(IntPtr cmd, uint shaderCount, IntPtr shaders);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindShaderConfig(IntPtr cmd, IntPtr config);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veUnbindShaderStage(IntPtr cmd, VkShaderStageFlags stage);
        #endregion

        #region Dynamic state
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetViewport(IntPtr cmd, float x, float y, float width, float height, float minDepth, float maxDepth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetScissor(IntPtr cmd, int x, int y, uint width, uint height);

        // State overrides
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veOverrideRasterState(IntPtr cmd, ref VERasterConfig raster);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veOverrideDepthState(IntPtr cmd, ref VEDepthConfig depth);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veOverrideBlendState(IntPtr cmd, ref VEBlendConfig blend);
        #endregion

        #region Quick toggles
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetWireframe(IntPtr cmd, bool enabled);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetAlphaBlending(IntPtr cmd, bool enabled);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetDepthTesting(IntPtr cmd, bool testEnabled, bool writeEnabled);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetCulling(IntPtr cmd, VkCullModeFlags cullMode);
        #endregion

        #region Push constants and binding
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void vePushConstants(IntPtr cmd, IntPtr data, UIntPtr size, UIntPtr offset);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindIndexBuffer(IntPtr cmd, ulong indexBuffer, uint offset, VkIndexType format);
        #endregion

        #region Drawing
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDraw(IntPtr cmd, uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndexed(IntPtr cmd, uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndirect(IntPtr cmd, ulong indirectBuffer, UIntPtr offset, uint drawCount, uint stride);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndexedIndirect(IntPtr cmd, ulong indirectBuffer, UIntPtr offset, uint drawCount, uint stride);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndirectCount(IntPtr cmd, ulong indirectBuffer, UIntPtr indirectOffset, ulong countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndexedIndirectCount(IntPtr cmd, ulong indirectBuffer, UIntPtr indirectOffset, ulong countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride);
        #endregion

        #region Compute
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDispatch(IntPtr cmd, uint groupCountX, uint groupCountY, uint groupCountZ);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDispatchIndirect(IntPtr cmd, ulong indirectBuffer, UIntPtr offset);
        #endregion

        #region Barriers
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierVertexToFragment(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierComputeToVertex(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierComputeToCompute(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierGraphicsToPresent(IntPtr cmd);
        #endregion

        #region Texture transitions
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTexture(IntPtr cmd, uint texture, uint oldLayout, uint newLayout);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForShaderRead(IntPtr cmd, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForColorAttachment(IntPtr cmd, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForDepthAttachment(IntPtr cmd, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForTransferSrc(IntPtr cmd, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForTransferDst(IntPtr cmd, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForPresent(IntPtr cmd, uint texture);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureToLayout(IntPtr cmd, uint texture, uint newLayout);
        #endregion

        #region Swapchain
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateSwapchain(IntPtr device, IntPtr windowHandle, uint width, uint height, VkFormat format, bool vsync);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroySwapchain(IntPtr swapchain);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veAcquireNextImage(IntPtr swapchain);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePresentImage(IntPtr swapchain, IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veResizeSwapchain(IntPtr swapchain, uint width, uint height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetSwapchainSize(IntPtr swapchain, out uint width, out uint height);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetSwapchainFormat(IntPtr swapchain);
        #endregion

        #region Debug and Profiling
        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBeginDebugLabel(IntPtr cmd, IntPtr label, VEColor color);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veEndDebugLabel(IntPtr cmd);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veInsertDebugLabel(IntPtr cmd, IntPtr label, VEColor color);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetBufferDebugName(IntPtr device, ulong address, IntPtr name);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetTextureDebugName(IntPtr device, uint texture, IntPtr name);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetSamplerDebugName(IntPtr device, uint sampler, IntPtr name);
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
        internal static extern void vePrintDebugInfo(IntPtr device);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void vePrintRenderConfig(IntPtr config);

        [DllImport(Constants.LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veValidateRenderConfig(IntPtr config);
        #endregion        
    }
}