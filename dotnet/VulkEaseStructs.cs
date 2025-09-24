using System;
using System.Runtime.InteropServices;

namespace VulkEase
{
    // Handle structures
    [StructLayout(LayoutKind.Sequential)]
    public struct VEContext
    {
        public IntPtr native;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEDevice
    {
        public IntPtr native;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VECommandBuffer
    {
        public IntPtr native;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VESwapchain
    {
        public IntPtr native;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEShader
    {
        public IntPtr native;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderConfig
    {
        public IntPtr native;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexConfig
    {
        public IntPtr native;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEShaderConfig
    {
        public IntPtr native;
    }

    // Basic structures
    [StructLayout(LayoutKind.Sequential)]
    public struct VEColor
    {
        public float r, g, b, a;

        public VEColor(float r, float g, float b, float a)
        {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
        }

        public static VEColor Black => new VEColor(0.0f, 0.0f, 0.0f, 1.0f);
        public static VEColor White => new VEColor(1.0f, 1.0f, 1.0f, 1.0f);
        public static VEColor Red => new VEColor(1.0f, 0.0f, 0.0f, 1.0f);
        public static VEColor Green => new VEColor(0.0f, 1.0f, 0.0f, 1.0f);
        public static VEColor Blue => new VEColor(0.0f, 0.0f, 1.0f, 1.0f);
        public static VEColor Yellow => new VEColor(1.0f, 1.0f, 0.0f, 1.0f);
        public static VEColor Cyan => new VEColor(0.0f, 1.0f, 1.0f, 1.0f);
        public static VEColor Magenta => new VEColor(1.0f, 0.0f, 1.0f, 1.0f);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERect2D
    {
        public int x, y;
        public uint width, height;

        public VERect2D(int x, int y, uint width, uint height)
        {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEViewport
    {
        public float x, y;
        public float width, height;
        public float minDepth, maxDepth;

        public VEViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f)
        {
            this.x = x;
            this.y = y;
            this.width = width;
            this.height = height;
            this.minDepth = minDepth;
            this.maxDepth = maxDepth;
        }
    }

    // Buffer descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VEBufferDesc
    {
        public UIntPtr size;
        public VkBufferUsageFlags usage;
        public IntPtr initialData;
        public UIntPtr initialDataSize;
        [MarshalAs(UnmanagedType.U1)]
        public bool persistentlyMapped;
        public IntPtr debugName;
    }

    // Texture descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VETextureDesc
    {
        public uint width;
        public uint height;
        public uint depth;
        public uint mipLevels;
        public uint arrayLayers;
        public VkFormat format;
        public VkImageUsageFlags usage;
        public VkSampleCountFlags sampleCount;
        public IntPtr initialData;
        public UIntPtr initialDataSize;
        public IntPtr debugName;
    }

    // Sampler descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VESamplerDesc
    {
        public VkFilter minFilter;
        public VkFilter magFilter;
        public VkFilter mipmapFilter;
        public VkSamplerAddressMode addressModeU;
        public VkSamplerAddressMode addressModeV;
        public VkSamplerAddressMode addressModeW;
        public float maxAnisotropy;
        [MarshalAs(UnmanagedType.U1)]
        public bool compareEnable;
        public VkCompareOp compareOp;
        public float minLod;
        public float maxLod;
        public IntPtr debugName;
    }

    // Raster configuration
    [StructLayout(LayoutKind.Sequential)]
    public struct VERasterConfig
    {
        public VkCullModeFlags cullMode;
        public VkFrontFace frontFace;
        public VkPolygonMode polygonMode;
        public float lineWidth;
        [MarshalAs(UnmanagedType.U1)]
        public bool depthBiasEnable;
        public float depthBiasConstantFactor;
        public float depthBiasClamp;
        public float depthBiasSlopeFactor;
        [MarshalAs(UnmanagedType.U1)]
        public bool depthClampEnable;
        [MarshalAs(UnmanagedType.U1)]
        public bool rasterizerDiscardEnable;
    }

    // Depth configuration
    [StructLayout(LayoutKind.Sequential)]
    public struct VEDepthConfig
    {
        [MarshalAs(UnmanagedType.U1)]
        public bool depthTestEnable;
        [MarshalAs(UnmanagedType.U1)]
        public bool depthWriteEnable;
        public VkCompareOp depthCompareOp;
        [MarshalAs(UnmanagedType.U1)]
        public bool depthBoundsTestEnable;
        public float minDepthBounds;
        public float maxDepthBounds;
        [MarshalAs(UnmanagedType.U1)]
        public bool stencilTestEnable;
        public VkStencilOp frontFailOp;
        public VkStencilOp frontPassOp;
        public VkStencilOp frontDepthFailOp;
        public VkCompareOp frontCompareOp;
        public uint frontCompareMask;
        public uint frontWriteMask;
        public uint frontReference;
        public VkStencilOp backFailOp;
        public VkStencilOp backPassOp;
        public VkStencilOp backDepthFailOp;
        public VkCompareOp backCompareOp;
        public uint backCompareMask;
        public uint backWriteMask;
        public uint backReference;
    }

    // Blend attachment
    [StructLayout(LayoutKind.Sequential)]
    public struct VEBlendAttachment
    {
        [MarshalAs(UnmanagedType.U1)]
        public bool blendEnable;
        public VkBlendFactor srcColorBlendFactor;
        public VkBlendFactor dstColorBlendFactor;
        public VkBlendOp colorBlendOp;
        public VkBlendFactor srcAlphaBlendFactor;
        public VkBlendFactor dstAlphaBlendFactor;
        public VkBlendOp alphaBlendOp;
        public VkColorComponentFlags colorWriteMask;
    }

    // Blend configuration
    [StructLayout(LayoutKind.Sequential)]
    public struct VEBlendConfig
    {
        [MarshalAs(UnmanagedType.U1)]
        public bool logicOpEnable;
        public VkLogicOp logicOp;
        public uint attachmentCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public VEBlendAttachment[] attachments;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public float[] blendConstants;
    }

    // Multisample configuration
    [StructLayout(LayoutKind.Sequential)]
    public struct VEMultisampleConfig
    {
        public VkSampleCountFlags rasterizationSamples;
        [MarshalAs(UnmanagedType.U1)]
        public bool sampleShadingEnable;
        public float minSampleShading;
        [MarshalAs(UnmanagedType.U1)]
        public bool alphaToCoverageEnable;
        [MarshalAs(UnmanagedType.U1)]
        public bool alphaToOneEnable;
    }

    // Vertex binding
    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexBinding
    {
        public uint binding;
        public uint stride;
        public VkVertexInputRate inputRate;
        public uint divisor;
    }

    // Vertex attribute
    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexAttribute
    {
        public uint location;
        public uint binding;
        public VkFormat format;
        public uint offset;
    }

    // Vertex input configuration
    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexInputConfig
    {
        public uint bindingCount;
        public IntPtr bindings; // VEVertexBinding*
        public uint attributeCount;
        public IntPtr attributes; // VEVertexAttribute*
        public VkPrimitiveTopology topology;
        [MarshalAs(UnmanagedType.U1)]
        public bool primitiveRestartEnable;
        public uint patchControlPoints;
    }

    // Render config descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderConfigDesc
    {
        public VEConfigTypeFlags configTypes;
        public IntPtr viewportConfig; // const VEViewport*
        public IntPtr scissorConfig; // const VERect2D*
        public IntPtr rasterConfig; // const VERasterConfig*
        public IntPtr depthConfig; // const VEDepthConfig*
        public IntPtr blendConfig; // const VEBlendConfig*
        public IntPtr multisampleConfig; // const VEMultisampleConfig*
        public IntPtr vertexInputConfig; // const VEVertexInputConfig*
        public uint shaderCount;
        public IntPtr shaders; // VEShader* const*
        public IntPtr debugName;
    }

    // Shader config descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VEShaderConfigDesc
    {
        public VEShader vertexShader;
        public VEShader fragmentShader;
        public VEShader geometryShader;
        public VEShader tessControlShader;
        public VEShader tessEvalShader;
        public VEShader computeShader;
        public IntPtr debugName;
    }

    // Rendering attachment
    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderingAttachment
    {
        public uint texture; // VETextureIndex
        public VkAttachmentLoadOp loadOp;
        public VkAttachmentStoreOp storeOp;
        public VEColor clearValue;
        public uint resolveTexture; // VETextureIndex
    }

    // Rendering info
    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderingInfo
    {
        public uint renderAreaX, renderAreaY;
        public uint renderAreaWidth, renderAreaHeight;
        public uint colorAttachmentCount;
        public IntPtr colorAttachments; // const VERenderingAttachment*
        public IntPtr depthAttachment; // const VERenderingAttachment*
        public IntPtr stencilAttachment; // const VERenderingAttachment*
    }

    // Statistics structures
    [StructLayout(LayoutKind.Sequential)]
    public struct VEPerformanceStats
    {
        public ulong frameTime;
        public uint drawCalls;
        public uint computeDispatches;
        public ulong verticesRendered;
        public ulong trianglesRendered;
        public uint pipelineBinds;
        public uint descriptorBinds;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEMemoryHeapStats
    {
        public ulong size;
        public ulong used;
        public ulong budget;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEMemoryStats
    {
        public ulong totalAllocated;
        public ulong totalUsed;
        public uint bufferCount;
        public uint textureCount;
        public uint samplerCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public VEMemoryHeapStats[] heaps;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderConfigStats
    {
        public uint totalConfigs;
        public uint activeConfigs;
        public uint configSwitches;
        public uint stateSwitches;
        public uint overrides;
    }

    // Push constants structures
    [StructLayout(LayoutKind.Sequential)]
    public struct VEGraphicsPushConstants
    {
        public ulong vertexBuffer;
        public ulong indexBuffer;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public ulong[] uniformBuffers;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public uint[] textures;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public uint[] samplers;
        public float objectScale;
        public uint activeTextureCount;
        public uint activeSamplerCount;
        public uint activeUniformCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEComputePushConstants
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public ulong[] buffers;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public uint[] textures;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public uint[] samplers;
        public uint elementCount;
        public uint activeBufferCount;
        public uint activeTextureCount;
        public uint activeSamplerCount;
    }
}