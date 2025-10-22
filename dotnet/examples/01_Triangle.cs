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
        private VERenderConfig _renderConfig;

        // Animation state
        private Stopwatch _stopwatch = Stopwatch.StartNew();

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
            }
        }

        protected override void OnRenderFrame(FrameEventArgs args)
        {
            base.OnRenderFrame(args);

            Render();
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

                // Create device
                _device = VulkEase.VulkEase.CreateDevice(_context);
                Console.WriteLine($"VulkEase device created successfully");
                Console.WriteLine($"Device: {VulkEase.VulkEase.GetDeviceName(_device)}");
                Console.WriteLine($"Driver: {VulkEase.VulkEase.GetDriverVersion(_device)}");

                // Get native window handle
                IntPtr windowHandle = GetNativeWindowHandle();
                if (windowHandle == IntPtr.Zero)
                {
                    Console.Error.WriteLine("Failed to get native window handle");
                    return false;
                }

                // Create swapchain
                _swapchain = VulkEase.VulkEase.CreateSwapchain(_device, windowHandle,
                    (uint)WindowWidth, (uint)WindowHeight, VkFormat.VK_FORMAT_B8G8R8A8_SRGB, EnableVSync);

                Console.WriteLine($"Swapchain created: {WindowWidth}x{WindowHeight}");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to initialize VulkEase: {ex.Message}");
                string? lastError = VulkEase.VulkEase.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
                return false;
            }
        }

        private unsafe IntPtr GetNativeWindowHandle()
        {
            // OpenTK's WindowPtr contains the GLFW window handle
            // Use OpenTK's GLFW bindings to get platform-specific handles
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                // On Linux, we need both X11 display and window
                IntPtr display = (IntPtr)GLFW.GetX11Display();
                IntPtr window = (IntPtr)GLFW.GetX11Window(WindowPtr);
                
                // Create array with both handles
                IntPtr[] handles = { display, window };
                IntPtr handleArray = Marshal.AllocHGlobal(handles.Length * IntPtr.Size);
                Marshal.Copy(handles.Select(h => h.ToInt64()).ToArray(), 0, handleArray, handles.Length);
                return handleArray;
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                // On Windows, get Win32 window handle
                return GLFW.GetWin32Window(WindowPtr);
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                // On macOS, get Cocoa window handle
                return GLFW.GetCocoaWindow(WindowPtr);
            }

            return IntPtr.Zero;
        }

        private bool CreateShaders()
        {
            try
            {
                // Load shaders from SPIR-V files
                _vertexShader = VulkEase.VulkEase.LoadShader(_device, "examples/shaders/triangle.vert.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "SpinningTriangleVertex");

                _fragmentShader = VulkEase.VulkEase.LoadShader(_device, "examples/shaders/triangle.frag.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "SpinningTriangleFragment");

                // Create default render config
                _renderConfig = VulkEase.VulkEase.CreateOpaqueRenderConfig(_device, "opaque render config");

                Console.WriteLine("Shaders loaded successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create shaders: {ex.Message}");
                string? lastError = VulkEase.VulkEase.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
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
                    IntPtr debugNamePtr = Marshal.StringToHGlobalAnsi("TriangleVertexBuffer");
                    try
                    {
                        var bufferDesc = new VEBufferDesc
                        {
                            size = dataSize,
                            usage = VkBufferUsageFlags.VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            initialData = dataPtr,
                            initialDataSize = dataSize,
                            persistentlyMapped = false,
                            debugName = debugNamePtr
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
                        Marshal.FreeHGlobal(debugNamePtr);
                    }
                }
                finally
                {
                    handle.Free();
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create buffers: {ex.Message}");
                string? lastError = VulkEase.VulkEase.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
                return false;
            }
        }

        private void Render()
        {
            try
            {
                // Acquire next swapchain image
                VETextureIndex backbuffer = VulkEase.VulkEase.AcquireNextImage(_swapchain);
                if (backbuffer.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
                {
                    return; // Swapchain needs recreation or other error
                }

                // Begin command buffer
                var cmd = VulkEase.VulkEase.BeginCommandBuffer(_device);

                VulkEase.VulkEase.TransitionTextureForColorAttachment(cmd, backbuffer);

                // Calculate animation
                float elapsedTime = (float)_stopwatch.Elapsed.TotalSeconds;
                float rotationAngle = elapsedTime * 2.0f; // 2 radians per second

                // Create rotation matrix for spinning animation
                var rotationMatrix = CreateRotationMatrix(rotationAngle);

                // Set up rendering info
                var colorAttachment = new VERenderingAttachment
                {
                    texture = backbuffer,
                    loadOp = VkAttachmentLoadOp.VK_ATTACHMENT_LOAD_OP_CLEAR,
                    storeOp = VkAttachmentStoreOp.VK_ATTACHMENT_STORE_OP_STORE,
                    clearValue = new VEColor(0.1f, 0.1f, 0.1f, 1.0f), // Dark gray background
                    resolveTexture = VEConstants.VE_INVALID_TEXTURE_INDEX
                };

                VulkEase.VulkEase.GetSwapchainSize(_swapchain, out uint width, out uint height);

                // Create rendering info with C#-friendly API
                var renderingInfo = new VERenderingInfo
                {
                    RenderAreaX = 0,
                    RenderAreaY = 0,
                    RenderAreaWidth = width,
                    RenderAreaHeight = height
                };
                renderingInfo.ColorAttachments.Add(colorAttachment);

                // Begin rendering
                VulkEase.VulkEase.BeginRendering(cmd, renderingInfo);

                // Bind shaders
                VulkEase.VulkEase.BindShader(cmd, _vertexShader);
                VulkEase.VulkEase.BindShader(cmd, _fragmentShader);

                // Setup viewport and scissor state
                VulkEase.VulkEase.SetViewport(cmd, 0.0f, 0.0f, width, height, 0.0f, 1.0f);
                VulkEase.VulkEase.SetScissor(cmd, 0, 0, width, height);

                // Default render state
                VulkEase.VulkEase.ApplyRenderConfig(cmd, _renderConfig);

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

                // Present
                VulkEase.VulkEase.TransitionTextureForPresent(cmd, backbuffer);
                var result = VulkEase.VulkEase.PresentImage(_swapchain, cmd);

                if (result == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    // Handle swapchain recreation
                    VulkEase.VulkEase.ResizeSwapchain(_swapchain, width, height);
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

            if (_renderConfig.native != IntPtr.Zero)
            {
                VulkEase.VulkEase.DestroyRenderConfig(_renderConfig);
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