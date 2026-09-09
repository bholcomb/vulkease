using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using System;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using VulkEase;

using VE = VulkEase.VulkEase;

namespace VulkEaseExamples
{
    /// <summary>
    /// Example 3: Spinning Cube
    /// A classic spinning textured cube using VulkEase's modern Vulkan 1.4 API
    /// with shader objects, dynamic rendering, and bindless resources
    /// </summary>
    public class CubeExample : GameWindow
    {
        private const int WindowWidth = 1920;
        private const int WindowHeight = 1080;
        private const bool EnableVSync = true;
        private const float PI = MathF.PI;

        // Vertex data structure
        [StructLayout(LayoutKind.Sequential)]
        private struct Vertex
        {
            public Vector3 Position;
            public Vector2 TexCoord;

            public Vertex(Vector3 position, Vector2 texCoord)
            {
                Position = position;
                TexCoord = texCoord;
            }
        }

        // Cube vertices with proper texture coordinates for each face
        private static readonly Vertex[] CubeVertices = {
            // Front face
            new(new Vector3(-1.0f, -1.0f,  1.0f), new Vector2(0.0f, 0.0f)),
            new(new Vector3( 1.0f, -1.0f,  1.0f), new Vector2(1.0f, 0.0f)),
            new(new Vector3( 1.0f,  1.0f,  1.0f), new Vector2(1.0f, 1.0f)),
            new(new Vector3(-1.0f,  1.0f,  1.0f), new Vector2(0.0f, 1.0f)),

            // Back face
            new(new Vector3(-1.0f, -1.0f, -1.0f), new Vector2(1.0f, 0.0f)),
            new(new Vector3( 1.0f, -1.0f, -1.0f), new Vector2(0.0f, 0.0f)),
            new(new Vector3( 1.0f,  1.0f, -1.0f), new Vector2(0.0f, 1.0f)),
            new(new Vector3(-1.0f,  1.0f, -1.0f), new Vector2(1.0f, 1.0f)),
        };

        // Cube indices (36 indices for 12 triangles)
        private static readonly ushort[] CubeIndices = {
            // Front face
            0, 1, 2, 2, 3, 0,
            // Back face
            4, 6, 5, 6, 4, 7,
            // Left face
            4, 0, 3, 3, 7, 4,
            // Right face
            1, 5, 6, 6, 2, 1,
            // Top face
            3, 2, 6, 6, 7, 3,
            // Bottom face
            4, 5, 1, 1, 0, 4,
        };

        // VulkEase state
        private VEContext _context;
        private VEDevice _device;
        private VESwapchain _swapchain;
        private VERenderTarget _renderTarget;
        private VETexture _colorTexture;
        private VETexture _depthTexture;

        // Resources
        private VEBufferAddress _vertexBuffer;
        private VEBufferAddress _indexBuffer;
        private VEBufferAddress _uniformBuffer;
        private VETexture _texture;
        private VESamplerIndex _sampler;

        // Shaders
        private VEShader _vertexShader;
        private VEShader _fragmentShader;

        // Render configuration
        private VEGraphicsPipeline _pipeline;

        // Animation
        private float _rotationAngle;
        private Stopwatch _stopwatch = Stopwatch.StartNew();
        
        // Frame timing
        private VEFrameTimingInfo _timing;

        public CubeExample() : base(GameWindowSettings.Default,
            new NativeWindowSettings()
            {
                Title = "VulkEase - Spinning Cube",
                ClientSize = new Vector2i(WindowWidth, WindowHeight),
                StartVisible = false,
                StartFocused = true,
                API = ContextAPI.NoAPI,
                Profile = ContextProfile.Core,
                Flags = ContextFlags.ForwardCompatible
            })
        {
        }

        protected override void OnLoad()
        {
            base.OnLoad();

            Console.WriteLine("VulkEase Spinning Cube Demo");
            Console.WriteLine("===========================");
            Console.WriteLine("Modern Vulkan 1.4 with shader objects and bindless resources");
            Console.WriteLine();

            if (!InitializeVulkEase())
            {
                Close();
                return;
            }

            if (!LoadShaders())
            {
                Close();
                return;
            }

            if (!CreateGeometry())
            {
                Close();
                return;
            }

            if (!LoadTexture())
            {
                Close();
                return;
            }

            if (!CreatePipeline())
            {
                Close();
                return;
            }

            _stopwatch = Stopwatch.StartNew();

            Console.WriteLine("Initialization complete. Starting main loop...");
            Console.WriteLine("Controls: Close window or press ESC to exit");
            Console.WriteLine();
            IsVisible = true;
        }

        protected override void OnResize(ResizeEventArgs e)
        {
            base.OnResize(e);

            if (_swapchain.native != IntPtr.Zero && e.Width > 0 && e.Height > 0)
            {
                VE.ResizeSwapchain(_swapchain, (uint)e.Width, (uint)e.Height);
                VE.ResizeRenderTarget(_device, ref _renderTarget, (uint)e.Width, (uint)e.Height);
                _colorTexture = VulkEase.VulkEase.GetTextureFromView(_device, _renderTarget.ColorAttachments[0].view);
                if (_renderTarget.DepthAttachment.HasValue)
                    _depthTexture = VulkEase.VulkEase.GetTextureFromView(_device, _renderTarget.DepthAttachment.Value.view);
            }
        }

        protected override void OnRenderFrame(FrameEventArgs args)
        {
            base.OnRenderFrame(args);

            // Handle window minimize
            if (ClientSize.X == 0 || ClientSize.Y == 0)
            {
                return;
            }

            // Begin frame timing
            VE.BeginFrame(_device);

            UpdateAnimation();
            RenderCube();

            // End frame and get timing info
            VE.EndFrame(_device, out _timing);

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
                _context = VE.CreateContext("VulkEase Cube Demo");
                Console.WriteLine("VulkEase initialized successfully");

                // Create device (auto-select best GPU)
                _device = VE.CreateDevice(_context);
                Console.WriteLine("VulkEase device created successfully");
                Console.WriteLine($"Device: {VE.GetDeviceName(_device)}");
                Console.WriteLine($"Driver: {VE.GetDriverVersion(_device)}");

                // Create swapchain
                Console.WriteLine($"Creating swapchain: {WindowWidth}x{WindowHeight}...");
                var swapchainDesc = new VESwapchainDesc
                {
                    Surface = GetSurfaceDesc(),
                    Width = (uint)WindowWidth,
                    Height = (uint)WindowHeight,
                    ColorFormat = VkFormat.VK_FORMAT_B8G8R8A8_SRGB,
                    VSync = EnableVSync
                };
                _swapchain = VE.CreateSwapchain(_device, swapchainDesc);

                if (_swapchain.native == IntPtr.Zero)
                {
                    Console.Error.WriteLine("Failed to create swapchain");
                    return false;
                }

                // Create render target with depth buffer
                _renderTarget = VE.CreateSimpleRenderTarget(_device,
                    (uint)WindowWidth, (uint)WindowHeight,
                    VkFormat.VK_FORMAT_B8G8R8A8_SRGB,
                    VkFormat.VK_FORMAT_D32_SFLOAT,
                    new VEColor(0.1f, 0.2f, 0.3f, 1.0f), 1.0f,
                    out _colorTexture, out _depthTexture);

                Console.WriteLine($"Swapchain and render target created successfully: {WindowWidth}x{WindowHeight}");
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

        private bool LoadShaders()
        {
            try
            {
                // Load vertex shader
                _vertexShader = VE.LoadShaderFromFile(_device, "examples/shaders/cube.vert.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "CubeVertexShader");

                // Load fragment shader
                _fragmentShader = VE.LoadShaderFromFile(_device, "examples/shaders/cube.frag.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "CubeFragmentShader");

                Console.WriteLine("Shaders loaded successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to load shaders: {ex.Message}");
                return false;
            }
        }

        private bool CreateGeometry()
        {
            try
            {
                // Create vertex buffer
                GCHandle vertexHandle = GCHandle.Alloc(CubeVertices, GCHandleType.Pinned);
                try
                {
                    IntPtr vertexDataPtr = vertexHandle.AddrOfPinnedObject();
                    ulong vertexDataSize = (ulong)(CubeVertices.Length * Marshal.SizeOf<Vertex>());

                    _vertexBuffer = VE.CreateVertexBuffer(_device, vertexDataPtr, vertexDataSize, "CubeVertices");

                    if (_vertexBuffer.native == VEConstants.VE_INVALID_ADDRESS.native)
                    {
                        Console.Error.WriteLine("Failed to create vertex buffer");
                        return false;
                    }
                }
                finally
                {
                    vertexHandle.Free();
                }

                // Create index buffer
                GCHandle indexHandle = GCHandle.Alloc(CubeIndices, GCHandleType.Pinned);
                try
                {
                    IntPtr indexDataPtr = indexHandle.AddrOfPinnedObject();
                    ulong indexDataSize = (ulong)(CubeIndices.Length * sizeof(ushort));

                    _indexBuffer = VE.CreateIndexBuffer(_device, indexDataPtr, indexDataSize, "CubeIndices");

                    if (_indexBuffer.native == VEConstants.VE_INVALID_ADDRESS.native)
                    {
                        Console.Error.WriteLine("Failed to create index buffer");
                        return false;
                    }
                }
                finally
                {
                    indexHandle.Free();
                }

                // Create uniform buffer (persistently mapped for easy updates)
                ulong uniformBufferSize = (ulong)(16 * sizeof(float)); // 4x4 matrix
                _uniformBuffer = VE.CreateUniformBuffer(_device, uniformBufferSize, true, "CubeUniforms");

                if (_uniformBuffer.native == VEConstants.VE_INVALID_ADDRESS.native)
                {
                    Console.Error.WriteLine("Failed to create uniform buffer");
                    return false;
                }

                Console.WriteLine("Geometry created successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create geometry: {ex.Message}");
                return false;
            }
        }

        private bool LoadTexture()
        {
            try
            {
                // Load texture from file
                _texture = VE.LoadTexture(_device, "examples/data/testCard.png",
                    VkImageUsageFlags.VK_IMAGE_USAGE_SAMPLED_BIT, true);

                if (_texture.native == VEConstants.VE_INVALID_TEXTURE.native)
                {
                    Console.WriteLine("Texture file not found, creating fallback texture...");
                    
                    // Create a simple fallback texture if file doesn't exist
                    _texture = VE.CreateTexture2D(_device, 2, 2, VkFormat.VK_FORMAT_R8G8B8A8_UNORM,
                        VkImageUsageFlags.VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlags.VK_IMAGE_USAGE_TRANSFER_DST_BIT, "FallbackTexture");

                    if (_texture.native == VEConstants.VE_INVALID_TEXTURE.native)
                    {
                        Console.Error.WriteLine("Failed to create fallback texture");
                        return false;
                    }
                }

                // Create linear sampler
                _sampler = VE.CreateLinearSampler(_device);

                if (_sampler.native == VEConstants.VE_INVALID_SAMPLER_INDEX.native)
                {
                    Console.Error.WriteLine("Failed to create sampler");
                    return false;
                }

                Console.WriteLine("Texture loaded successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to load texture: {ex.Message}");
                return false;
            }
        }

        private bool CreatePipeline()
        {
            try
            {
                // Create opaque graphics pipeline with depth testing
                _pipeline = VE.CreateOpaquePipeline(_device, _vertexShader, _fragmentShader, "CubePipeline");

                Console.WriteLine("Graphics pipeline created successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create graphics pipeline: {ex.Message}");
                return false;
            }
        }

        private void UpdateAnimation()
        {
            // Update rotation
            float deltaTime = (float)_stopwatch.Elapsed.TotalSeconds;
            _rotationAngle = deltaTime * 0.8f; // Rotate at 0.8 radians per second

            if (_rotationAngle > 2.0f * PI)
            {
                _rotationAngle -= 2.0f * PI;
            }
        }

        private float[] CreatePerspectiveMatrix(float fov, float aspect, float near, float far)
        {
            var m = new float[16];
            float tanHalfFov = MathF.Tan(fov * 0.5f);

            // Flipped Y for Vulkan screen coordinate system
            m[0] = 1.0f / (aspect * tanHalfFov);
            m[5] = -1.0f / tanHalfFov; // Flip Y for Vulkan
            m[10] = far / (near - far); // Z [0,1] mapping
            m[11] = -1.0f;
            m[14] = -(far * near) / (far - near); // Z [0,1] mapping
            m[15] = 0.0f;

            return m;
        }

        private float[] CreateRotationMatrixY(float angle)
        {
            var m = CreateIdentityMatrix();
            float c = MathF.Cos(angle);
            float s = MathF.Sin(angle);

            m[0] = c; m[2] = s;
            m[8] = -s; m[10] = c;

            return m;
        }

        private float[] CreateTranslationMatrix(float x, float y, float z)
        {
            var m = CreateIdentityMatrix();
            m[12] = x;
            m[13] = y;
            m[14] = z;
            return m;
        }

        private float[] CreateIdentityMatrix()
        {
            var m = new float[16];
            m[0] = m[5] = m[10] = m[15] = 1.0f;
            return m;
        }

        private float[] MultiplyMatrix(float[] a, float[] b)
        {
            var result = new float[16];
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    for (int k = 0; k < 4; k++)
                    {
                        result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
                    }
                }
            }
            return result;
        }

        private void RenderCube()
        {
            try
            {
                // Begin command buffer
                var cmd = VE.BeginCommandBuffer(_device);

                uint width = _renderTarget.RenderAreaWidth;
                uint height = _renderTarget.RenderAreaHeight;

                // Build transformation matrices
                float aspect = (float)width / (float)height;
                var projection = CreatePerspectiveMatrix(PI * 0.25f, aspect, 0.1f, 100.0f);
                var view = CreateTranslationMatrix(0.0f, 0.0f, -5.0f);
                var model = CreateRotationMatrixY(_rotationAngle);
                
                var vm = MultiplyMatrix(model, view);
                var mvp = MultiplyMatrix(vm, projection);

                // Update uniform buffer with MVP matrix
                GCHandle mvpHandle = GCHandle.Alloc(mvp, GCHandleType.Pinned);
                try
                {
                    IntPtr mvpPtr = mvpHandle.AddrOfPinnedObject();
                    ulong mvpSize = (ulong)(mvp.Length * sizeof(float));
                    var updateResult = VE.UpdateBuffer(_device, _uniformBuffer, mvpPtr, mvpSize, 0);
                    if (updateResult != VEResult.VE_SUCCESS)
                    {
                        Console.Error.WriteLine("Failed to update uniform buffer");
                        return;
                    }
                }
                finally
                {
                    mvpHandle.Free();
                }

                // Begin rendering to render target (transitions handled automatically)
                VE.BeginRendering(cmd, _renderTarget);

                // Apply graphics state (sets required dynamic states for shader objects)
                VE.ApplyGraphicsState(cmd, _pipeline, null, null, null);

                // Set up push constants with bindless resource indices
                var pushConstants = new VEGraphicsPushConstants
                {
                    vertexBuffer = _vertexBuffer.native,
                    indexBuffer = _indexBuffer.native,
                    uniformBuffers = new ulong[8] { _uniformBuffer.native, 0, 0, 0, 0, 0, 0, 0 },
                    textures = new uint[16] { VulkEase.VulkEase.GetDefaultTextureView(_device, _texture).native,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                    samplers = new uint[16] { _sampler.native, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                    reserved = new uint[7],
                    objectScale = 1.0f,
                    activeTextureCount = 1,
                    activeSamplerCount = 1,
                    activeUniformCount = 1
                };

                VE.PushConstants(cmd, pushConstants);

                // Draw the cube
                VE.BindIndexBuffer(cmd, _indexBuffer, 0, VkIndexType.VK_INDEX_TYPE_UINT16);
                VE.DrawIndexed(cmd, (uint)CubeIndices.Length, 1, 0, 0, 0);

                // End rendering
                VE.EndRendering(cmd);

                // Blit color texture to swapchain (acquires swapchain image internally)
                var blitResult = VE.BlitTextureToSwapchain(cmd, _colorTexture, _swapchain, VkFilter.VK_FILTER_LINEAR);

                if (blitResult == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    var swapExtent = VE.GetSwapchainSize(_swapchain);
                    uint newWidth = swapExtent.width;
                    uint newHeight = swapExtent.height;
                    if (newWidth > 0 && newHeight > 0)
                    {
                        VE.ResizeSwapchain(_swapchain, newWidth, newHeight);
                        VE.ResizeRenderTarget(_device, ref _renderTarget, newWidth, newHeight);
                        _colorTexture = VulkEase.VulkEase.GetTextureFromView(_device, _renderTarget.ColorAttachments[0].view);
                        if (_renderTarget.DepthAttachment.HasValue)
                            _depthTexture = VulkEase.VulkEase.GetTextureFromView(_device, _renderTarget.DepthAttachment.Value.view);
                    }
                    return;
                }

                // Present the frame (auto-releases command buffer)
                var result = VE.PresentImage(_swapchain, cmd);

                if (result == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    var swapExtent = VE.GetSwapchainSize(_swapchain);
                    uint newWidth = swapExtent.width;
                    uint newHeight = swapExtent.height;
                    if (newWidth > 0 && newHeight > 0)
                    {
                        VE.ResizeSwapchain(_swapchain, newWidth, newHeight);
                        VE.ResizeRenderTarget(_device, ref _renderTarget, newWidth, newHeight);
                        _colorTexture = VulkEase.VulkEase.GetTextureFromView(_device, _renderTarget.ColorAttachments[0].view);
                        if (_renderTarget.DepthAttachment.HasValue)
                            _depthTexture = VulkEase.VulkEase.GetTextureFromView(_device, _renderTarget.DepthAttachment.Value.view);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Render error: {ex.Message}");
            }
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
            Console.WriteLine("Shutting down gracefully...");

            if (_device.native != IntPtr.Zero)
            {
                VE.DeviceWaitIdle(_device);
            }

            // Destroy graphics pipeline
            if (_pipeline.native != IntPtr.Zero)
            {
                VE.DestroyGraphicsPipeline(_pipeline);
            }

            // Destroy shaders
            if (_vertexShader.native != IntPtr.Zero)
            {
                VE.DestroyShader(_vertexShader);
            }

            if (_fragmentShader.native != IntPtr.Zero)
            {
                VE.DestroyShader(_fragmentShader);
            }

            // Destroy resources
            if (_sampler.native != VEConstants.VE_INVALID_SAMPLER_INDEX.native)
            {
                VE.DestroySampler(_device, _sampler);
            }

            if (_texture.native != VEConstants.VE_INVALID_TEXTURE.native)
            {
                VE.DestroyTexture(_device, _texture);
            }

            if (_uniformBuffer.native != VEConstants.VE_INVALID_ADDRESS.native)
            {
                VE.DestroyBuffer(_device, _uniformBuffer);
            }

            if (_indexBuffer.native != VEConstants.VE_INVALID_ADDRESS.native)
            {
                VE.DestroyBuffer(_device, _indexBuffer);
            }

            if (_vertexBuffer.native != VEConstants.VE_INVALID_ADDRESS.native)
            {
                VE.DestroyBuffer(_device, _vertexBuffer);
            }

            // Destroy VulkEase objects
            if (_colorTexture.native != VEConstants.VE_INVALID_TEXTURE.native)
            {
                VE.DestroyTexture(_device, _colorTexture);
            }
            if (_depthTexture.native != VEConstants.VE_INVALID_TEXTURE.native)
            {
                VE.DestroyTexture(_device, _depthTexture);
            }

            if (_swapchain.native != IntPtr.Zero)
            {
                VE.DestroySwapchain(_swapchain);
            }

            if (_device.native != IntPtr.Zero)
            {
                VE.DestroyDevice(_device);
            }

            if (_context.native != IntPtr.Zero)
            {
                VE.DestroyContext(_context);
            }

            base.OnUnload();
        }

        public static void Main(string[] args)
        {
            Console.WriteLine("VulkEase Spinning Cube Example");
            Console.WriteLine("==============================");
            Console.WriteLine("Press ESC to exit");
            Console.WriteLine();

            using var cube = new CubeExample();
            cube.Run();
        }
    }
}
