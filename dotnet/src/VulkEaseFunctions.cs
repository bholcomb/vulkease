using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace VulkEase
{
    // Constants
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
    /// VulkEase main API class - Modern Vulkan 1.3+ Bindless Graphics API
    /// </summary>
    public static class VulkEase
    {
        // Utility method for string marshaling
        private static IntPtr StringToHGlobalAnsi(string str)
        {
            return string.IsNullOrEmpty(str) ? IntPtr.Zero : Marshal.StringToHGlobalAnsi(str);
        }

        private static string PtrToStringAnsi(IntPtr ptr)
        {
            return ptr == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(ptr);
        }

        // Version and API Information
        public static UInt32 GetVersion() => VulkEaseDll.veGetVersion();

        // Context and Device Management
        public static VEContext CreateContext(string applicationName)
        {
            var namePtr = StringToHGlobalAnsi(applicationName);
            try
            {
                return new VEContext { native = VulkEaseDll.veCreateContext(applicationName) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static void DestroyContext(VEContext context)
        {
            VulkEaseDll.veDestroyContext(context.native);
        }

        public static VEDevice CreateDevice(VEContext context)
        {
            return new VEDevice { native = VulkEaseDll.veCreateDevice(context.native) };
        }

        public static void DestroyDevice(VEDevice device)
        {
            VulkEaseDll.veDestroyDevice(device.native);
        }

        public static VEResult DeviceWaitIdle(VEDevice device)
        {
            return VulkEaseDll.veDeviceWaitIdle(device.native);
        }

        public static string GetDeviceName(VEDevice device)
        {
            return PtrToStringAnsi(VulkEaseDll.veGetDeviceName(device.native));
        }

        public static string GetDriverVersion(VEDevice device)
        {
            return PtrToStringAnsi(VulkEaseDll.veGetDriverVersion(device.native));
        }

        public static UInt32 GetVulkanVersion(VEDevice device)
        {
            return VulkEaseDll.veGetVulkanVersion(device.native);
        }

        public static string GetLastError()
        {
            return PtrToStringAnsi(VulkEaseDll.veGetLastError());
        }

        // Buffer Management
        public static VEBufferAddress CreateBuffer(VEDevice device, VEBufferDesc desc)
        {
            return new VEBufferAddress { native = VulkEaseDll.veCreateBuffer(device.native, ref desc)};
        }

        public static void DestroyBuffer(VEDevice device, VEBufferAddress address)
        {
            VulkEaseDll.veDestroyBuffer(device.native, address.native);
        }

        public static VEResult MapBuffer(VEDevice device, VEBufferAddress address, out IntPtr mappedData)
        {
            return VulkEaseDll.veMapBuffer(device.native, address.native, out mappedData);
        }

        public static void UnmapBuffer(VEDevice device, VEBufferAddress address)
        {
            VulkEaseDll.veUnmapBuffer(device.native, address.native);
        }

        public static VEResult UpdateBuffer(VEDevice device, VEBufferAddress address, IntPtr data, UInt64 size, UInt64 offset = default)
        {
            return VulkEaseDll.veUpdateBuffer(device.native, address.native, data, size, offset);
        }

        public static UIntPtr GetBufferSize(VEDevice device, VEBufferAddress address)
        {
            return VulkEaseDll.veGetBufferSize(device.native, address.native);
        }

        public static VkBufferUsageFlags GetBufferUsage(VEDevice device, VEBufferAddress address)
        {
            return VulkEaseDll.veGetBufferUsage(device.native, address.native);
        }

        // Convenience buffer functions
        public static VEBufferAddress CreateVertexBuffer(VEDevice device, IntPtr vertices, UInt64 size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VEBufferAddress { native = VulkEaseDll.veCreateVertexBuffer(device.native, vertices, size, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateIndexBuffer(VEDevice device, IntPtr indices, UInt64 size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VEBufferAddress { native = VulkEaseDll.veCreateIndexBuffer(device.native, indices, size, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateUniformBuffer(VEDevice device, UInt64 size, bool persistentlyMapped, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VEBufferAddress { native = VulkEaseDll.veCreateUniformBuffer(device.native, size, persistentlyMapped, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateStorageBuffer(VEDevice device, UInt64 size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VEBufferAddress { native = VulkEaseDll.veCreateStorageBuffer(device.native, size, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateIndirectBuffer(VEDevice device, UInt64 size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VEBufferAddress { native = VulkEaseDll.veCreateIndirectBuffer(device.native, size, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        // Texture Management
        public static VETextureIndex CreateTexture(VEDevice device, VETextureDesc desc)
        {
            return new VETextureIndex { native = VulkEaseDll.veCreateTexture(device.native, ref desc) };
        }

        public static void DestroyTexture(VEDevice device, VETextureIndex index)
        {
            VulkEaseDll.veDestroyTexture(device.native, index.native);
        }

        public static VEResult GetTextureSize(VEDevice device, VETextureIndex index, out UInt32 width, out UInt32 height, out UInt32 depth)
        {
            return VulkEaseDll.veGetTextureSize(device.native, index.native, out width, out height, out depth);
        }

        public static VkFormat GetTextureFormat(VEDevice device, VETextureIndex index)
        {
            return VulkEaseDll.veGetTextureFormat(device.native, index.native);
        }

        // Convenience texture functions
        public static VETextureIndex CreateTexture1D(VEDevice device, UInt32 width, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veCreateTexture1D(device.native, width, format, usage, namePtr)};
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture2D(VEDevice device, UInt32 width, UInt32 height, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veCreateTexture2D(device.native, width, height, format, usage, namePtr)};
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture3D(VEDevice device, UInt32 width, UInt32 height, UInt32 depth, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veCreateTexture3D(device.native, width, height, depth, format, usage, namePtr)};
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture2DArray(VEDevice device, UInt32 width, UInt32 height, UInt32 layers, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veCreateTexture2DArray(device.native, width, height, layers, format, usage, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTextureCube(VEDevice device, UInt32 size, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veCreateTextureCube(device.native, size, format, usage, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture2DMultisample(VEDevice device, UInt32 width, UInt32 height, VkFormat format, VkSampleCountFlags sampleCount, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veCreateTexture2DMultisample(device.native, width, height, format, sampleCount, usage, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex LoadTexture(VEDevice device, string filename, VkImageUsageFlags usage, bool generateMips)
        {
            var namePtr = StringToHGlobalAnsi(filename);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veLoadTexture(device.native, namePtr, usage, generateMips) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex LoadHDRTexture(VEDevice device, string filename, VkImageUsageFlags usage, bool generateMips)
        {
            var namePtr = StringToHGlobalAnsi(filename);
            try
            {
                return new VETextureIndex { native = VulkEaseDll.veLoadHDRTexture(device.native, namePtr, usage, generateMips) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

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
                    return new VETextureIndex { native = VulkEaseDll.veLoadCubeTexture(device.native, arrayPtr, usage, generateMips) };
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

        public static VEResult GenerateMipmaps(VEDevice device, VECommandBuffer cmd, VETextureIndex texture)
        {
            return VulkEaseDll.veGenerateMipmaps(device.native, cmd.native, texture.native);
        }

        public static VEResult GenerateMipmapsImmediate(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseDll.veGenerateMipmapsImmediate(device.native, texture.native);
        }

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

        // Sampler Management
        public static VESamplerIndex CreateSampler(VEDevice device, VESamplerDesc desc)
        {
            return new VESamplerIndex { native = VulkEaseDll.veCreateSampler(device.native, ref desc) };
        }

        public static void DestroySampler(VEDevice device, VESamplerIndex index)
        {
            VulkEaseDll.veDestroySampler(device.native, index.native);
        }

        public static VESamplerIndex CreateLinearSampler(VEDevice device)
        {
            return new VESamplerIndex { native = VulkEaseDll.veCreateLinearSampler(device.native) };
        }

        public static VESamplerIndex CreateNearestSampler(VEDevice device)
        {
            return new VESamplerIndex { native = VulkEaseDll.veCreateNearestSampler(device.native) };
        }

        public static VESamplerIndex CreateAnisotropicSampler(VEDevice device, float maxAnisotropy)
        {
            return new VESamplerIndex { native = VulkEaseDll.veCreateAnisotropicSampler(device.native, maxAnisotropy) };
        }

        public static VESamplerIndex CreateShadowSampler(VEDevice device)
        {
            return new VESamplerIndex { native = VulkEaseDll.veCreateShadowSampler(device.native) };
        }

        // Shader Objects
        public static VEShader CreateShaderFromSPIRV(VEDevice device, VkShaderStageFlags stage, UInt32[] code, string entryPoint, string debugName = null)
        {
            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                IntPtr codePtr = Marshal.AllocHGlobal(code.Length * sizeof(UInt32));
                try
                {
                    Marshal.Copy(Array.ConvertAll(code, x => (int)x), 0, codePtr, code.Length);
                    return new VEShader { native = VulkEaseDll.veCreateShaderFromSPIRV(device.native, stage, codePtr, (UIntPtr)(code.Length * sizeof(UInt32)), entryPtr, namePtr) };
                }
                finally
                {
                    Marshal.FreeHGlobal(codePtr);
                }
            }
            finally
            {
                if (entryPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(entryPtr);
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEShader CreateShaderFromGLSL(VEDevice device, VkShaderStageFlags stage, string source, string entryPoint, string debugName = null)
        {
            var sourcePtr = StringToHGlobalAnsi(source);
            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VEShader { native = VulkEaseDll.veCreateShaderFromGLSL(device.native, stage, sourcePtr, entryPtr, namePtr) };
            }
            finally
            {
                if (sourcePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(sourcePtr);
                if (entryPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(entryPtr);
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEShader LoadShader(VEDevice device, string filename, VkShaderStageFlags stage, string entryPoint, string debugName = null)
        {
            var filenamePtr = StringToHGlobalAnsi(filename);
            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VEShader { native = VulkEaseDll.veLoadShader(device.native, filenamePtr, stage, entryPtr, namePtr) };
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

        public static void DestroyShader(VEShader shader)
        {
            VulkEaseDll.veDestroyShader(shader.native);
        }

        public static VEResult EnableShaderHotReload(VEShader shader, string sourceFile)
        {
            var sourcePtr = StringToHGlobalAnsi(sourceFile);
            try
            {
                return VulkEaseDll.veEnableShaderHotReload(shader.native, sourcePtr);
            }
            finally
            {
                if (sourcePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(sourcePtr);
            }
        }

        public static VEResult ReloadShader(VEShader shader)
        {
            return VulkEaseDll.veReloadShader(shader.native);
        }

        public static VEShaderConfig CreateShaderConfig(VEDevice device, VEShaderConfigDesc desc)
        {
            return new VEShaderConfig { native = VulkEaseDll.veCreateShaderConfig(device.native, ref desc) };
        }

        public static void DestroyShaderConfig(VEShaderConfig config)
        {
            VulkEaseDll.veDestroyShaderConfig(config.native);
        }

        // Render Configuration Management
        public static VERenderConfig CreateRenderConfig(VEDevice device, VERenderConfigDesc desc)
        {
            return new VERenderConfig { native = VulkEaseDll.veCreateRenderConfig(device.native, ref desc) };
        }

        public static void DestroyRenderConfig(VERenderConfig config)
        {
            VulkEaseDll.veDestroyRenderConfig(config.native);
        }

        public static VEVertexConfig CreateVertexConfig(VEDevice device, VEVertexBinding[] bindings, VEVertexAttribute[] attributes)
        {
            IntPtr bindingsPtr = IntPtr.Zero;
            IntPtr attributesPtr = IntPtr.Zero;

            try
            {
                if (bindings != null && bindings.Length > 0)
                {
                    bindingsPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VEVertexBinding>() * bindings.Length);
                    for (int i = 0; i < bindings.Length; i++)
                    {
                        Marshal.StructureToPtr(bindings[i], bindingsPtr + i * Marshal.SizeOf<VEVertexBinding>(), false);
                    }
                }

                if (attributes != null && attributes.Length > 0)
                {
                    attributesPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VEVertexAttribute>() * attributes.Length);
                    for (int i = 0; i < attributes.Length; i++)
                    {
                        Marshal.StructureToPtr(attributes[i], attributesPtr + i * Marshal.SizeOf<VEVertexAttribute>(), false);
                    }
                }

                return new VEVertexConfig
                {
                    native = VulkEaseDll.veCreateVertexConfig(device.native,
                        (UInt32)(bindings?.Length ?? 0), bindingsPtr,
                        (UInt32)(attributes?.Length ?? 0), attributesPtr)
                };
            }
            finally
            {
                if (bindingsPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(bindingsPtr);
                if (attributesPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(attributesPtr);
            }
        }

        public static void DestroyVertexConfig(VEVertexConfig config)
        {
            VulkEaseDll.veDestroyVertexConfig(config.native);
        }

        // Default configurations
        public static VERasterConfig DefaultRasterConfig() => VulkEaseDll.veDefaultRasterConfig();
        public static VEDepthConfig DefaultDepthConfig() => VulkEaseDll.veDefaultDepthConfig();
        public static VEBlendConfig DefaultOpaqueBlendConfig() => VulkEaseDll.veDefaultOpaqueBlendConfig();
        public static VEBlendConfig DefaultAlphaBlendConfig() => VulkEaseDll.veDefaultAlphaBlendConfig();
        public static VEBlendConfig DefaultAdditiveBlendConfig() => VulkEaseDll.veDefaultAdditiveBlendConfig();
        public static VEMultisampleConfig DefaultMultisampleConfig() => VulkEaseDll.veDefaultMultisampleConfig();
        public static VEVertexInputConfig DefaultVertexInputConfig() => VulkEaseDll.veDefaultVertexInputConfig();

        // Common render configurations
        public static VERenderConfig CreateOpaqueRenderConfig(VEDevice device, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseDll.veCreateOpaqueRenderConfig(device.native, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VERenderConfig CreateTransparentRenderConfig(VEDevice device, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseDll.veCreateTransparentRenderConfig(device.native, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VERenderConfig CreateWireframeRenderConfig(VEDevice device, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseDll.veCreateWireframeRenderConfig(device.native, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VERenderConfig CreateShadowRenderConfig(VEDevice device, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseDll.veCreateShadowRenderConfig(device.native, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VERenderConfig CreateUIRenderConfig(VEDevice device, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseDll.veCreateUIRenderConfig(device.native, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        // Configuration composition
        public static VERenderConfig CreateConfigVariant(VERenderConfig baseConfig, VERenderConfigDesc overrides)
        {
            return new VERenderConfig { native = VulkEaseDll.veCreateConfigVariant(baseConfig.native, ref overrides) };
        }

        public static VERenderConfig CloneRenderConfig(VERenderConfig config, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseDll.veCloneRenderConfig(config.native, namePtr) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        // Command Buffer and Rendering
        public static VECommandBuffer BeginCommandBuffer(VEDevice device)
        {
            return new VECommandBuffer { native = VulkEaseDll.veBeginCommandBuffer(device.native) };
        }

        public static VEResult SubmitCommandBuffer(VECommandBuffer cmd, bool waitForCompletion = false)
        {
            return VulkEaseDll.veSubmitCommandBuffer(cmd.native, waitForCompletion);
        }

        public static void BeginRendering(VECommandBuffer cmd, VERenderingInfo renderingInfo)
        {
            // Pin color attachments array if it has any elements
            // VERenderingAttachment is a blittable struct, so we can pin the array directly
            IntPtr colorAttachmentsPtr = IntPtr.Zero;
            GCHandle colorAttachmentsHandle = default;
            
            if (renderingInfo.ColorAttachments != null && renderingInfo.ColorAttachments.Count > 0)
            {
                var colorAttachmentsArray = renderingInfo.ColorAttachments.ToArray();
                colorAttachmentsHandle = GCHandle.Alloc(colorAttachmentsArray, GCHandleType.Pinned);
                colorAttachmentsPtr = colorAttachmentsHandle.AddrOfPinnedObject();
            }

            // Handle optional depth attachment - pin the struct to get its address
            IntPtr depthAttachmentPtr = IntPtr.Zero;
            GCHandle depthAttachmentHandle = default;
            
            if (renderingInfo.DepthAttachment.HasValue)
            {
                var depthAttachment = renderingInfo.DepthAttachment.Value;
                depthAttachmentHandle = GCHandle.Alloc(depthAttachment, GCHandleType.Pinned);
                depthAttachmentPtr = depthAttachmentHandle.AddrOfPinnedObject();
            }

            // Handle optional stencil attachment - pin the struct to get its address
            IntPtr stencilAttachmentPtr = IntPtr.Zero;
            GCHandle stencilAttachmentHandle = default;
            
            if (renderingInfo.StencilAttachment.HasValue)
            {
                var stencilAttachment = renderingInfo.StencilAttachment.Value;
                stencilAttachmentHandle = GCHandle.Alloc(stencilAttachment, GCHandleType.Pinned);
                stencilAttachmentPtr = stencilAttachmentHandle.AddrOfPinnedObject();
            }

            try
            {
                // Create the internal struct with pointers
                var internalInfo = new VERenderingInfoInternal
                {
                    renderAreaX = renderingInfo.RenderAreaX,
                    renderAreaY = renderingInfo.RenderAreaY,
                    renderAreaWidth = renderingInfo.RenderAreaWidth,
                    renderAreaHeight = renderingInfo.RenderAreaHeight,
                    colorAttachmentCount = (uint)(renderingInfo.ColorAttachments?.Count ?? 0),
                    colorAttachments = colorAttachmentsPtr,
                    depthAttachment = depthAttachmentPtr,
                    stencilAttachment = stencilAttachmentPtr
                };

                // Call the native function
                VulkEaseDll.veBeginRendering(cmd.native, ref internalInfo);
            }
            finally
            {
                // Free all pinned handles
                if (colorAttachmentsHandle.IsAllocated)
                    colorAttachmentsHandle.Free();
                if (depthAttachmentHandle.IsAllocated)
                    depthAttachmentHandle.Free();
                if (stencilAttachmentHandle.IsAllocated)
                    stencilAttachmentHandle.Free();
            }
        }

        public static void EndRendering(VECommandBuffer cmd)
        {
            VulkEaseDll.veEndRendering(cmd.native);
        }

        public static void ApplyRenderConfig(VECommandBuffer cmd, VERenderConfig config)
        {
            VulkEaseDll.veApplyRenderConfig(cmd.native, config.native);
        }

        // Shader binding
        public static void BindShader(VECommandBuffer cmd, VEShader shader)
        {
            VulkEaseDll.veBindShader(cmd.native, shader.native);
        }

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

        public static void BindShaderConfig(VECommandBuffer cmd, VEShaderConfig config)
        {
            VulkEaseDll.veBindShaderConfig(cmd.native, config.native);
        }

        public static void UnbindShaderStage(VECommandBuffer cmd, VkShaderStageFlags stage)
        {
            VulkEaseDll.veUnbindShaderStage(cmd.native, stage);
        }

        // Dynamic state
        public static void SetViewport(VECommandBuffer cmd, float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f)
        {
            VulkEaseDll.veSetViewport(cmd.native, x, y, width, height, minDepth, maxDepth);
        }

        public static void SetScissor(VECommandBuffer cmd, int x, int y, UInt32 width, UInt32 height)
        {
            VulkEaseDll.veSetScissor(cmd.native, x, y, width, height);
        }

        // State overrides
        public static void OverrideRasterState(VECommandBuffer cmd, VERasterConfig raster)
        {
            VulkEaseDll.veOverrideRasterState(cmd.native, ref raster);
        }

        public static void OverrideDepthState(VECommandBuffer cmd, VEDepthConfig depth)
        {
            VulkEaseDll.veOverrideDepthState(cmd.native, ref depth);
        }

        public static void OverrideBlendState(VECommandBuffer cmd, VEBlendConfig blend)
        {
            VulkEaseDll.veOverrideBlendState(cmd.native, ref blend);
        }

        // Quick toggles
        public static void SetWireframe(VECommandBuffer cmd, bool enabled)
        {
            VulkEaseDll.veSetWireframe(cmd.native, enabled);
        }

        public static void SetAlphaBlending(VECommandBuffer cmd, bool enabled)
        {
            VulkEaseDll.veSetAlphaBlending(cmd.native, enabled);
        }

        public static void SetDepthTesting(VECommandBuffer cmd, bool testEnabled, bool writeEnabled)
        {
            VulkEaseDll.veSetDepthTesting(cmd.native, testEnabled, writeEnabled);
        }

        public static void SetCulling(VECommandBuffer cmd, VkCullModeFlags cullMode)
        {
            VulkEaseDll.veSetCulling(cmd.native, cullMode);
        }

        // Push constants and binding
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

        public static void BindIndexBuffer(VECommandBuffer cmd, VEBufferAddress indexBuffer, UInt32 offset, VkIndexType format)
        {
            VulkEaseDll.veBindIndexBuffer(cmd.native, indexBuffer.native, offset, format);
        }

        // Drawing
        public static void Draw(VECommandBuffer cmd, UInt32 vertexCount, UInt32 instanceCount = 1, UInt32 firstVertex = 0, UInt32 firstInstance = 0)
        {
            VulkEaseDll.veDraw(cmd.native, vertexCount, instanceCount, firstVertex, firstInstance);
        }

        public static void DrawIndexed(VECommandBuffer cmd, UInt32 indexCount, UInt32 instanceCount = 1, UInt32 firstIndex = 0, int vertexOffset = 0, UInt32 firstInstance = 0)
        {
            VulkEaseDll.veDrawIndexed(cmd.native, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }

        public static void DrawIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset, UInt32 drawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndirect(cmd.native, indirectBuffer.native, offset, drawCount, stride);
        }

        public static void DrawIndexedIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset, UInt32 drawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndexedIndirect(cmd.native, indirectBuffer.native, offset, drawCount, stride);
        }

        public static void DrawIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr indirectOffset, VEBufferAddress countBuffer, UInt64 countOffset, UInt32 maxDrawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndirectCount(cmd.native, indirectBuffer.native, indirectOffset, countBuffer.native, countOffset, maxDrawCount, stride);
        }

        public static void DrawIndexedIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr indirectOffset, VEBufferAddress countBuffer, UInt64 countOffset, UInt32 maxDrawCount, UInt32 stride)
        {
            VulkEaseDll.veDrawIndexedIndirectCount(cmd.native, indirectBuffer.native, indirectOffset, countBuffer.native, countOffset, maxDrawCount, stride);
        }

        // Compute
        public static void Dispatch(VECommandBuffer cmd, UInt32 groupCountX, UInt32 groupCountY, UInt32 groupCountZ)
        {
            VulkEaseDll.veDispatch(cmd.native, groupCountX, groupCountY, groupCountZ);
        }

        public static void DispatchIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset)
        {
            VulkEaseDll.veDispatchIndirect(cmd.native, indirectBuffer.native, offset);
        }

        // Barriers
        public static void BarrierVertexToFragment(VECommandBuffer cmd) => VulkEaseDll.veBarrierVertexToFragment(cmd.native);
        public static void BarrierComputeToVertex(VECommandBuffer cmd) => VulkEaseDll.veBarrierComputeToVertex(cmd.native);
        public static void BarrierComputeToCompute(VECommandBuffer cmd) => VulkEaseDll.veBarrierComputeToCompute(cmd.native);
        public static void BarrierGraphicsToPresent(VECommandBuffer cmd) => VulkEaseDll.veBarrierGraphicsToPresent(cmd.native);

        // Texture transitions
        public static void TransitionTexture(VECommandBuffer cmd, VETextureIndex texture, VkImageLayout oldLayout, VkImageLayout newLayout)
        {
            VulkEaseDll.veTransitionTexture(cmd.native, texture.native, oldLayout, newLayout);
        }

        public static void TransitionTextureForShaderRead(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForShaderRead(cmd.native, texture.native);

        public static void TransitionTextureForColorAttachment(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForColorAttachment(cmd.native, texture.native);

        public static void TransitionTextureForDepthAttachment(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForDepthAttachment(cmd.native, texture.native);

        public static void TransitionTextureForTransferSrc(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForTransferSrc(cmd.native, texture.native);

        public static void TransitionTextureForTransferDst(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForTransferDst(cmd.native, texture.native);

        public static void TransitionTextureForPresent(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseDll.veTransitionTextureForPresent(cmd.native, texture.native);

        public static void TransitionTextureToLayout(VECommandBuffer cmd, VETextureIndex texture, VkImageLayout newLayout)
        {
            VulkEaseDll.veTransitionTextureToLayout(cmd.native, texture.native, newLayout);
        }

        // Swapchain
        public static VESwapchain CreateSwapchain(VEDevice device, IntPtr windowHandle, UInt32 width, UInt32 height, VkFormat format, bool vsync)
        {
            return new VESwapchain { native = VulkEaseDll.veCreateSwapchain(device.native, windowHandle, width, height, format, vsync) };
        }

        public static void DestroySwapchain(VESwapchain swapchain)
        {
            VulkEaseDll.veDestroySwapchain(swapchain.native);
        }

        public static VETextureIndex AcquireNextImage(VESwapchain swapchain)
        {
            return new VETextureIndex { native = VulkEaseDll.veAcquireNextImage(swapchain.native) };
        }

        public static VEResult PresentImage(VESwapchain swapchain, VECommandBuffer cmd)
        {
            return  VulkEaseDll.vePresentImage(swapchain.native, cmd.native);
        }

        public static VEResult ResizeSwapchain(VESwapchain swapchain, UInt32 width, UInt32 height)
        {
            return VulkEaseDll.veResizeSwapchain(swapchain.native, width, height);
        }

        public static VEResult GetSwapchainSize(VESwapchain swapchain, out UInt32 width, out UInt32 height)
        {
            return VulkEaseDll.veGetSwapchainSize(swapchain.native, out width, out height);
        }

        public static VkFormat GetSwapchainFormat(VESwapchain swapchain)
        {
            return VulkEaseDll.veGetSwapchainFormat(swapchain.native);
        }

        // Debug and Profiling
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

        public static void EndDebugLabel(VECommandBuffer cmd)
        {
            VulkEaseDll.veEndDebugLabel(cmd.native);
        }

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

        // Statistics
        public static VEResult GetPerformanceStats(VEDevice device, out VEPerformanceStats stats)
        {
            return VulkEaseDll.veGetPerformanceStats(device.native, out stats);
        }

        public static VEResult GetMemoryStats(VEDevice device, out VEMemoryStats stats)
        {
            return VulkEaseDll.veGetMemoryStats(device.native, out stats);
        }

        public static VEResult GetRenderConfigStats(VEDevice device, out VERenderConfigStats stats)
        {
            return VulkEaseDll.veGetRenderConfigStats(device.native, out stats);
        }

        // Debug information
        public static void PrintDebugInfo(VEDevice device)
        {
            VulkEaseDll.vePrintDebugInfo(device.native);
        }

        public static void PrintRenderConfig(VERenderConfig config)
        {
            VulkEaseDll.vePrintRenderConfig(config.native);
        }

        public static VEResult ValidateRenderConfig(VERenderConfig config)
        {
            return VulkEaseDll.veValidateRenderConfig(config.native);
        }
    }
}