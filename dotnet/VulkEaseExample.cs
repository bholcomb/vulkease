/**
 * VulkEase.NET - Usage Example
 * Demonstrates how to use the C# bindings for basic rendering
 */

using System;
using System.Runtime.InteropServices;
using VulkEase;

namespace VulkEaseExample
{
    // Simple vertex structure
    [StructLayout(LayoutKind.Sequential)]
    public struct Vertex
    {
        public float X, Y, Z;    // Position
        public float R, G, B, A; // Color
        public float U, V;       // Texture coordinates

        public Vertex(float x, float y, float z, float r, float g, float b, float a, float u, float v)
        {
            X = x; Y = y; Z = z;
            R = r; G = g; B = b; A = a;
            U = u; V = v;
        }
    }

    class Program
    {
        static void Main(string[] args)
        {
            try
            {
                // Create context and device
                using var context = new Context("VulkEase.NET Example");
                using var device = context.CreateDevice();

                Console.WriteLine($"VulkEase Version: {VulkEaseHelper.FormatVersion(context.GetVersion())}");
                Console.WriteLine($"Device: {device.DeviceName}");
                Console.WriteLine($"Driver: {device.DriverVersion}");
                Console.WriteLine($"Vulkan: {VulkEaseHelper.FormatVersion(device.VulkanVersion)}");

                // Check feature support
                Console.WriteLine($"Buffer Device Address: {device.SupportsBufferDeviceAddress}");
                Console.WriteLine($"Descriptor Indexing: {device.SupportsDescriptorIndexing}");
                Console.WriteLine($"Shader Objects: {device.SupportsShaderObjects}");

                // Create triangle vertices
                var vertices = new[]
                {
                    new Vertex(-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f), // Bottom left - red
                    new Vertex( 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f), // Bottom right - green  
                    new Vertex( 0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 1.0f)  // Top - blue
                };

                // Create vertex buffer
                var vertexBuffer = device.CreateVertexBuffer(vertices, "TriangleVertices");
                Console.WriteLine($"Created vertex buffer at address: 0x{vertexBuffer.Value:X}");

                // Create uniform buffer for matrices
                var uniformBuffer = device.CreateUniformBuffer(64, true, "Matrices"); // 4x4 matrix
                Console.WriteLine($"Created uniform buffer at address: 0x{uniformBuffer.Value:X}");

                // Load and create texture
                var texture = device.LoadTexture("assets/texture.jpg", VETextureUsage.Sampled, true);
                Console.WriteLine($"Loaded texture with index: {texture.Value}");

                // Create sampler
                var sampler = device.CreateLinearSampler();
                Console.WriteLine($"Created sampler with index: {sampler.Value}");

                // Load shaders
                using var vertexShader = device.LoadShader("shaders/triangle.vert.spv", VEShaderStage.Vertex, "main", "VertexShader");
                using var fragmentShader = device.LoadShader("shaders/triangle.frag.spv", VEShaderStage.Fragment, "main", "FragmentShader");

                // Create render configuration
                using var renderConfig = device.CreateOpaqueRenderConfig("TriangleRenderConfig");

                // Create swapchain (assuming you have a window handle)
                // var windowHandle = GetWindowHandle(); // Platform-specific
                // using var swapchain = device.CreateSwapchain(windowHandle, 1280, 720);

                // Example render loop structure
                // RenderLoop(device, swapchain, renderConfig, vertexShader, fragmentShader, 
                //           vertexBuffer, uniformBuffer, texture, sampler);

                // Print resource statistics
                var memStats = device.GetMemoryStats();
                Console.WriteLine($"Total allocated memory: {memStats.TotalAllocated / 1024 / 1024} MB");
                Console.WriteLine($"Active buffers: {memStats.BufferCount}");
                Console.WriteLine($"Active textures: {memStats.TextureCount}");
                Console.WriteLine($"Active samplers: {memStats.SamplerCount}");

                // Cleanup
                device.DestroyBuffer(vertexBuffer);
                device.DestroyBuffer(uniformBuffer);
                device.DestroyTexture(texture);
                device.DestroySampler(sampler);

                Console.WriteLine("VulkEase.NET example completed successfully!");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
            }
        }

        static void RenderLoop(Device device, Swapchain swapchain, RenderConfig renderConfig,
                              Shader vertexShader, Shader fragmentShader,
                              VEBufferAddress vertexBuffer, VEBufferAddress uniformBuffer,
                              VETextureIndex texture, VESamplerIndex sampler)
        {
            // Main render loop
            bool running = true;
            while (running)
            {
                // Process input/events
                // running = ProcessEvents();

                try
                {
                    // Acquire next swapchain image
                    var backbuffer = swapchain.AcquireNextImage();
                    if (!backbuffer.IsValid)
                        continue; // Swapchain needs recreation

                    // Begin command buffer
                    var cmd = device.BeginCommandBuffer();

                    // Set up rendering info
                    var colorAttachment = VulkEaseHelper.ColorAttachment(backbuffer, VELoadOp.Clear, new VEColor(0.1f, 0.1f, 0.1f, 1.0f));
                    var renderingInfo = new VERenderingInfo
                    {
                        RenderAreaX = 0,
                        RenderAreaY = 0,
                        RenderAreaWidth = 1280,
                        RenderAreaHeight = 720,
                        ColorAttachmentCount = 1,
                        ColorAttachments = Marshal.AllocHGlobal(Marshal.SizeOf<VERenderingAttachment>())
                    };

                    Marshal.StructureToPtr(colorAttachment, renderingInfo.ColorAttachments, false);

                    // Begin rendering with debug scope
                    using (new DebugScope(cmd, "Frame", VEColor.Blue))
                    {
                        using (new RenderScope(cmd, renderingInfo))
                        {
                            // Set viewport
                            cmd.SetViewport(0, 0, 1280, 720);

                            // Apply render configuration
                            cmd.ApplyRenderConfig(renderConfig);

                            // Bind shaders
                            cmd.BindShaders(new[] { vertexShader, fragmentShader });

                            // Set push constants
                            var pushConstants = new VEGraphicsPushConstants
                            {
                                VertexBuffer = vertexBuffer,
                                UniformBuffer = uniformBuffer,
                                DiffuseTexture = texture,
                                Sampler = sampler,
                                ObjectScale = 1.0f
                            };
                            cmd.PushConstants(pushConstants);

                            // Draw triangle
                            cmd.Draw(3, 1, 0, 0);
                        }
                    }

                    // Submit command buffer and present
                    cmd.Submit(false);
                    var result = swapchain.Present(cmd);

                    if (result == VEResult.ErrorSwapchainOutOfDate)
                    {
                        // Recreate swapchain
                        swapchain.Resize(1280, 720);
                    }

                    Marshal.FreeHGlobal(renderingInfo.ColorAttachments);

                    // Update frame statistics
                    device.UpdateFrameStats();
                }
                catch (VulkEaseException ex)
                {
                    Console.WriteLine($"Render error: {ex.Message}");
                    if (ex.ResultCode == VEResult.ErrorDeviceLost)
                        break;
                }
            }
        }

        // Platform-specific window handle acquisition would go here
        // static IntPtr GetWindowHandle() { ... }
    }

    // =============================================================================
    // Advanced Usage Examples
    // =============================================================================

    public static class AdvancedExamples
    {
        public static void ComputeShaderExample(Device device)
        {
            // Create compute buffers
            var inputBuffer = device.CreateStorageBuffer(1024 * sizeof(float), "ComputeInput");
            var outputBuffer = device.CreateStorageBuffer(1024 * sizeof(float), "ComputeOutput");

            // Load compute shader
            using var computeShader = device.LoadShader("shaders/process.comp.spv", VEShaderStage.Compute, "main");

            // Begin command buffer
            var cmd = device.BeginCommandBuffer();

            // Bind compute shader
            Native.veBindComputeShader(cmd.Handle, computeShader.Handle);

            // Set compute constants
            var computeConstants = new VEComputePushConstants
            {
                InputBuffer = inputBuffer,
                OutputBuffer = outputBuffer,
                ElementCount = 1024
            };
            cmd.PushConstants(computeConstants);

            // Dispatch compute work
            Native.veCalculateDispatchSize(1024, 64, out uint numGroups);
            cmd.Dispatch(numGroups, 1, 1);

            // Add compute barrier
            cmd.BarrierComputeToGraphics();

            // Submit
            cmd.Submit(true);

            // Cleanup
            device.DestroyBuffer(inputBuffer);
            device.DestroyBuffer(outputBuffer);
        }

        public static void MultiPassRenderingExample(Device device, Swapchain swapchain)
        {
            // Create depth buffer
            var depthTexture = device.CreateTexture2D(1280, 720, VEFormat.D32Sfloat, 
                VETextureUsage.DepthStencilAttachment, "DepthBuffer");

            // Create shadow map
            var shadowMap = device.CreateTexture2D(1024, 1024, VEFormat.D32Sfloat,
                VETextureUsage.DepthStencilAttachment | VETextureUsage.Sampled, "ShadowMap");

            // Create different render configurations
            using var shadowConfig = device.CreateShadowRenderConfig("ShadowPass");
            using var opaqueConfig = device.CreateOpaqueRenderConfig("OpaquePass");
            using var transparentConfig = device.CreateTransparentRenderConfig("TransparentPass");

            var cmd = device.BeginCommandBuffer();

            using (new DebugScope(cmd, "Shadow Pass", VEColor.Red))
            {
                // Shadow pass rendering
                var shadowAttachment = VulkEaseHelper.DepthAttachment(shadowMap);
                var shadowInfo = new VERenderingInfo
                {
                    RenderAreaWidth = 1024,
                    RenderAreaHeight = 1024,
                    DepthAttachment = Marshal.AllocHGlobal(Marshal.SizeOf<VERenderingAttachment>())
                };
                Marshal.StructureToPtr(shadowAttachment, shadowInfo.DepthAttachment, false);

                using (new RenderScope(cmd, shadowInfo))
                {
                    cmd.ApplyRenderConfig(shadowConfig);
                    // Render shadow casters...
                }

                Marshal.FreeHGlobal(shadowInfo.DepthAttachment);
            }

            using (new DebugScope(cmd, "Main Pass", VEColor.Green))
            {
                var backbuffer = swapchain.AcquireNextImage();
                var colorAttachment = VulkEaseHelper.ColorAttachment(backbuffer);
                var depthAttachment = VulkEaseHelper.DepthAttachment(depthTexture);

                var mainInfo = new VERenderingInfo
                {
                    RenderAreaWidth = 1280,
                    RenderAreaHeight = 720,
                    ColorAttachmentCount = 1,
                    ColorAttachments = Marshal.AllocHGlobal(Marshal.SizeOf<VERenderingAttachment>()),
                    DepthAttachment = Marshal.AllocHGlobal(Marshal.SizeOf<VERenderingAttachment>())
                };

                Marshal.StructureToPtr(colorAttachment, mainInfo.ColorAttachments, false);
                Marshal.StructureToPtr(depthAttachment, mainInfo.DepthAttachment, false);

                using (new RenderScope(cmd, mainInfo))
                {
                    // Opaque pass
                    cmd.ApplyRenderConfig(opaqueConfig);
                    // Render opaque objects...

                    // Transparent pass
                    cmd.ApplyRenderConfig(transparentConfig);
                    // Render transparent objects...
                }

                Marshal.FreeHGlobal(mainInfo.ColorAttachments);
                Marshal.FreeHGlobal(mainInfo.DepthAttachment);
            }

            cmd.Submit(false);
            swapchain.Present(cmd);

            // Cleanup
            device.DestroyTexture(depthTexture);
            device.DestroyTexture(shadowMap);
        }
    }
}