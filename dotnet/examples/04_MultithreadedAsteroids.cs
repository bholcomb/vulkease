using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using VulkEase;
using static VulkEase.VulkEase;

// Use System.Numerics for Matrix4x4 operations
using Matrix4x4 = System.Numerics.Matrix4x4;
using SysVector3 = System.Numerics.Vector3;
using SysVector4 = System.Numerics.Vector4;

namespace VulkEaseExamples
{
    public sealed class MultithreadedAsteroids : GameWindow
    {
        private const int InstanceCount = 4096;
        private const int ChunkSize = 64;
        private const float CameraRadius = 55.0f;
        private static readonly string VertexShaderPath = "examples/shaders/asteroid.vert.spv";
        private static readonly string FragmentShaderPath = "examples/shaders/asteroid.frag.spv";

        [StructLayout(LayoutKind.Sequential)]
        private struct Vertex
        {
            public SysVector3 Position;
            public SysVector3 Normal;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct InstanceData
        {
            public Matrix4x4 Model;
            public SysVector4 Color;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct CameraData
        {
            public Matrix4x4 ViewProj;
            public SysVector4 CameraPos;
        }

        private readonly record struct Chunk(int FirstInstance, int Count);

        private readonly record struct ChunkRecord(int ChunkIndex, VECommandBuffer CommandBuffer);

        private readonly record struct ChunkUpdate(int Start, InstanceData[] Data);

        private readonly struct MetricsSample
        {
            public MetricsSample(ulong frame, double frameMs, double recordMs, double submitMs, double streamMs, double fps, int workers, bool multithreaded)
            {
                Frame = frame;
                FrameMs = frameMs;
                RecordMs = recordMs;
                SubmitMs = submitMs;
                StreamMs = streamMs;
                Fps = fps;
                Workers = workers;
                Multithreaded = multithreaded;
            }

            public ulong Frame { get; }
            public double FrameMs { get; }
            public double RecordMs { get; }
            public double SubmitMs { get; }
            public double StreamMs { get; }
            public double Fps { get; }
            public int Workers { get; }
            public bool Multithreaded { get; }
        }

        private sealed class MetricsCollector
        {
            private readonly Queue<MetricsSample> _samples = new();
            private readonly int _capacity;
            private double _consoleTimer;

            public MetricsCollector(int capacity = 240)
            {
                _capacity = Math.Max(1, capacity);
            }

            public void Submit(MetricsSample sample, double deltaSeconds)
            {
                if (_samples.Count == _capacity)
                {
                    _samples.Dequeue();
                }
                _samples.Enqueue(sample);
                _consoleTimer += deltaSeconds;
                if (_consoleTimer >= 2.0)
                {
                    _consoleTimer = 0.0;
                    Console.WriteLine($"[Metrics] frame={sample.Frame} fps={sample.Fps:F1} ms(frame/record/submit/stream)={sample.FrameMs:F2}/{sample.RecordMs:F2}/{sample.SubmitMs:F2}/{sample.StreamMs:F2} workers={sample.Workers} mode={(sample.Multithreaded ? "MT" : "ST")}");
                }
            }

            public string TitleSummary()
            {
                if (_samples.Count == 0)
                    return string.Empty;

                MetricsSample latest = _samples.Last();
                double avgFrame = _samples.Average(s => s.FrameMs);
                double avgRecord = _samples.Average(s => s.RecordMs);
                return $"VulkEase Asteroids | {(latest.Multithreaded ? "MT" : "ST")} | FPS {latest.Fps:F1} | Frame {avgFrame:F2}ms | Record {avgRecord:F2}ms";
            }
        }

        private sealed class SimulationSystem
        {
            private readonly InstanceData[] _state;
            private readonly Channel<ChunkUpdate> _channel;
            private readonly Random _rng = new();
            private readonly CancellationTokenSource _cts = new();
            private readonly int _chunkSize;
            private readonly int _chunkCount;

            public SimulationSystem(InstanceData[] initialState, int chunkSize)
            {
                _state = initialState;
                _chunkSize = chunkSize;
                _chunkCount = (initialState.Length + chunkSize - 1) / chunkSize;
                _channel = Channel.CreateUnbounded<ChunkUpdate>(new UnboundedChannelOptions { SingleReader = true, SingleWriter = false });
            }

            public ChannelReader<ChunkUpdate> Reader => _channel.Reader;

            public void Start()
            {
                Task.Run(async () =>
                {
                    try
                    {
                        while (!_cts.Token.IsCancellationRequested)
                        {
                            int chunk = _rng.Next(_chunkCount);
                            var update = MutateChunk(chunk);
                            await _channel.Writer.WriteAsync(update, _cts.Token).ConfigureAwait(false);
                            await Task.Delay(16, _cts.Token).ConfigureAwait(false);
                        }
                    }
                    catch (OperationCanceledException)
                    {
                    }
                    finally
                    {
                        _channel.Writer.TryComplete();
                    }
                }, _cts.Token);
            }

            public void Stop()
            {
                _cts.Cancel();
            }

            public void CopyState(InstanceData[] destination)
            {
                Array.Copy(_state, destination, _state.Length);
            }

            private ChunkUpdate MutateChunk(int chunkIndex)
            {
                int start = chunkIndex * _chunkSize;
                int end = Math.Min(start + _chunkSize, _state.Length);
                var data = new InstanceData[end - start];

                for (int i = start; i < end; ++i)
                {
                    InstanceData instance = _state[i];

                    float jitter = (float)(_rng.NextDouble() - 0.5) * 0.1f;
                    var axis = SysVector3.Normalize(new SysVector3(
                        (float)_rng.NextDouble(),
                        (float)_rng.NextDouble(),
                        (float)_rng.NextDouble()));
                    var rotation = Matrix4x4.CreateFromAxisAngle(axis, jitter);
                    instance.Model = rotation * instance.Model;

                    float drift = (float)(_rng.NextDouble() - 0.5) * 0.15f;
                    instance.Model.M41 += instance.Model.M11 * drift;
                    instance.Model.M42 += ((float)_rng.NextDouble() - 0.5f) * 0.05f;
                    instance.Model.M43 += instance.Model.M13 * drift;

                    float tint = 0.02f * ((float)_rng.NextDouble() - 0.5f);
                    instance.Color.X = Math.Clamp(instance.Color.X + tint, 0.2f, 1.0f);
                    instance.Color.Y = Math.Clamp(instance.Color.Y + tint, 0.2f, 1.0f);
                    instance.Color.Z = Math.Clamp(instance.Color.Z + tint, 0.2f, 1.0f);

                    _state[i] = instance;
                    data[i - start] = instance;
                }

                return new ChunkUpdate(start, data);
            }
        }

        private readonly MetricsCollector _metrics = new();

        private VEContext _context;
        private VEDevice _device;
        private VESwapchain _swapchain;

        private VEShader _vertexShader;
        private VEShader _fragmentShader;
        private VEShaderConfig _shaderConfig;
        private VERenderConfig _renderConfig;

        private VEBufferAddress _vertexBuffer;
        private VEBufferAddress _indexBuffer;
        private VEBufferAddress _instanceBuffer;
        private VEBufferAddress _cameraBuffer;

        // Depth buffer for proper 3D rendering
        private VETextureIndex _depthTexture;
        private uint _depthWidth;
        private uint _depthHeight;

        private readonly List<Chunk> _chunks = new();
        private InstanceData[] _instances = Array.Empty<InstanceData>();

        private readonly List<ChunkRecord> _recorded = new();
        private readonly List<VECommandBuffer> _secondaryScratch = new();

        private SimulationSystem _simulation;
        private ChannelReader<ChunkUpdate> _updateReader;

        private readonly Stopwatch _frameTimer = Stopwatch.StartNew();
        private ulong _frameIndex;
        private double _elapsedTime;  // Total elapsed time for framerate-independent animation
        private bool _multithreaded = true;

        private uint _indexCount;

        // Swapchain format for inheritance info
        private VkFormat _swapchainFormat = VkFormat.VK_FORMAT_B8G8R8A8_SRGB;

        // Cached rendering inheritance info for secondary command buffers (allocated in unmanaged memory)
        private IntPtr _inheritanceRenderingPtr;
        private IntPtr _colorFormatsPtr;

        public MultithreadedAsteroids(string[] args)
            : base(GameWindowSettings.Default, new NativeWindowSettings
            {
                Title = "VulkEase Asteroids",
                ClientSize = new Vector2i(1280, 720),
                StartVisible = false,
                StartFocused = true,
                API = ContextAPI.NoAPI,
                Flags = ContextFlags.Default,
            })
        {
            foreach (string arg in args)
            {
                if (string.Equals(arg, "--no-mt", StringComparison.OrdinalIgnoreCase))
                {
                    _multithreaded = false;
                }
            }
        }

        protected override void OnLoad()
        {
            base.OnLoad();

            Console.WriteLine("VulkEase Multithreaded Asteroids");
            Console.WriteLine("Controls: M toggles multi-threaded command recording, ESC quits");

            if (!InitializeVulkEase())
            {
                Close();
                return;
            }

            if (!LoadAssets())
            {
                Close();
                return;
            }

            InitializeSimulation();

            IsVisible = true;
        }

        protected override void OnUnload()
        {
            _simulation?.Stop();

            if (_device.native != IntPtr.Zero)
            {
                DeviceWaitIdle(_device);
            }

            CleanupInheritanceInfo();

            if (_depthTexture.native != VEConstants.VE_INVALID_TEXTURE_INDEX.native)
                DestroyTexture(_device, _depthTexture);
            if (_cameraBuffer.native != 0)
                DestroyBuffer(_device, _cameraBuffer);
            if (_instanceBuffer.native != 0)
                DestroyBuffer(_device, _instanceBuffer);
            if (_indexBuffer.native != 0)
                DestroyBuffer(_device, _indexBuffer);
            if (_vertexBuffer.native != 0)
                DestroyBuffer(_device, _vertexBuffer);

            if (_renderConfig.native != IntPtr.Zero)
                DestroyRenderConfig(_renderConfig);
            if (_shaderConfig.native != IntPtr.Zero)
                DestroyShaderConfig(_shaderConfig);
            if (_fragmentShader.native != IntPtr.Zero)
                DestroyShader(_fragmentShader);
            if (_vertexShader.native != IntPtr.Zero)
                DestroyShader(_vertexShader);

            if (_swapchain.native != IntPtr.Zero)
                DestroySwapchain(_swapchain);
            if (_device.native != IntPtr.Zero)
                DestroyDevice(_device);
            if (_context.native != IntPtr.Zero)
                DestroyContext(_context);

            base.OnUnload();
        }

        protected override void OnResize(ResizeEventArgs e)
        {
            base.OnResize(e);

            if (_swapchain.native != IntPtr.Zero && e.Width > 0 && e.Height > 0)
            {
                ResizeSwapchain(_swapchain, (uint)e.Width, (uint)e.Height);
                RecreateDepthBuffer((uint)e.Width, (uint)e.Height);
            }
        }

        private bool RecreateDepthBuffer(uint width, uint height)
        {
            if (_device.native == IntPtr.Zero)
                return false;

            // Skip if dimensions haven't changed
            if (width == _depthWidth && height == _depthHeight && 
                _depthTexture.native != VEConstants.VE_INVALID_TEXTURE_INDEX.native)
                return true;

            // Wait for GPU to finish using the old depth buffer
            DeviceWaitIdle(_device);

            // Destroy old depth buffer
            if (_depthTexture.native != VEConstants.VE_INVALID_TEXTURE_INDEX.native)
            {
                DestroyTexture(_device, _depthTexture);
                _depthTexture = VEConstants.VE_INVALID_TEXTURE_INDEX;
            }

            // Create new depth buffer at the new size
            _depthTexture = CreateTexture2D(_device, width, height, VkFormat.VK_FORMAT_D32_SFLOAT,
                VkImageUsageFlags.VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VkImageUsageFlags.VK_IMAGE_USAGE_SAMPLED_BIT,
                "AsteroidDepthBuffer");

            if (_depthTexture.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
            {
                Console.WriteLine($"Failed to recreate depth buffer: {GetLastError()}");
                return false;
            }

            _depthWidth = width;
            _depthHeight = height;
            return true;
        }

        protected override void OnKeyDown(KeyboardKeyEventArgs e)
        {
            base.OnKeyDown(e);
            if (e.Key == Keys.Escape)
            {
                Close();
            }
            else if (e.Key == Keys.M)
            {
                _multithreaded = !_multithreaded;
                Console.WriteLine($"[Asteroids] Multithreaded recording {(_multithreaded ? "enabled" : "disabled")}");
            }
        }

        protected override void OnRenderFrame(FrameEventArgs args)
        {
            base.OnRenderFrame(args);

            if (ClientSize.X == 0 || ClientSize.Y == 0)
            {
                return;
            }

            double streamingMs = DrainSimulationUpdates();

            double recordMs = RecordChunks();
            double submitMs = SubmitFrame();

            double frameMs = _frameTimer.Elapsed.TotalMilliseconds;
            _frameTimer.Restart();

            // Track elapsed time for framerate-independent animation
            _elapsedTime += args.Time;

            double fps = frameMs > 0.0 ? 1000.0 / frameMs : 0.0;
            _metrics.Submit(new MetricsSample(_frameIndex, frameMs, recordMs, submitMs, streamingMs, fps,
                _multithreaded ? Environment.ProcessorCount : 1, _multithreaded), args.Time);
            Title = _metrics.TitleSummary();

            _frameIndex++;
        }

        private unsafe bool InitializeVulkEase()
        {
            _context = CreateContext("VulkEase Asteroids");
            if (_context.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create context: " + GetLastError());
                return false;
            }

            _device = CreateDevice(_context);
            if (_device.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create device: " + GetLastError());
                return false;
            }

            Console.WriteLine($"Using device {GetDeviceName(_device)}");

            IntPtr windowHandle;
            IntPtr linuxHandleBlock = IntPtr.Zero;
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                windowHandle = GLFW.GetWin32Window(WindowPtr);
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                IntPtr display = (IntPtr)GLFW.GetX11Display();
                IntPtr window = (IntPtr)GLFW.GetX11Window(WindowPtr);
                linuxHandleBlock = Marshal.AllocHGlobal(IntPtr.Size * 2);
                Marshal.WriteIntPtr(linuxHandleBlock, 0, display);
                Marshal.WriteIntPtr(linuxHandleBlock, IntPtr.Size, window);
                windowHandle = linuxHandleBlock;
            }
            else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                windowHandle = GLFW.GetCocoaWindow(WindowPtr);
            }
            else
            {
                Console.Error.WriteLine("Unsupported platform");
                return false;
            }

            _swapchain = CreateSwapchain(_device, windowHandle, (uint)ClientSize.X, (uint)ClientSize.Y,
                VkFormat.VK_FORMAT_B8G8R8A8_SRGB, true);
            if (_swapchain.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create swapchain: " + GetLastError());
                if (linuxHandleBlock != IntPtr.Zero)
                    Marshal.FreeHGlobal(linuxHandleBlock);
                return false;
            }

            if (linuxHandleBlock != IntPtr.Zero)
                Marshal.FreeHGlobal(linuxHandleBlock);

            // Initialize inheritance info for secondary command buffers (dynamic rendering)
            InitializeInheritanceInfo();

            return true;
        }

        private void InitializeInheritanceInfo()
        {
            // Allocate unmanaged memory for the color formats array
            _colorFormatsPtr = Marshal.AllocHGlobal(sizeof(int)); // VkFormat is an int (4 bytes)
            Marshal.WriteInt32(_colorFormatsPtr, (int)_swapchainFormat);

            // Allocate and populate the inheritance rendering info in unmanaged memory
            var inheritanceRendering = new VkCommandBufferInheritanceRenderingInfo
            {
                sType = 1000044004, // VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO
                pNext = IntPtr.Zero,
                flags = 0,
                viewMask = 0,
                colorAttachmentCount = 1,
                pColorAttachmentFormats = _colorFormatsPtr,
                depthAttachmentFormat = VkFormat.VK_FORMAT_D32_SFLOAT,
                stencilAttachmentFormat = VkFormat.VK_FORMAT_UNDEFINED,
                rasterizationSamples = VkSampleCountFlags.VK_SAMPLE_COUNT_1_BIT
            };

            _inheritanceRenderingPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VkCommandBufferInheritanceRenderingInfo>());
            Marshal.StructureToPtr(inheritanceRendering, _inheritanceRenderingPtr, false);
        }

        private void CleanupInheritanceInfo()
        {
            if (_inheritanceRenderingPtr != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(_inheritanceRenderingPtr);
                _inheritanceRenderingPtr = IntPtr.Zero;
            }
            if (_colorFormatsPtr != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(_colorFormatsPtr);
                _colorFormatsPtr = IntPtr.Zero;
            }
        }

        private bool LoadAssets()
        {
            _vertexShader = LoadShaderFromFile(_device, VertexShaderPath, VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "AsteroidVertex");
            if (_vertexShader.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to load vertex shader: " + GetLastError());
                return false;
            }

            _fragmentShader = LoadShaderFromFile(_device, FragmentShaderPath, VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "AsteroidFragment");
            if (_fragmentShader.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to load fragment shader: " + GetLastError());
                return false;
            }

            VEShaderConfigDesc shaderDesc = new()
            {
                vertexShader = _vertexShader,
                fragmentShader = _fragmentShader,
                debugName = "AsteroidShader"
            };
            _shaderConfig = CreateShaderConfig(_device, shaderDesc);
            if (_shaderConfig.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create shader config: " + GetLastError());
                return false;
            }

            _renderConfig = CreateOpaqueRenderConfig(_device, "AsteroidRenderConfig");
            if (_renderConfig.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create render config: " + GetLastError());
                return false;
            }

            var vertices = BuildIcosahedronVertices();
            var indices = BuildIcosahedronIndices();

            _vertexBuffer = CreateVertexBufferFromArray(_device, vertices, "AsteroidVertices");
            if (_vertexBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create vertex buffer: " + GetLastError());
                return false;
            }

            _indexBuffer = CreateIndexBufferFromArray(_device, indices, "AsteroidIndices");
            if (_indexBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create index buffer: " + GetLastError());
                return false;
            }
            _indexCount = (uint)indices.Length;

            _instances = CreateInitialInstances();

            VEBufferDesc instanceDesc = new()
            {
                size = (ulong)(_instances.Length * Marshal.SizeOf<InstanceData>()),
                usage = VkBufferUsageFlags.VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VkBufferUsageFlags.VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                persistentlyMapped = false,
                debugName = "AsteroidInstances"
            };
            _instanceBuffer = CreateBuffer(_device, instanceDesc);
            if (_instanceBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create instance buffer: " + GetLastError());
                return false;
            }
            UploadInstances(_instances, 0);

            VEBufferDesc cameraDesc = new()
            {
                size = (ulong)Marshal.SizeOf<CameraData>(),
                usage = VkBufferUsageFlags.VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                persistentlyMapped = false,
                debugName = "AsteroidCamera"
            };
            _cameraBuffer = CreateBuffer(_device, cameraDesc);
            if (_cameraBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create camera buffer: " + GetLastError());
                return false;
            }

            // Create depth buffer
            GetSwapchainSize(_swapchain, out uint fbWidth, out uint fbHeight);
            _depthTexture = CreateTexture2D(_device, fbWidth, fbHeight, VkFormat.VK_FORMAT_D32_SFLOAT,
                VkImageUsageFlags.VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VkImageUsageFlags.VK_IMAGE_USAGE_SAMPLED_BIT,
                "AsteroidDepthBuffer");
            if (_depthTexture.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
            {
                Console.Error.WriteLine("Failed to create depth buffer: " + GetLastError());
                return false;
            }
            _depthWidth = fbWidth;
            _depthHeight = fbHeight;

            _chunks.Clear();
            for (int first = 0; first < _instances.Length; first += ChunkSize)
            {
                int count = Math.Min(ChunkSize, _instances.Length - first);
                _chunks.Add(new Chunk(first, count));
            }

            return true;
        }

        private void InitializeSimulation()
        {
            _simulation = new SimulationSystem(_instances.ToArray(), ChunkSize);
            _simulation.CopyState(_instances);
            UploadInstances(_instances, 0);
            _simulation.Start();
            _updateReader = _simulation.Reader;
        }

        private double DrainSimulationUpdates()
        {
            if (_updateReader == null)
                return 0.0;

            Stopwatch sw = Stopwatch.StartNew();
            int applied = 0;
            while (_updateReader.TryRead(out ChunkUpdate update))
            {
                Array.Copy(update.Data, 0, _instances, update.Start, update.Data.Length);
                UploadInstances(update.Data, update.Start);
                applied++;
            }
            sw.Stop();
            return applied > 0 ? sw.Elapsed.TotalMilliseconds : 0.0;
        }

        private double RecordChunks()
        {
            Stopwatch sw = Stopwatch.StartNew();
            _recorded.Clear();

            if (_multithreaded)
            {
                var tasks = new Task<ChunkRecord>[_chunks.Count];
                for (int i = 0; i < _chunks.Count; ++i)
                {
                    int chunkIndex = i;
                    tasks[i] = Task.Run(() => RecordChunk(chunkIndex));
                }
                Task.WaitAll(tasks);
                _recorded.AddRange(tasks.Select(t => t.Result));
            }
            else
            {
                for (int i = 0; i < _chunks.Count; ++i)
                {
                    _recorded.Add(RecordChunk(i));
                }
            }

            _recorded.Sort((a, b) => a.ChunkIndex.CompareTo(b.ChunkIndex));

            sw.Stop();
            return sw.Elapsed.TotalMilliseconds;
        }

        private double SubmitFrame()
        {
            Stopwatch sw = Stopwatch.StartNew();

            var backbuffer = AcquireNextImage(_swapchain);
            if (backbuffer.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
            {
                // Swapchain may have been recreated or window minimized - try to handle resize
                GetSwapchainSize(_swapchain, out uint newWidth, out uint newHeight);
                if (newWidth > 0 && newHeight > 0)
                {
                    ResizeSwapchain(_swapchain, newWidth, newHeight);
                    RecreateDepthBuffer(newWidth, newHeight);
                }
                sw.Stop();
                return sw.Elapsed.TotalMilliseconds;
            }

            // Check if swapchain size changed and depth buffer needs recreation
            GetSwapchainSize(_swapchain, out uint width, out uint height);
            if (width != _depthWidth || height != _depthHeight)
            {
                RecreateDepthBuffer(width, height);
            }

            var cmd = BeginCommandBuffer(_device);

            TransitionTextureForColorAttachment(cmd, backbuffer);
            TransitionTextureForDepthAttachment(cmd, _depthTexture);

            var colorAttachment = new VERenderingAttachment
            {
                texture = backbuffer,
                loadOp = VkAttachmentLoadOp.VK_ATTACHMENT_LOAD_OP_CLEAR,
                storeOp = VkAttachmentStoreOp.VK_ATTACHMENT_STORE_OP_STORE,
                clearValue = new VEColor(0.01f, 0.01f, 0.015f, 1.0f),
                resolveTexture = VEConstants.VE_INVALID_TEXTURE_INDEX
            };

            var depthAttachment = new VERenderingAttachment
            {
                texture = _depthTexture,
                loadOp = VkAttachmentLoadOp.VK_ATTACHMENT_LOAD_OP_CLEAR,
                storeOp = VkAttachmentStoreOp.VK_ATTACHMENT_STORE_OP_DONT_CARE,
                clearValue = new VEColor(1.0f, 0.0f, 0.0f, 0.0f),  // Clear depth to 1.0
                resolveTexture = VEConstants.VE_INVALID_TEXTURE_INDEX
            };

            var renderingInfo = new VERenderingInfo
            {
                RenderAreaX = 0,
                RenderAreaY = 0,
                RenderAreaWidth = width,
                RenderAreaHeight = height,
                DepthAttachment = depthAttachment
            };
            renderingInfo.ColorAttachments.Add(colorAttachment);

            BeginRendering(cmd, renderingInfo);

            UpdateCameraBuffer(width, height);

            if (_recorded.Count > 0)
            {
                _secondaryScratch.Clear();
                foreach (var entry in _recorded)
                {
                    _secondaryScratch.Add(entry.CommandBuffer);
                }
                ExecuteSecondaryCommandBuffers(cmd, _secondaryScratch);
            }

            EndRendering(cmd);

            TransitionTextureForPresent(cmd, backbuffer);

            var present = PresentImage(_swapchain, cmd);
            if (present == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
            {
                GetSwapchainSize(_swapchain, out uint newWidth, out uint newHeight);
                if (newWidth > 0 && newHeight > 0)
                {
                    ResizeSwapchain(_swapchain, newWidth, newHeight);
                    RecreateDepthBuffer(newWidth, newHeight);
                }
            }

            sw.Stop();
            return sw.Elapsed.TotalMilliseconds;
        }

        private void UpdateCameraBuffer(uint width, uint height)
        {
            // Use actual elapsed time so animation speed is framerate-independent
            float time = (float)_elapsedTime;
            SysVector3 eye = new(
                MathF.Cos(time * 0.25f) * CameraRadius,
                15.0f,
                MathF.Sin(time * 0.25f) * CameraRadius);
            SysVector3 target = SysVector3.Zero;
            SysVector3 up = SysVector3.UnitY;

            Matrix4x4 view = Matrix4x4.CreateLookAt(eye, target, up);

            float aspect = width / (float)height;
            Matrix4x4 proj = Matrix4x4.CreatePerspectiveFieldOfView(MathF.PI / 3.0f, aspect, 0.1f, 500.0f);
            proj.M22 *= -1.0f; // Vulkan clip space Y flipped

            Matrix4x4 viewProj = view * proj;

            CameraData data = new()
            {
                ViewProj = Matrix4x4.Transpose(viewProj),
                CameraPos = new SysVector4(eye.X, eye.Y, eye.Z, 1.0f)
            };

            IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf<CameraData>());
            Marshal.StructureToPtr(data, ptr, false);
            UpdateBuffer(_device, _cameraBuffer, ptr, (ulong)Marshal.SizeOf<CameraData>(), 0);
            Marshal.FreeHGlobal(ptr);
        }

        private ChunkRecord RecordChunk(int chunkIndex)
        {
            var chunk = _chunks[chunkIndex];

            // Create secondary command buffer descriptor with inheritance info for dynamic rendering
            var inheritanceInfo = new VkCommandBufferInheritanceInfo
            {
                sType = 41, // VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO
                pNext = _inheritanceRenderingPtr, // Points to our pre-allocated unmanaged memory
                renderPass = IntPtr.Zero,
                subpass = 0,
                framebuffer = IntPtr.Zero,
                occlusionQueryEnable = 0, // VK_FALSE
                queryFlags = 0,
                pipelineStatistics = 0
            };

            var desc = new VESecondaryCommandBufferDesc
            {
                usageFlags = 0, // Let C code use default (ONE_TIME_SUBMIT | RENDER_PASS_CONTINUE)
                inheritanceInfo = inheritanceInfo,
                beginRecording = true
            };

            var cmd = BeginSecondaryCommandBuffer(_device, desc);

            BindShaderConfig(cmd, _shaderConfig);
            ApplyRenderConfig(cmd, _renderConfig);
            SetViewport(cmd, 0.0f, 0.0f, ClientSize.X, ClientSize.Y, 0.0f, 1.0f);
            SetScissor(cmd, 0, 0, (uint)ClientSize.X, (uint)ClientSize.Y);

            var push = new VEGraphicsPushConstants
            {
                vertexBuffer = _vertexBuffer.native,
                indexBuffer = _indexBuffer.native,
                uniformBuffers = new ulong[8],
                textures = new uint[16],
                samplers = new uint[16],
                reserved = new uint[7],
                objectScale = 1.0f,
                activeUniformCount = 2,
                activeTextureCount = 0,
                activeSamplerCount = 0
            };
            push.uniformBuffers[0] = _cameraBuffer.native;
            push.uniformBuffers[1] = _instanceBuffer.native;

            PushConstants(cmd, push);

            BindIndexBuffer(cmd, _indexBuffer, 0, VkIndexType.VK_INDEX_TYPE_UINT16);
            DrawIndexed(cmd, _indexCount, (uint)chunk.Count, 0, 0, (uint)chunk.FirstInstance);

            EndCommandBuffer(cmd);

            return new ChunkRecord(chunkIndex, cmd);
        }

        private void UploadInstances(IReadOnlyList<InstanceData> instances, int offset)
        {
            int stride = Marshal.SizeOf<InstanceData>();
            int size = instances.Count * stride;
            IntPtr ptr = Marshal.AllocHGlobal(size);

            for (int i = 0; i < instances.Count; ++i)
            {
                Marshal.StructureToPtr(instances[i], ptr + i * stride, false);
            }

            UpdateBuffer(_device, _instanceBuffer, ptr, (ulong)size, (ulong)(offset * stride));
            Marshal.FreeHGlobal(ptr);
        }

        private static Vertex[] BuildIcosahedronVertices()
        {
            float phi = (1.0f + MathF.Sqrt(5.0f)) * 0.5f;
            var positions = new[]
            {
                new SysVector3(-1,  phi, 0),
                new SysVector3( 1,  phi, 0),
                new SysVector3(-1, -phi, 0),
                new SysVector3( 1, -phi, 0),
                new SysVector3(0, -1,  phi),
                new SysVector3(0,  1,  phi),
                new SysVector3(0, -1, -phi),
                new SysVector3(0,  1, -phi),
                new SysVector3( phi, 0, -1),
                new SysVector3( phi, 0,  1),
                new SysVector3(-phi, 0, -1),
                new SysVector3(-phi, 0,  1)
            };

            var vertices = new Vertex[positions.Length];
            for (int i = 0; i < positions.Length; ++i)
            {
                SysVector3 pos = SysVector3.Normalize(positions[i]);
                vertices[i] = new Vertex
                {
                    Position = pos,
                    Normal = pos
                };
            }

            return vertices;
        }

        private static ushort[] BuildIcosahedronIndices()
        {
            return new ushort[]
            {
                0, 11, 5,  0, 5,  1,  0, 1,  7,  0, 7,  10, 0, 10, 11,
                1, 5,  9,  5, 11, 4,  11, 10, 2, 10, 7,  6,  7, 1,  8,
                3, 9,  4,  3, 4,  2,  3, 2,  6,  3, 6,  8,  3, 8,  9,
                4, 9,  5,  2, 4,  11, 6, 2,  10, 8, 6,  7,  9, 8,  1
            };
        }

        private static InstanceData[] CreateInitialInstances()
        {
            var rng = new Random(1234);
            var data = new InstanceData[InstanceCount];
            for (int i = 0; i < InstanceCount; ++i)
            {
                float angle = (float)i / InstanceCount * MathF.PI * 2.0f;
                float radius = 25.0f + (float)rng.NextDouble() * 25.0f;
                float height = ((float)rng.NextDouble() - 0.5f) * 10.0f;
                float scale = 0.5f + (float)rng.NextDouble();

                Matrix4x4 model =
                    Matrix4x4.CreateScale(scale) *
                    Matrix4x4.CreateFromAxisAngle(SysVector3.Normalize(new SysVector3(
                        (float)rng.NextDouble(),
                        (float)rng.NextDouble(),
                        (float)rng.NextDouble())), (float)rng.NextDouble() * MathF.PI * 2.0f) *
                    Matrix4x4.CreateTranslation(
                        MathF.Cos(angle) * radius,
                        height,
                        MathF.Sin(angle) * radius);

                data[i] = new InstanceData
                {
                    Model = Matrix4x4.Transpose(model),
                    Color = new SysVector4(
                        0.5f + 0.5f * (float)rng.NextDouble(),
                        0.4f + 0.4f * (float)rng.NextDouble(),
                        0.3f + 0.4f * (float)rng.NextDouble(),
                        1.0f)
                };
            }

            return data;
        }

        private static VEBufferAddress CreateVertexBufferFromArray(VEDevice device, Vertex[] vertices, string name)
        {
            GCHandle handle = GCHandle.Alloc(vertices, GCHandleType.Pinned);
            try
            {
                return CreateVertexBuffer(device, handle.AddrOfPinnedObject(), (ulong)(vertices.Length * Marshal.SizeOf<Vertex>()), name);
            }
            finally
            {
                handle.Free();
            }
        }

        private static VEBufferAddress CreateIndexBufferFromArray(VEDevice device, ushort[] indices, string name)
        {
            GCHandle handle = GCHandle.Alloc(indices, GCHandleType.Pinned);
            try
            {
                return CreateIndexBuffer(device, handle.AddrOfPinnedObject(), (ulong)(indices.Length * sizeof(ushort)), name);
            }
            finally
            {
                handle.Free();
            }
        }

        public static void Main(string[] args)
        {
            using var window = new MultithreadedAsteroids(args);
            window.Run();
        }
    }
}

