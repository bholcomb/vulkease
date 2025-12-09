using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using VulkEase;

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
            public Vector3 Position;
            public Vector3 Normal;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct InstanceData
        {
            public Matrix4x4 Model;
            public Vector4 Color;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct CameraData
        {
            public Matrix4x4 ViewProj;
            public Vector4 CameraPos;
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
                    var axis = Vector3.Normalize(new Vector3(
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

        private readonly List<Chunk> _chunks = new();
        private InstanceData[] _instances = Array.Empty<InstanceData>();

        private readonly List<ChunkRecord> _recorded = new();
        private readonly List<VECommandBuffer> _secondaryScratch = new();

        private SimulationSystem _simulation;
        private ChannelReader<ChunkUpdate> _updateReader;

        private readonly Stopwatch _frameTimer = Stopwatch.StartNew();
        private ulong _frameIndex;
        private bool _multithreaded = true;

        private uint _indexCount;

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
                VulkEase.DeviceWaitIdle(_device);
            }

            if (_cameraBuffer.native != 0)
                VulkEase.DestroyBuffer(_device, _cameraBuffer);
            if (_instanceBuffer.native != 0)
                VulkEase.DestroyBuffer(_device, _instanceBuffer);
            if (_indexBuffer.native != 0)
                VulkEase.DestroyBuffer(_device, _indexBuffer);
            if (_vertexBuffer.native != 0)
                VulkEase.DestroyBuffer(_device, _vertexBuffer);

            if (_renderConfig.native != IntPtr.Zero)
                VulkEase.DestroyRenderConfig(_renderConfig);
            if (_shaderConfig.native != IntPtr.Zero)
                VulkEase.DestroyShaderConfig(_shaderConfig);
            if (_fragmentShader.native != IntPtr.Zero)
                VulkEase.DestroyShader(_fragmentShader);
            if (_vertexShader.native != IntPtr.Zero)
                VulkEase.DestroyShader(_vertexShader);

            if (_swapchain.native != IntPtr.Zero)
                VulkEase.DestroySwapchain(_swapchain);
            if (_device.native != IntPtr.Zero)
                VulkEase.DestroyDevice(_device);
            if (_context.native != IntPtr.Zero)
                VulkEase.DestroyContext(_context);

            base.OnUnload();
        }

        protected override void OnResize(ResizeEventArgs e)
        {
            base.OnResize(e);

            if (_swapchain.native != IntPtr.Zero && e.Width > 0 && e.Height > 0)
            {
                VulkEase.ResizeSwapchain(_swapchain, (uint)e.Width, (uint)e.Height);
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

            double streamingMs = DrainSimulationUpdates();

            double recordMs = RecordChunks();
            double submitMs = SubmitFrame();

            double frameMs = _frameTimer.Elapsed.TotalMilliseconds;
            _frameTimer.Restart();

            double fps = frameMs > 0.0 ? 1000.0 / frameMs : 0.0;
            _metrics.Submit(new MetricsSample(_frameIndex, frameMs, recordMs, submitMs, streamingMs, fps,
                _multithreaded ? Environment.ProcessorCount : 1, _multithreaded), args.Time);
            Title = _metrics.TitleSummary();

            _frameIndex++;
        }

        private bool InitializeVulkEase()
        {
            _context = VulkEase.CreateContext("VulkEase Asteroids");
            if (_context.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create context: " + VulkEase.GetLastError());
                return false;
            }

            _device = VulkEase.CreateDevice(_context);
            if (_device.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create device: " + VulkEase.GetLastError());
                return false;
            }

            Console.WriteLine($"Using device {VulkEase.GetDeviceName(_device)}");

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

            _swapchain = VulkEase.CreateSwapchain(_device, windowHandle, (uint)ClientSize.X, (uint)ClientSize.Y,
                VkFormat.VK_FORMAT_B8G8R8A8_SRGB, true);
            if (_swapchain.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create swapchain: " + VulkEase.GetLastError());
                if (linuxHandleBlock != IntPtr.Zero)
                    Marshal.FreeHGlobal(linuxHandleBlock);
                return false;
            }

            if (linuxHandleBlock != IntPtr.Zero)
                Marshal.FreeHGlobal(linuxHandleBlock);

            return true;
        }

        private bool LoadAssets()
        {
            _vertexShader = VulkEase.LoadShaderFromFile(_device, VertexShaderPath, VkShaderStageFlags.VK_SHADER_STAGE_VERTEX_BIT, "main", "AsteroidVertex");
            if (_vertexShader.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to load vertex shader: " + VulkEase.GetLastError());
                return false;
            }

            _fragmentShader = VulkEase.LoadShaderFromFile(_device, FragmentShaderPath, VkShaderStageFlags.VK_SHADER_STAGE_FRAGMENT_BIT, "main", "AsteroidFragment");
            if (_fragmentShader.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to load fragment shader: " + VulkEase.GetLastError());
                return false;
            }

            VEShaderConfigDesc shaderDesc = new()
            {
                vertexShader = _vertexShader,
                fragmentShader = _fragmentShader,
                debugName = "AsteroidShader"
            };
            _shaderConfig = VulkEase.CreateShaderConfig(_device, shaderDesc);
            if (_shaderConfig.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create shader config: " + VulkEase.GetLastError());
                return false;
            }

            _renderConfig = VulkEase.CreateOpaqueRenderConfig(_device, "AsteroidRenderConfig");
            if (_renderConfig.native == IntPtr.Zero)
            {
                Console.Error.WriteLine("Failed to create render config: " + VulkEase.GetLastError());
                return false;
            }

            var vertices = BuildIcosahedronVertices();
            var indices = BuildIcosahedronIndices();

            _vertexBuffer = CreateVertexBuffer(_device, vertices, "AsteroidVertices");
            if (_vertexBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create vertex buffer: " + VulkEase.GetLastError());
                return false;
            }

            _indexBuffer = CreateIndexBuffer(_device, indices, "AsteroidIndices");
            if (_indexBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create index buffer: " + VulkEase.GetLastError());
                return false;
            }
            _indexCount = (uint)indices.Length;

            _instances = CreateInitialInstances();

            VEBufferDesc instanceDesc = new()
            {
                size = (ulong)(_instances.Length * Marshal.SizeOf<InstanceData>()),
                usage = VkBufferUsageFlags.VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VkBufferUsageFlags.VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                persistentlyMapped = false,
                debugName = Marshal.StringToHGlobalAnsi("AsteroidInstances")
            };
            _instanceBuffer = VulkEase.CreateBuffer(_device, instanceDesc);
            Marshal.FreeHGlobal(instanceDesc.debugName);
            if (_instanceBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create instance buffer: " + VulkEase.GetLastError());
                return false;
            }
            UploadInstances(_instances, 0);

            VEBufferDesc cameraDesc = new()
            {
                size = (ulong)Marshal.SizeOf<CameraData>(),
                usage = VkBufferUsageFlags.VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                persistentlyMapped = false,
                debugName = Marshal.StringToHGlobalAnsi("AsteroidCamera")
            };
            _cameraBuffer = VulkEase.CreateBuffer(_device, cameraDesc);
            Marshal.FreeHGlobal(cameraDesc.debugName);
            if (_cameraBuffer.native == 0)
            {
                Console.Error.WriteLine("Failed to create camera buffer: " + VulkEase.GetLastError());
                return false;
            }

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

            var backbuffer = VulkEase.AcquireNextImage(_swapchain);
            if (backbuffer.native == VEConstants.VE_INVALID_TEXTURE_INDEX.native)
            {
                sw.Stop();
                return sw.Elapsed.TotalMilliseconds;
            }

            var cmd = VulkEase.BeginCommandBuffer(_device);

            VulkEase.TransitionTextureForColorAttachment(cmd, backbuffer);

            VulkEase.GetSwapchainSize(_swapchain, out uint width, out uint height);

            var colorAttachment = new VERenderingAttachment
            {
                texture = backbuffer,
                loadOp = VkAttachmentLoadOp.VK_ATTACHMENT_LOAD_OP_CLEAR,
                storeOp = VkAttachmentStoreOp.VK_ATTACHMENT_STORE_OP_STORE,
                clearValue = new VEColor(0.01f, 0.01f, 0.015f, 1.0f),
                resolveTexture = VEConstants.VE_INVALID_TEXTURE_INDEX
            };

            var renderingInfo = new VERenderingInfo
            {
                RenderAreaX = 0,
                RenderAreaY = 0,
                RenderAreaWidth = width,
                RenderAreaHeight = height
            };
            renderingInfo.ColorAttachments.Add(colorAttachment);

            VulkEase.BeginRendering(cmd, renderingInfo);

            UpdateCameraBuffer(width, height);

            if (_recorded.Count > 0)
            {
                _secondaryScratch.Clear();
                foreach (var entry in _recorded)
                {
                    _secondaryScratch.Add(entry.CommandBuffer);
                }
                VulkEase.ExecuteSecondaryCommandBuffers(cmd, _secondaryScratch);
            }

            VulkEase.EndRendering(cmd);

            VulkEase.TransitionTextureForPresent(cmd, backbuffer);

            var present = VulkEase.PresentImage(_swapchain, cmd);
            if (present == VEResult.VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
            {
                VulkEase.GetSwapchainSize(_swapchain, out uint newWidth, out uint newHeight);
                if (newWidth > 0 && newHeight > 0)
                    VulkEase.ResizeSwapchain(_swapchain, newWidth, newHeight);
            }

            sw.Stop();
            return sw.Elapsed.TotalMilliseconds;
        }

        private void UpdateCameraBuffer(uint width, uint height)
        {
            float time = (float)_frameIndex * 0.016f;
            Vector3 eye = new(
                MathF.Cos(time * 0.25f) * CameraRadius,
                15.0f,
                MathF.Sin(time * 0.25f) * CameraRadius);
            Vector3 target = Vector3.Zero;
            Vector3 up = Vector3.UnitY;

            Matrix4x4 view = Matrix4x4.CreateLookAt(
                new System.Numerics.Vector3(eye.X, eye.Y, eye.Z),
                new System.Numerics.Vector3(target.X, target.Y, target.Z),
                new System.Numerics.Vector3(up.X, up.Y, up.Z));

            float aspect = width / (float)height;
            Matrix4x4 proj = Matrix4x4.CreatePerspectiveFieldOfView(MathF.PI / 3.0f, aspect, 0.1f, 500.0f);
            proj.M22 *= -1.0f; // Vulkan clip space Y flipped

            Matrix4x4 viewProj = view * proj;

            CameraData data = new()
            {
                ViewProj = Matrix4x4.Transpose(viewProj),
                CameraPos = new Vector4(eye.X, eye.Y, eye.Z, 1.0f)
            };

            IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf<CameraData>());
            Marshal.StructureToPtr(data, ptr, false);
            VulkEase.UpdateBuffer(_device, _cameraBuffer, ptr, (ulong)Marshal.SizeOf<CameraData>(), 0);
            Marshal.FreeHGlobal(ptr);
        }

        private ChunkRecord RecordChunk(int chunkIndex)
        {
            var chunk = _chunks[chunkIndex];
            var cmd = VulkEase.BeginSecondaryCommandBuffer(_device);

            VulkEase.BindShaderConfig(cmd, _shaderConfig);
            VulkEase.ApplyRenderConfig(cmd, _renderConfig);
            VulkEase.SetViewport(cmd, 0.0f, 0.0f, ClientSize.X, ClientSize.Y, 0.0f, 1.0f);
            VulkEase.SetScissor(cmd, 0, 0, (uint)ClientSize.X, (uint)ClientSize.Y);

            var push = new VEGraphicsPushConstants
            {
                vertexBuffer = _vertexBuffer.native,
                indexBuffer = _indexBuffer.native,
                uniformBuffers = new ulong[4],
                textures = new uint[8],
                samplers = new uint[8],
                objectScale = 1.0f,
                activeUniformCount = 2,
                activeTextureCount = 0,
                activeSamplerCount = 0
            };
            push.uniformBuffers[0] = _cameraBuffer.native;
            push.uniformBuffers[1] = _instanceBuffer.native;

            IntPtr pcPtr = Marshal.AllocHGlobal(Marshal.SizeOf<VEGraphicsPushConstants>());
            Marshal.StructureToPtr(push, pcPtr, false);
            VulkEase.PushConstants(cmd, pcPtr, (nuint)Marshal.SizeOf<VEGraphicsPushConstants>(), 0);
            Marshal.FreeHGlobal(pcPtr);

            VulkEase.BindIndexBuffer(cmd, _indexBuffer, 0, VkIndexType.VK_INDEX_TYPE_UINT16);
            VulkEase.DrawIndexed(cmd, _indexCount, (uint)chunk.Count, 0, 0, (uint)chunk.FirstInstance);

            VulkEase.EndCommandBuffer(cmd);

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

            VulkEase.UpdateBuffer(_device, _instanceBuffer, ptr, (ulong)size, (ulong)(offset * stride));
            Marshal.FreeHGlobal(ptr);
        }

        private static Vertex[] BuildIcosahedronVertices()
        {
            float phi = (1.0f + MathF.Sqrt(5.0f)) * 0.5f;
            var positions = new[]
            {
                new Vector3(-1,  phi, 0),
                new Vector3( 1,  phi, 0),
                new Vector3(-1, -phi, 0),
                new Vector3( 1, -phi, 0),
                new Vector3(0, -1,  phi),
                new Vector3(0,  1,  phi),
                new Vector3(0, -1, -phi),
                new Vector3(0,  1, -phi),
                new Vector3( phi, 0, -1),
                new Vector3( phi, 0,  1),
                new Vector3(-phi, 0, -1),
                new Vector3(-phi, 0,  1)
            };

            var vertices = new Vertex[positions.Length];
            for (int i = 0; i < positions.Length; ++i)
            {
                Vector3 pos = Vector3.Normalize(positions[i]);
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
                    Matrix4x4.CreateFromAxisAngle(Vector3.Normalize(new Vector3(
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
                    Color = new Vector4(
                        0.5f + 0.5f * (float)rng.NextDouble(),
                        0.4f + 0.4f * (float)rng.NextDouble(),
                        0.3f + 0.4f * (float)rng.NextDouble(),
                        1.0f)
                };
            }

            return data;
        }

        private static unsafe VEBufferAddress CreateVertexBuffer(VEDevice device, Vertex[] vertices, string name)
        {
            GCHandle handle = GCHandle.Alloc(vertices, GCHandleType.Pinned);
            IntPtr namePtr = Marshal.StringToHGlobalAnsi(name);
            try
            {
                return VulkEase.CreateVertexBuffer(device, handle.AddrOfPinnedObject(), (ulong)(vertices.Length * Marshal.SizeOf<Vertex>()), namePtr);
            }
            finally
            {
                handle.Free();
                Marshal.FreeHGlobal(namePtr);
            }
        }

        private static unsafe VEBufferAddress CreateIndexBuffer(VEDevice device, ushort[] indices, string name)
        {
            GCHandle handle = GCHandle.Alloc(indices, GCHandleType.Pinned);
            IntPtr namePtr = Marshal.StringToHGlobalAnsi(name);
            try
            {
                return VulkEase.CreateIndexBuffer(device, handle.AddrOfPinnedObject(), (ulong)(indices.Length * sizeof(ushort)), namePtr);
            }
            finally
            {
                handle.Free();
                Marshal.FreeHGlobal(namePtr);
            }
        }

        public static void Main(string[] args)
        {
            using var window = new MultithreadedAsteroids(args);
            window.Run();
        }
    }
}

