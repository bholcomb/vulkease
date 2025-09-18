/**
 * VulkEase.NET - C# Bindings for VulkEase 2.0
 * Complete Modern Vulkan 1.3+ Bindless Graphics API
 */

using System;
using System.Runtime.InteropServices;
using System.Security;

namespace VulkEase
{
    // =============================================================================
    // Constants and Version Information
    // =============================================================================
    
    public static class Constants
    {
        public const uint VULKEASE_VERSION_MAJOR = 2;
        public const uint VULKEASE_VERSION_MINOR = 0;
        public const uint VULKEASE_VERSION_PATCH = 0;
        public const uint VULKEASE_API_VERSION_2_0 = (2u << 22) | (0u << 12) | 0u;
        
        public const ulong VE_INVALID_ADDRESS = 0UL;
        public const uint VE_INVALID_TEXTURE_INDEX = 0xFFFFFFFFu;
        public const uint VE_INVALID_SAMPLER_INDEX = 0xFFFFFFFFu;
        
        public const string LIBRARY_NAME = "vulkease2";
    }

    // =============================================================================
    // Core Types and Handles
    // =============================================================================
    
    public struct VEContext
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VEContext context) => context.Handle;
        public static implicit operator VEContext(IntPtr handle) => new VEContext { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    public struct VEDevice
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VEDevice device) => device.Handle;
        public static implicit operator VEDevice(IntPtr handle) => new VEDevice { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    public struct VECommandBuffer
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VECommandBuffer cmd) => cmd.Handle;
        public static implicit operator VECommandBuffer(IntPtr handle) => new VECommandBuffer { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    public struct VESwapchain
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VESwapchain swapchain) => swapchain.Handle;
        public static implicit operator VESwapchain(IntPtr handle) => new VESwapchain { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    public struct VEShader
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VEShader shader) => shader.Handle;
        public static implicit operator VEShader(IntPtr handle) => new VEShader { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    public struct VERenderConfig
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VERenderConfig config) => config.Handle;
        public static implicit operator VERenderConfig(IntPtr handle) => new VERenderConfig { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    public struct VEVertexConfig
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VEVertexConfig config) => config.Handle;
        public static implicit operator VEVertexConfig(IntPtr handle) => new VEVertexConfig { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    public struct VEShaderConfig
    {
        public IntPtr Handle;
        public static implicit operator IntPtr(VEShaderConfig config) => config.Handle;
        public static implicit operator VEShaderConfig(IntPtr handle) => new VEShaderConfig { Handle = handle };
        public bool IsValid => Handle != IntPtr.Zero;
    }

    // GPU addresses and bindless indices
    public struct VEBufferAddress
    {
        public ulong Value;
        public static implicit operator ulong(VEBufferAddress addr) => addr.Value;
        public static implicit operator VEBufferAddress(ulong value) => new VEBufferAddress { Value = value };
        public bool IsValid => Value != Constants.VE_INVALID_ADDRESS;
    }

    public struct VETextureIndex
    {
        public uint Value;
        public static implicit operator uint(VETextureIndex index) => index.Value;
        public static implicit operator VETextureIndex(uint value) => new VETextureIndex { Value = value };
        public bool IsValid => Value != Constants.VE_INVALID_TEXTURE_INDEX;
    }

    public struct VESamplerIndex
    {
        public uint Value;
        public static implicit operator uint(VESamplerIndex index) => index.Value;
        public static implicit operator VESamplerIndex(uint value) => new VESamplerIndex { Value = value };
        public bool IsValid => Value != Constants.VE_INVALID_SAMPLER_INDEX;
    }

    // =============================================================================
    // Enumerations
    // =============================================================================

    public enum VEResult : uint
    {
        Success = 0,
        ErrorOutOfMemory = 1,
        ErrorDeviceLost = 2,
        ErrorInvalidParameter = 3,
        ErrorFeatureNotSupported = 4,
        ErrorShaderCompilationFailed = 5,
        ErrorSwapchainOutOfDate = 6,
        ErrorUnknown = 999
    }

    public enum VEFormat : uint
    {
        // 8-bit formats
        R8Unorm = 9,
        Rg8Unorm = 16,
        Rgba8Unorm = 37,
        Rgba8Srgb = 43,
        Bgra8Unorm = 44,
        Bgra8Srgb = 50,
        
        // 16-bit formats
        R16Unorm = 70,
        R16Sfloat = 76,
        Rg16Unorm = 77,
        Rg16Sfloat = 83,
        Rgba16Unorm = 91,
        Rgba16Sfloat = 97,
        
        // 32-bit formats
        R32Uint = 98,
        R32Sint = 99,
        R32Sfloat = 100,
        Rg32Uint = 101,
        Rg32Sint = 102,
        Rg32Sfloat = 103,
        Rgb32Uint = 104,
        Rgb32Sint = 105,
        Rgb32Sfloat = 106,
        Rgba32Uint = 107,
        Rgba32Sint = 108,
        Rgba32Sfloat = 109,
        
        // Depth/stencil formats
        D16Unorm = 124,
        X8D24UnormPack32 = 125,
        D32Sfloat = 126,
        S8Uint = 127,
        D16UnormS8Uint = 128,
        D24UnormS8Uint = 129,
        D32SfloatS8Uint = 130,
        
        // Compressed formats
        Bc1RgbUnorm = 131,
        Bc1RgbSrgb = 132,
        Bc1RgbaUnorm = 133,
        Bc1RgbaSrgb = 134,
        Bc2Unorm = 135,
        Bc2Srgb = 136,
        Bc3Unorm = 137,
        Bc3Srgb = 138,
        Bc4Unorm = 139,
        Bc4Snorm = 140,
        Bc5Unorm = 141,
        Bc5Snorm = 142,
        Bc6hUfloat = 143,
        Bc6hSfloat = 144,
        Bc7Unorm = 145,
        Bc7Srgb = 146
    }

    [Flags]
    public enum VEBufferUsage : uint
    {
        Vertex = 0x1,
        Index = 0x2,
        Uniform = 0x4,
        Storage = 0x8,
        Indirect = 0x10,
        TransferSrc = 0x20,
        TransferDst = 0x40,
        ConditionalRendering = 0x200
    }

    [Flags]
    public enum VETextureUsage : uint
    {
        Sampled = 0x1,
        Storage = 0x2,
        ColorAttachment = 0x4,
        DepthStencilAttachment = 0x8,
        TransferSrc = 0x10,
        TransferDst = 0x20,
        InputAttachment = 0x80
    }

    [Flags]
    public enum VEShaderStage : uint
    {
        Vertex = 0x1,
        TessellationControl = 0x2,
        TessellationEvaluation = 0x4,
        Geometry = 0x8,
        Fragment = 0x10,
        Compute = 0x20
    }

    public enum VEFilter : uint
    {
        Nearest = 0,
        Linear = 1
    }

    public enum VEAddressMode : uint
    {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
        MirrorClampToEdge = 4
    }

    public enum VELoadOp : uint
    {
        Load = 0,
        Clear = 1,
        DontCare = 2
    }

    public enum VEStoreOp : uint
    {
        Store = 0,
        DontCare = 1
    }

    [Flags]
    public enum VEConfigType : uint
    {
        VertexInput = 0x1,
        Rasterization = 0x2,
        DepthStencil = 0x4,
        ColorBlend = 0x8,
        Multisample = 0x10,
        Shaders = 0x20,
        Complete = 0x3F
    }

    public enum VECullMode : uint
    {
        None = 0,
        Front = 1,
        Back = 2,
        FrontAndBack = 3
    }

    public enum VEFrontFace : uint
    {
        CounterClockwise = 0,
        Clockwise = 1
    }

    public enum VEPolygonMode : uint
    {
        Fill = 0,
        Line = 1,
        Point = 2
    }

    public enum VECompareOp : uint
    {
        Never = 0,
        Less = 1,
        Equal = 2,
        LessOrEqual = 3,
        Greater = 4,
        NotEqual = 5,
        GreaterOrEqual = 6,
        Always = 7
    }

    public enum VEStencilOp : uint
    {
        Keep = 0,
        Zero = 1,
        Replace = 2,
        IncrementAndClamp = 3,
        DecrementAndClamp = 4,
        Invert = 5,
        IncrementAndWrap = 6,
        DecrementAndWrap = 7
    }

    public enum VEBlendFactor : uint
    {
        Zero = 0,
        One = 1,
        SrcColor = 2,
        OneMinusSrcColor = 3,
        DstColor = 4,
        OneMinusDstColor = 5,
        SrcAlpha = 6,
        OneMinusSrcAlpha = 7,
        DstAlpha = 8,
        OneMinusDstAlpha = 9,
        ConstantColor = 10,
        OneMinusConstantColor = 11,
        ConstantAlpha = 12,
        OneMinusConstantAlpha = 13
    }

    public enum VEBlendOp : uint
    {
        Add = 0,
        Subtract = 1,
        ReverseSubtract = 2,
        Min = 3,
        Max = 4
    }

    public enum VELogicOp : uint
    {
        Clear = 0,
        And = 1,
        AndReverse = 2,
        Copy = 3,
        AndInverted = 4,
        NoOp = 5,
        Xor = 6,
        Or = 7,
        Nor = 8,
        Equivalent = 9,
        Invert = 10,
        OrReverse = 11,
        CopyInverted = 12,
        OrInverted = 13,
        Nand = 14,
        Set = 15
    }

    [Flags]
    public enum VEColorComponentFlags : uint
    {
        R = 0x1,
        G = 0x2,
        B = 0x4,
        A = 0x8,
        All = 0xF
    }

    public enum VEVertexInputRate : uint
    {
        Vertex = 0,
        Instance = 1
    }

    public enum VEPrimitiveTopology : uint
    {
        PointList = 0,
        LineList = 1,
        LineStrip = 2,
        TriangleList = 3,
        TriangleStrip = 4,
        TriangleFan = 5,
        LineListWithAdjacency = 6,
        LineStripWithAdjacency = 7,
        TriangleListWithAdjacency = 8,
        TriangleStripWithAdjacency = 9,
        PatchList = 10
    }

    public enum VESampleCount : uint
    {
        Count1 = 1,
        Count2 = 2,
        Count4 = 4,
        Count8 = 8,
        Count16 = 16,
        Count32 = 32,
        Count64 = 64
    }

    public enum VEObjectType : uint
    {
        Buffer = 0,
        Image = 1,
        ImageView = 2,
        Sampler = 3,
        Shader = 4,
        CommandBuffer = 5,
        Queue = 6,
        Device = 7,
        Instance = 8,
        PhysicalDevice = 9
    }

    public enum VEMessageSeverity : uint
    {
        Verbose = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    }

    // =============================================================================
    // Structure Types
    // =============================================================================

    [StructLayout(LayoutKind.Sequential)]
    public struct VEColor
    {
        public float R;
        public float G;
        public float B;
        public float A;

        public VEColor(float r, float g, float b, float a)
        {
            R = r; G = g; B = b; A = a;
        }

        public static readonly VEColor Black = new VEColor(0.0f, 0.0f, 0.0f, 1.0f);
        public static readonly VEColor White = new VEColor(1.0f, 1.0f, 1.0f, 1.0f);
        public static readonly VEColor Red = new VEColor(1.0f, 0.0f, 0.0f, 1.0f);
        public static readonly VEColor Green = new VEColor(0.0f, 1.0f, 0.0f, 1.0f);
        public static readonly VEColor Blue = new VEColor(0.0f, 0.0f, 1.0f, 1.0f);
        public static readonly VEColor Yellow = new VEColor(1.0f, 1.0f, 0.0f, 1.0f);
        public static readonly VEColor Cyan = new VEColor(0.0f, 1.0f, 1.0f, 1.0f);
        public static readonly VEColor Magenta = new VEColor(1.0f, 0.0f, 1.0f, 1.0f);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERect2D
    {
        public int X;
        public int Y;
        public uint Width;
        public uint Height;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEViewport
    {
        public float X;
        public float Y;
        public float Width;
        public float Height;
        public float MinDepth;
        public float MaxDepth;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEBufferDesc
    {
        public UIntPtr Size;
        public VEBufferUsage Usage;
        public IntPtr InitialData;
        public UIntPtr InitialDataSize;
        [MarshalAs(UnmanagedType.I1)]
        public bool PersistentlyMapped;
        [MarshalAs(UnmanagedType.LPUTF8Str)]
        public string DebugName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VETextureDesc
    {
        public uint Width;
        public uint Height;
        public uint Depth;
        public uint MipLevels;
        public uint ArrayLayers;
        public VEFormat Format;
        public VETextureUsage Usage;
        public VESampleCount SampleCount;
        public IntPtr InitialData;
        public UIntPtr InitialDataSize;
        [MarshalAs(UnmanagedType.LPUTF8Str)]
        public string DebugName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VESamplerDesc
    {
        public VEFilter MinFilter;
        public VEFilter MagFilter;
        public VEFilter MipmapFilter;
        public VEAddressMode AddressModeU;
        public VEAddressMode AddressModeV;
        public VEAddressMode AddressModeW;
        public float MaxAnisotropy;
        [MarshalAs(UnmanagedType.I1)]
        public bool CompareEnable;
        public VECompareOp CompareOp;
        public float MinLod;
        public float MaxLod;
        [MarshalAs(UnmanagedType.LPUTF8Str)]
        public string DebugName;
    }

    // =============================================================================
    // Render Configuration Structures
    // =============================================================================

    [StructLayout(LayoutKind.Sequential)]
    public struct VERasterConfig
    {
        public VECullMode CullMode;
        public VEFrontFace FrontFace;
        public VEPolygonMode PolygonMode;
        public float LineWidth;
        [MarshalAs(UnmanagedType.I1)]
        public bool DepthBiasEnable;
        public float DepthBiasConstantFactor;
        public float DepthBiasClamp;
        public float DepthBiasSlopeFactor;
        [MarshalAs(UnmanagedType.I1)]
        public bool DepthClampEnable;
        [MarshalAs(UnmanagedType.I1)]
        public bool RasterizerDiscardEnable;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEDepthConfig
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool DepthTestEnable;
        [MarshalAs(UnmanagedType.I1)]
        public bool DepthWriteEnable;
        public VECompareOp DepthCompareOp;
        [MarshalAs(UnmanagedType.I1)]
        public bool DepthBoundsTestEnable;
        public float MinDepthBounds;
        public float MaxDepthBounds;
        [MarshalAs(UnmanagedType.I1)]
        public bool StencilTestEnable;
        
        // Front face stencil
        public VEStencilOp FrontFailOp;
        public VEStencilOp FrontPassOp;
        public VEStencilOp FrontDepthFailOp;
        public VECompareOp FrontCompareOp;
        public uint FrontCompareMask;
        public uint FrontWriteMask;
        public uint FrontReference;
        
        // Back face stencil
        public VEStencilOp BackFailOp;
        public VEStencilOp BackPassOp;
        public VEStencilOp BackDepthFailOp;
        public VECompareOp BackCompareOp;
        public uint BackCompareMask;
        public uint BackWriteMask;
        public uint BackReference;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEBlendAttachment
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool BlendEnable;
        public VEBlendFactor SrcColorBlendFactor;
        public VEBlendFactor DstColorBlendFactor;
        public VEBlendOp ColorBlendOp;
        public VEBlendFactor SrcAlphaBlendFactor;
        public VEBlendFactor DstAlphaBlendFactor;
        public VEBlendOp AlphaBlendOp;
        public VEColorComponentFlags ColorWriteMask;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct VEBlendConfig
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool LogicOpEnable;
        public VELogicOp LogicOp;
        public uint AttachmentCount;
        public fixed byte Attachments[8 * 32]; // 8 attachments * sizeof(VEBlendAttachment)
        public fixed float BlendConstants[4];
        
        public Span<VEBlendAttachment> GetAttachments()
        {
            fixed (byte* ptr = Attachments)
            {
                return new Span<VEBlendAttachment>(ptr, 8);
            }
        }
        
        public Span<float> GetBlendConstants()
        {
            fixed (float* ptr = BlendConstants)
            {
                return new Span<float>(ptr, 4);
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEMultisampleConfig
    {
        public VESampleCount RasterizationSamples;
        [MarshalAs(UnmanagedType.I1)]
        public bool SampleShadingEnable;
        public float MinSampleShading;
        [MarshalAs(UnmanagedType.I1)]
        public bool AlphaToCoverageEnable;
        [MarshalAs(UnmanagedType.I1)]
        public bool AlphaToOneEnable;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexBinding
    {
        public uint Binding;
        public uint Stride;
        public VEVertexInputRate InputRate;
        public uint Divisor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexAttribute
    {
        public uint Location;
        public uint Binding;
        public VEFormat Format;
        public uint Offset;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexInputConfig
    {
        public uint BindingCount;
        public IntPtr Bindings;
        public uint AttributeCount;
        public IntPtr Attributes;
        public VEPrimitiveTopology Topology;
        [MarshalAs(UnmanagedType.I1)]
        public bool PrimitiveRestartEnable;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderConfigDesc
    {
        public VEConfigType ConfigTypes;
        public IntPtr RasterConfig;
        public IntPtr DepthConfig;
        public IntPtr BlendConfig;
        public IntPtr MultisampleConfig;
        public IntPtr VertexInputConfig;
        public uint ShaderCount;
        public IntPtr Shaders;
        [MarshalAs(UnmanagedType.LPUTF8Str)]
        public string DebugName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEShaderConfigDesc
    {
        public VEShader VertexShader;
        public VEShader FragmentShader;
        public VEShader GeometryShader;
        public VEShader TessControlShader;
        public VEShader TessEvalShader;
        public VEShader ComputeShader;
        [MarshalAs(UnmanagedType.LPUTF8Str)]
        public string DebugName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderingAttachment
    {
        public VETextureIndex Texture;
        public VELoadOp LoadOp;
        public VEStoreOp StoreOp;
        public VEColor ClearValue;
        public VETextureIndex ResolveTexture;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderingInfo
    {
        public uint RenderAreaX;
        public uint RenderAreaY;
        public uint RenderAreaWidth;
        public uint RenderAreaHeight;
        public uint ColorAttachmentCount;
        public IntPtr ColorAttachments;
        public IntPtr DepthAttachment;
        public IntPtr StencilAttachment;
    }

    // =============================================================================
    // Statistics and Debug Structures
    // =============================================================================

    [StructLayout(LayoutKind.Sequential)]
    public struct VEPerformanceStats
    {
        public float FrameTime;
        public float Fps;
        public float AverageFrameTime;
        public float AverageFps;
        public float MinFrameTime;
        public float MaxFrameTime;
        public ulong TotalFrames;
        public uint DrawCalls;
        public uint ComputeDispatches;
        public ulong VerticesRendered;
        public ulong PrimitivesRendered;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct VEMemoryStats
    {
        public ulong TotalAllocatedBytes;
        public ulong TotalUsedBytes;
        public ulong TotalFreeBytes;
        public uint AllocationCount;
        public uint UnusedRangeCount;
        public ulong BufferMemoryUsage;
        public ulong TextureMemoryUsage;
        public uint HeapCount;
        public fixed ulong HeapSizes[16];
        public ulong DeviceLocalHeapSize;
        public ulong HostVisibleHeapSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderConfigStats
    {
        public uint TotalConfigs;
        public uint ActiveConfigs;
        public uint VertexConfigs;
        public uint ShaderConfigs;
        public uint MaxConfigs;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct VEComputeCapabilities
    {
        public fixed uint MaxWorkgroupCount[3];
        public fixed uint MaxWorkgroupSize[3];
        public uint MaxWorkgroupInvocations;
        public uint MaxSharedMemorySize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEGraphicsPushConstants
    {
        public VEBufferAddress VertexBuffer;
        public VEBufferAddress IndexBuffer;
        public VEBufferAddress UniformBuffer;
        public VETextureIndex DiffuseTexture;
        public VETextureIndex NormalTexture;
        public VESamplerIndex Sampler;
        public float ObjectScale;
        public uint Padding;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct VEComputePushConstants
    {
        public VEBufferAddress InputBuffer;
        public VEBufferAddress OutputBuffer;
        public VEBufferAddress ParamBuffer;
        public VETextureIndex InputTexture;
        public VETextureIndex OutputTexture;
        public uint ElementCount;
        public fixed uint Padding[2];
    }

    // =============================================================================
    // P/Invoke Function Declarations
    // =============================================================================

    [SuppressUnmanagedCodeSecurity]
    public static class Native
    {
        // =============================================================================
        // Context and Device Management
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint veGetVersion();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEContext veCreateContext([MarshalAs(UnmanagedType.LPUTF8Str)] string applicationName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyContext(VEContext context);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEDevice veCreateDevice(VEContext context);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyDevice(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veDeviceWaitIdle(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veSupportsBufferDeviceAddress(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veSupportsDescriptorIndexing(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veSupportsShaderObjects(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veSupportsExtendedDynamicState3(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veSupportsVertexInputDynamicState(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPUTF8Str)]
        public static extern string veGetDeviceName(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPUTF8Str)]
        public static extern string veGetDriverVersion(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint veGetVulkanVersion(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPUTF8Str)]
        public static extern string veGetLastError();

        // =============================================================================
        // Buffer Management
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBufferAddress veCreateBuffer(VEDevice device, ref VEBufferDesc desc);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyBuffer(VEDevice device, VEBufferAddress address);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veMapBuffer(VEDevice device, VEBufferAddress address, out IntPtr mappedData);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veUnmapBuffer(VEDevice device, VEBufferAddress address);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veUpdateBuffer(VEDevice device, VEBufferAddress address,
            IntPtr data, UIntPtr size, UIntPtr offset);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern UIntPtr veGetBufferSize(VEDevice device, VEBufferAddress address);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBufferUsage veGetBufferUsage(VEDevice device, VEBufferAddress address);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBufferAddress veCreateVertexBuffer(VEDevice device, IntPtr vertices,
            UIntPtr size, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBufferAddress veCreateIndexBuffer(VEDevice device, IntPtr indices,
            UIntPtr size, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBufferAddress veCreateUniformBuffer(VEDevice device, UIntPtr size,
            [MarshalAs(UnmanagedType.I1)] bool persistentlyMapped, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBufferAddress veCreateStorageBuffer(VEDevice device, UIntPtr size,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBufferAddress veCreateIndirectBuffer(VEDevice device, UIntPtr size,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        // =============================================================================
        // Texture Management
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veCreateTexture(VEDevice device, ref VETextureDesc desc);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyTexture(VEDevice device, VETextureIndex index);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veGetTextureSize(VEDevice device, VETextureIndex index,
            out uint width, out uint height, out uint depth);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEFormat veGetTextureFormat(VEDevice device, VETextureIndex index);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veCreateTexture1D(VEDevice device, uint width, VEFormat format,
            VETextureUsage usage, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veCreateTexture2D(VEDevice device, uint width, uint height,
            VEFormat format, VETextureUsage usage, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veCreateTexture3D(VEDevice device, uint width, uint height, uint depth,
            VEFormat format, VETextureUsage usage, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veCreateTexture2DArray(VEDevice device, uint width, uint height,
            uint layers, VEFormat format, VETextureUsage usage, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veCreateTextureCube(VEDevice device, uint size, VEFormat format,
            VETextureUsage usage, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veCreateTexture2DMultisample(VEDevice device, uint width, uint height,
            VEFormat format, VESampleCount sampleCount, VETextureUsage usage, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veLoadTexture(VEDevice device, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename,
            VETextureUsage usage, [MarshalAs(UnmanagedType.I1)] bool generateMips);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veLoadHDRTexture(VEDevice device, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename,
            VETextureUsage usage, [MarshalAs(UnmanagedType.I1)] bool generateMips);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veLoadCubeTexture(VEDevice device, IntPtr filenames,
            VETextureUsage usage, [MarshalAs(UnmanagedType.I1)] bool generateMips);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veGenerateMipmaps(VEDevice device, VECommandBuffer cmd, VETextureIndex texture);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veSaveTexture(VEDevice device, VETextureIndex texture,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string filename);

        // =============================================================================
        // Sampler Management
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VESamplerIndex veCreateSampler(VEDevice device, ref VESamplerDesc desc);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroySampler(VEDevice device, VESamplerIndex index);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VESamplerIndex veCreateLinearSampler(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VESamplerIndex veCreateNearestSampler(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VESamplerIndex veCreateAnisotropicSampler(VEDevice device, float maxAnisotropy);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VESamplerIndex veCreateShadowSampler(VEDevice device);

        // =============================================================================
        // Shader Objects
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEShader veCreateShaderFromSPIRV(VEDevice device, VEShaderStage stage,
            IntPtr code, UIntPtr codeSize, [MarshalAs(UnmanagedType.LPUTF8Str)] string entryPoint,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEShader veCreateShaderFromGLSL(VEDevice device, VEShaderStage stage,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string source, [MarshalAs(UnmanagedType.LPUTF8Str)] string entryPoint,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEShader veLoadShader(VEDevice device, [MarshalAs(UnmanagedType.LPUTF8Str)] string filename,
            VEShaderStage stage, [MarshalAs(UnmanagedType.LPUTF8Str)] string entryPoint,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyShader(VEShader shader);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veEnableShaderHotReload(VEShader shader,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string sourceFile);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veReloadShader(VEShader shader);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEShaderConfig veCreateShaderConfig(VEDevice device, ref VEShaderConfigDesc desc);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyShaderConfig(VEShaderConfig config);

        // =============================================================================
        // Render Configuration Management
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCreateRenderConfig(VEDevice device, ref VERenderConfigDesc desc);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyRenderConfig(VERenderConfig config);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEVertexConfig veCreateVertexConfig(VEDevice device, uint bindingCount,
            IntPtr bindings, uint attributeCount, IntPtr attributes);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroyVertexConfig(VEVertexConfig config);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERasterConfig veDefaultRasterConfig();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEDepthConfig veDefaultDepthConfig();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBlendConfig veDefaultOpaqueBlendConfig();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBlendConfig veDefaultAlphaBlendConfig();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEBlendConfig veDefaultAdditiveBlendConfig();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEMultisampleConfig veDefaultMultisampleConfig();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEVertexInputConfig veDefaultVertexInputConfig();

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCreateOpaqueRenderConfig(VEDevice device,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCreateTransparentRenderConfig(VEDevice device,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCreateWireframeRenderConfig(VEDevice device,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCreateShadowRenderConfig(VEDevice device,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCreateUIRenderConfig(VEDevice device,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCreateConfigVariant(VERenderConfig baseConfig, ref VERenderConfigDesc overrides);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veMergeRenderConfigs(VEDevice device, uint configCount,
            IntPtr configs, [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfig veCloneRenderConfig(VERenderConfig config,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string debugName);

        // =============================================================================
        // Command Buffer and Rendering
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VECommandBuffer veBeginCommandBuffer(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veSubmitCommandBuffer(VECommandBuffer cmd, [MarshalAs(UnmanagedType.I1)] bool waitForCompletion);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBeginRendering(VECommandBuffer cmd, ref VERenderingInfo renderingInfo);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veEndRendering(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veApplyRenderConfig(VECommandBuffer cmd, VERenderConfig config);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veApplyVertexConfig(VECommandBuffer cmd, VEVertexConfig config);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBindShader(VECommandBuffer cmd, VEShader shader);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBindShaders(VECommandBuffer cmd, uint shaderCount, IntPtr shaders);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBindShaderConfig(VECommandBuffer cmd, VEShaderConfig config);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veUnbindShaderStage(VECommandBuffer cmd, VEShaderStage stage);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetViewport(VECommandBuffer cmd, float x, float y, float width, float height,
            float minDepth, float maxDepth);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetScissor(VECommandBuffer cmd, int x, int y, uint width, uint height);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetViewports(VECommandBuffer cmd, uint firstViewport, uint viewportCount, IntPtr viewports);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetScissors(VECommandBuffer cmd, uint firstScissor, uint scissorCount, IntPtr scissors);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veOverrideRasterState(VECommandBuffer cmd, ref VERasterConfig raster);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veOverrideDepthState(VECommandBuffer cmd, ref VEDepthConfig depth);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veOverrideBlendState(VECommandBuffer cmd, ref VEBlendConfig blend);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetWireframe(VECommandBuffer cmd, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetAlphaBlending(VECommandBuffer cmd, [MarshalAs(UnmanagedType.I1)] bool enabled);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetDepthTesting(VECommandBuffer cmd, [MarshalAs(UnmanagedType.I1)] bool testEnabled,
            [MarshalAs(UnmanagedType.I1)] bool writeEnabled);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetCulling(VECommandBuffer cmd, VECullMode cullMode);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void vePushConstants(VECommandBuffer cmd, IntPtr data, UIntPtr size, UIntPtr offset);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDraw(VECommandBuffer cmd, uint vertexCount, uint instanceCount,
            uint firstVertex, uint firstInstance);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDrawIndexed(VECommandBuffer cmd, uint indexCount, uint instanceCount,
            uint firstIndex, int vertexOffset, uint firstInstance);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDrawIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer,
            UIntPtr offset, uint drawCount, uint stride);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDrawIndexedIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer,
            UIntPtr offset, uint drawCount, uint stride);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDrawIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer,
            UIntPtr indirectOffset, VEBufferAddress countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDrawIndexedIndirectCount(VECommandBuffer cmd, VEBufferAddress indirectBuffer,
            UIntPtr indirectOffset, VEBufferAddress countBuffer, UIntPtr countOffset, uint maxDrawCount, uint stride);

        // =============================================================================
        // Compute Shaders
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDispatch(VECommandBuffer cmd, uint groupCountX, uint groupCountY, uint groupCountZ);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDispatchIndirect(VECommandBuffer cmd, VEBufferAddress indirectBuffer, UIntPtr offset);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBindComputeShader(VECommandBuffer cmd, VEShader shader);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetComputeConstants(VECommandBuffer cmd, IntPtr data, UIntPtr size, UIntPtr offset);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veGetComputeWorkgroupSize(VEShader shader, out uint x, out uint y, out uint z);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veCalculateDispatchSize(uint totalWork, uint workgroupSize, out uint numGroups);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veCalculateDispatchSize2D(uint totalWorkX, uint totalWorkY,
            uint workgroupSizeX, uint workgroupSizeY, out uint numGroupsX, out uint numGroupsY);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veCalculateDispatchSize3D(uint totalWorkX, uint totalWorkY, uint totalWorkZ,
            uint workgroupSizeX, uint workgroupSizeY, uint workgroupSizeZ,
            out uint numGroupsX, out uint numGroupsY, out uint numGroupsZ);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veGetComputeCapabilities(VEDevice device, out VEComputeCapabilities capabilities);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint veGetOptimalWorkgroupSize(VEDevice device);

        // =============================================================================
        // Synchronization and Memory Barriers
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierVertexToFragment(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierComputeToVertex(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierComputeToCompute(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierGraphicsToPresent(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierComputeToGraphics(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierGraphicsToCompute(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierComputeToTransfer(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierTransferToCompute(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierBuffer(VECommandBuffer cmd, VEBufferAddress buffer,
            uint srcStage, uint dstStage, uint srcAccess, uint dstAccess);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBarrierImage(VECommandBuffer cmd, VETextureIndex texture,
            uint oldLayout, uint newLayout, uint srcStage, uint dstStage, uint srcAccess, uint dstAccess);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veTransitionTexture(VECommandBuffer cmd, VETextureIndex texture,
            uint oldLayout, uint newLayout);

        // =============================================================================
        // Swapchain and Presentation
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VESwapchain veCreateSwapchain(VEDevice device, IntPtr windowHandle,
            uint width, uint height, VEFormat format);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veDestroySwapchain(VESwapchain swapchain);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VETextureIndex veAcquireNextImage(VESwapchain swapchain);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult vePresentImage(VESwapchain swapchain, VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veResizeSwapchain(VESwapchain swapchain, uint width, uint height);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veGetSwapchainSize(VESwapchain swapchain, out uint width, out uint height);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEFormat veGetSwapchainFormat(VESwapchain swapchain);

        // =============================================================================
        // Debug and Profiling
        // =============================================================================

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veSetDebugName(VEDevice device, ulong objectHandle, VEObjectType objectType,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veInsertDebugLabel(VECommandBuffer cmd, [MarshalAs(UnmanagedType.LPUTF8Str)] string labelName,
            IntPtr color);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veBeginDebugRegion(VECommandBuffer cmd, [MarshalAs(UnmanagedType.LPUTF8Str)] string regionName,
            IntPtr color);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veEndDebugRegion(VECommandBuffer cmd);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veLogMessage(VEDevice device, VEMessageSeverity severity,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string message);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEPerformanceStats veGetPerformanceStats(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veResetPerformanceStats(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEMemoryStats veGetMemoryStats(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VERenderConfigStats veGetRenderConfigStats(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veBeginGPUTiming(VECommandBuffer cmd, uint queryIndex);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veEndGPUTiming(VECommandBuffer cmd, uint queryIndex);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern VEResult veGetGPUTimingResults(VEDevice device, uint queryIndex, out double timeInMilliseconds);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint veGetBufferCount(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint veGetTextureCount(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint veGetSamplerCount(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint veGetShaderCount(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern void veUpdateFrameStats(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veValidateDevice(VEDevice device);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veValidateBuffer(VEDevice device, VEBufferAddress address);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veValidateTexture(VEDevice device, VETextureIndex index);

        [DllImport(Constants.LIBRARY_NAME, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool veValidateSampler(VEDevice device, VESamplerIndex index);
    }
}