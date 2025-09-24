using System;
using System.Runtime.InteropServices;
using System.Text;
using VulkEase.Interop;

namespace VulkEase
{
    // Type aliases
    public using VEBufferAddress = System.UInt64;
    public using VETextureIndex = System.UInt32;
    public using VESamplerIndex = System.UInt32;
    public using VEConfigTypeFlags = System.UInt32;

    // Constants
    public static class VEConstants
    {
        public const ulong VE_INVALID_ADDRESS = 0UL;
        public const uint VE_INVALID_TEXTURE_INDEX = 0xFFFFFFFF;
        public const uint VE_INVALID_SAMPLER_INDEX = 0xFFFFFFFF;

        public const uint VULKEASE_VERSION_MAJOR = 2;
        public const uint VULKEASE_VERSION_MINOR = 0;
        public const uint VULKEASE_VERSION_PATCH = 0;

        public static uint VE_MAKE_VERSION(uint major, uint minor, uint patch) =>
            (((major) << 22) | ((minor) << 12) | (patch));

        public static readonly uint VULKEASE_API_VERSION_2_0 = VE_MAKE_VERSION(2, 0, 0);
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
        public static uint GetVersion() => VulkEaseInterop.veGetVersion();

        // Context and Device Management
        public static VEContext CreateContext(string applicationName)
        {
            var namePtr = StringToHGlobalAnsi(applicationName);
            try
            {
                return new VEContext { native = VulkEaseInterop.veCreateContext(applicationName) };
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static void DestroyContext(VEContext context)
        {
            VulkEaseInterop.veDestroyContext(context.native);
        }

        public static VEDevice CreateDevice(VEContext context)
        {
            return new VEDevice { native = VulkEaseInterop.veCreateDevice(context.native) };
        }

        public static void DestroyDevice(VEDevice device)
        {
            VulkEaseInterop.veDestroyDevice(device.native);
        }

        public static VEResult DeviceWaitIdle(VEDevice device)
        {
            return VulkEaseInterop.veDeviceWaitIdle(device.native);
        }

        public static string GetDeviceName(VEDevice device)
        {
            return PtrToStringAnsi(VulkEaseInterop.veGetDeviceName(device.native));
        }

        public static string GetDriverVersion(VEDevice device)
        {
            return PtrToStringAnsi(VulkEaseInterop.veGetDriverVersion(device.native));
        }

        public static uint GetVulkanVersion(VEDevice device)
        {
            return VulkEaseInterop.veGetVulkanVersion(device.native);
        }

        public static string GetLastError()
        {
            return PtrToStringAnsi(VulkEaseInterop.veGetLastError());
        }

        // Buffer Management
        public static VEBufferAddress CreateBuffer(VEDevice device, VEBufferDesc desc)
        {
            return VulkEaseInterop.veCreateBuffer(device.native, ref desc);
        }

        public static void DestroyBuffer(VEDevice device, VEBufferAddress address)
        {
            VulkEaseInterop.veDestroyBuffer(device.native, address);
        }

        public static VEResult MapBuffer(VEDevice device, VEBufferAddress address, out IntPtr mappedData)
        {
            return VulkEaseInterop.veMapBuffer(device.native, address, out mappedData);
        }

        public static void UnmapBuffer(VEDevice device, VEBufferAddress address)
        {
            VulkEaseInterop.veUnmapBuffer(device.native, address);
        }

        public static VEResult UpdateBuffer(VEDevice device, VEBufferAddress address, IntPtr data, UIntPtr size, UIntPtr offset = default)
        {
            return VulkEaseInterop.veUpdateBuffer(device.native, address, data, size, offset);
        }

        public static UIntPtr GetBufferSize(VEDevice device, VEBufferAddress address)
        {
            return VulkEaseInterop.veGetBufferSize(device.native, address);
        }

        public static VkBufferUsageFlags GetBufferUsage(VEDevice device, VEBufferAddress address)
        {
            return VulkEaseInterop.veGetBufferUsage(device.native, address);
        }

        // Convenience buffer functions
        public static VEBufferAddress CreateVertexBuffer(VEDevice device, IntPtr vertices, UIntPtr size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateVertexBuffer(device.native, vertices, size, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateIndexBuffer(VEDevice device, IntPtr indices, UIntPtr size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateIndexBuffer(device.native, indices, size, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateUniformBuffer(VEDevice device, UIntPtr size, bool persistentlyMapped, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateUniformBuffer(device.native, size, persistentlyMapped, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateStorageBuffer(VEDevice device, UIntPtr size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateStorageBuffer(device.native, size, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VEBufferAddress CreateIndirectBuffer(VEDevice device, UIntPtr size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateIndirectBuffer(device.native, size, namePtr);
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
            return VulkEaseInterop.veCreateTexture(device.native, ref desc);
        }

        public static void DestroyTexture(VEDevice device, VETextureIndex index)
        {
            VulkEaseInterop.veDestroyTexture(device.native, index);
        }

        public static VEResult GetTextureSize(VEDevice device, VETextureIndex index, out uint width, out uint height, out uint depth)
        {
            return VulkEaseInterop.veGetTextureSize(device.native, index, out width, out height, out depth);
        }

        public static VkFormat GetTextureFormat(VEDevice device, VETextureIndex index)
        {
            return VulkEaseInterop.veGetTextureFormat(device.native, index);
        }

        // Convenience texture functions
        public static VETextureIndex CreateTexture1D(VEDevice device, uint width, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateTexture1D(device.native, width, format, usage, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture2D(VEDevice device, uint width, uint height, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateTexture2D(device.native, width, height, format, usage, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture3D(VEDevice device, uint width, uint height, uint depth, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateTexture3D(device.native, width, height, depth, format, usage, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture2DArray(VEDevice device, uint width, uint height, uint layers, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateTexture2DArray(device.native, width, height, layers, format, usage, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTextureCube(VEDevice device, uint size, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateTextureCube(device.native, size, format, usage, namePtr);
            }
            finally
            {
                if (namePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(namePtr);
            }
        }

        public static VETextureIndex CreateTexture2DMultisample(VEDevice device, uint width, uint height, VkFormat format, VkSampleCountFlags sampleCount, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return VulkEaseInterop.veCreateTexture2DMultisample(device.native, width, height, format, sampleCount, usage, namePtr);
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
                return VulkEaseInterop.veLoadTexture(device.native, namePtr, usage, generateMips);
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
                return VulkEaseInterop.veLoadHDRTexture(device.native, namePtr, usage, generateMips);
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
                    return VulkEaseInterop.veLoadCubeTexture(device.native, arrayPtr, usage, generateMips);
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
            return VulkEaseInterop.veGenerateMipmaps(device.native, cmd.native, texture);
        }

        public static VEResult GenerateMipmapsImmediate(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseInterop.veGenerateMipmapsImmediate(device.native, texture);
        }

        public static VEResult SaveTexture(VEDevice device, VETextureIndex texture, string filename)
        {
            var namePtr = StringToHGlobalAnsi(filename);
            try
            {
                return VulkEaseInterop.veSaveTexture(device.native, texture, namePtr);
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
            return VulkEaseInterop.veCreateSampler(device.native, ref desc);
        }

        public static void DestroySampler(VEDevice device, VESamplerIndex index)
        {
            VulkEaseInterop.veDestroySampler(device.native, index);
        }

        public static VESamplerIndex CreateLinearSampler(VEDevice device)
        {
            return VulkEaseInterop.veCreateLinearSampler(device.native);
        }

        public static VESamplerIndex CreateNearestSampler(VEDevice device)
        {
            return VulkEaseInterop.veCreateNearestSampler(device.native);
        }

        public static VESamplerIndex CreateAnisotropicSampler(VEDevice device, float maxAnisotropy)
        {
            return VulkEaseInterop.veCreateAnisotropicSampler(device.native, maxAnisotropy);
        }

        public static VESamplerIndex CreateShadowSampler(VEDevice device)
        {
            return VulkEaseInterop.veCreateShadowSampler(device.native);
        }

        // Shader Objects
        public static VEShader CreateShaderFromSPIRV(VEDevice device, VkShaderStageFlags stage, uint[] code, string entryPoint, string debugName = null)
        {
            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                IntPtr codePtr = Marshal.AllocHGlobal(code.Length * sizeof(uint));
                try
                {
                    Marshal.Copy(Array.ConvertAll(code, x => (int)x), 0, codePtr, code.Length);
                    return new VEShader { native = VulkEaseInterop.veCreateShaderFromSPIRV(device.native, stage, codePtr, (UIntPtr)(code.Length * sizeof(uint)), entryPtr, namePtr) };
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
                return new VEShader { native = VulkEaseInterop.veCreateShaderFromGLSL(device.native, stage, sourcePtr, entryPtr, namePtr) };
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
                return new VEShader { native = VulkEaseInterop.veLoadShader(device.native, filenamePtr, stage, entryPtr, namePtr) };
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
            VulkEaseInterop.veDestroyShader(shader.native);
        }

        public static VEResult EnableShaderHotReload(VEShader shader, string sourceFile)
        {
            var sourcePtr = StringToHGlobalAnsi(sourceFile);
            try
            {
                return VulkEaseInterop.veEnableShaderHotReload(shader.native, sourcePtr);
            }
            finally
            {
                if (sourcePtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(sourcePtr);
            }
        }

        public static VEResult ReloadShader(VEShader shader)
        {
            return VulkEaseInterop.veReloadShader(shader.native);
        }

        public static VEShaderConfig CreateShaderConfig(VEDevice device, VEShaderConfigDesc desc)
        {
            return new VEShaderConfig { native = VulkEaseInterop.veCreateShaderConfig(device.native, ref desc) };
        }

        public static void DestroyShaderConfig(VEShaderConfig config)
        {
            VulkEaseInterop.veDestroyShaderConfig(config.native);
        }

        // Render Configuration Management
        public static VERenderConfig CreateRenderConfig(VEDevice device, VERenderConfigDesc desc)
        {
            return new VERenderConfig { native = VulkEaseInterop.veCreateRenderConfig(device.native, ref desc) };
        }

        public static void DestroyRenderConfig(VERenderConfig config)
        {
            VulkEaseInterop.veDestroyRenderConfig(config.native);
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
                    native = VulkEaseInterop.veCreateVertexConfig(device.native,
                        (uint)(bindings?.Length ?? 0), bindingsPtr,
                        (uint)(attributes?.Length ?? 0), attributesPtr)
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
            VulkEaseInterop.veDestroyVertexConfig(config.native);
        }

        // Default configurations
        public static VERasterConfig DefaultRasterConfig() => VulkEaseInterop.veDefaultRasterConfig();
        public static VEDepthConfig DefaultDepthConfig() => VulkEaseInterop.veDefaultDepthConfig();
        public static VEBlendConfig DefaultOpaqueBlendConfig() => VulkEaseInterop.veDefaultOpaqueBlendConfig();
        public static VEBlendConfig DefaultAlphaBlendConfig() => VulkEaseInterop.veDefaultAlphaBlendConfig();
        public static VEBlendConfig DefaultAdditiveBlendConfig() => VulkEaseInterop.veDefaultAdditiveBlendConfig();
        public static VEMultisampleConfig DefaultMultisampleConfig() => VulkEaseInterop.veDefaultMultisampleConfig();
        public static VEVertexInputConfig DefaultVertexInputConfig() => VulkEaseInterop.veDefaultVertexInputConfig();

        // Common render configurations
        public static VERenderConfig CreateOpaqueRenderConfig(VEDevice device, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseInterop.veCreateOpaqueRenderConfig(device.native, namePtr) };
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
                return new VERenderConfig { native = VulkEaseInterop.veCreateTransparentRenderConfig(device.native, namePtr) };
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
                return new VERenderConfig { native = VulkEaseInterop.veCreateWireframeRenderConfig(device.native, namePtr) };
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
                return new VERenderConfig { native = VulkEaseInterop.veCreateShadowRenderConfig(device.native, namePtr) };
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
                return new VERenderConfig { native = VulkEaseInterop.veCreateUIRenderConfig(device.native, namePtr) };
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
            return new VERenderConfig { native = VulkEaseInterop.veCreateConfigVariant(baseConfig.native, ref overrides) };
        }

        public static VERenderConfig CloneRenderConfig(VERenderConfig config, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
            try
            {
                return new VERenderConfig { native = VulkEaseInterop.veCloneRenderConfig(config.native, namePtr) };
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
            return new VECommandBuffer { native = VulkEaseInterop.veBeginCommandBuffer(device.native) };
        }

        public static VEResult SubmitCommandBuffer(VECommandBuffer cmd, bool waitForCompletion = false)
        {
            return VulkEaseInterop.veSubmitCommandBuffer(cmd.native, waitForCompletion);
        }

        public static void BeginRendering(VECommandBuffer cmd, VERenderingInfo renderingInfo)
        {
            VulkEaseInterop.veBeginRendering(cmd.native, ref renderingInfo);
        }

        public static void EndRendering(VECommandBuffer cmd)
        {
            VulkEaseInterop.veEndRendering(cmd.native);
        }

        public static void ApplyRenderConfig(VECommandBuffer cmd, VERenderConfig config)
        {
            VulkEaseInterop.veApplyRenderConfig(cmd.native, config.native);
        }

        // Shader binding
        public static void BindShader(VECommandBuffer cmd, VEShader shader)
        {
            VulkEaseInterop.veBindShader(cmd.native, shader.native);
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

                VulkEaseInterop.veBindShaders(cmd.native, (uint)(shaders?.Length ?? 0), shadersPtr);
            }
            finally
            {
                if (shadersPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(shadersPtr);
            }
        }

        public static void BindShaderConfig(VECommandBuffer cmd, VEShaderConfig config)
        {
            VulkEaseInterop.veBindShaderConfig(cmd.native, config.native);
        }

        public static void UnbindShaderStage(VECommandBuffer cmd, VkShaderStageFlags stage)
        {
            VulkEaseInterop.veUnbindShaderStage(cmd.native, stage);
        }

        // Dynamic state
        public static void SetViewport(VECommandBuffer cmd, float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f)
        {
            VulkEaseInterop.veSetViewport(cmd.native, x, y, width, height, minDepth, maxDepth);
        }

        public static void SetScissor(VECommandBuffer cmd, int x, int y, uint width, uint height)
        {
            VulkEaseInterop.veSetScissor(cmd.native, x, y, width, height);
        }

        // State overrides
        public static void OverrideRasterState(VECommandBuffer cmd, VERasterConfig raster)
        {
            VulkEaseInterop.veOverrideRasterState(cmd.native, ref raster);
        }

        public static void OverrideDepthState(VECommandBuffer cmd, VEDepthConfig depth)
        {
            VulkEaseInterop.veOverrideDepthState(cmd.native, ref depth);
        }

        public static void OverrideBlendState(VECommandBuffer cmd, VEBlendConfig blend)
        {
            VulkEaseInterop.veOverrideBlendState(cmd.native, ref blend);
        }

        // Quick toggles
        public static void SetWireframe(VECommandBuffer cmd, bool enabled)
        {
            VulkEaseInterop.veSetWireframe(cmd.native, enabled);
        }

        public static void SetAlphaBlending(VECommandBuffer cmd, bool enabled)
        {
            VulkEaseInterop.veSetAlphaBlending(cmd.native, enabled);
        }

        public static void SetDepthTesting(VECommandBuffer cmd, bool testEnabled, bool writeEnabled)
        {
            VulkEaseInterop.veSetDepthTesting(cmd.native, testEnabled, writeEnabled);
        }

        public static void SetCulling(VECommandBuffer cmd, VkCullModeFlags cullMode)
        {
            VulkEaseInterop.veSetCulling(cmd.native, cullMode);
        }

        // Push constants and binding
        public static void PushConstants<T>(VECommandBuffer cmd, T data, UIntPtr offset = default) where T : struct
        {
            IntPtr dataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<T>());
            try
            {
                Marshal.StructureToPtr(data, dataPtr, false);
                VulkEaseInterop.vePushConstants(cmd.native, dataPtr, (UIntPtr)Marshal.SizeOf<T>(), offset);
            }
            finally
            {
                Marshal.FreeHGlobal(dataPtr);
            }
        }

        public static void BindIndexBuffer(VECommandBuffer cmd, VEBufferAddress indexBuffer, uint offset, VkIndexType format)
        {
            VulkEaseInterop.veBindIndexBuffer(cmd.native, indexBuffer, offset, format);
        }

        // Drawing
        public static void Draw(VECommandBuffer cmd, uint vertexCount, uint instanceCount = 1, uint firstVertex = 0, uint firstInstance = 0)
        {
            VulkEaseInterop.veDraw(cmd.native, vertexCount, instanceCount, firstVertex, firstInstance);
        }

        public static void DrawIndexed(VECommandBuffer cmd, uint indexCount, uint instanceCount = 1, uint firstIndex = 0, int vertexOffset = 0, uint firstInstance = 0)
        {
            VulkEaseInterop.veDrawIndexed(cmd.native, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }

        public static void DrawIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset, uint drawCount, uint stride)
        {
            VulkEaseInterop.veDrawIndirect(cmd.native, indirectBuffer, offset, drawCount, stride);
        }

        public static void DrawIndexedIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset, uint drawCount, uint stride)
        {
            VulkEaseInterop.veDrawIndexedIndirect(cmd.native, indirectBuffer, offset, drawCount, stride);
        }

        public static void DrawIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr indirectOffset, VEBufferAddress countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride)
        {
            VulkEaseInterop.veDrawIndirectCount(cmd.native, indirectBuffer, indirectOffset, countBuffer, countOffset, maxDrawCount, stride);
        }

        public static void DrawIndexedIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr indirectOffset, VEBufferAddress countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride)
        {
            VulkEaseInterop.veDrawIndexedIndirectCount(cmd.native, indirectBuffer, indirectOffset, countBuffer, countOffset, maxDrawCount, stride);
        }

        // Compute
        public static void Dispatch(VECommandBuffer cmd, uint groupCountX, uint groupCountY, uint groupCountZ)
        {
            VulkEaseInterop.veDispatch(cmd.native, groupCountX, groupCountY, groupCountZ);
        }

        public static void DispatchIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset)
        {
            VulkEaseInterop.veDispatchIndirect(cmd.native, indirectBuffer, offset);
        }

        // Barriers
        public static void BarrierVertexToFragment(VECommandBuffer cmd) => VulkEaseInterop.veBarrierVertexToFragment(cmd.native);
        public static void BarrierComputeToVertex(VECommandBuffer cmd) => VulkEaseInterop.veBarrierComputeToVertex(cmd.native);
        public static void BarrierComputeToCompute(VECommandBuffer cmd) => VulkEaseInterop.veBarrierComputeToCompute(cmd.native);
        public static void BarrierGraphicsToPresent(VECommandBuffer cmd) => VulkEaseInterop.veBarrierGraphicsToPresent(cmd.native);

        // Texture transitions
        public static void TransitionTexture(VECommandBuffer cmd, VETextureIndex texture, uint oldLayout, uint newLayout)
        {
            VulkEaseInterop.veTransitionTexture(cmd.native, texture, oldLayout, newLayout);
        }

        public static void TransitionTextureForShaderRead(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseInterop.veTransitionTextureForShaderRead(cmd.native, texture);

        public static void TransitionTextureForColorAttachment(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseInterop.veTransitionTextureForColorAttachment(cmd.native, texture);

        public static void TransitionTextureForDepthAttachment(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseInterop.veTransitionTextureForDepthAttachment(cmd.native, texture);

        public static void TransitionTextureForTransferSrc(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseInterop.veTransitionTextureForTransferSrc(cmd.native, texture);

        public static void TransitionTextureForTransferDst(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseInterop.veTransitionTextureForTransferDst(cmd.native, texture);

        public static void TransitionTextureForPresent(VECommandBuffer cmd, VETextureIndex texture) =>
            VulkEaseInterop.veTransitionTextureForPresent(cmd.native, texture);

        public static void TransitionTextureToLayout(VECommandBuffer cmd, VETextureIndex texture, uint newLayout)
        {
            VulkEaseInterop.veTransitionTextureToLayout(cmd.native, texture, newLayout);
        }

        // Swapchain
        public static VESwapchain CreateSwapchain(VEDevice device, IntPtr windowHandle, uint width, uint height, VkFormat format, bool vsync)
        {
            return new VESwapchain { native = VulkEaseInterop.veCreateSwapchain(device.native, windowHandle, width, height, format, vsync) };
        }

        public static void DestroySwapchain(VESwapchain swapchain)
        {
            VulkEaseInterop.veDestroySwapchain(swapchain.native);
        }

        public static VETextureIndex AcquireNextImage(VESwapchain swapchain)
        {
            return VulkEaseInterop.veAcquireNextImage(swapchain.native);
        }

        public static VEResult PresentImage(VESwapchain swapchain, VECommandBuffer cmd)
        {
            return VulkEaseInterop.vePresentImage(swapchain.native, cmd.native);
        }

        public static VEResult ResizeSwapchain(VESwapchain swapchain, uint width, uint height)
        {
            return VulkEaseInterop.veResizeSwapchain(swapchain.native, width, height);
        }

        public static VEResult GetSwapchainSize(VESwapchain swapchain, out uint width, out uint height)
        {
            return VulkEaseInterop.veGetSwapchainSize(swapchain.native, out width, out height);
        }

        public static VkFormat GetSwapchainFormat(VESwapchain swapchain)
        {
            return VulkEaseInterop.veGetSwapchainFormat(swapchain.native);
        }

        // Debug and Profiling
        public static void BeginDebugLabel(VECommandBuffer cmd, string label, VEColor color)
        {
            var labelPtr = StringToHGlobalAnsi(label);
            try
            {
                VulkEaseInterop.veBeginDebugLabel(cmd.native, labelPtr, color);
            }
            finally
            {
                if (labelPtr != IntPtr.Zero)
                    Marshal.FreeHGlobal(labelPtr);
            }
        }

        public static void EndDebugLabel(VECommandBuffer cmd)
        {
            VulkEaseInterop.veEndDebugLabel(cmd.native);
        }

        public static void InsertDebugLabel(VECommandBuffer cmd, string label, VEColor color)
        {
            var labelPtr = StringToHGlobalAnsi(label);
            try
            {
                VulkEaseInterop.veInsertDebugLabel(cmd.native, labelPtr, color);
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
                return VulkEaseInterop.veSetBufferDebugName(device.native, address, namePtr);
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
                return VulkEaseInterop.veSetTextureDebugName(device.native, texture, namePtr);
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
                return VulkEaseInterop.veSetSamplerDebugName(device.native, sampler, namePtr);
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
            return VulkEaseInterop.veGetPerformanceStats(device.native, out stats);
        }

        public static VEResult GetMemoryStats(VEDevice device, out VEMemoryStats stats)
        {
            return VulkEaseInterop.veGetMemoryStats(device.native, out stats);
        }

        public static VEResult GetRenderConfigStats(VEDevice device, out VERenderConfigStats stats)
        {
            return VulkEaseInterop.veGetRenderConfigStats(device.native, out stats);
        }

        // Debug information
        public static void PrintDebugInfo(VEDevice device)
        {
            VulkEaseInterop.vePrintDebugInfo(device.native);
        }

        public static void PrintRenderConfig(VERenderConfig config)
        {
            VulkEaseInterop.vePrintRenderConfig(config.native);
        }

        public static VEResult ValidateRenderConfig(VERenderConfig config)
        {
            return VulkEaseInterop.veValidateRenderConfig(config.native);
        }
    }
}