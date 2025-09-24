using System;
using System.Runtime.InteropServices;

namespace VulkEase.Interop
{
    internal static class VulkEaseInterop
    {
        private const string LibraryName = "vulkease";

        // Context and Device Management
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veGetVersion();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateContext([MarshalAs(UnmanagedType.LPStr)] string applicationName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyContext(IntPtr context);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateDevice(IntPtr context);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyDevice(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veDeviceWaitIdle(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetDeviceName(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetDriverVersion(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veGetVulkanVersion(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veGetLastError();

        // Buffer Management
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateBuffer(IntPtr device, ref VEBufferDesc desc);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyBuffer(IntPtr device, ulong address);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veMapBuffer(IntPtr device, ulong address, out IntPtr mappedData);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veUnmapBuffer(IntPtr device, ulong address);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veUpdateBuffer(IntPtr device, ulong address, IntPtr data, UIntPtr size, UIntPtr offset);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UIntPtr veGetBufferSize(IntPtr device, ulong address);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkBufferUsageFlags veGetBufferUsage(IntPtr device, ulong address);

        // Convenience buffer functions
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateVertexBuffer(IntPtr device, IntPtr vertices, UIntPtr size, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateIndexBuffer(IntPtr device, IntPtr indices, UIntPtr size, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateUniformBuffer(IntPtr device, UIntPtr size, bool persistentlyMapped, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateStorageBuffer(IntPtr device, UIntPtr size, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong veCreateIndirectBuffer(IntPtr device, UIntPtr size, IntPtr debugName);

        // Texture Management
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture(IntPtr device, ref VETextureDesc desc);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyTexture(IntPtr device, uint index);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetTextureSize(IntPtr device, uint index, out uint width, out uint height, out uint depth);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetTextureFormat(IntPtr device, uint index);

        // Convenience texture functions
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture1D(IntPtr device, uint width, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture2D(IntPtr device, uint width, uint height, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture3D(IntPtr device, uint width, uint height, uint depth, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture2DArray(IntPtr device, uint width, uint height, uint layers, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTextureCube(IntPtr device, uint size, VkFormat format, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateTexture2DMultisample(IntPtr device, uint width, uint height, VkFormat format, VkSampleCountFlags sampleCount, VkImageUsageFlags usage, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veLoadTexture(IntPtr device, IntPtr filename, VkImageUsageFlags usage, bool generateMips);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veLoadHDRTexture(IntPtr device, IntPtr filename, VkImageUsageFlags usage, bool generateMips);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veLoadCubeTexture(IntPtr device, IntPtr filenames, VkImageUsageFlags usage, bool generateMips);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGenerateMipmaps(IntPtr device, IntPtr cmd, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGenerateMipmapsImmediate(IntPtr device, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSaveTexture(IntPtr device, uint texture, IntPtr filename);

        // Sampler Management
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateSampler(IntPtr device, ref VESamplerDesc desc);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroySampler(IntPtr device, uint index);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateLinearSampler(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateNearestSampler(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateAnisotropicSampler(IntPtr device, float maxAnisotropy);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veCreateShadowSampler(IntPtr device);

        // Shader Objects
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShaderFromSPIRV(IntPtr device, VkShaderStageFlags stage, IntPtr code, UIntPtr codeSize, IntPtr entryPoint, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShaderFromGLSL(IntPtr device, VkShaderStageFlags stage, IntPtr source, IntPtr entryPoint, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veLoadShader(IntPtr device, IntPtr filename, VkShaderStageFlags stage, IntPtr entryPoint, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyShader(IntPtr shader);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veEnableShaderHotReload(IntPtr shader, IntPtr sourceFile);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veReloadShader(IntPtr shader);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShaderConfig(IntPtr device, ref VEShaderConfigDesc desc);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyShaderConfig(IntPtr config);

        // Render Configuration Management
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateRenderConfig(IntPtr device, ref VERenderConfigDesc desc);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyRenderConfig(IntPtr config);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateVertexConfig(IntPtr device, uint bindingCount, IntPtr bindings, uint attributeCount, IntPtr attributes);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroyVertexConfig(IntPtr config);

        // Default configurations
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VERasterConfig veDefaultRasterConfig();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEDepthConfig veDefaultDepthConfig();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEBlendConfig veDefaultOpaqueBlendConfig();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEBlendConfig veDefaultAlphaBlendConfig();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEBlendConfig veDefaultAdditiveBlendConfig();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEMultisampleConfig veDefaultMultisampleConfig();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEVertexInputConfig veDefaultVertexInputConfig();

        // Common render configurations
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateOpaqueRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateTransparentRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateWireframeRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateShadowRenderConfig(IntPtr device, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateUIRenderConfig(IntPtr device, IntPtr debugName);

        // Configuration composition
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateConfigVariant(IntPtr baseConfig, ref VERenderConfigDesc overrides);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veMergeRenderConfigs(IntPtr device, uint configCount, IntPtr configs, IntPtr debugName);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCloneRenderConfig(IntPtr config, IntPtr debugName);

        // Command Buffer and Rendering
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veBeginCommandBuffer(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSubmitCommandBuffer(IntPtr cmd, bool waitForCompletion);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBeginRendering(IntPtr cmd, ref VERenderingInfo renderingInfo);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veEndRendering(IntPtr cmd);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veApplyRenderConfig(IntPtr cmd, IntPtr config);

        // Shader binding
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindShader(IntPtr cmd, IntPtr shader);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindShaders(IntPtr cmd, uint shaderCount, IntPtr shaders);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindShaderConfig(IntPtr cmd, IntPtr config);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veUnbindShaderStage(IntPtr cmd, VkShaderStageFlags stage);

        // Dynamic state
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetViewport(IntPtr cmd, float x, float y, float width, float height, float minDepth, float maxDepth);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetScissor(IntPtr cmd, int x, int y, uint width, uint height);

        // State overrides
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veOverrideRasterState(IntPtr cmd, ref VERasterConfig raster);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veOverrideDepthState(IntPtr cmd, ref VEDepthConfig depth);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veOverrideBlendState(IntPtr cmd, ref VEBlendConfig blend);

        // Quick toggles
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetWireframe(IntPtr cmd, bool enabled);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetAlphaBlending(IntPtr cmd, bool enabled);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetDepthTesting(IntPtr cmd, bool testEnabled, bool writeEnabled);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veSetCulling(IntPtr cmd, VkCullModeFlags cullMode);

        // Push constants and binding
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void vePushConstants(IntPtr cmd, IntPtr data, UIntPtr size, UIntPtr offset);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBindIndexBuffer(IntPtr cmd, ulong indexBuffer, uint offset, VkIndexType format);

        // Drawing
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDraw(IntPtr cmd, uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndexed(IntPtr cmd, uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndirect(IntPtr cmd, ulong indirectBuffer, UIntPtr offset, uint drawCount, uint stride);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndexedIndirect(IntPtr cmd, ulong indirectBuffer, UIntPtr offset, uint drawCount, uint stride);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndirectCount(IntPtr cmd, ulong indirectBuffer, UIntPtr indirectOffset, ulong countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDrawIndexedIndirectCount(IntPtr cmd, ulong indirectBuffer, UIntPtr indirectOffset, ulong countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride);

        // Compute
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDispatch(IntPtr cmd, uint groupCountX, uint groupCountY, uint groupCountZ);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDispatchIndirect(IntPtr cmd, ulong indirectBuffer, UIntPtr offset);

        // Barriers
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierVertexToFragment(IntPtr cmd);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierComputeToVertex(IntPtr cmd);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierComputeToCompute(IntPtr cmd);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBarrierGraphicsToPresent(IntPtr cmd);

        // Texture transitions
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTexture(IntPtr cmd, uint texture, uint oldLayout, uint newLayout);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForShaderRead(IntPtr cmd, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForColorAttachment(IntPtr cmd, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForDepthAttachment(IntPtr cmd, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForTransferSrc(IntPtr cmd, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForTransferDst(IntPtr cmd, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureForPresent(IntPtr cmd, uint texture);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veTransitionTextureToLayout(IntPtr cmd, uint texture, uint newLayout);

        // Swapchain
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr veCreateSwapchain(IntPtr device, IntPtr windowHandle, uint width, uint height, VkFormat format, bool vsync);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veDestroySwapchain(IntPtr swapchain);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint veAcquireNextImage(IntPtr swapchain);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult vePresentImage(IntPtr swapchain, IntPtr cmd);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veResizeSwapchain(IntPtr swapchain, uint width, uint height);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetSwapchainSize(IntPtr swapchain, out uint width, out uint height);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VkFormat veGetSwapchainFormat(IntPtr swapchain);

        // Debug and Profiling
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veBeginDebugLabel(IntPtr cmd, IntPtr label, VEColor color);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veEndDebugLabel(IntPtr cmd);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void veInsertDebugLabel(IntPtr cmd, IntPtr label, VEColor color);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetBufferDebugName(IntPtr device, ulong address, IntPtr name);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetTextureDebugName(IntPtr device, uint texture, IntPtr name);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veSetSamplerDebugName(IntPtr device, uint sampler, IntPtr name);

        // Statistics
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetPerformanceStats(IntPtr device, out VEPerformanceStats stats);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetMemoryStats(IntPtr device, out VEMemoryStats stats);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veGetRenderConfigStats(IntPtr device, out VERenderConfigStats stats);

        // Debug information
        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void vePrintDebugInfo(IntPtr device);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void vePrintRenderConfig(IntPtr config);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern VEResult veValidateRenderConfig(IntPtr config);
    }
}