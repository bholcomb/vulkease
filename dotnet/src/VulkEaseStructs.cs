using System;
using System.Runtime.InteropServices;

namespace VulkEase
{
    // Handle structures
    [StructLayout(LayoutKind.Sequential)] public struct VEContext { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEDevice { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VECommandBuffer { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VESwapchain { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEShader { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VERenderConfig { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEVertexConfig { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEShaderConfig { public IntPtr native; } 

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
        public UInt32 width, height;

        public VERect2D(int x, int y, UInt32 width, UInt32 height)
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
        public UInt64 size;
        public VkBufferUsageFlags usage;
        public IntPtr initialData;
        public UInt64 initialDataSize;
        [MarshalAs(UnmanagedType.U1)]
        public bool persistentlyMapped;
        public IntPtr debugName;
    }

    // Texture descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VETextureDesc
    {
        public UInt32 width;
        public UInt32 height;
        public UInt32 depth;
        public UInt32 mipLevels;
        public UInt32 arrayLayers;
        public VkFormat format;
        public VkImageUsageFlags usage;
        public VkSampleCountFlags sampleCount;
        public IntPtr initialData;
        public UInt64 initialDataSize;
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
        public UInt32 frontCompareMask;
        public UInt32 frontWriteMask;
        public UInt32 frontReference;
        public VkStencilOp backFailOp;
        public VkStencilOp backPassOp;
        public VkStencilOp backDepthFailOp;
        public VkCompareOp backCompareOp;
        public UInt32 backCompareMask;
        public UInt32 backWriteMask;
        public UInt32 backReference;
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
        public UInt32 attachmentCount;
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
        public UInt32 binding;
        public UInt32 stride;
        public VkVertexInputRate inputRate;
        public UInt32 divisor;
    }

    // Vertex attribute
    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexAttribute
    {
        public UInt32 location;
        public UInt32 binding;
        public VkFormat format;
        public UInt32 offset;
    }

    // Vertex input configuration
    [StructLayout(LayoutKind.Sequential)]
    public struct VEVertexInputConfig
    {
        public UInt32 bindingCount;
        public IntPtr bindings; // VEVertexBinding*
        public UInt32 attributeCount;
        public IntPtr attributes; // VEVertexAttribute*
        public VkPrimitiveTopology topology;
        [MarshalAs(UnmanagedType.U1)]
        public bool primitiveRestartEnable;
        public UInt32 patchControlPoints;
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
        public UInt32 shaderCount;
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
        public UInt32 texture; // VETextureIndex
        public VkAttachmentLoadOp loadOp;
        public VkAttachmentStoreOp storeOp;
        public VEColor clearValue;
        public UInt32 resolveTexture; // VETextureIndex
    }

    // Rendering info
    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderingInfo
    {
        public UInt32 renderAreaX, renderAreaY;
        public UInt32 renderAreaWidth, renderAreaHeight;
        public UInt32 colorAttachmentCount;
        public IntPtr colorAttachments; // const VERenderingAttachment*
        public IntPtr depthAttachment; // const VERenderingAttachment*
        public IntPtr stencilAttachment; // const VERenderingAttachment*
    }

    // Statistics structures
    [StructLayout(LayoutKind.Sequential)]
    public struct VEPerformanceStats
    {
        public UInt64 frameTime;
        public UInt32 drawCalls;
        public UInt32 computeDispatches;
        public UInt64 verticesRendered;
        public UInt64 trianglesRendered;
        public UInt32 pipelineBinds;
        public UInt32 descriptorBinds;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEMemoryHeapStats
    {
        public UInt64 size;
        public UInt64 used;
        public UInt64 budget;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEMemoryStats
    {
        public UInt64 totalAllocated;
        public UInt64 totalUsed;
        public UInt32 bufferCount;
        public UInt32 textureCount;
        public UInt32 samplerCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public VEMemoryHeapStats[] heaps;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderConfigStats
    {
        public UInt32 totalConfigs;
        public UInt32 activeConfigs;
        public UInt32 configSwitches;
        public UInt32 stateSwitches;
        public UInt32 overrides;
    }

    // Push constants structures
    [StructLayout(LayoutKind.Sequential)]
    public struct VEGraphicsPushConstants
    {
        public UInt64 vertexBuffer;
        public UInt64 indexBuffer;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public UInt64[] uniformBuffers;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public UInt32[] textures;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public UInt32[] samplers;
        public float objectScale;
        public UInt32 activeTextureCount;
        public UInt32 activeSamplerCount;
        public UInt32 activeUniformCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VEComputePushConstants
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public UInt64[] buffers;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public UInt32[] textures;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public UInt32[] samplers;
        public UInt32 elementCount;
        public UInt32 activeBufferCount;
        public UInt32 activeTextureCount;
        public UInt32 activeSamplerCount;
    }
}