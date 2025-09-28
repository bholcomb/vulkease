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
    /// Example 2: Compute Shader Particle System
    /// Demonstrates compute shaders, storage buffers, and GPU-driven rendering
    /// </summary>
    public class ComputeParticlesExample : GameWindow
    {
        private const int WindowWidth = 1024;
        private const int WindowHeight = 768;
        private const bool EnableVSync = true;
        private const int ParticleCount = 10000;
        private const int WorkgroupSize = 64;

        // Particle structure (matches GPU layout)
        [StructLayout(LayoutKind.Sequential)]
        private struct Particle
        {
            public Vector2 Position;
            public Vector2 Velocity;
            public float Life;
            public float Size;
            public Vector4 Color;
            public float Padding; // Align to 16 bytes

            public Particle(Vector2 position, Vector2 velocity, float life, float size, Vector4 color)
            {
                Position = position;
                Velocity = velocity;
                Life = life;
                Size = size;
                Color = color;
                Padding = 0.0f;
            }
        }

        // Compute push constants
        [StructLayout(LayoutKind.Sequential)]
        private struct ComputePushConstants
        {
            public ulong ParticleBufferAddress;
            public float DeltaTime;
            public float Time;
            public Vector2 AttractorPos;
            public float AttractorStrength;
            public uint ParticleCount;
        }

        // Graphics push constants
        [StructLayout(LayoutKind.Sequential)]
        private struct GraphicsPushConstants
        {
            public ulong VertexBufferAddress;
            public ulong ParticleBufferAddress;
            public Vector2 ScreenSize;
            public Vector2 Padding;
        }

        // VulkEase state
        private VEContext _context;
        private VEDevice _device;
        private VESwapchain _swapchain;
        private VEShader _computeShader;
        private VEShader _vertexShader;
        private VEShader _fragmentShader;
        private VERenderConfig _renderConfig;
        private VEBufferAddress _particleBuffer = VEConstants.VE_INVALID_ADDRESS;
        private VEBufferAddress _vertexBuffer = VEConstants.VE_INVALID_ADDRESS;

        // Animation state
        private double _lastTime;

        // Simple quad vertices for instanced particle rendering
        private static readonly Vector2[] QuadVertices = {
            new(-0.5f, -0.5f),
            new( 0.5f, -0.5f),
            new( 0.5f,  0.5f),
            new(-0.5f,  0.5f)
        };

        public ComputeParticlesExample() : base(GameWindowSettings.Default,
            new NativeWindowSettings()
            {
                Title = "VulkEase Compute Particles",
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

            Console.WriteLine("Initializing VulkEase Compute Particles Example...");

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

            if (!CreateRenderConfig())
            {
                Close();
                return;
            }

            _lastTime = GLFW.GetTime();

            Console.WriteLine($"Particle simulation initialized with {ParticleCount} particles");
            Console.WriteLine("Move mouse to attract particles!");
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

            double currentTime = GLFW.GetTime();
            float deltaTime = (float)(currentTime - _lastTime);
            float time = (float)currentTime;
            _lastTime = currentTime;

            Render(deltaTime, time);
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
                _context = VulkEase.CreateContext("VulkEase Compute Particle System");
                Console.WriteLine("VulkEase initialized successfully");

                // Create device
                _device = VulkEase.CreateDevice(_context);
                Console.WriteLine("VulkEase device created successfully");
                Console.WriteLine($"Device: {VulkEase.GetDeviceName(_device)}");
                Console.WriteLine($"Compute workgroup size: {WorkgroupSize} (assumed optimal)");

                // Get native window handle
                IntPtr windowHandle = GetNativeWindowHandle();
                if (windowHandle == IntPtr.Zero)
                {
                    Console.Error.WriteLine("Failed to get native window handle");
                    return false;
                }

                // Create swapchain
                _swapchain = VulkEase.CreateSwapchain(_device, windowHandle,
                    (uint)WindowWidth, (uint)WindowHeight, VkFormat.VK_FORMAT_B8G8R8A8_SRGB, EnableVSync);

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

        private bool CreateShaders()
        {
            try
            {
                // Load compute shader for particle simulation
                _computeShader = VulkEase.LoadShader(_device, "examples/shaders/particles.comp.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_COMPUTE_BIT, "main", "ParticleComputeShader");

                // Load graphics shaders for particle rendering
                _vertexShader = VulkEase.LoadShader(_device, "examples/shaders/particles.vert.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "ParticleVertexShader");

                _fragmentShader = VulkEase.LoadShader(_device, "examples/shaders/particles.frag.spv",
                    VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "ParticleFragmentShader");

                Console.WriteLine("Shaders loaded successfully");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create shaders: {ex.Message}");
                string? lastError = VulkEase.GetLastError();
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
                // Initialize particle data
                var particles = InitializeParticles(ParticleCount);

                // Create particle storage buffer (read/write from compute shader)
                var particleBufferDesc = new VEBufferDesc
                {
                    size = (ulong)(particles.Length * Marshal.SizeOf<Particle>()),
                    usage = VkBufferUsageFlags.VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    persistentlyMapped = false,
                    debugName = "ParticleStorageBuffer"
                };

                _particleBuffer = VulkEase.CreateBuffer(_device, particleBufferDesc, particles);

                if (_particleBuffer == VEConstants.VE_INVALID_ADDRESS)
                {
                    Console.Error.WriteLine("Failed to create particle buffer");
                    return false;
                }

                // Create vertex buffer for quad geometry
                var vertexBufferDesc = new VEBufferDesc
                {
                    size = (ulong)(QuadVertices.Length * Marshal.SizeOf<Vector2>()),
                    usage = VkBufferUsageFlags.VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    persistentlyMapped = false,
                    debugName = "QuadVertexBuffer"
                };

                _vertexBuffer = VulkEase.CreateBuffer(_device, vertexBufferDesc, QuadVertices);

                if (_vertexBuffer == VEConstants.VE_INVALID_ADDRESS)
                {
                    Console.Error.WriteLine("Failed to create vertex buffer");
                    return false;
                }

                Console.WriteLine($"Created particle buffer (0x{_particleBuffer:X}) for {ParticleCount} particles");
                Console.WriteLine($"Created vertex buffer (0x{_vertexBuffer:X}) for quad geometry");
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Failed to create buffers: {ex.Message}");
                string? lastError = VulkEase.GetLastError();
                if (lastError != null)
                {
                    Console.Error.WriteLine($"VulkEase Error: {lastError}");
                }
                return false;
            }
        }

        private Particle[] InitializeParticles(int count)
        {
            var particles = new Particle[count];
            var random = new Random();

            for (int i = 0; i < count; i++)
            {
                // Random position in circle
                float angle = (float)random.NextDouble() * 2.0f * MathF.PI;
                float radius = (float)random.NextDouble() * 0.5f;
                var position = new Vector2(MathF.Cos(angle) * radius, MathF.Sin(angle) * radius);

                // Random velocity
                var velocity = new Vector2(
                    ((float)random.NextDouble() - 0.5f) * 0.1f,
                    ((float)random.NextDouble() - 0.5f) * 0.1f
                );

                // Random properties
                float life = (float)random.NextDouble() * 5.0f + 1.0f;
                float size = (float)random.NextDouble() * 0.02f + 0.005f;

                // Random color
                var color = new Vector4(
                    (float)random.NextDouble(),
                    (float)random.NextDouble(),
                    (float)random.NextDouble(),
                    1.0f
                );

                particles[i] = new Particle(position, velocity, life, size, color);
            }

            return particles;
        }

        private bool CreateRenderConfig()
        {
            try
            {
                // Use transparent render config for particles with alpha blending
                _renderConfig = VulkEase.CreateTransparentRenderConfig(_device, "ParticleRenderConfig");
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

        private void RunComputeShader(VECommandBuffer cmd, float deltaTime, float time, Vector2 mousePos)
        {
            // Begin compute debug region
            var computeColor = new VEColor(1.0f, 0.0f, 1.0f, 1.0f);
            VulkEase.BeginDebugLabel(cmd, "Particle Simulation", computeColor);

            // Bind compute shader
            VulkEase.BindShader(cmd, _computeShader);

            // Set compute push constants
            var computeConstants = new ComputePushConstants
            {
                ParticleBufferAddress = _particleBuffer,
                DeltaTime = deltaTime,
                Time = time,
                AttractorPos = mousePos,
                AttractorStrength = 0.1f,
                ParticleCount = ParticleCount
            };

            VulkEase.PushConstants(cmd, computeConstants);

            // Calculate dispatch size
            uint numGroups = (uint)((ParticleCount + WorkgroupSize - 1) / WorkgroupSize);

            // Dispatch compute work
            VulkEase.Dispatch(cmd, numGroups, 1, 1);

            // Add barrier between compute and graphics
            VulkEase.BarrierComputeToVertex(cmd);

            VulkEase.EndDebugLabel(cmd);
        }

        private void RenderParticles(VECommandBuffer cmd, uint backbuffer)
        {
            VulkEase.GetSwapchainSize(_swapchain, out uint width, out uint height);

            // Set up rendering info
            var colorAttachment = new VERenderingAttachment
            {
                texture = backbuffer,
                loadOp = VkAttachmentLoadOp.VK_ATTACHMENT_LOAD_OP_CLEAR,
                storeOp = VkAttachmentStoreOp.VK_ATTACHMENT_STORE_OP_STORE,
                clearValue = new VEColor(0.05f, 0.05f, 0.1f, 1.0f), // Dark blue background
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

            // Begin graphics debug region
            var graphicsColor = new VEColor(0.0f, 1.0f, 0.5f, 1.0f);
            VulkEase.BeginDebugLabel(cmd, "Particle Rendering", graphicsColor);

            // Begin rendering
            VulkEase.BeginRendering(cmd, renderingInfo);

            // Setup viewport and scissor state
            VulkEase.SetViewport(cmd, 0.0f, 0.0f, width, height, 0.0f, 1.0f);
            VulkEase.SetScissor(cmd, 0, 0, width, height);

            // Apply render configuration (with alpha blending)
            VulkEase.ApplyRenderConfig(cmd, _renderConfig);

            // Bind graphics shaders
            VulkEase.BindShader(cmd, _vertexShader);
            VulkEase.BindShader(cmd, _fragmentShader);

            // Set graphics push constants
            var graphicsConstants = new GraphicsPushConstants
            {
                VertexBufferAddress = _vertexBuffer,
                ParticleBufferAddress = _particleBuffer,
                ScreenSize = new Vector2(width, height),
                Padding = Vector2.Zero
            };

            VulkEase.PushConstants(cmd, graphicsConstants);

            // Draw instanced particles (4 vertices per quad, ParticleCount instances)
            VulkEase.Draw(cmd, 4, (uint)ParticleCount, 0, 0);

            // End rendering
            VulkEase.EndRendering(cmd);
            VulkEase.EndDebugLabel(cmd);
        }

        private void Render(float deltaTime, float time)
        {
            try
            {
                // Convert to normalized coordinates [-1, 1] - use animated attractor for demo
                float normalizedMouseX = MathF.Sin(time);
                float normalizedMouseY = MathF.Cos(time);

                // Acquire next swapchain image
                uint backbuffer = VulkEase.AcquireNextImage(_swapchain);
                if (backbuffer == VEConstants.VE_INVALID_TEXTURE_INDEX)
                {
                    return;
                }

                // Begin command buffer
                var cmd = VulkEase.BeginCommandBuffer(_device);

                VulkEase.TransitionTextureForColorAttachment(cmd, backbuffer);

                // Run compute shader to simulate particles
                RunComputeShader(cmd, deltaTime, time, new Vector2(normalizedMouseX, normalizedMouseY));

                // Render particles
                RenderParticles(cmd, backbuffer);

                // Present
                VulkEase.TransitionTextureForPresent(cmd, backbuffer);
                var result = VulkEase.PresentImage(_swapchain, cmd);

                if (result == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
                {
                    VulkEase.GetSwapchainSize(_swapchain, out uint swWidth, out uint swHeight);
                    VulkEase.ResizeSwapchain(_swapchain, swWidth, swHeight);
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Render error: {ex.Message}");
            }
        }

        protected override void OnUnload()
        {
            Console.WriteLine("Shutting down Compute Particles example...");

            if (_device.native != IntPtr.Zero)
            {
                VulkEase.DeviceWaitIdle(_device);
            }

            if (_particleBuffer != VEConstants.VE_INVALID_ADDRESS)
            {
                VulkEase.DestroyBuffer(_device, _particleBuffer);
            }

            if (_vertexBuffer != VEConstants.VE_INVALID_ADDRESS)
            {
                VulkEase.DestroyBuffer(_device, _vertexBuffer);
            }

            if (_renderConfig.native != IntPtr.Zero)
            {
                VulkEase.DestroyRenderConfig(_renderConfig);
            }

            if (_computeShader.native != IntPtr.Zero)
            {
                VulkEase.DestroyShader(_computeShader);
            }

            if (_vertexShader.native != IntPtr.Zero)
            {
                VulkEase.DestroyShader(_vertexShader);
            }

            if (_fragmentShader.native != IntPtr.Zero)
            {
                VulkEase.DestroyShader(_fragmentShader);
            }

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
            Console.WriteLine("VulkEase Compute Particles Example");
            Console.WriteLine("==================================");
            Console.WriteLine("Press ESC to exit");
            Console.WriteLine();

            using var particles = new ComputeParticlesExample();
            particles.Run();
        }
    }
}