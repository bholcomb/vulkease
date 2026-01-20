using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace VulkEase
{
    // Handle structures
    [StructLayout(LayoutKind.Sequential)] public struct VEContext { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEDevice { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VECommandBuffer { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VESwapchain { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VERenderTarget { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEShader { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEGraphicsPipeline { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEDrawState { public IntPtr native; }
    [StructLayout(LayoutKind.Sequential)] public struct VEBufferAddress { public UInt64 native; }
    [StructLayout(LayoutKind.Sequential)] public struct VETextureIndex { public UInt32 native; }
    [StructLayout(LayoutKind.Sequential)] public struct VESamplerIndex { public UInt32 native; }

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

    // Surface type enum
    public enum VESurfaceType
    {
        Win32,
        Xlib,
        Wayland,
        Cocoa,
        Android
    }

    // Surface descriptor (for C# use - will be converted to native format)
    public struct VESurfaceDesc
    {
        public VESurfaceType Type;
        // Platform-specific handles
        public IntPtr Handle1;  // HWND (Windows), Display* (Xlib), wl_display* (Wayland), NSWindow* (Cocoa), ANativeWindow* (Android)
        public IntPtr Handle2;  // Window (Xlib as ulong), wl_surface* (Wayland) - unused for others
    }

    // Internal surface descriptor for P/Invoke - matches C layout exactly
    // On 64-bit: type (4 bytes) + 4 bytes padding + union (16 bytes for Xlib case)
    [StructLayout(LayoutKind.Explicit, Size = 24)]
    internal struct VESurfaceDescInternal
    {
        [FieldOffset(0)] public VESurfaceType type;
        // Union starts at offset 8 (aligned to pointer size)
        // Win32
        [FieldOffset(8)] public IntPtr win32_hwnd;
        // Xlib  
        [FieldOffset(8)] public IntPtr xlib_display;
        [FieldOffset(16)] public UInt64 xlib_window;
        // Wayland
        [FieldOffset(8)] public IntPtr wayland_display;
        [FieldOffset(16)] public IntPtr wayland_surface;
        // Cocoa
        [FieldOffset(8)] public IntPtr cocoa_window;
        // Android
        [FieldOffset(8)] public IntPtr android_nativeWindow;
    }

    // Swapchain descriptor (for C# use)
    public struct VESwapchainDesc
    {
        public VESurfaceDesc Surface;
        public UInt32 Width;
        public UInt32 Height;
        public VkFormat ColorFormat;
        public bool VSync;
        public string? DebugName;
    }

    // Internal swapchain descriptor for P/Invoke
    [StructLayout(LayoutKind.Sequential)]
    internal struct VESwapchainDescInternal
    {
        public VESurfaceDescInternal surface;
        public UInt32 width;
        public UInt32 height;
        public VkFormat colorFormat;
        [MarshalAs(UnmanagedType.I1)]
        public bool vsync;
        public IntPtr debugName;
    }

    // Render target descriptor (for C# use)
    public struct VERenderTargetDesc
    {
        public UInt32 Width;
        public UInt32 Height;
        public VkFormat ColorFormat;
        public VkFormat DepthFormat;
        public VkSampleCountFlags SampleCount;
        public bool HasResolveTarget;
        public string? DebugName;
    }

    // Internal render target descriptor for P/Invoke
    [StructLayout(LayoutKind.Sequential)]
    internal struct VERenderTargetDescInternal
    {
        public UInt32 width;
        public UInt32 height;
        public VkFormat colorFormat;
        public VkFormat depthFormat;
        public VkSampleCountFlags sampleCount;
        [MarshalAs(UnmanagedType.I1)]
        public bool hasResolveTarget;
        public IntPtr debugName;
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
        [MarshalAs(UnmanagedType.LPStr)]
        public string? debugName;
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
        [MarshalAs(UnmanagedType.LPStr)]
        public string? debugName;
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
        [MarshalAs(UnmanagedType.LPStr)]
        public string? debugName;
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

    // Vulkan extent structs (used by value-returning native getters)
    [StructLayout(LayoutKind.Sequential)]
    public struct VkExtent2D
    {
        public UInt32 width;
        public UInt32 height;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VkExtent3D
    {
        public UInt32 width;
        public UInt32 height;
        public UInt32 depth;
    }

    // Stencil op state
    [StructLayout(LayoutKind.Sequential)]
    public struct VEStencilOpState
    {
        public VkStencilOp failOp;
        public VkStencilOp passOp;
        public VkStencilOp depthFailOp;
        public VkCompareOp compareOp;
        public UInt32 compareMask;
        public UInt32 writeMask;
        public UInt32 reference;
    }

    // Graphics pipeline descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VEGraphicsPipelineDesc
    {
        // Shaders
        public VEShader vertexShader;
        public VEShader fragmentShader;
        public VEShader geometryShader;
        public VEShader tessControlShader;
        public VEShader tessEvalShader;
        
        // Vertex input
        public UInt32 vertexBindingCount;
        public IntPtr vertexBindings;  // const VEVertexBinding*
        public UInt32 vertexAttributeCount;
        public IntPtr vertexAttributes;  // const VEVertexAttribute*
        
        // Depth config
        [MarshalAs(UnmanagedType.U1)]
        public bool depthTestEnable;
        [MarshalAs(UnmanagedType.U1)]
        public bool depthWriteEnable;
        public VkCompareOp depthCompareOp;
        [MarshalAs(UnmanagedType.U1)]
        public bool depthBoundsTestEnable;
        public float minDepthBounds;
        public float maxDepthBounds;
        
        // Stencil config
        [MarshalAs(UnmanagedType.U1)]
        public bool stencilTestEnable;
        public VEStencilOpState frontStencil;
        public VEStencilOpState backStencil;
        
        // Blend config
        [MarshalAs(UnmanagedType.U1)]
        public bool logicOpEnable;
        public VkLogicOp logicOp;
        public UInt32 blendAttachmentCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public VEBlendAttachment[] blendAttachments;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public float[] blendConstants;
        
        // Rasterization (material-level defaults)
        public VkCullModeFlags cullMode;
        public VkFrontFace frontFace;
        
        // Multisampling
        [MarshalAs(UnmanagedType.U1)]
        public bool sampleShadingEnable;
        public float minSampleShading;
        
        [MarshalAs(UnmanagedType.LPStr)]
        public string? debugName;
    }

    // Draw state descriptor
    [StructLayout(LayoutKind.Sequential)]
    public struct VEDrawStateDesc
    {
        // Geometry interpretation
        public VkPrimitiveTopology topology;
        [MarshalAs(UnmanagedType.U1)]
        public bool primitiveRestartEnable;
        public UInt32 patchControlPoints;
        
        // Rasterization overrides
        public VkPolygonMode polygonMode;
        public float lineWidth;
        public VkCullModeFlags cullMode;
        public VkFrontFace frontFace;
        [MarshalAs(UnmanagedType.U1)]
        public bool rasterizerDiscardEnable;
        
        // Depth bias
        [MarshalAs(UnmanagedType.U1)]
        public bool depthBiasEnable;
        public float depthBiasConstantFactor;
        public float depthBiasClamp;
        public float depthBiasSlopeFactor;
        [MarshalAs(UnmanagedType.U1)]
        public bool depthClampEnable;
        
        // Coverage
        [MarshalAs(UnmanagedType.U1)]
        public bool alphaToCoverageEnable;
        [MarshalAs(UnmanagedType.U1)]
        public bool alphaToOneEnable;
        
        [MarshalAs(UnmanagedType.LPStr)]
        public string? debugName;
    }

    // Rendering attachment
    [StructLayout(LayoutKind.Sequential)]
    public struct VERenderingAttachment
    {
        public VETextureIndex texture;
        public VkAttachmentLoadOp loadOp;
        public VkAttachmentStoreOp storeOp;
        public VEColor clearValue;
        public VETextureIndex resolveTexture;
    }

    // Constants for attachment array layout
    public static class VERenderingConstants
    {
        public const int VE_MAX_COLOR_ATTACHMENTS = 8;
        public const int VE_DEPTH_ATTACHMENT_INDEX = VE_MAX_COLOR_ATTACHMENTS;
        public const int VE_STENCIL_ATTACHMENT_INDEX = VE_MAX_COLOR_ATTACHMENTS + 1;
        public const int VE_TOTAL_ATTACHMENT_SLOTS = VE_MAX_COLOR_ATTACHMENTS + 2;
    }

    // Internal PInvoke-friendly rendering info struct - matches C layout exactly
    // Layout of attachments array:
    //   [0..colorAttachmentCount-1] = color attachments
    //   [VE_DEPTH_ATTACHMENT_INDEX] = depth attachment (if hasDepthAttachment)
    //   [VE_STENCIL_ATTACHMENT_INDEX] = stencil attachment (if hasStencilAttachment)
    [StructLayout(LayoutKind.Sequential)]
    internal struct VERenderingInfoInternal
    {
        public Int32 renderAreaX, renderAreaY;
        public UInt32 renderAreaWidth, renderAreaHeight;
        public UInt32 colorAttachmentCount;
        [MarshalAs(UnmanagedType.I1)]
        public bool hasDepthAttachment;
        [MarshalAs(UnmanagedType.I1)]
        public bool hasStencilAttachment;
        // Fixed-size array of attachments (VE_TOTAL_ATTACHMENT_SLOTS = 10)
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 10)]
        public VERenderingAttachment[] attachments;
    }

    // User-friendly rendering info class (C# friendly)
    public class VERenderingInfo
    {
        public Int32 RenderAreaX { get; set; }
        public Int32 RenderAreaY { get; set; }
        public UInt32 RenderAreaWidth { get; set; }
        public UInt32 RenderAreaHeight { get; set; }
        public List<VERenderingAttachment> ColorAttachments { get; set; } = new List<VERenderingAttachment>();
        public VERenderingAttachment? DepthAttachment { get; set; }
        public VERenderingAttachment? StencilAttachment { get; set; }

        public VERenderingInfo()
        {
        }

        public VERenderingInfo(UInt32 renderAreaWidth, UInt32 renderAreaHeight)
        {
            RenderAreaX = 0;
            RenderAreaY = 0;
            RenderAreaWidth = renderAreaWidth;
            RenderAreaHeight = renderAreaHeight;
        }

        public VERenderingInfo(Int32 renderAreaX, Int32 renderAreaY, UInt32 renderAreaWidth, UInt32 renderAreaHeight)
        {
            RenderAreaX = renderAreaX;
            RenderAreaY = renderAreaY;
            RenderAreaWidth = renderAreaWidth;
            RenderAreaHeight = renderAreaHeight;
        }

        // Convert to internal struct for P/Invoke
        internal VERenderingInfoInternal ToInternal()
        {
            var result = new VERenderingInfoInternal
            {
                renderAreaX = RenderAreaX,
                renderAreaY = RenderAreaY,
                renderAreaWidth = RenderAreaWidth,
                renderAreaHeight = RenderAreaHeight,
                colorAttachmentCount = (uint)ColorAttachments.Count,
                hasDepthAttachment = DepthAttachment.HasValue,
                hasStencilAttachment = StencilAttachment.HasValue,
                attachments = new VERenderingAttachment[VERenderingConstants.VE_TOTAL_ATTACHMENT_SLOTS]
            };

            // Copy color attachments
            for (int i = 0; i < ColorAttachments.Count && i < VERenderingConstants.VE_MAX_COLOR_ATTACHMENTS; i++)
            {
                result.attachments[i] = ColorAttachments[i];
            }

            // Set depth attachment if present
            if (DepthAttachment.HasValue)
            {
                result.attachments[VERenderingConstants.VE_DEPTH_ATTACHMENT_INDEX] = DepthAttachment.Value;
            }

            // Set stencil attachment if present
            if (StencilAttachment.HasValue)
            {
                result.attachments[VERenderingConstants.VE_STENCIL_ATTACHMENT_INDEX] = StencilAttachment.Value;
            }

            return result;
        }
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
    public struct VEGraphicsPipelineStats
    {
        public UInt32 totalPipelines;
        public UInt32 activePipelines;
        public UInt32 pipelineBinds;
        public UInt32 stateSwitches;
        public UInt32 overrides;
    }

    // Push constants structures - Updated to match C API (256 bytes each)
    [StructLayout(LayoutKind.Sequential)]
    public struct VEGraphicsPushConstants
    {
        public UInt64 vertexBuffer;                              // 8 bytes
        public UInt64 indexBuffer;                               // 8 bytes
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public UInt64[] uniformBuffers;                          // 64 bytes (8 * 8)
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public UInt32[] textures;                                // 64 bytes (16 * 4)
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public UInt32[] samplers;                                // 64 bytes (16 * 4)
        public float objectScale;                                // 4 bytes
        public UInt32 activeTextureCount;                        // 4 bytes
        public UInt32 activeSamplerCount;                        // 4 bytes
        public UInt32 activeUniformCount;                        // 4 bytes
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 7)]
        public UInt32[] reserved;                                // 28 bytes (7 * 4)
    }                                                            // Total: 256 bytes

    [StructLayout(LayoutKind.Sequential)]
    public struct VEComputePushConstants
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public UInt64[] buffers;                                 // 128 bytes (16 * 8)
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public UInt32[] textures;                                // 64 bytes (16 * 4)
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public UInt32[] samplers;                                // 32 bytes (8 * 4)
        public UInt32 elementCount;                              // 4 bytes
        public UInt32 activeBufferCount;                         // 4 bytes
        public UInt32 activeTextureCount;                        // 4 bytes
        public UInt32 activeSamplerCount;                        // 4 bytes
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public UInt32[] reserved;                                // 16 bytes (4 * 4)
    }                                                            // Total: 256 bytes

    // Submit info for advanced synchronization
    [StructLayout(LayoutKind.Sequential)]
    public struct VESubmitInfo
    {
        public IntPtr waitSemaphores;              // const VkSemaphore*
        public IntPtr waitSemaphoreValues;         // const uint64_t*
        public IntPtr waitStageMasks;              // const VkPipelineStageFlags2*
        public UInt32 waitSemaphoreCount;

        public IntPtr signalSemaphores;            // const VkSemaphore*
        public IntPtr signalSemaphoreValues;       // const uint64_t*
        public UInt32 signalSemaphoreCount;

        public IntPtr fence;                       // VkFence
        [MarshalAs(UnmanagedType.U1)]
        public bool waitForCompletion;
    }

    // Secondary command buffer descriptor (flattened, no Vulkan inheritance structs)
    [StructLayout(LayoutKind.Sequential)]
    public struct VESecondaryCommandBufferDesc
    {
        public UInt32 usageFlags;                             // VkCommandBufferUsageFlags
        public UInt32 colorAttachmentCount;                   // Up to 8
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public VkFormat[] colorAttachmentFormats;             // Fixed array of 8
        public VkFormat depthAttachmentFormat;                // VK_FORMAT_UNDEFINED if none
        public VkFormat stencilAttachmentFormat;              // VK_FORMAT_UNDEFINED if none
        public VkSampleCountFlags rasterizationSamples;       // 0 defaults to 1
        public UInt32 viewMask;                               // Multiview mask
        [MarshalAs(UnmanagedType.U1)]
        public bool occlusionQueryEnable;
        public VkQueryControlFlags occlusionQueryFlags;
        [MarshalAs(UnmanagedType.U1)]
        public bool beginRecording;
    }
}