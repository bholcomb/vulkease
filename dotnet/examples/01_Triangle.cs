using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using System;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using VulkEase;

namespace VulkEaseExamples
{
    [StructLayout(LayoutKind.Sequential)]
    public struct TrianglePushConstants
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public float[] mvpMatrix;
        public UInt64 vertexBufferAddress;
    };
    
    /// <summary>
    /// Example 1: Spinning Triangle
    /// Demonstrates basic VulkEase setup with OpenTK window, vertex buffers, and animated rendering
    /// Features embedded GLSL shaders and a spinning colorful triangle
    /// </summary>
    public class TriangleExample : GameWindow
    {
        private const int WindowWidth = 800;
        private const int WindowHeight = 600;
        private const bool EnableVSync = true;

        // Vertex structure
        [StructLayout(LayoutKind.Sequential)]
        private struct Vertex
        {
            public Vector3 Position;
            public Vector4 Color;

            public Vertex(Vector3 position, Vector4 color)
            {
                Position = position;
                Color = color;
            }
        }

        // VulkEase state
        private VEContext _context;
        private VEDevice _device;
        private VESwapchain _swapchain;
        private VEShader _vertexShader;
        private VEShader _fragmentShader;
        private VEBufferAddress _vertexBuffer;
        private VEGraphicsPipeline _pipeline;
        private VERenderTarget _renderTarget;
        private VETextureIndex _colorTexture;

        // Animation state
        private Stopwatch _stopwatch = Stopwatch.StartNew();
        
        // Frame timing
        private VEFrameTimingInfo _timing;

        // Vertex data for a triangle
        private static readonly Vertex[] TriangleVertices = {
            new(new Vector3(-0.5f, -0.5f, 0.0f), new Vector4(1.0f, 0.0f, 0.0f, 1.0f)), // Bottom left - Red
            new(new Vector3( 0.5f, -0.5f, 0.0f), new Vector4(0.0f, 1.0f, 0.0f, 1.0f)), // Bottom right - Green
            new(new Vector3( 0.0f,  0.5f, 0.0f), new Vector4(0.0f, 0.0f, 1.0f, 1.0f))  // Top - Blue
        };

        public TriangleExample() : base(GameWindowSettings.Default,
            new NativeWindowSettings()
            {
                Title = "VulkEase Spinning Triangle",
                ClientSize = new Vector2i(WindowWidth, WindowHeight),
                StartVisible = false,
                StartFocused = true,
                API = ContextAPI.NoAPI, // Important: No OpenGL context
                Profile = ContextProfile.Core,
                Flags = ContextFlags.ForwardCompatible
            })
        {
        }

        protected override void OnLoad()
        {
            base.OnLoad();

            Console.WriteLine("Initializing VulkEase Triangle Example...");

            if (!InitializeVulkEase())
            {
                Close();
                return;
            }

            if (!CreateShaders())
            {
                Close();
                return;
            }

            if (!CreateBuffers())
            {
                Close();
                return;
            }

            _stopwatch = Stopwatch.StartNew();

            Console.WriteLine("Triangle example initialized successfully!");
            IsVisible = true;
        }

        protected override void OnResize(ResizeEventArgs e)
        {
            base.OnResize(e);

            if (_swapchain.native != IntPtr.Zero && e.Width > 0 && e.Height > 0)
            {
                VulkEase.VulkEase.ResizeSwapchain(_swapchain, (uint)e.Width, (uint)e.Height);
                VulkEase.VulkEase.ResizeRenderTarget(_device, ref _renderTarget, (uint)e.Width, (uint)e.Height);
                _colorTexture = _renderTarget.ColorAttachments[0].texture;
            }
        }

        protected override void OnRenderFrame(FrameEventArgs args)
        {
            base.OnRenderFrame(args);

            // Begin frame timing
            VulkEase.VulkEase.BeginFrame(_device);

            Render();

            // End frame and get timing info
            VulkEase.VulkEase.EndFrame(_device, out _timing);

            // Print frame timing every 1000 frames
            if (_timing.frameNumber % 1000 == 0)
            {
                Console.WriteLine($"Frame {_timing.frameNumber}: {_timing.frameTimeMs:F2} ms ({_timing.fps:F1} FPS) | Avg: {_timing.avgFrameTimeMs:F2} ms ({_timing.avgFps:F1} FPS) | Min: {_timing.minFrameTimeMs:F2} ms | Max: {_timing.maxFrameTimeMs:F2} ms");
            }
        }

        protected override void OnKeyDown(KeyboardKeyEventArgs e)
        {
            base.OnKeyDown(e);

            if (e.Key == Keys.Escape)
            {
                Close();
            }
        }

        private bool InitializeVulkEase()
        {
            try
            {
                // Create context
                _context = VulkEase.VulkEase.CreateContext("VulkEase Spinning Triangle");
                Console.WriteLine("VulkEase initialized successfully");

                // Create device (auto-select best GPU)
                _device = VulkEase.VulkEase.CreateDevice(_context);
                Console.WriteLine($"VulkEase device created successfully");
                Console.WriteLine($"Device: {VulkEase.VulkEase.GetDeviceName(_device)}");
                Console.WriteLine($"Driver: {VulkEase.VulkEase.GetDriverVersion(_device)}");

                // Create swapchain
                var swapchainDesc = new VESwapchainDesc
                {
                    Surface = GetSurfaceDesc(),
                    Width = (uint)WindowWidth,
                    Height = (uint)WindowHeight,
                    ColorFormat = VkFormat.VK_FORMAT_B8G8R8A8_SRGB,
                    VSync = EnableVSync
                };
                _swapchain = VulkEase.VulkEase.CreateSwapchain(_device, swapchainDesc);

                // Create simple render target (no depth for triangle)
                VETextureIndex depthTex;
                _renderTarget = VulkEase.VulkEase.CreateSimpleRenderTarget(_device,
                    (uint)WindowWidth, (uint)WindowHeight,
                    VkFormat.VK_FORMAT_B8G8R8A8_SRGB,
                    VkFormat.VK_FORMAT_UNDEFINED, // No depth buffer
                    new VEColor(0.1f, 0.1f, 0.1f, 1.0f), 1.0f,
                    out _colorTexture, out depthTex);

                Console.WriteLine($"Swapchain and render target created: {WindowWidth}x{WindowHeight}");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to initialize VulkEase: {ex.Message}");
                return false;
            }
        }

        private unsafe VESurfaceDesc GetSurfaceDesc()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                // On Linux, use X11
                IntPtr display = (IntPtr)GLFW.GetX11Display();
                IntPtr window = (IntPtr)GLFW.GetX11Window(WindowPtr);
                return new VESurfaceDesc
                {
                    Type = VESurfaceType.Xlib,
                    Handle1 = display,
                    Handle2 = window
                };
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                return new VESurfaceDesc
                {
                    Type = VESurfaceType.Win32,
                    Handle1 = GLFW.GetWin32Window(WindowPtr)
                };
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                return new VESurfaceDesc
                {
                    Type = VESurfaceType.Cocoa,
                    Handle1 = GLFW.GetCocoaWindow(WindowPtr)
                };
            }
            throw new PlatformNotSupportedException("Unsupported platform");
        }

        private bool CreateShaders()
        {
            try
            {
                // Load shaders from SPIR-V files
                _vertexShader = VulkEase.VulkEase.LoadShaderFromFile(_device, "examples/shaders/triangle.vert.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "SpinningTriangleVertex");

                _fragmentShader = VulkEase.VulkEase.LoadShaderFromFile(_device, "examples/shaders/triangle.frag.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "SpinningTriangleFragment");

                // Create opaque graphics pipeline
                _pipeline = VulkEase.VulkEase.CreateOpaquePipeline(_device, _vertexShader, _fragmentShader, "opaque pipeline");

                Console.WriteLine("Shaders and pipeline created successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create shaders: {ex.Message}");
                return false;
            }
        }

        private bool CreateBuffers()
        {
            try
            {
                // Pin the vertex data and get a pointer to it
                GCHandle handle = GCHandle.Alloc(TriangleVertices, GCHandleType.Pinned);
                try
                {
                    IntPtr dataPtr = handle.AddrOfPinnedObject();
                    ulong dataSize = (ulong)(TriangleVertices.Length * Marshal.SizeOf<Vertex>());

                    // Create vertex buffer with initial data
                    var bufferDesc = new VEBufferDesc
                    {
                        size = dataSize,
                        usage = VkBufferUsageFlags.VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        initialData = dataPtr,
                        initialDataSize = dataSize,
                        persistentlyMapped = false,
                        debugName = "TriangleVertexBuffer"
                    };

                    _vertexBuffer = VulkEase.VulkEase.CreateBuffer(_device, bufferDesc);

                    if (_vertexBuffer.native == VEConstants.VE_INVALID_ADDRESS.native)
                    {
                        Console.Error.WriteLine("Failed to create vertex buffer");
                        return false;
                    }

                    Console.WriteLine($"Created vertex buffer at address: 0x{_vertexBuffer.native:X}");
                    return true;
                }
                finally
                {
                    handle.Free();
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create buffers: {ex.Message}");
                return false;
            }
        }

        private void Render()
        {
            try
            {
                // Begin command buffer
                var cmd = VulkEase.VulkEase.BeginCommandBuffer(_device);

                // Calculate animation
                float elapsedTime = (float)_stopwatch.Elapsed.TotalSeconds;
                float rotationAngle = elapsedTime * 2.0f; // 2 radians per second

                // Create rotation matrix for spinning animation
                var rotationMatrix = CreateRotationMatrix(rotationAngle);

                // Begin rendering to render target (transitions handled automatically)
                VulkEase.VulkEase.BeginRendering(cmd, _renderTarget);

                // Apply graphics state (sets required dynamic states for shader objects)
                VulkEase.VulkEase.ApplyGraphicsState(cmd, _pipeline, null, null, null);

                // Set up push constants with the vertex buffer address
                var pushConstants = new TrianglePushConstants
                {
                    mvpMatrix = rotationMatrix,
                    vertexBufferAddress = _vertexBuffer.native
                };

                VulkEase.VulkEase.PushConstants(cmd, pushConstants);

                // Draw triangle (3 vertices, no indices)
                VulkEase.VulkEase.Draw(cmd, 3, 1, 0, 0);

                // End rendering
                VulkEase.VulkEase.EndRendering(cmd);

                // Blit color texture to swapchain (acquires swapchain image internally)
                var blitResult = VulkEase.VulkEase.BlitTextureToSwapchain(cmd, _colorTexture, _swapchain, VkFilter.VK_FILTER_LINEAR);

                if (blitResult == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    var swapExtent = VulkEase.VulkEase.GetSwapchainSize(_swapchain);
                    uint swWidth = swapExtent.width;
                    uint swHeight = swapExtent.height;
                    VulkEase.VulkEase.ResizeSwapchain(_swapchain, swWidth, swHeight);
                    VulkEase.VulkEase.ResizeRenderTarget(_device, ref _renderTarget, swWidth, swHeight);
                    _colorTexture = _renderTarget.ColorAttachments[0].texture;
                    return;
                }

                // Present (auto-releases command buffer)
                var result = VulkEase.VulkEase.PresentImage(_swapchain, cmd);

                if (result == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    var swapExtent = VulkEase.VulkEase.GetSwapchainSize(_swapchain);
                    uint swWidth = swapExtent.width;
                    uint swHeight = swapExtent.height;
                    VulkEase.VulkEase.ResizeSwapchain(_swapchain, swWidth, swHeight);
                    VulkEase.VulkEase.ResizeRenderTarget(_device, ref _renderTarget, swWidth, swHeight);
                    _colorTexture = _renderTarget.ColorAttachments[0].texture;
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Render error: {ex.Message}");
            }
        }

        private float[] CreateRotationMatrix(float angleRadians)
        {
            var matrix = new float[16];

            // Identity matrix
            matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;

            // Apply rotation around Z-axis
            float cosTheta = MathF.Cos(angleRadians);
            float sinTheta = MathF.Sin(angleRadians);

            matrix[0] = cosTheta;   // m[0][0]
            matrix[1] = sinTheta;   // m[0][1]
            matrix[4] = -sinTheta;  // m[1][0]
            matrix[5] = cosTheta;   // m[1][1]

            return matrix;
        }

        protected override void OnUnload()
        {
            // Print final timing summary
            Console.WriteLine();
            Console.WriteLine("=== Final Timing Summary ===");
            Console.WriteLine($"Total frames: {_timing.frameNumber}");
            Console.WriteLine($"Average frame time: {_timing.avgFrameTimeMs:F2} ms ({_timing.avgFps:F1} FPS)");
            Console.WriteLine($"Min frame time: {_timing.minFrameTimeMs:F2} ms");
            Console.WriteLine($"Max frame time: {_timing.maxFrameTimeMs:F2} ms");
            Console.WriteLine();
            Console.WriteLine("Shutting down Triangle example...");

            if (_device.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DeviceWaitIdle(_device);
            }

            if (_vertexBuffer.native != VEConstants.VE_INVALID_ADDRESS.native)
            {
                VulkEase.VulkEase.DestroyBuffer(_device, _vertexBuffer);
            }

            if (_vertexShader.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DestroyShader(_vertexShader);
            }

            if (_fragmentShader.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DestroyShader(_fragmentShader);
            }

            if (_pipeline.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DestroyGraphicsPipeline(_pipeline);
            }

            if (_colorTexture.native != VEConstants.VE_INVALID_TEXTURE_INDEX.native)
            {
                VulkEase.VulkEase.DestroyTexture(_device, _colorTexture);
            }

            if (_swapchain.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DestroySwapchain(_swapchain);
            }

            if (_device.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DestroyDevice(_device);
            }

            if (_context.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DestroyContext(_context);
            }

            base.OnUnload();
        }

        public static void Main(string[] args)
        {
            Console.WriteLine("VulkEase Triangle Example");
            Console.WriteLine("========================");
            Console.WriteLine("Press ESC to exit");
            Console.WriteLine();

            using var triangle = new TriangleExample();
            triangle.Run();
        }
    }
}