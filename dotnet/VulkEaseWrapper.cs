/**
 * VulkEase.NET - High-Level C# Wrapper
 * Provides a more convenient .NET-friendly API over the P/Invoke bindings
 */

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace VulkEase
{
    // =============================================================================
    // High-Level Wrapper Classes
    // =============================================================================

    public class VulkEaseException : Exception
    {
        public VEResult ResultCode { get; }
        
        public VulkEaseException(VEResult result) : base($"VulkEase operation failed: {result}")
        {
            ResultCode = result;
        }
        
        public VulkEaseException(VEResult result, string message) : base(message)
        {
            ResultCode = result;
        }
        
        public VulkEaseException(string message) : base(message)
        {
            ResultCode = VEResult.ErrorUnknown;
        }
    }

    public class Context : IDisposable
    {
        private VEContext _handle;
        private bool _disposed;

        public VEContext Handle => _handle;
        public bool IsValid => _handle.IsValid && !_disposed;

        public Context(string applicationName)
        {
            _handle = Native.veCreateContext(applicationName);
            if (!_handle.IsValid)
            {
                throw new VulkEaseException("Failed to create VulkEase context");
            }
        }

        public Device CreateDevice()
        {
            CheckDisposed();
            return new Device(this);
        }

        public uint GetVersion()
        {
            return Native.veGetVersion();
        }

        public string GetLastError()
        {
            return Native.veGetLastError() ?? "No error information available";
        }

        private void CheckDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(Context));
        }

        public void Dispose()
        {
            if (!_disposed && _handle.IsValid)
            {
                Native.veDestroyContext(_handle);
                _handle = new VEContext();
                _disposed = true;
            }
        }
    }

    public class Device : IDisposable
    {
        private VEDevice _handle;
        private Context _context;
        private bool _disposed;

        public VEDevice Handle => _handle;
        public Context Context => _context;
        public bool IsValid => _handle.IsValid && !_disposed;

        internal Device(Context context)
        {
            _context = context ?? throw new ArgumentNullException(nameof(context));
            _handle = Native.veCreateDevice(context.Handle);
            if (!_handle.IsValid)
            {
                throw new VulkEaseException("Failed to create VulkEase device");
            }
        }

        // Device Information
        public string DeviceName => Native.veGetDeviceName(_handle) ?? "Unknown Device";
        public string DriverVersion => Native.veGetDriverVersion(_handle) ?? "Unknown Version";
        public uint VulkanVersion => Native.veGetVulkanVersion(_handle);

        // Feature Support
        public bool SupportsBufferDeviceAddress => Native.veSupportsBufferDeviceAddress(_handle);
        public bool SupportsDescriptorIndexing => Native.veSupportsDescriptorIndexing(_handle);
        public bool SupportsShaderObjects => Native.veSupportsShaderObjects(_handle);
        public bool SupportsExtendedDynamicState3 => Native.veSupportsExtendedDynamicState3(_handle);
        public bool SupportsVertexInputDynamicState => Native.veSupportsVertexInputDynamicState(_handle);

        public void WaitIdle()
        {
            CheckDisposed();
            var result = Native.veDeviceWaitIdle(_handle);
            if (result != VEResult.Success)
                throw new VulkEaseException(result);
        }

        // Buffer Management
        public VEBufferAddress CreateBuffer(VEBufferDesc desc)
        {
            CheckDisposed();
            var address = Native.veCreateBuffer(_handle, ref desc);
            if (!address.IsValid)
                throw new VulkEaseException("Failed to create buffer");
            return address;
        }

        public VEBufferAddress CreateVertexBuffer<T>(T[] vertices, string debugName = null) where T : unmanaged
        {
            CheckDisposed();
            var handle = GCHandle.Alloc(vertices, GCHandleType.Pinned);
            try
            {
                var size = (UIntPtr)(vertices.Length * Marshal.SizeOf<T>());
                return Native.veCreateVertexBuffer(_handle, handle.AddrOfPinnedObject(), size, debugName);
            }
            finally
            {
                handle.Free();
            }
        }

        public VEBufferAddress CreateIndexBuffer(uint[] indices, string debugName = null)
        {
            CheckDisposed();
            var handle = GCHandle.Alloc(indices, GCHandleType.Pinned);
            try
            {
                var size = (UIntPtr)(indices.Length * sizeof(uint));
                return Native.veCreateIndexBuffer(_handle, handle.AddrOfPinnedObject(), size, debugName);
            }
            finally
            {
                handle.Free();
            }
        }

        public VEBufferAddress CreateUniformBuffer(int size, bool persistentlyMapped = true, string debugName = null)
        {
            CheckDisposed();
            return Native.veCreateUniformBuffer(_handle, (UIntPtr)size, persistentlyMapped, debugName);
        }

        public VEBufferAddress CreateStorageBuffer(int size, string debugName = null)
        {
            CheckDisposed();
            return Native.veCreateStorageBuffer(_handle, (UIntPtr)size, debugName);
        }

        public void DestroyBuffer(VEBufferAddress address)
        {
            CheckDisposed();
            Native.veDestroyBuffer(_handle, address);
        }

        public void UpdateBuffer<T>(VEBufferAddress address, T[] data, int offset = 0) where T : unmanaged
        {
            CheckDisposed();
            var handle = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                var size = (UIntPtr)(data.Length * Marshal.SizeOf<T>());
                var result = Native.veUpdateBuffer(_handle, address, handle.AddrOfPinnedObject(), size, (UIntPtr)offset);
                if (result != VEResult.Success)
                    throw new VulkEaseException(result);
            }
            finally
            {
                handle.Free();
            }
        }

        public unsafe IntPtr MapBuffer(VEBufferAddress address)
        {
            CheckDisposed();
            var result = Native.veMapBuffer(_handle, address, out IntPtr mappedData);
            if (result != VEResult.Success)
                throw new VulkEaseException(result);
            return mappedData;
        }

        public void UnmapBuffer(VEBufferAddress address)
        {
            CheckDisposed();
            Native.veUnmapBuffer(_handle, address);
        }

        // Texture Management
        public VETextureIndex CreateTexture(VETextureDesc desc)
        {
            CheckDisposed();
            var index = Native.veCreateTexture(_handle, ref desc);
            if (!index.IsValid)
                throw new VulkEaseException("Failed to create texture");
            return index;
        }

        public VETextureIndex CreateTexture2D(uint width, uint height, VEFormat format, 
            VETextureUsage usage, string debugName = null)
        {
            CheckDisposed();
            return Native.veCreateTexture2D(_handle, width, height, format, usage, debugName);
        }

        public VETextureIndex LoadTexture(string filename, VETextureUsage usage = VETextureUsage.Sampled, 
            bool generateMips = true)
        {
            CheckDisposed();
            var index = Native.veLoadTexture(_handle, filename, usage, generateMips);
            if (!index.IsValid)
                throw new VulkEaseException($"Failed to load texture: {filename}");
            return index;
        }

        public void DestroyTexture(VETextureIndex index)
        {
            CheckDisposed();
            Native.veDestroyTexture(_handle, index);
        }

        public (uint width, uint height, uint depth) GetTextureSize(VETextureIndex index)
        {
            CheckDisposed();
            var result = Native.veGetTextureSize(_handle, index, out uint width, out uint height, out uint depth);
            if (result != VEResult.Success)
                throw new VulkEaseException(result);
            return (width, height, depth);
        }

        // Sampler Management
        public VESamplerIndex CreateSampler(VESamplerDesc desc)
        {
            CheckDisposed();
            var index = Native.veCreateSampler(_handle, ref desc);
            if (!index.IsValid)
                throw new VulkEaseException("Failed to create sampler");
            return index;
        }

        public VESamplerIndex CreateLinearSampler()
        {
            CheckDisposed();
            return Native.veCreateLinearSampler(_handle);
        }

        public VESamplerIndex CreateNearestSampler()
        {
            CheckDisposed();
            return Native.veCreateNearestSampler(_handle);
        }

        public void DestroySampler(VESamplerIndex index)
        {
            CheckDisposed();
            Native.veDestroySampler(_handle, index);
        }

        // Shader Management
        public Shader CreateShaderFromSPIRV(VEShaderStage stage, byte[] spirvCode, 
            string entryPoint = "main", string debugName = null)
        {
            CheckDisposed();
            var handle = GCHandle.Alloc(spirvCode, GCHandleType.Pinned);
            try
            {
                var shader = Native.veCreateShaderFromSPIRV(_handle, stage, handle.AddrOfPinnedObject(), 
                    (UIntPtr)spirvCode.Length, entryPoint, debugName);
                if (!shader.IsValid)
                    throw new VulkEaseException("Failed to create shader from SPIR-V");
                return new Shader(shader);
            }
            finally
            {
                handle.Free();
            }
        }

        public Shader CreateShaderFromGLSL(VEShaderStage stage, string glslSource, 
            string entryPoint = "main", string debugName = null)
        {
            CheckDisposed();
            var shader = Native.veCreateShaderFromGLSL(_handle, stage, glslSource, entryPoint, debugName);
            if (!shader.IsValid)
                throw new VulkEaseException("Failed to create shader from GLSL");
            return new Shader(shader);
        }

        public Shader LoadShader(string filename, VEShaderStage stage, 
            string entryPoint = "main", string debugName = null)
        {
            CheckDisposed();
            var shader = Native.veLoadShader(_handle, filename, stage, entryPoint, debugName);
            if (!shader.IsValid)
                throw new VulkEaseException($"Failed to load shader: {filename}");
            return new Shader(shader);
        }

        // Render Configuration Management
        public RenderConfig CreateRenderConfig(VERenderConfigDesc desc)
        {
            CheckDisposed();
            var config = Native.veCreateRenderConfig(_handle, ref desc);
            if (!config.IsValid)
                throw new VulkEaseException("Failed to create render config");
            return new RenderConfig(config);
        }

        public RenderConfig CreateOpaqueRenderConfig(string debugName = null)
        {
            CheckDisposed();
            var config = Native.veCreateOpaqueRenderConfig(_handle, debugName);
            return new RenderConfig(config);
        }

        public RenderConfig CreateTransparentRenderConfig(string debugName = null)
        {
            CheckDisposed();
            var config = Native.veCreateTransparentRenderConfig(_handle, debugName);
            return new RenderConfig(config);
        }

        // Command Buffer Management
        public CommandBuffer BeginCommandBuffer()
        {
            CheckDisposed();
            var cmd = Native.veBeginCommandBuffer(_handle);
            if (!cmd.IsValid)
                throw new VulkEaseException("Failed to begin command buffer");
            return new CommandBuffer(cmd);
        }

        // Swapchain Management
        public Swapchain CreateSwapchain(IntPtr windowHandle, uint width, uint height, 
            VEFormat format = VEFormat.Bgra8Srgb)
        {
            CheckDisposed();
            var swapchain = Native.veCreateSwapchain(_handle, windowHandle, width, height, format);
            if (!swapchain.IsValid)
                throw new VulkEaseException("Failed to create swapchain");
            return new Swapchain(swapchain);
        }

        // Statistics and Profiling
        public VEPerformanceStats GetPerformanceStats()
        {
            CheckDisposed();
            return Native.veGetPerformanceStats(_handle);
        }

        public VEMemoryStats GetMemoryStats()
        {
            CheckDisposed();
            return Native.veGetMemoryStats(_handle);
        }

        public void UpdateFrameStats()
        {
            CheckDisposed();
            Native.veUpdateFrameStats(_handle);
        }

        // Debug Functions
        public void SetDebugName(ulong objectHandle, VEObjectType objectType, string name)
        {
            CheckDisposed();
            Native.veSetDebugName(_handle, objectHandle, objectType, name);
        }

        public void LogMessage(VEMessageSeverity severity, string message)
        {
            CheckDisposed();
            Native.veLogMessage(_handle, severity, message);
        }

        private void CheckDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(Device));
        }

        public void Dispose()
        {
            if (!_disposed && _handle.IsValid)
            {
                Native.veDestroyDevice(_handle);
                _handle = new VEDevice();
                _disposed = true;
            }
        }
    }

    public class Shader : IDisposable
    {
        private VEShader _handle;
        private bool _disposed;

        public VEShader Handle => _handle;
        public bool IsValid => _handle.IsValid && !_disposed;

        internal Shader(VEShader handle)
        {
            _handle = handle;
        }

        public VEResult EnableHotReload(string sourceFile)
        {
            CheckDisposed();
            return Native.veEnableShaderHotReload(_handle, sourceFile);
        }

        public VEResult Reload()
        {
            CheckDisposed();
            return Native.veReloadShader(_handle);
        }

        private void CheckDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(Shader));
        }

        public void Dispose()
        {
            if (!_disposed && _handle.IsValid)
            {
                Native.veDestroyShader(_handle);
                _handle = new VEShader();
                _disposed = true;
            }
        }
    }

    public class RenderConfig : IDisposable
    {
        private VERenderConfig _handle;
        private bool _disposed;

        public VERenderConfig Handle => _handle;
        public bool IsValid => _handle.IsValid && !_disposed;

        internal RenderConfig(VERenderConfig handle)
        {
            _handle = handle;
        }

        private void CheckDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(RenderConfig));
        }

        public void Dispose()
        {
            if (!_disposed && _handle.IsValid)
            {
                Native.veDestroyRenderConfig(_handle);
                _handle = new VERenderConfig();
                _disposed = true;
            }
        }
    }

    public class CommandBuffer
    {
        private VECommandBuffer _handle;

        public VECommandBuffer Handle => _handle;
        public bool IsValid => _handle.IsValid;

        internal CommandBuffer(VECommandBuffer handle)
        {
            _handle = handle;
        }

        // Rendering
        public void BeginRendering(VERenderingInfo renderingInfo)
        {
            Native.veBeginRendering(_handle, ref renderingInfo);
        }

        public void EndRendering()
        {
            Native.veEndRendering(_handle);
        }

        public void ApplyRenderConfig(RenderConfig config)
        {
            Native.veApplyRenderConfig(_handle, config.Handle);
        }

        public void BindShader(Shader shader)
        {
            Native.veBindShader(_handle, shader.Handle);
        }

        public unsafe void BindShaders(Shader[] shaders)
        {
            var handles = new VEShader[shaders.Length];
            for (int i = 0; i < shaders.Length; i++)
            {
                handles[i] = shaders[i].Handle;
            }
            
            fixed (VEShader* ptr = handles)
            {
                Native.veBindShaders(_handle, (uint)handles.Length, (IntPtr)ptr);
            }
        }

        // Viewport and Scissor
        public void SetViewport(float x, float y, float width, float height, 
            float minDepth = 0.0f, float maxDepth = 1.0f)
        {
            Native.veSetViewport(_handle, x, y, width, height, minDepth, maxDepth);
        }

        public void SetScissor(int x, int y, uint width, uint height)
        {
            Native.veSetScissor(_handle, x, y, width, height);
        }

        // State Overrides
        public void SetWireframe(bool enabled)
        {
            Native.veSetWireframe(_handle, enabled);
        }

        public void SetAlphaBlending(bool enabled)
        {
            Native.veSetAlphaBlending(_handle, enabled);
        }

        public void SetDepthTesting(bool testEnabled, bool writeEnabled = true)
        {
            Native.veSetDepthTesting(_handle, testEnabled, writeEnabled);
        }

        public void SetCulling(VECullMode cullMode)
        {
            Native.veSetCulling(_handle, cullMode);
        }

        // Push Constants
        public unsafe void PushConstants<T>(T data) where T : unmanaged
        {
            var size = (UIntPtr)Marshal.SizeOf<T>();
            Native.vePushConstants(_handle, (IntPtr)(&data), size, UIntPtr.Zero);
        }

        public unsafe void PushConstants<T>(T data, int offset) where T : unmanaged
        {
            var size = (UIntPtr)Marshal.SizeOf<T>();
            Native.vePushConstants(_handle, (IntPtr)(&data), size, (UIntPtr)offset);
        }

        // Drawing Commands
        public void Draw(uint vertexCount, uint instanceCount = 1, 
            uint firstVertex = 0, uint firstInstance = 0)
        {
            Native.veDraw(_handle, vertexCount, instanceCount, firstVertex, firstInstance);
        }

        public void DrawIndexed(uint indexCount, uint instanceCount = 1, 
            uint firstIndex = 0, int vertexOffset = 0, uint firstInstance = 0)
        {
            Native.veDrawIndexed(_handle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }

        public void DrawIndirect(VEBufferAddress indirectBuffer, int offset = 0, 
            uint drawCount = 1, uint stride = 16)
        {
            Native.veDrawIndirect(_handle, indirectBuffer, (UIntPtr)offset, drawCount, stride);
        }

        // Compute Commands
        public void Dispatch(uint groupCountX, uint groupCountY = 1, uint groupCountZ = 1)
        {
            Native.veDispatch(_handle, groupCountX, groupCountY, groupCountZ);
        }

        public void DispatchIndirect(VEBufferAddress indirectBuffer, int offset = 0)
        {
            Native.veDispatchIndirect(_handle, indirectBuffer, (UIntPtr)offset);
        }

        // Barriers
        public void BarrierVertexToFragment()
        {
            Native.veBarrierVertexToFragment(_handle);
        }

        public void BarrierComputeToGraphics()
        {
            Native.veBarrierComputeToGraphics(_handle);
        }

        public void BarrierGraphicsToCompute()
        {
            Native.veBarrierGraphicsToCompute(_handle);
        }

        // Debug Labels
        public unsafe void BeginDebugRegion(string regionName, VEColor color = default)
        {
            if (color.Equals(default(VEColor)))
                color = VEColor.Blue;
                
            Native.veBeginDebugRegion(_handle, regionName, (IntPtr)(&color));
        }

        public void EndDebugRegion()
        {
            Native.veEndDebugRegion(_handle);
        }

        public unsafe void InsertDebugLabel(string labelName, VEColor color = default)
        {
            if (color.Equals(default(VEColor)))
                color = VEColor.White;
                
            Native.veInsertDebugLabel(_handle, labelName, (IntPtr)(&color));
        }

        // Submit Command Buffer
        public VEResult Submit(bool waitForCompletion = true)
        {
            return Native.veSubmitCommandBuffer(_handle, waitForCompletion);
        }
    }

    public class Swapchain : IDisposable
    {
        private VESwapchain _handle;
        private bool _disposed;

        public VESwapchain Handle => _handle;
        public bool IsValid => _handle.IsValid && !_disposed;

        internal Swapchain(VESwapchain handle)
        {
            _handle = handle;
        }

        public VETextureIndex AcquireNextImage()
        {
            CheckDisposed();
            return Native.veAcquireNextImage(_handle);
        }

        public VEResult Present(CommandBuffer cmd)
        {
            CheckDisposed();
            return Native.vePresentImage(_handle, cmd.Handle);
        }

        public VEResult Resize(uint width, uint height)
        {
            CheckDisposed();
            return Native.veResizeSwapchain(_handle, width, height);
        }

        public (uint width, uint height) GetSize()
        {
            CheckDisposed();
            var result = Native.veGetSwapchainSize(_handle, out uint width, out uint height);
            if (result != VEResult.Success)
                throw new VulkEaseException(result);
            return (width, height);
        }

        public VEFormat GetFormat()
        {
            CheckDisposed();
            return Native.veGetSwapchainFormat(_handle);
        }

        private void CheckDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(Swapchain));
        }

        public void Dispose()
        {
            if (!_disposed && _handle.IsValid)
            {
                Native.veDestroySwapchain(_handle);
                _handle = new VESwapchain();
                _disposed = true;
            }
        }
    }

    // =============================================================================
    // Utility Classes and Extensions
    // =============================================================================

    public static class VulkEaseExtensions
    {
        public static void ThrowIfFailed(this VEResult result)
        {
            if (result != VEResult.Success)
                throw new VulkEaseException(result);
        }

        public static bool IsSuccess(this VEResult result)
        {
            return result == VEResult.Success;
        }

        public static bool IsFailed(this VEResult result)
        {
            return result != VEResult.Success;
        }
    }

    public static class DefaultConfigurations
    {
        public static VEBufferDesc VertexBuffer(int size, string debugName = null)
        {
            return new VEBufferDesc
            {
                Size = (UIntPtr)size,
                Usage = VEBufferUsage.Vertex,
                InitialData = IntPtr.Zero,
                InitialDataSize = UIntPtr.Zero,
                PersistentlyMapped = false,
                DebugName = debugName
            };
        }

        public static VEBufferDesc IndexBuffer(int size, string debugName = null)
        {
            return new VEBufferDesc
            {
                Size = (UIntPtr)size,
                Usage = VEBufferUsage.Index,
                InitialData = IntPtr.Zero,
                InitialDataSize = UIntPtr.Zero,
                PersistentlyMapped = false,
                DebugName = debugName
            };
        }

        public static VEBufferDesc UniformBuffer(int size, bool persistentlyMapped = true, string debugName = null)
        {
            return new VEBufferDesc
            {
                Size = (UIntPtr)size,
                Usage = VEBufferUsage.Uniform,
                InitialData = IntPtr.Zero,
                InitialDataSize = UIntPtr.Zero,
                PersistentlyMapped = persistentlyMapped,
                DebugName = debugName
            };
        }

        public static VETextureDesc Texture2D(uint width, uint height, VEFormat format, 
            VETextureUsage usage = VETextureUsage.Sampled, string debugName = null)
        {
            return new VETextureDesc
            {
                Width = width,
                Height = height,
                Depth = 1,
                MipLevels = 1,
                ArrayLayers = 1,
                Format = format,
                Usage = usage,
                SampleCount = VESampleCount.Count1,
                InitialData = IntPtr.Zero,
                InitialDataSize = UIntPtr.Zero,
                DebugName = debugName
            };
        }

        public static VESamplerDesc LinearSampler(string debugName = null)
        {
            return new VESamplerDesc
            {
                MinFilter = VEFilter.Linear,
                MagFilter = VEFilter.Linear,
                MipmapFilter = VEFilter.Linear,
                AddressModeU = VEAddressMode.Repeat,
                AddressModeV = VEAddressMode.Repeat,
                AddressModeW = VEAddressMode.Repeat,
                MaxAnisotropy = 1.0f,
                CompareEnable = false,
                CompareOp = VECompareOp.Always,
                MinLod = 0.0f,
                MaxLod = 1000.0f,
                DebugName = debugName
            };
        }

        public static VESamplerDesc NearestSampler(string debugName = null)
        {
            return new VESamplerDesc
            {
                MinFilter = VEFilter.Nearest,
                MagFilter = VEFilter.Nearest,
                MipmapFilter = VEFilter.Nearest,
                AddressModeU = VEAddressMode.Repeat,
                AddressModeV = VEAddressMode.Repeat,
                AddressModeW = VEAddressMode.Repeat,
                MaxAnisotropy = 1.0f,
                CompareEnable = false,
                CompareOp = VECompareOp.Always,
                MinLod = 0.0f,
                MaxLod = 1000.0f,
                DebugName = debugName
            };
        }

        public static VERasterConfig DefaultRaster()
        {
            return Native.veDefaultRasterConfig();
        }

        public static VEDepthConfig DefaultDepth()
        {
            return Native.veDefaultDepthConfig();
        }

        public static VEBlendConfig OpaqueBlend()
        {
            return Native.veDefaultOpaqueBlendConfig();
        }

        public static VEBlendConfig AlphaBlend()
        {
            return Native.veDefaultAlphaBlendConfig();
        }

        public static VEMultisampleConfig DefaultMultisample()
        {
            return Native.veDefaultMultisampleConfig();
        }
    }

    // =============================================================================
    // Convenience Methods for Common Operations
    // =============================================================================

    public static class VulkEaseHelper
    {
        public static uint MakeVersion(uint major, uint minor, uint patch)
        {
            return (major << 22) | (minor << 12) | patch;
        }

        public static (uint major, uint minor, uint patch) ParseVersion(uint version)
        {
            uint major = (version >> 22) & 0x3FF;
            uint minor = (version >> 12) & 0x3FF;
            uint patch = version & 0xFFF;
            return (major, minor, patch);
        }

        public static string FormatVersion(uint version)
        {
            var (major, minor, patch) = ParseVersion(version);
            return $"{major}.{minor}.{patch}";
        }

        public static VERenderingAttachment ColorAttachment(VETextureIndex texture, 
            VELoadOp loadOp = VELoadOp.Clear, VEColor clearColor = default)
        {
            if (clearColor.Equals(default(VEColor)))
                clearColor = VEColor.Black;

            return new VERenderingAttachment
            {
                Texture = texture,
                LoadOp = loadOp,
                StoreOp = VEStoreOp.Store,
                ClearValue = clearColor,
                ResolveTexture = Constants.VE_INVALID_TEXTURE_INDEX
            };
        }

        public static VERenderingAttachment DepthAttachment(VETextureIndex texture, 
            VELoadOp loadOp = VELoadOp.Clear)
        {
            return new VERenderingAttachment
            {
                Texture = texture,
                LoadOp = loadOp,
                StoreOp = VEStoreOp.Store,
                ClearValue = new VEColor(1.0f, 0.0f, 0.0f, 0.0f), // Depth = 1.0
                ResolveTexture = Constants.VE_INVALID_TEXTURE_INDEX
            };
        }

        public static byte[] LoadFile(string filename)
        {
            return System.IO.File.ReadAllBytes(filename);
        }

        public static string[] CubeFaceFiles(string baseName, string extension = "jpg")
        {
            return new[]
            {
                $"{baseName}_px.{extension}", // +X
                $"{baseName}_nx.{extension}", // -X
                $"{baseName}_py.{extension}", // +Y
                $"{baseName}_ny.{extension}", // -Y
                $"{baseName}_pz.{extension}", // +Z
                $"{baseName}_nz.{extension}"  // -Z
            };
        }
    }

    // =============================================================================
    // RAII Scope Helpers
    // =============================================================================

    public struct DebugScope : IDisposable
    {
        private CommandBuffer _cmd;

        public DebugScope(CommandBuffer cmd, string name, VEColor color = default)
        {
            _cmd = cmd;
            _cmd?.BeginDebugRegion(name, color);
        }

        public void Dispose()
        {
            _cmd?.EndDebugRegion();
        }
    }

    public struct RenderScope : IDisposable
    {
        private CommandBuffer _cmd;

        public RenderScope(CommandBuffer cmd, VERenderingInfo renderingInfo)
        {
            _cmd = cmd;
            _cmd?.BeginRendering(renderingInfo);
        }

        public void Dispose()
        {
            _cmd?.EndRendering();
        }
    }
}