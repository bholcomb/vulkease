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
    /// A classic spinning textured cube using VulkEase's modern Vulkan 1.3 API
    /// with shader objects, dynamic rendering, and bindless resources
    /// </summary>
    public class CubeExample : GameWindow
    {
        private const int WindowWidth = 800;
        private const int WindowHeight = 600;
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

        // Resources
        private VEBufferAddress _vertexBuffer;
        private VEBufferAddress _indexBuffer;
        private VEBufferAddress _uniformBuffer;
        private VETextureIndex _texture;
        private VESamplerIndex _sampler;

        // Shaders
        private VEShader _vertexShader;
        private VEShader _fragmentShader;

        // Render configuration
        private VERenderConfig _renderConfig;

        // Animation
        private float _rotationAngle;
        private Stopwatch _stopwatch = Stopwatch.StartNew();

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

            if (!CreateRenderConfig())
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

            UpdateAnimation();
            RenderCube();
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

                // Create device
                _device = VE.CreateDevice(_context);
                Console.WriteLine("VulkEase device created successfully");
                Console.WriteLine($"Device: {VE.GetDeviceName(_device)}");
                Console.WriteLine($"Driver: {VE.GetDriverVersion(_device)}");

                // Get native window handle
                IntPtr windowHandle = GetNativeWindowHandle();
                if (windowHandle == IntPtr.Zero)
                {
                    Console.Error.WriteLine("Failed to get native window handle");
                    return false;
                }

                // Create swapchain
                Console.WriteLine($"Creating swapchain: {WindowWidth}x{WindowHeight}...");
                _swapchain = VE.CreateSwapchain(_device, windowHandle,
                    (uint)WindowWidth, (uint)WindowHeight, VkFormat.VK_FORMAT_B8G8R8A8_SRGB, EnableVSync);

                if (_swapchain.native == IntPtr.Zero)
                {
                    Console.Error.WriteLine("Failed to create swapchain");
                    return false;
                }

                Console.WriteLine($"Swapchain created successfully: {WindowWidth}x{WindowHeight}");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to initialize VulkEase: {ex.Message}");
                string? lastError = VE.GetLastError();
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
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                return GLFW.GetWin32Window(WindowPtr);
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                IntPtr display = (IntPtr)GLFW.GetX11Display();
                IntPtr window = (IntPtr)GLFW.GetX11Window(WindowPtr);
                
                IntPtr[] handles = { display, window };
                IntPtr handleArray = Marshal.AllocHGlobal(handles.Length * IntPtr.Size);
                Marshal.Copy(handles.Select(h => h.ToInt64()).ToArray(), 0, handleArray, handles.Length);
                return handleArray;
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                return GLFW.GetCocoaWindow(WindowPtr);
            }

            return IntPtr.Zero;
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
                string? lastError = VE.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
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
                string? lastError = VE.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
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

                if (_texture.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
                {
                    Console.WriteLine("Texture file not found, creating fallback texture...");
                    
                    // Create a simple fallback texture if file doesn't exist
                    _texture = VE.CreateTexture2D(_device, 2, 2, VkFormat.VK_FORMAT_R8G8B8A8_UNORM,
                        VkImageUsageFlags.VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlags.VK_IMAGE_USAGE_TRANSFER_DST_BIT, "FallbackTexture");

                    if (_texture.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
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
                string? lastError = VE.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
                return false;
            }
        }

        private bool CreateRenderConfig()
        {
            try
            {
                // Create opaque render config with depth testing
                _renderConfig = VE.CreateOpaqueRenderConfig(_device, "CubeRenderConfig");

                Console.WriteLine("Render config created successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create render config: {ex.Message}");
                string? lastError = VE.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
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
                // Acquire next image
                VETextureIndex backbuffer = VE.AcquireNextImage(_swapchain);
                if (backbuffer.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
                {
                    return; // Swapchain out of date, will be recreated
                }

                // Begin command buffer
                var cmd = VE.BeginCommandBuffer(_device);

                VE.TransitionTextureForColorAttachment(cmd, backbuffer);

                // Begin rendering to backbuffer
                uint width = (uint)WindowWidth;
                uint height = (uint)WindowHeight;
                //VE.GetSwapchainSize(_swapchain, out uint width, out uint height);

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

                var colorAttachment = new VERenderingAttachment
                {
                    texture = backbuffer,
                    loadOp = VkAttachmentLoadOp.VK_ATTACHMENT_LOAD_OP_CLEAR,
                    storeOp = VkAttachmentStoreOp.VK_ATTACHMENT_STORE_OP_STORE,
                    clearValue = new VEColor(0.1f, 0.2f, 0.3f, 1.0f), // Dark blue background
                    resolveTexture = VEConstants.VE_INVALID_TEXTURE_INDEX
                };

                // Create rendering info with C#-friendly API
                var renderingInfo = new VERenderingInfo
                {
                    RenderAreaX = 0,
                    RenderAreaY = 0,
                    RenderAreaWidth = width,
                    RenderAreaHeight = height
                };
                renderingInfo.ColorAttachments.Add(colorAttachment);

                VE.BeginRendering(cmd, renderingInfo);

                // Set viewport and scissor
                VE.SetViewport(cmd, 0.0f, 0.0f, width, height, 0.0f, 1.0f);
                VE.SetScissor(cmd, 0, 0, width, height);

                // Bind shaders and render configuration
                VE.BindShader(cmd, _vertexShader);
                VE.BindShader(cmd, _fragmentShader);
                VE.ApplyRenderConfig(cmd, _renderConfig);

                // Set up push constants with bindless resource indices
                var pushConstants = new VEGraphicsPushConstants
                {
                    vertexBuffer = _vertexBuffer.native,
                    indexBuffer = _indexBuffer.native,
                    uniformBuffers = new ulong[4] { _uniformBuffer.native, 0, 0, 0 },
                    textures = new uint[8] { _texture.native, 0, 0, 0, 0, 0, 0, 0 },
                    samplers = new uint[8] { _sampler.native, 0, 0, 0, 0, 0, 0, 0 },
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

                // Present the frame
                VE.TransitionTextureForPresent(cmd, backbuffer);
                var result = VE.PresentImage(_swapchain, cmd);

                if (result == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    // Swapchain needs to be recreated (window resized)
                    VE.GetSwapchainSize(_swapchain, out uint newWidth, out uint newHeight);
                    if (newWidth > 0 && newHeight > 0)
                    {
                        VE.ResizeSwapchain(_swapchain, newWidth, newHeight);
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
            Console.WriteLine("Shutting down gracefully...");

            if (_device.native != IntPtr.Zero)
            {
                VE.DeviceWaitIdle(_device);
            }

            // Destroy render config
            if (_renderConfig.native != IntPtr.Zero)
            {
                VE.DestroyRenderConfig(_renderConfig);
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

            if (_texture.native != VEConstants.VE_INVALID_TEXTURE_INDEX.native)
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