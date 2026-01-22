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
        private const int ChunkSize = 512; // Match C++ chunking to reduce overhead
        private const float CameraRadius = 55.0f;
        private const int MaxWorkerThreads = 8;
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
            public Matrix4x4 Model;   // Stored row-major; uploaded as-is
            public SysVector4 Color;
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
                // Row-major: pre-multiply to match column-major R * M on GPU.
                instance.Model = rotation * instance.Model;

                    float drift = (float)(_rng.NextDouble() - 0.5) * 0.15f;
                    // Drift a bit along the local X/Z axes.
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
        private VERenderTarget _renderTarget;
        private VETextureIndex _colorTexture;
        private VETextureIndex _depthTexture;

        private VEShader _vertexShader;
        private VEShader _fragmentShader;
        private VEGraphicsPipeline _pipeline;

        private VEBufferAddress _vertexBuffer;
        private VEBufferAddress _indexBuffer;
        private VEBufferAddress _instanceBuffer;
        private VEBufferAddress _cameraBuffer;

        private readonly List<Chunk> _chunks = new();
        private InstanceData[] _instances = Array.Empty<InstanceData>();

        private readonly List<ChunkRecord> _recorded = new();
        private readonly List<VECommandBuffer> _secondaryScratch = new();

        private SimulationSystem _simulation;
        private ChannelReader<ChunkUpdate> _updateReader;
        private readonly int _workerCount;

        private readonly Stopwatch _frameTimer = Stopwatch.StartNew();
        private ulong _frameIndex;
        private double _elapsedTime;  // Total elapsed time for framerate-independent animation
        private bool _multithreaded = true;
        
        // VulkEase frame timing
        private VEFrameTimingInfo _veTiming;

        private uint _indexCount;

        // Swapchain format for inheritance info
        private VkFormat _swapchainFormat = VkFormat.VK_FORMAT_B8G8R8A8_SRGB;

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
            int hwThreads = Environment.ProcessorCount;
            _workerCount = Math.Min(MaxWorkerThreads, Math.Max(1, hwThreads > 4 ? hwThreads - 2 : 2));

            foreach (string arg in args)
            {
                if (string.Equals(arg, "--no-mt", StringComparison.OrdinalIgnoreCase))
                {
                    _multithreaded = false;
                }
            }

            Console.WriteLine($"Worker threads: {_workerCount}");
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

            // Print VulkEase timing summary
            Console.WriteLine();
            Console.WriteLine("=== VulkEase Timing Summary ===");
            Console.WriteLine($"Total frames: {_veTiming.frameNumber}");
            Console.WriteLine($"Average frame time: {_veTiming.avgFrameTimeMs:F2} ms ({_veTiming.avgFps:F1} FPS)");
            Console.WriteLine($"Min frame time: {_veTiming.minFrameTimeMs:F2} ms");
            Console.WriteLine($"Max frame time: {_veTiming.maxFrameTimeMs:F2} ms");
            Console.WriteLine();

            if (_device.native != IntPtr.Zero)
            {
                DeviceWaitIdle(_device);
            }

            if (_cameraBuffer.native != 0)
                DestroyBuffer(_device, _cameraBuffer);
            if (_instanceBuffer.native != 0)
                DestroyBuffer(_device, _instanceBuffer);
            if (_indexBuffer.native != 0)
                DestroyBuffer(_device, _indexBuffer);
            if (_vertexBuffer.native != 0)
                DestroyBuffer(_device, _vertexBuffer);

            if (_pipeline.native != IntPtr.Zero)
                DestroyGraphicsPipeline(_pipeline);
            if (_fragmentShader.native != IntPtr.Zero)
                DestroyShader(_fragmentShader);
            if (_vertexShader.native != IntPtr.Zero)
                DestroyShader(_vertexShader);

            if (_colorTexture.native != VEConstants.VE_INVALID_TEXTURE_INDEX.native)
                DestroyTexture(_device, _colorTexture);
            if (_depthTexture.native != VEConstants.VE_INVALID_TEXTURE_INDEX.native)
                DestroyTexture(_device, _depthTexture);
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
                ResizeRenderTarget(_device, ref _renderTarget, (uint)e.Width, (uint)e.Height);
                _colorTexture = _renderTarget.ColorAttachments[0].texture;
                if (_renderTarget.DepthAttachment.HasValue)
                    _depthTexture = _renderTarget.DepthAttachment.Value.texture;
            }
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

            // Begin VulkEase frame timing
            BeginFrame(_device);

            double streamingMs = DrainSimulationUpdates();

            double recordMs = RecordChunks();
            double submitMs = SubmitFrame();

            // End VulkEase frame timing
            EndFrame(_device, out _veTiming);

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
                Console.Error.WriteLine("Failed to create context");
                return false;
            }

            // Create device (auto-select best GPU)
            _device = CreateDevice(_context);
            if (_device.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create device");
                return false;
            }

            Console.WriteLine($"Using device {GetDeviceName(_device)}");

            // Create swapchain
            var swapchainDesc = new VESwapchainDesc
            {
                Surface = GetSurfaceDesc(),
                Width = (uint)ClientSize.X,
                Height = (uint)ClientSize.Y,
                ColorFormat = VkFormat.VK_FORMAT_B8G8R8A8_SRGB,
                VSync = true
            };
            _swapchain = CreateSwapchain(_device, swapchainDesc);
            if (_swapchain.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create swapchain");
                return false;
            }

            // Create render target with depth buffer
            _renderTarget = CreateSimpleRenderTarget(_device,
                (uint)ClientSize.X, (uint)ClientSize.Y,
                VkFormat.VK_FORMAT_B8G8R8A8_SRGB,
                VkFormat.VK_FORMAT_D32_SFLOAT,
                new VEColor(0.01f, 0.01f, 0.015f, 1.0f), 1.0f,
                out _colorTexture, out _depthTexture);
            if (_colorTexture.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
            {
                Console.Error.WriteLine("Failed to create render target");
                return false;
            }

            return true;
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


        private bool LoadAssets()
        {
            _vertexShader = LoadShaderFromFile(_device, VertexShaderPath, VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "AsteroidVertex");
            if (_vertexShader.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to load vertex shader");
                return false;
            }

            _fragmentShader = LoadShaderFromFile(_device, FragmentShaderPath, VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "AsteroidFragment");
            if (_fragmentShader.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to load fragment shader");
                return false;
            }

            _pipeline = CreateOpaquePipeline(_device, _vertexShader, _fragmentShader, "AsteroidPipeline");
            if (_pipeline.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create graphics pipeline");
                return false;
            }

            var vertices = BuildIcosahedronVertices();
            var indices = BuildIcosahedronIndices();

            _vertexBuffer = CreateVertexBufferFromArray(_device, vertices, "AsteroidVertices");
            if (_vertexBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create vertex buffer");
                return false;
            }

            _indexBuffer = CreateIndexBufferFromArray(_device, indices, "AsteroidIndices");
            if (_indexBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create index buffer");
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
                Console.Error.WriteLine("Failed to create instance buffer");
                return false;
            }
            UploadInstances(_instances, 0);

            VEBufferDesc cameraDesc = new()
            {
                size = (ulong)(sizeof(float) * 20), // 16 for viewProj, 4 for cameraPos
                usage = VkBufferUsageFlags.VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                persistentlyMapped = false,
                debugName = "AsteroidCamera"
            };
            _cameraBuffer = CreateBuffer(_device, cameraDesc);
            if (_cameraBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create camera buffer");
                return false;
            }

            // Depth buffer is managed by the render target

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
                var results = new ChunkRecord[_chunks.Count];
                Parallel.For(0, _chunks.Count, new ParallelOptions { MaxDegreeOfParallelism = _workerCount }, i =>
                {
                    results[i] = RecordChunk(i);
                });
                _recorded.AddRange(results);
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

            uint width = _renderTarget.RenderAreaWidth;
            uint height = _renderTarget.RenderAreaHeight;

            var cmd = BeginCommandBuffer(_device);

            // Begin rendering to render target (transitions handled automatically)
            BeginRendering(cmd, _renderTarget);

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

            // Blit color texture to swapchain (acquires swapchain image internally)
            var blitResult = BlitTextureToSwapchain(cmd, _colorTexture, _swapchain, VkFilter.VK_FILTER_LINEAR);

            if (blitResult == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
            {
                var swapExtent = GetSwapchainSize(_swapchain);
                uint newWidth = swapExtent.width;
                uint newHeight = swapExtent.height;
                if (newWidth > 0 && newHeight > 0)
                {
                    ResizeSwapchain(_swapchain, newWidth, newHeight);
                    ResizeRenderTarget(_device, ref _renderTarget, newWidth, newHeight);
                    _colorTexture = _renderTarget.ColorAttachments[0].texture;
                    if (_renderTarget.DepthAttachment.HasValue)
                        _depthTexture = _renderTarget.DepthAttachment.Value.texture;
                }
                sw.Stop();
                return sw.Elapsed.TotalMilliseconds;
            }

            // Present (auto-releases command buffer)
            var present = PresentImage(_swapchain, cmd);
            if (present == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
            {
                var swapExtent = GetSwapchainSize(_swapchain);
                uint newWidth = swapExtent.width;
                uint newHeight = swapExtent.height;
                if (newWidth > 0 && newHeight > 0)
                {
                    ResizeSwapchain(_swapchain, newWidth, newHeight);
                    ResizeRenderTarget(_device, ref _renderTarget, newWidth, newHeight);
                    _colorTexture = _renderTarget.ColorAttachments[0].texture;
                    if (_renderTarget.DepthAttachment.HasValue)
                        _depthTexture = _renderTarget.DepthAttachment.Value.texture;
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

            float aspect = width / (float)height;
            var view = CreateLookAtRH(eye, target, up);
            var proj = CreateVulkanPerspective(MathF.PI / 3.0f, aspect, 0.1f, 500.0f);

            // Row-major: view * proj (like cube example uses model * view * proj)
            var viewProj = MultiplyMatrix(view, proj);

            // Pack viewProj (16 floats) + cameraPos (4 floats) into a contiguous buffer
            var packed = new float[20];
            Array.Copy(viewProj, 0, packed, 0, 16);
            packed[16] = eye.X;
            packed[17] = eye.Y;
            packed[18] = eye.Z;
            packed[19] = 1.0f;

            int byteSize = sizeof(float) * packed.Length;
            IntPtr ptr = Marshal.AllocHGlobal(byteSize);
            try
            {
                Marshal.Copy(packed, 0, ptr, packed.Length);
                UpdateBuffer(_device, _cameraBuffer, ptr, (ulong)byteSize, 0);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }
        }

        private static float[] CreateIdentityMatrix()
        {
            var m = new float[16];
            m[0] = m[5] = m[10] = m[15] = 1.0f;
            return m;
        }

        private static float[] CreateVulkanPerspective(float fov, float aspect, float near, float far)
        {
            var m = new float[16];
            float t = MathF.Tan(fov * 0.5f);
            m[0] = 1.0f / (aspect * t);
            m[5] = -1.0f / t;                  // Flip Y
            m[10] = far / (near - far);        // Z in [0,1]
            m[11] = -1.0f;
            m[14] = -(far * near) / (far - near);
            m[15] = 0.0f;
            return m;
        }

        private static float[] CreateLookAtRH(SysVector3 eye, SysVector3 target, SysVector3 up)
        {
            SysVector3 f = SysVector3.Normalize(target - eye);
            SysVector3 s = SysVector3.Normalize(SysVector3.Cross(f, up));
            SysVector3 u = SysVector3.Cross(s, f);

            // Row-major matrix equivalent to C++ computeLookAt column-major
            var m = new float[16];
            m[0] = s.X;  m[1] = u.X;  m[2] = -f.X; m[3] = 0.0f;
            m[4] = s.Y;  m[5] = u.Y;  m[6] = -f.Y; m[7] = 0.0f;
            m[8] = s.Z;  m[9] = u.Z;  m[10] = -f.Z; m[11] = 0.0f;
            m[12] = -SysVector3.Dot(s, eye);
            m[13] = -SysVector3.Dot(u, eye);
            m[14] = SysVector3.Dot(f, eye);
            m[15] = 1.0f;
            return m;
        }

        private static float[] MultiplyMatrix(float[] a, float[] b)
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

        private static Matrix4x4 MatrixFromArray(float[] m)
        {
            // Assumes row-major ordering matching float[16]
            return new Matrix4x4(
                m[0], m[1], m[2], m[3],
                m[4], m[5], m[6], m[7],
                m[8], m[9], m[10], m[11],
                m[12], m[13], m[14], m[15]);
        }

        private ChunkRecord RecordChunk(int chunkIndex)
        {
            var chunk = _chunks[chunkIndex];

            var desc = new VESecondaryCommandBufferDesc
            {
                usageFlags = (uint)(VkCommandBufferUsageFlags.VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                                    VkCommandBufferUsageFlags.VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT),
                colorAttachmentCount = 1,
                colorAttachmentFormats = new VkFormat[8],
                depthAttachmentFormat = VkFormat.VK_FORMAT_D32_SFLOAT,
                stencilAttachmentFormat = VkFormat.VK_FORMAT_UNDEFINED,
                rasterizationSamples = VkSampleCountFlags.VK_SAMPLE_COUNT_1_BIT,
                viewMask = 0,
                occlusionQueryEnable = false,
                occlusionQueryFlags = 0,
                beginRecording = true
            };
            desc.colorAttachmentFormats[0] = _swapchainFormat;

            var cmd = BeginSecondaryCommandBuffer(_device, desc);

            // Apply graphics state (sets required dynamic states for shader objects)
            ApplyGraphicsState(cmd, _pipeline, null, null, null);

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
                // Upload row-major data directly (matches cube example behavior)
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

                var rotation = Matrix4x4.CreateFromAxisAngle(SysVector3.Normalize(new SysVector3(
                    (float)rng.NextDouble(),
                    (float)rng.NextDouble(),
                    (float)rng.NextDouble())), (float)rng.NextDouble() * MathF.PI * 2.0f);

                // Row-major: model = rotation * scale * translation (matches column-major T * R * S order)
                Matrix4x4 model =
                    rotation *
                    Matrix4x4.CreateScale(scale) *
                    Matrix4x4.CreateTranslation(
                        MathF.Cos(angle) * radius,
                        height,
                        MathF.Sin(angle) * radius);

                data[i] = new InstanceData
                {
                    Model = model,
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

