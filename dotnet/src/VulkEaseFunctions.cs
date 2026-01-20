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
        public static UInt32 GetVersion()
        {
            return VulkEaseDll.veGetVersion();
        }

        // Context and Device Management
        public static VEContext CreateContext(string applicationName, string[]? additionalInstanceExtensions = null)
        {
            IntPtr extensionsPtr = IntPtr.Zero;
            IntPtr[] extensionPtrs = null;
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

        public static void DestroyContext(VEContext context)
        {
            _ = VulkEaseDll.veDestroyContext(context.native);
        }

        public static VEResult EnumeratePhysicalDevices(VEContext context, out uint count)
        {
            return VulkEaseDll.veEnumeratePhysicalDevices(context.native, out count);
        }

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

        public static VEDevice CreateDevice(VEContext context, IntPtr preferredDevice = default, string[]? additionalDeviceExtensions = null)
        {
            IntPtr extensionsPtr = IntPtr.Zero;
            IntPtr[] extensionPtrs = null;
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

        public static void DestroyDevice(VEDevice device)
        {
            _ = VulkEaseDll.veDestroyDevice(device.native);
        }

        public static bool IsInstanceExtensionAvailable(string extensionName)
        {
            return VulkEaseDll.veIsInstanceExtensionAvailable(extensionName);
        }

        public static bool IsDeviceExtensionAvailable(VEContext context, string extensionName)
        {
            return VulkEaseDll.veIsDeviceExtensionAvailable(context.native, extensionName);
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

        // veGetLastError was removed from the native API. Use the message callback for diagnostics.

        // Vulkan Handle Accessors
        public static IntPtr GetVkInstance(VEContext context)
        {
            return VulkEaseDll.veGetVkInstance(context.native);
        }

        public static IntPtr GetVkPhysicalDevice(VEDevice device)
        {
            return VulkEaseDll.veGetVkPhysicalDevice(device.native);
        }

        public static IntPtr GetVkDevice(VEDevice device)
        {
            return VulkEaseDll.veGetVkDevice(device.native);
        }

        public static IntPtr GetVkGraphicsQueue(VEDevice device)
        {
            return VulkEaseDll.veGetVkGraphicsQueue(device.native);
        }

        public static IntPtr GetVkComputeQueue(VEDevice device)
        {
            return VulkEaseDll.veGetVkComputeQueue(device.native);
        }

        public static IntPtr GetVkTransferQueue(VEDevice device)
        {
            return VulkEaseDll.veGetVkTransferQueue(device.native);
        }

        public static IntPtr GetVkCommandBufferFence(VECommandBuffer cmd)
        {
            return VulkEaseDll.veGetVkCommandBufferFence(cmd.native);
        }

        // Buffer Management
        public static VEBufferAddress CreateBuffer(VEDevice device, VEBufferDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateBuffer(device.native, ref desc, out ulong address);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateBuffer failed: {result}");
            return new VEBufferAddress { native = address };
        }

        public static void DestroyBuffer(VEDevice device, VEBufferAddress address)
        {
            // Destruction is best-effort; ignore the error code for convenience.
            _ = VulkEaseDll.veDestroyBuffer(device.native, address.native);
        }

        public static VEResult MapBuffer(VEDevice device, VEBufferAddress address, out IntPtr mappedData)
        {
            return VulkEaseDll.veMapBuffer(device.native, address.native, out mappedData);
        }

        public static void UnmapBuffer(VEDevice device, VEBufferAddress address)
        {
            _ = VulkEaseDll.veUnmapBuffer(device.native, address.native);
        }

        public static VEResult UpdateBuffer(VEDevice device, VEBufferAddress address, IntPtr data, UInt64 size, UInt64 offset = default)
        {
            return VulkEaseDll.veUpdateBuffer(device.native, address.native, data, size, offset);
        }

        public static UIntPtr GetBufferSize(VEDevice device, VEBufferAddress address)
        {
            return new UIntPtr(VulkEaseDll.veGetBufferSize(device.native, address.native));
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

        public static VEBufferAddress CreateIndexBuffer(VEDevice device, IntPtr indices, UInt64 size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VEBufferAddress CreateUniformBuffer(VEDevice device, UInt64 size, bool persistentlyMapped, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VEBufferAddress CreateStorageBuffer(VEDevice device, UInt64 size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VEBufferAddress CreateIndirectBuffer(VEDevice device, UInt64 size, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        // Texture Management
        public static VETextureIndex CreateTexture(VEDevice device, VETextureDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateTexture(device.native, ref desc, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateTexture failed: {result}");
            return new VETextureIndex { native = index };
        }

        public static void DestroyTexture(VEDevice device, VETextureIndex index)
        {
            _ = VulkEaseDll.veDestroyTexture(device.native, index.native);
        }

        public static VkFormat GetTextureFormat(VEDevice device, VETextureIndex index)
        {
            return VulkEaseDll.veGetTextureFormat(device.native, index.native);
        }

        public static VkExtent3D GetTextureSize(VEDevice device, VETextureIndex index)
        {
            return VulkEaseDll.veGetTextureSize(device.native, index.native);
        }

        // Convenience texture functions
        public static VETextureIndex CreateTexture1D(VEDevice device, UInt32 width, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VETextureIndex CreateTexture2D(VEDevice device, UInt32 width, UInt32 height, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VETextureIndex CreateTexture3D(VEDevice device, UInt32 width, UInt32 height, UInt32 depth, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VETextureIndex CreateTexture2DArray(VEDevice device, UInt32 width, UInt32 height, UInt32 layers, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VETextureIndex CreateTextureCube(VEDevice device, UInt32 size, VkFormat format, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VETextureIndex CreateTexture2DMultisample(VEDevice device, UInt32 width, UInt32 height, VkFormat format, VkSampleCountFlags sampleCount, VkImageUsageFlags usage, string debugName = null)
        {
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static VEResult GenerateMipmaps(VEDevice device, VECommandBuffer cmd, VETextureIndex texture)
        {
            return VulkEaseDll.veGenerateMipmaps(device.native, cmd.native, texture.native);
        }

        public static VEResult GenerateMipmapsImmediate(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseDll.veGenerateMipmapsImmediate(device.native, texture.native);
        }

        // Host Image Copy Functions
        public static VEResult HostWriteTextureRegion(VEDevice device, VETextureIndex texture, IntPtr srcData,
            UIntPtr dataSize, UInt32 offsetX, UInt32 offsetY, UInt32 offsetZ,
            UInt32 width, UInt32 height, UInt32 depth)
        {
            return VulkEaseDll.veHostWriteTextureRegion(device.native, texture.native, srcData, dataSize,
                offsetX, offsetY, offsetZ, width, height, depth);
        }

        public static VEResult HostReadTextureRegion(VEDevice device, VETextureIndex texture, IntPtr dstData,
            UIntPtr dataSize, UInt32 offsetX, UInt32 offsetY, UInt32 offsetZ,
            UInt32 width, UInt32 height, UInt32 depth)
        {
            return VulkEaseDll.veHostReadTextureRegion(device.native, texture.native, dstData, dataSize,
                offsetX, offsetY, offsetZ, width, height, depth);
        }

        public static VEResult HostWriteTexture(VEDevice device, VETextureIndex texture, IntPtr srcData, UIntPtr dataSize)
        {
            return VulkEaseDll.veHostWriteTexture(device.native, texture.native, srcData, dataSize);
        }

        public static VEResult HostReadTexture(VEDevice device, VETextureIndex texture, IntPtr dstData, UIntPtr dataSize)
        {
            return VulkEaseDll.veHostReadTexture(device.native, texture.native, dstData, dataSize);
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
            VEResult result = VulkEaseDll.veCreateSampler(device.native, ref desc, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        public static void DestroySampler(VEDevice device, VESamplerIndex index)
        {
            _ = VulkEaseDll.veDestroySampler(device.native, index.native);
        }

        public static VESamplerIndex CreateLinearSampler(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateLinearSampler(device.native, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateLinearSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        public static VESamplerIndex CreateNearestSampler(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateNearestSampler(device.native, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateNearestSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        public static VESamplerIndex CreateAnisotropicSampler(VEDevice device, float maxAnisotropy)
        {
            VEResult result = VulkEaseDll.veCreateAnisotropicSampler(device.native, maxAnisotropy, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateAnisotropicSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        public static VESamplerIndex CreateShadowSampler(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateShadowSampler(device.native, out uint index);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateShadowSampler failed: {result}");
            return new VESamplerIndex { native = index };
        }

        // Shader Objects
        public static VEShader LoadShaderFromBuffer(VEDevice device, byte[] buffer, VkShaderStageFlags stage, string entryPoint, string debugName = null)
        {
            if (buffer == null || buffer.Length == 0)
            {
                throw new ArgumentException("Shader buffer must not be null or empty.", nameof(buffer));
            }

            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName);
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
                {
                    Marshal.FreeHGlobal(bufferPtr);
                }
                if (entryPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(entryPtr);
                }
                if (namePtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(namePtr);
                }
            }
        }
        public static VEShader LoadShaderFromFile(VEDevice device, string filename, VkShaderStageFlags stage, string entryPoint, string debugName = null)
        {
            var filenamePtr = StringToHGlobalAnsi(filename);
            var entryPtr = StringToHGlobalAnsi(entryPoint);
            var namePtr = StringToHGlobalAnsi(debugName);
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

        public static void DestroyShader(VEShader shader)
        {
            _ = VulkEaseDll.veDestroyShader(shader.native);
        }

        public static void SetShaderHotReloadEnabled(VEDevice device, bool enable)
        {
            VulkEaseDll.veSetShaderHotReloadEnabled(device.native, enable);
        }

        public static bool ShaderNeedsReload(VEShader shader)
        {
            return VulkEaseDll.veShaderNeedsReload(shader.native);
        }

        public static VEResult ReloadShader(VEShader shader)
        {
            return VulkEaseDll.veReloadShader(shader.native);
        }

        // Graphics Pipeline Management
        public static VEGraphicsPipeline CreateGraphicsPipeline(VEDevice device, VEGraphicsPipelineDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateGraphicsPipeline(device.native, ref desc, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateGraphicsPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        public static void DestroyGraphicsPipeline(VEGraphicsPipeline pipeline)
        {
            _ = VulkEaseDll.veDestroyGraphicsPipeline(pipeline.native);
        }

        public static VEGraphicsPipelineDesc DefaultGraphicsPipelineDesc()
        {
            return VulkEaseDll.veDefaultGraphicsPipelineDesc();
        }

        /// <summary>
        /// Creates an opaque graphics pipeline with depth testing enabled and no blending.
        /// </summary>
        public static VEGraphicsPipeline CreateOpaquePipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string debugName)
        {
            VEResult result = VulkEaseDll.veCreateOpaquePipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateOpaquePipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Creates a transparent graphics pipeline with alpha blending enabled.
        /// </summary>
        public static VEGraphicsPipeline CreateTransparentPipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string debugName)
        {
            VEResult result = VulkEaseDll.veCreateTransparentPipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateTransparentPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Creates an additive blending graphics pipeline.
        /// </summary>
        public static VEGraphicsPipeline CreateAdditivePipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string debugName)
        {
            VEResult result = VulkEaseDll.veCreateAdditivePipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateAdditivePipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Creates a shadow rendering graphics pipeline (depth-only).
        /// </summary>
        public static VEGraphicsPipeline CreateShadowPipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string debugName)
        {
            VEResult result = VulkEaseDll.veCreateShadowPipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateShadowPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        /// <summary>
        /// Creates a UI overlay graphics pipeline with transparency.
        /// </summary>
        public static VEGraphicsPipeline CreateUIOverlayPipeline(VEDevice device, VEShader vertexShader, VEShader fragmentShader, string debugName)
        {
            VEResult result = VulkEaseDll.veCreateUIOverlayPipeline(device.native, vertexShader.native, fragmentShader.native,
                IntPtr.Zero, 0, IntPtr.Zero, 0, debugName, out IntPtr pipelinePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateUIOverlayPipeline failed: {result}");
            return new VEGraphicsPipeline { native = pipelinePtr };
        }

        // Draw State Management
        public static VEDrawState CreateDrawState(VEDevice device, VEDrawStateDesc desc)
        {
            VEResult result = VulkEaseDll.veCreateDrawState(device.native, ref desc, out IntPtr drawStatePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateDrawState failed: {result}");
            return new VEDrawState { native = drawStatePtr };
        }

        public static void DestroyDrawState(VEDrawState drawState)
        {
            _ = VulkEaseDll.veDestroyDrawState(drawState.native);
        }

        public static VEDrawState CreateDefaultDrawState(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateDefaultDrawState(device.native, out IntPtr drawStatePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateDefaultDrawState failed: {result}");
            return new VEDrawState { native = drawStatePtr };
        }

        public static VEDrawState CreateWireframeDrawState(VEDevice device)
        {
            VEResult result = VulkEaseDll.veCreateWireframeDrawState(device.native, out IntPtr drawStatePtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veCreateWireframeDrawState failed: {result}");
            return new VEDrawState { native = drawStatePtr };
        }

        public static VEDrawStateDesc DefaultDrawStateDesc()
        {
            return VulkEaseDll.veDefaultDrawStateDesc();
        }

        public static VERenderingInfo CreateRenderingInfo(uint width, uint height)
        {
            return new VERenderingInfo(width, height);
        }

        // Command Buffer and Rendering
        public static VECommandBuffer BeginCommandBuffer(VEDevice device)
        {
            VEResult result = VulkEaseDll.veBeginCommandBuffer(device.native, out IntPtr cmdPtr);
            if (result != VEResult.VE_SUCCESS)
                throw new InvalidOperationException($"veBeginCommandBuffer failed: {result}");
            return new VECommandBuffer { native = cmdPtr };
        }

        public static VECommandBuffer BeginSecondaryCommandBuffer(VEDevice device, VESecondaryCommandBufferDesc desc)
        {
            // Ensure fixed-length array is initialized for marshalling
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

        public static VEResult PopulateSecondaryDescFromRenderingInfo(VEDevice device, VERenderingInfo renderingInfo, ref VESecondaryCommandBufferDesc desc)
        {
            if (desc.colorAttachmentFormats == null || desc.colorAttachmentFormats.Length != 8)
            {
                desc.colorAttachmentFormats = new VkFormat[8];
            }

            var internalInfo = renderingInfo.ToInternal();
            return VulkEaseDll.vePopulateSecondaryDescFromRenderingInfo(device.native, ref internalInfo, ref desc);
        }

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

        public static VEResult EndCommandBuffer(VECommandBuffer cmd)
        {
            return VulkEaseDll.veEndCommandBuffer(cmd.native);
        }

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

        // Vulkan escape hatch getters (VK_NULL_HANDLE on failure)
        public static IntPtr GetVkBufferFromAddress(VEDevice device, ulong address)
        {
            return VulkEaseDll.veGetVkBufferFromAddress(device.native, address);
        }

        public static IntPtr GetVkImageFromTexture(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseDll.veGetVkImageFromTexture(device.native, texture.native);
        }

        public static IntPtr GetVkImageViewFromTexture(VEDevice device, VETextureIndex texture)
        {
            return VulkEaseDll.veGetVkImageViewFromTexture(device.native, texture.native);
        }

        public static IntPtr GetVkSamplerFromIndex(VEDevice device, VESamplerIndex sampler)
        {
            return VulkEaseDll.veGetVkSamplerFromIndex(device.native, sampler.native);
        }

        public static IntPtr GetVkSwapchain(VESwapchain swapchain)
        {
            return VulkEaseDll.veGetVkSwapchain(swapchain.native);
        }

        public static IntPtr GetVkSwapchainImage(VESwapchain swapchain, uint imageIndex)
        {
            return VulkEaseDll.veGetVkSwapchainImage(swapchain.native, imageIndex);
        }

        public static IntPtr GetVkSwapchainImageView(VESwapchain swapchain, uint imageIndex)
        {
            return VulkEaseDll.veGetVkSwapchainImageView(swapchain.native, imageIndex);
        }

        // veSubmitCommandBufferEx was removed from the native API. Use SubmitCommandBuffer(...) instead.

        public static VEResult ResetCommandBuffer(VECommandBuffer cmd)
        {
            return VulkEaseDll.veResetCommandBuffer(cmd.native);
        }

        public static void ReleaseCommandBuffer(VECommandBuffer cmd)
        {
            VulkEaseDll.veReleaseCommandBuffer(cmd.native);
        }

        public static void BeginRendering(VECommandBuffer cmd, VERenderingInfo renderingInfo)
        {
            var internalInfo = renderingInfo.ToInternal();
            VulkEaseDll.veBeginRendering(cmd.native, ref internalInfo);
        }

        public static void EndRendering(VECommandBuffer cmd)
        {
            VulkEaseDll.veEndRendering(cmd.native);
        }

        public static void BindGraphicsPipeline(VECommandBuffer cmd, VEGraphicsPipeline pipeline)
        {
            VulkEaseDll.veBindGraphicsPipeline(cmd.native, pipeline.native);
        }

        public static void ApplyDrawState(VECommandBuffer cmd, VEDrawState drawState)
        {
            VulkEaseDll.veApplyDrawState(cmd.native, drawState.native);
        }

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
        public static VESwapchain CreateSwapchain(VEDevice device, VESwapchainDesc desc)
        {
            var namePtr = StringToHGlobalAnsi(desc.DebugName);
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

        public static void DestroySwapchain(VESwapchain swapchain)
        {
            _ = VulkEaseDll.veDestroySwapchain(swapchain.native);
        }

        public static VEResult PresentImage(VESwapchain swapchain, VECommandBuffer cmd, bool releaseCommandBuffer = true)
        {
            return VulkEaseDll.vePresentImage(swapchain.native, cmd.native, releaseCommandBuffer);
        }

        public static VEResult ResizeSwapchain(VESwapchain swapchain, UInt32 width, UInt32 height)
        {
            return VulkEaseDll.veResizeSwapchain(swapchain.native, width, height);
        }

        public static VkExtent2D GetSwapchainSize(VESwapchain swapchain)
        {
            return VulkEaseDll.veGetSwapchainSize(swapchain.native);
        }

        public static VkFormat GetSwapchainFormat(VESwapchain swapchain)
        {
            return VulkEaseDll.veGetSwapchainFormat(swapchain.native);
        }

        // Render Targets
        public static VERenderTarget CreateRenderTarget(VEDevice device, VERenderTargetDesc desc)
        {
            var namePtr = StringToHGlobalAnsi(desc.DebugName);
            try
            {
                var internalDesc = new VERenderTargetDescInternal
                {
                    width = desc.Width,
                    height = desc.Height,
                    colorFormat = desc.ColorFormat,
                    depthFormat = desc.DepthFormat,
                    sampleCount = desc.SampleCount,
                    hasResolveTarget = desc.HasResolveTarget,
                    debugName = namePtr
                };
                VEResult result = VulkEaseDll.veCreateRenderTarget(device.native, ref internalDesc, out IntPtr native);
                if (result != VEResult.VE_SUCCESS)
                    throw new InvalidOperationException($"veCreateRenderTarget failed: {result}");
                return new VERenderTarget { native = native };
            }
            finally
            {
                if (namePtr != IntPtr.Zero) Marshal.FreeHGlobal(namePtr);
            }
        }

        public static void DestroyRenderTarget(VERenderTarget renderTarget)
        {
            _ = VulkEaseDll.veDestroyRenderTarget(renderTarget.native);
        }

        public static VEResult ResizeRenderTarget(VERenderTarget renderTarget, UInt32 width, UInt32 height)
        {
            return VulkEaseDll.veResizeRenderTarget(renderTarget.native, width, height);
        }

        public static VETextureIndex GetRenderTargetColorTexture(VERenderTarget renderTarget)
        {
            return new VETextureIndex { native = VulkEaseDll.veGetRenderTargetColorTexture(renderTarget.native) };
        }

        public static VETextureIndex GetRenderTargetDepthTexture(VERenderTarget renderTarget)
        {
            return new VETextureIndex { native = VulkEaseDll.veGetRenderTargetDepthTexture(renderTarget.native) };
        }

        public static VETextureIndex GetRenderTargetResolveTexture(VERenderTarget renderTarget)
        {
            return new VETextureIndex { native = VulkEaseDll.veGetRenderTargetResolveTexture(renderTarget.native) };
        }

        public static VkExtent2D GetRenderTargetSize(VERenderTarget renderTarget)
        {
            return VulkEaseDll.veGetRenderTargetSize(renderTarget.native);
        }

        public static VkFormat GetRenderTargetColorFormat(VERenderTarget renderTarget)
        {
            return VulkEaseDll.veGetRenderTargetColorFormat(renderTarget.native);
        }

        public static VkFormat GetRenderTargetDepthFormat(VERenderTarget renderTarget)
        {
            return VulkEaseDll.veGetRenderTargetDepthFormat(renderTarget.native);
        }

        public static VEResult BlitToSwapchain(VECommandBuffer cmd, VERenderTarget renderTarget, VESwapchain swapchain, VkFilter filter)
        {
            return VulkEaseDll.veBlitToSwapchain(cmd.native, renderTarget.native, swapchain.native, filter);
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

        public static VEResult GetGraphicsPipelineStats(VEDevice device, out VEGraphicsPipelineStats stats)
        {
            return VulkEaseDll.veGetGraphicsPipelineStats(device.native, out stats);
        }

        // Debug information
        public static void PrintDebugInfo(VEDevice device)
        {
            VulkEaseDll.vePrintDebugInfo(device.native);
        }

        public static void PrintProfileInfo(VEDevice device)
        {
            VulkEaseDll.vePrintProfileInfo(device.native);
        }
    }
}