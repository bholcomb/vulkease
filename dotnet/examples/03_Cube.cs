using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using System;
using System.Runtime.InteropServices;
using VulkEase;

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

        // Matrix 4x4 for transformations
        [StructLayout(LayoutKind.Sequential)]
        private struct Mat4
        {
            public unsafe fixed float m[16];

            public unsafe Mat4(params float[] values)
            {
                if (values.Length != 16) throw new ArgumentException("Matrix must have 16 elements");
                for (int i = 0; i < 16; i++) m[i] = values[i];
            }

            public static unsafe Mat4 Identity()
            {
                var matrix = new Mat4();
                for (int i = 0; i < 16; i++) matrix.m[i] = 0.0f;
                matrix.m[0] = matrix.m[5] = matrix.m[10] = matrix.m[15] = 1.0f;
                return matrix;
            }
        }

        // Uniform buffer data
        [StructLayout(LayoutKind.Sequential)]
        private struct UniformData
        {
            public Mat4 MvpMatrix;
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
        private VEBufferAddress _vertexBuffer = VEConstants.VE_INVALID_ADDRESS;
        private VEBufferAddress _indexBuffer = VEConstants.VE_INVALID_ADDRESS;
        private VEBufferAddress _uniformBuffer = VEConstants.VE_INVALID_ADDRESS;
        private uint _texture = VEConstants.VE_INVALID_TEXTURE_INDEX;
        private uint _sampler = VEConstants.VE_INVALID_SAMPLER_INDEX;

        // Shaders
        private VEShader _vertexShader;
        private VEShader _fragmentShader;
        private VEShaderConfig _shaderConfig;

        // Render configuration
        private VERenderConfig _renderConfig;

        // Animation
        private float _rotationAngle;
        private double _lastTime;

        public CubeExample() : base(GameWindowSettings.Default,
            new NativeWindowSettings()
            {
                Title = "VulkEase - Spinning Cube",
                Size = new Vector2i(WindowWidth, WindowHeight),
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
            Console.WriteLine("Modern Vulkan 1.3 with shader objects and bindless resources");
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

            _lastTime = GLFW.GetTime();

            Console.WriteLine("Initialization complete. Starting main loop...");
            Console.WriteLine("Controls: Close window to exit");
            Console.WriteLine();
            IsVisible = true;
        }

        protected override void OnResize(ResizeEventArgs e)
        {
            base.OnResize(e);

            if (_swapchain.native != IntPtr.Zero && e.Width > 0 && e.Height > 0)
            {
                VulkEase.ResizeSwapchain(_swapchain, (uint)e.Width, (uint)e.Height);
            }
        }

        protected override void OnRenderFrame(FrameEventArgs args)
        {
            base.OnRenderFrame(args);

            // Handle window minimize
            if (Size.X == 0 || Size.Y == 0)
            {
                return;
            }

            UpdateUniforms();
            RenderFrame();
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
                _context = VulkEase.CreateContext("VulkEase Cube Demo");
                Console.WriteLine("VulkEase initialized successfully");

                // Create device
                _device = VulkEase.CreateDevice(_context);
                Console.WriteLine("VulkEase device created successfully");
                Console.WriteLine($"Device: {VulkEase.GetDeviceName(_device)}");
                Console.WriteLine($"Driver: {VulkEase.GetDriverVersion(_device)}");

                // Get native window handle
                IntPtr windowHandle = GetNativeWindowHandle();
                if (windowHandle == IntPtr.Zero)
                {
                    Console.Error.WriteLine("Failed to get native window handle");
                    return false;
                }

                // Create swapchain
                _swapchain = VulkEase.CreateSwapchain(_device, windowHandle,
                    WindowWidth, WindowHeight, VkFormat.VK_FORMAT_B8G8R8A8_SRGB, EnableVSync);

                Console.WriteLine($"Swapchain created: {WindowWidth}x{WindowHeight}");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to initialize VulkEase: {ex.Message}");
                string? lastError = VulkEase.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
                return false;
            }
        }

        private IntPtr GetNativeWindowHandle()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                return GLFW.GetWin32Window(WindowPtr);
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                IntPtr display = GLFW.GetX11Display();
                IntPtr window = GLFW.GetX11Window(WindowPtr);
                
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
                _vertexShader = VulkEase.LoadShader(_device, "examples/shaders/cube.vert.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "CubeVertexShader");

                // Load fragment shader
                _fragmentShader = VulkEase.LoadShader(_device, "examples/shaders/cube.frag.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "CubeFragmentShader");

                // Create shader configuration
                var shaderConfigDesc = new VEShaderConfigDesc
                {
                    vertexShader = _vertexShader,
                    fragmentShader = _fragmentShader,
                    debugName = "CubeShaderConfig"
                };

                _shaderConfig = VulkEase.CreateShaderConfig(_device, shaderConfigDesc);

                Console.WriteLine("Shaders loaded successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to load shaders: {ex.Message}");
                string? lastError = VulkEase.GetLastError();
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
                _vertexBuffer = VulkEase.CreateVertexBuffer(_device, CubeVertices, "CubeVertices");

                if (_vertexBuffer == VEConstants.VE_INVALID_ADDRESS)
                {
                    Console.Error.WriteLine("Failed to create vertex buffer");
                    return false;
                }

                // Create index buffer
                _indexBuffer = VulkEase.CreateIndexBuffer(_device, CubeIndices, "CubeIndices");

                if (_indexBuffer == VEConstants.VE_INVALID_ADDRESS)
                {
                    Console.Error.WriteLine("Failed to create index buffer");
                    return false;
                }

                // Create uniform buffer (persistently mapped for easy updates)
                _uniformBuffer = VulkEase.CreateUniformBuffer(_device, 
                    (uint)Marshal.SizeOf<UniformData>(), true, "CubeUniforms");

                if (_uniformBuffer == VEConstants.VE_INVALID_ADDRESS)
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
                string? lastError = VulkEase.GetLastError();
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
                _texture = VulkEase.LoadTexture(_device, "examples/data/testCard.png",
                    VkImageUsageFlags.VK_IMAGE_USAGE_SAMPLED_BIT, true);

                if (_texture == VEConstants.VE_INVALID_TEXTURE_INDEX)
                {
                    Console.WriteLine("Texture file not found, creating fallback texture...");
                    
                    // Create a simple fallback texture if file doesn't exist
                    _texture = VulkEase.CreateTexture2D(_device, 2, 2, VkFormat.VK_FORMAT_R8G8B8A8_UNORM,
                        VkImageUsageFlags.VK_IMAGE_USAGE_SAMPLED_BIT, "FallbackTexture");

                    if (_texture == VEConstants.VE_INVALID_TEXTURE_INDEX)
                    {
                        Console.Error.WriteLine("Failed to create fallback texture");
                        return false;
                    }

                    // Fill with simple checkerboard pattern
                    uint[] pixels = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
                    var result = VulkEase.UpdateBuffer(_device, _texture, pixels, 0);
                    if (result != VEResult.VE_SUCCESS)
                    {
                        Console.WriteLine("Warning: Could not update fallback texture");
                    }
                }

                // Create linear sampler
                _sampler = VulkEase.CreateLinearSampler(_device);

                if (_sampler == VEConstants.VE_INVALID_SAMPLER_INDEX)
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
                string? lastError = VulkEase.GetLastError();
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
                // Define vertex input layout
                var bindings = new VEVertexBinding[]
                {
                    new()
                    {
                        binding = 0,
                        stride = (uint)Marshal.SizeOf<Vertex>(),
                        inputRate = VkVertexInputRate.VK_VERTEX_INPUT_RATE_VERTEX,
                        divisor = 1
                    }
                };

                var attributes = new VEVertexAttribute[]
                {
                    new()
                    {
                        location = 0, // Position
                        binding = 0,
                        format = VkFormat.VK_FORMAT_R32G32B32_SFLOAT,
                        offset = 0
                    },
                    new()
                    {
                        location = 1, // Texture coordinates
                        binding = 0,
                        format = VkFormat.VK_FORMAT_R32G32_SFLOAT,
                        offset = 12 // sizeof(Vector3)
                    }
                };

                var vertexInput = new VEVertexInputConfig
                {
                    bindingCount = 1,
                    bindings = bindings,
                    attributeCount = 2,
                    attributes = attributes,
                    topology = VkPrimitiveTopology.VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    primitiveRestartEnable = false
                };

                // Create render configuration
                var configDesc = new VERenderConfigDesc
                {
                    configTypes = VEConfigType.VE_CONFIG_TYPE_VERTEX_INPUT | 
                                  VEConfigType.VE_CONFIG_TYPE_RASTERIZATION |
                                  VEConfigType.VE_CONFIG_TYPE_DEPTH_STENCIL | 
                                  VEConfigType.VE_CONFIG_TYPE_COLOR_BLEND,
                    vertexInputConfig = vertexInput,
                    rasterConfig = null, // Use defaults
                    depthConfig = null, // Use defaults
                    blendConfig = null, // Use defaults
                    debugName = "CubeRenderConfig"
                };

                _renderConfig = VulkEase.CreateRenderConfig(_device, configDesc);

                Console.WriteLine("Render config created successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create render config: {ex.Message}");
                string? lastError = VulkEase.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
                return false;
            }
        }

        private void UpdateUniforms()
        {
            // Update rotation
            double currentTime = GLFW.GetTime();
            float deltaTime = (float)(currentTime - _lastTime);
            _lastTime = currentTime;
            _rotationAngle += deltaTime * 0.8f; // Rotate at 0.8 radians per second

            if (_rotationAngle > 2.0f * PI)
            {
                _rotationAngle -= 2.0f * PI;
            }

            // Build transformation matrices
            VulkEase.GetSwapchainSize(_swapchain, out uint width, out uint height);
            float aspect = (float)width / (float)height;

            var projection = CreatePerspectiveMatrix(PI * 0.25f, aspect, 0.1f, 100.0f);
            var view = CreateTranslationMatrix(0.0f, 0.0f, -5.0f);
            var model = CreateRotationMatrixY(_rotationAngle);
            
            var vm = MultiplyMatrix(ref model, ref view);
            var mvp = MultiplyMatrix(ref vm, ref projection);

            // Update uniform buffer
            var uniformData = new UniformData { MvpMatrix = mvp };
            var result = VulkEase.UpdateBuffer(_device, _uniformBuffer, uniformData, 0);

            if (result != VEResult.VE_SUCCESS)
            {
                Console.Error.WriteLine("Failed to update uniform buffer");
            }
        }

        private unsafe Mat4 CreatePerspectiveMatrix(float fov, float aspect, float near, float far)
        {
            var m = new Mat4();
            float tanHalfFov = MathF.Tan(fov * 0.5f);

            // Flipped Y for Vulkan screen coordinate system
            m.m[0] = 1.0f / (aspect * tanHalfFov);
            m.m[5] = -1.0f / tanHalfFov; // Flip Y for Vulkan
            m.m[10] = far / (near - far); // Z [0,1] mapping
            m.m[11] = -1.0f;
            m.m[14] = -(far * near) / (far - near); // Z [0,1] mapping

            return m;
        }

        private unsafe Mat4 CreateRotationMatrixY(float angle)
        {
            var m = Mat4.Identity();
            float c = MathF.Cos(angle);
            float s = MathF.Sin(angle);

            m.m[0] = c; m.m[2] = s;
            m.m[8] = -s; m.m[10] = c;

            return m;
        }

        private unsafe Mat4 CreateTranslationMatrix(float x, float y, float z)
        {
            var m = Mat4.Identity();
            m.m[12] = x;
            m.m[13] = y;
            m.m[14] = z;
            return m;
        }

        private unsafe Mat4 MultiplyMatrix(ref Mat4 a, ref Mat4 b)
        {
            var result = new Mat4();
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    for (int k = 0; k < 4; k++)
                    {
                        result.m[i * 4 + j] += a.m[i * 4 + k] * b.m[k * 4 + j];
                    }
                }
            }
            return result;
        }

        private void RenderFrame()
        {
            try
            {
                // Acquire next image
                uint backbuffer = VulkEase.AcquireNextImage(_swapchain);
                if (backbuffer == VEConstants.VE_INVALID_TEXTURE_INDEX)
                {
                    return; // Swapchain out of date, will be recreated
                }

                // Begin command buffer
                var cmd = VulkEase.BeginCommandBuffer(_device);

                VulkEase.TransitionTextureForColorAttachment(cmd, backbuffer);

                // Begin rendering to backbuffer
                VulkEase.GetSwapchainSize(_swapchain, out uint width, out uint height);

                var colorAttachment = new VERenderingAttachment
                {
                    texture = backbuffer,
                    loadOp = VkAttachmentLoadOp.VK_ATTACHMENT_LOAD_OP_CLEAR,
                    storeOp = VkAttachmentStoreOp.VK_ATTACHMENT_STORE_OP_STORE,
                    clearValue = new VEColor(0.1f, 0.2f, 0.3f, 1.0f), // Dark blue background
                    resolveTexture = VEConstants.VE_INVALID_TEXTURE_INDEX
                };

                var renderingInfo = new VERenderingInfo
                {
                    renderAreaX = 0,
                    renderAreaY = 0,
                    renderAreaWidth = width,
                    renderAreaHeight = height,
                    colorAttachmentCount = 1,
                    colorAttachments = new[] { colorAttachment },
                    depthAttachment = null,
                    stencilAttachment = null
                };

                VulkEase.BeginRendering(cmd, renderingInfo);

                // Set viewport and scissor
                VulkEase.SetViewport(cmd, 0.0f, 0.0f, width, height, 0.0f, 1.0f);
                VulkEase.SetScissor(cmd, 0, 0, width, height);

                // Bind shaders and render configuration
                VulkEase.BindShaderConfig(cmd, _shaderConfig);
                VulkEase.ApplyRenderConfig(cmd, _renderConfig);

                // Set up push constants with bindless resource indices
                var pushConstants = new VEGraphicsPushConstants();
                pushConstants.vertexBuffer = _vertexBuffer;
                pushConstants.indexBuffer = _indexBuffer;
                pushConstants.uniformBuffers[0] = _uniformBuffer;
                pushConstants.textures[0] = _texture;
                pushConstants.samplers[0] = _sampler;
                pushConstants.activeUniformCount = 1;
                pushConstants.activeTextureCount = 1;
                pushConstants.activeSamplerCount = 1;
                pushConstants.objectScale = 1.0f;

                VulkEase.PushConstants(cmd, pushConstants);

                // Draw the cube
                VulkEase.BindIndexBuffer(cmd, _indexBuffer, 0, VkIndexType.VK_INDEX_TYPE_UINT16);
                VulkEase.DrawIndexed(cmd, (uint)CubeIndices.Length, 1, 0, 0, 0);

                // End rendering
                VulkEase.EndRendering(cmd);

                // Present the frame
                VulkEase.TransitionTextureForPresent(cmd, backbuffer);
                var result = VulkEase.PresentImage(_swapchain, cmd);

                if (result == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    // Swapchain needs to be recreated (window resized)
                    VulkEase.GetSwapchainSize(_swapchain, out uint newWidth, out uint newHeight);
                    if (newWidth > 0 && newHeight > 0)
                    {
                        VulkEase.ResizeSwapchain(_swapchain, newWidth, newHeight);
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
                VulkEase.DeviceWaitIdle(_device);
            }

            // Destroy render config
            if (_renderConfig.native != IntPtr.Zero)
            {
                VulkEase.DestroyRenderConfig(_renderConfig);
            }

            // Destroy shaders
            if (_shaderConfig.native != IntPtr.Zero)
            {
                VulkEase.DestroyShaderConfig(_shaderConfig);
            }

            if (_vertexShader.native != IntPtr.Zero)
            {
                VulkEase.DestroyShader(_vertexShader);
            }

            if (_fragmentShader.native != IntPtr.Zero)
            {
                VulkEase.DestroyShader(_fragmentShader);
            }

            // Destroy resources
            if (_sampler != VEConstants.VE_INVALID_SAMPLER_INDEX)
            {
                VulkEase.DestroySampler(_device, _sampler);
            }

            if (_texture != VEConstants.VE_INVALID_TEXTURE_INDEX)
            {
                VulkEase.DestroyTexture(_device, _texture);
            }

            if (_uniformBuffer != VEConstants.VE_INVALID_ADDRESS)
            {
                VulkEase.DestroyBuffer(_device, _uniformBuffer);
            }

            if (_indexBuffer != VEConstants.VE_INVALID_ADDRESS)
            {
                VulkEase.DestroyBuffer(_device, _indexBuffer);
            }

            if (_vertexBuffer != VEConstants.VE_INVALID_ADDRESS)
            {
                VulkEase.DestroyBuffer(_device, _vertexBuffer);
            }

            // Destroy VulkEase objects
            if (_swapchain.native != IntPtr.Zero)
            {
                VulkEase.DestroySwapchain(_swapchain);
            }

            if (_device.native != IntPtr.Zero)
            {
                VulkEase.DestroyDevice(_device);
            }

            if (_context.native != IntPtr.Zero)
            {
                VulkEase.DestroyContext(_context);
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