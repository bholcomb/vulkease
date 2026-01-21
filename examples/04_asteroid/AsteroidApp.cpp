#include "AsteroidApp.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace
{
constexpr uint32_t kInitialWidth = 1280;
constexpr uint32_t kInitialHeight = 720;
constexpr uint32_t kAsteroidCount = 4096;
constexpr uint32_t kChunkSize = 512;  // Larger chunks = less overhead
constexpr uint32_t kMaxWorkerThreads = 8;  // Cap workers to avoid contention
constexpr float kCameraRadius = 55.0f;
constexpr float kPi = 3.14159265359f;

constexpr const char *kVertexShaderPath = "examples/shaders/asteroid.vert.spv";
constexpr const char *kFragmentShaderPath = "examples/shaders/asteroid.frag.spv";

void fatal(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   std::vfprintf(stderr, fmt, args);
   std::fputc('\n', stderr);
   va_end(args);
   std::fflush(stderr);
   std::exit(EXIT_FAILURE);
}

void glfwErrorCallback(int code, const char *description)
{
   std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

struct Vertex
{
   float position[3];
   float normal[3];
};

std::vector<Vertex> makeIcosahedronVertices()
{
   const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
   const float invLen = 1.0f / std::sqrt(1.0f + phi * phi);

   std::vector<Vertex> vertices = {
       {{-1, phi, 0}, {-1, phi, 0}},
       {{1, phi, 0}, {1, phi, 0}},
       {{-1, -phi, 0}, {-1, -phi, 0}},
       {{1, -phi, 0}, {1, -phi, 0}},
       {{0, -1, phi}, {0, -1, phi}},
       {{0, 1, phi}, {0, 1, phi}},
       {{0, -1, -phi}, {0, -1, -phi}},
       {{0, 1, -phi}, {0, 1, -phi}},
       {{phi, 0, -1}, {phi, 0, -1}},
       {{phi, 0, 1}, {phi, 0, 1}},
       {{-phi, 0, -1}, {-phi, 0, -1}},
       {{-phi, 0, 1}, {-phi, 0, 1}},
   };

   for (auto &v : vertices)
   {
      for (int i = 0; i < 3; ++i)
      {
         v.position[i] *= invLen;
         v.normal[i] = v.position[i];
      }
   }

   return vertices;
}

std::vector<uint16_t> makeIcosahedronIndices()
{
   return {0, 11, 5,  0, 5,  1,  0, 1,  7,  0, 7,  10, 0, 10, 11, 1, 5,  9,  5, 11, 4,
           11, 10, 2, 10, 7,  6,  7, 1,  8,  3, 9,  4, 3, 4,  2,  3, 2,  6,  3, 6,  8,
           3, 8, 9,  4, 9,  5,  2, 4,  11, 6, 2,  10, 8, 6,  7,  9, 8,  1};
}

void computeLookAt(float out[16], const std::array<float, 3> &eye, const std::array<float, 3> &target,
                   const std::array<float, 3> &up)
{
   std::array<float, 3> f{target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
   float fLen = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
   for (auto &component : f)
   {
      component /= fLen;
   }

   std::array<float, 3> s{f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]};
   float sLen = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
   for (auto &component : s)
   {
      component /= sLen;
   }

  std::array<float, 3> u{s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};

   out[0] = s[0];
   out[1] = u[0];
   out[2] = -f[0];
   out[3] = 0.0f;

   out[4] = s[1];
   out[5] = u[1];
   out[6] = -f[1];
   out[7] = 0.0f;

   out[8] = s[2];
   out[9] = u[2];
   out[10] = -f[2];
   out[11] = 0.0f;

   out[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
   out[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
   out[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
   out[15] = 1.0f;
}

void computePerspective(float out[16], float fovRadians, float aspect, float nearPlane, float farPlane)
{
   // Column-major perspective matrix for Vulkan (Y-inverted, Z range [0,1])
   std::fill(out, out + 16, 0.0f);
   const float f = 1.0f / std::tan(fovRadians * 0.5f);
   out[0] = f / aspect;              // m[0][0]
   out[5] = -f;                      // m[1][1] - Vulkan Y flip
   out[10] = farPlane / (nearPlane - farPlane);  // m[2][2]
   out[11] = -1.0f;                  // m[2][3] - perspective divide
   out[14] = (nearPlane * farPlane) / (nearPlane - farPlane);  // m[3][2] - FIXED: was wrong sign
}

void multiplyMatrix(const float a[16], const float b[16], float out[16])
{
   // Column-major matrix multiplication: result = a * b
   // For column-major: out[col*4 + row] = sum over k of a[k*4 + row] * b[col*4 + k]
   float temp[16];
   for (int col = 0; col < 4; ++col)
   {
      for (int row = 0; row < 4; ++row)
      {
         float sum = 0.0f;
         for (int k = 0; k < 4; ++k)
         {
            sum += a[k * 4 + row] * b[col * 4 + k];
         }
         temp[col * 4 + row] = sum;
      }
   }
   std::memcpy(out, temp, sizeof(temp));
}

} // namespace

AsteroidApp::AsteroidApp()
    : metricsCollector_(240),
      metricsReporter_(metricsCollector_),
      csvFile_(nullptr, &std::fclose)
{
   metricsReporter_.addSink({"console", 2.0, [this](const MetricsSummary &summary) { logToConsole(summary); }});
   metricsReporter_.addSink({"title", 0.25, [this](const MetricsSummary &summary) { updateWindowTitle(summary); }});
}

AsteroidApp::~AsteroidApp()
{
   simulation_.stop();
   workerPool_.reset();
   destroyAssets();
   shutdownVulkEase();
   destroyWindow();
}

bool AsteroidApp::run(int argc, char **argv)
{
   if (!parseCommandLine(argc, argv, options_))
   {
      return false;
   }

   if (!initWindow())
   {
      fatal("Failed to init window");
   }

   if (!initVulkEase())
   {
      fatal("Failed to init VulkEase");
   }

   if (!createAssets())
   {
      fatal("Failed to create assets");
   }

   simulation_.initialize(instanceCount_, kChunkSize);
   simulation_.copyState(cpuInstances_);
   veUpdateBuffer(device_, instanceBuffer_, cpuInstances_.data(),
                  static_cast<uint64_t>(cpuInstances_.size() * sizeof(AsteroidInstance)), 0);
   simulation_.start();

   const auto hwThreads = std::thread::hardware_concurrency();
   const auto workerCount = std::min<std::size_t>(kMaxWorkerThreads,
                                                   std::max<std::size_t>(1, hwThreads > 4 ? hwThreads - 2 : 2));
   workerPool_ = std::make_unique<ThreadPool>(workerCount);
   multithreaded_ = options_.startMultithreaded;

   if (options_.enableCsv)
   {
      csvFile_.reset(std::fopen(options_.csvPath.c_str(), "w"));
      if (csvFile_)
      {
         std::fprintf(csvFile_.get(),
                      "frame,frame_ms,record_ms,submit_ms,stream_ms,fps,workers,chunks,instances,"
                      "multithreaded\n");
      }
      metricsReporter_.addSink({"csv", 0.0, [this](const MetricsSummary &summary) { appendCsv(summary); }});
   }

   mainLoop();

   return true;
}

bool AsteroidApp::parseCommandLine(int argc, char **argv, CommandLineOptions &outOptions)
{
   for (int i = 1; i < argc; ++i)
   {
      std::string arg(argv[i]);
      if (arg == "--no-mt")
      {
         outOptions.startMultithreaded = false;
      }
      else if (arg == "--metrics-csv")
      {
         outOptions.enableCsv = true;
         if (i + 1 < argc)
         {
            outOptions.csvPath = argv[++i];
         }
      }
      else if (arg == "--help" || arg == "-h")
      {
         std::cout << "VulkEase Asteroid Field Demo\n"
                   << "Options:\n"
                   << "  --no-mt              Start with multithreading disabled\n"
                   << "  --metrics-csv [path] Write metrics to CSV file (default asteroid_metrics.csv)\n"
                   << "  -h, --help           Show this help\n";
         return false;
      }
   }
   return true;
}

bool AsteroidApp::initWindow()
{
   glfwSetErrorCallback(glfwErrorCallback);

   if (!glfwInit())
   {
      std::fprintf(stderr, "Failed to initialise GLFW\n");
      return false;
   }

   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
   glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

   window_ = glfwCreateWindow(static_cast<int>(kInitialWidth), static_cast<int>(kInitialHeight),
                              "VulkEase - Multithreaded Asteroids", nullptr, nullptr);
   if (!window_)
   {
      std::fprintf(stderr, "Failed to create GLFW window\n");
      glfwTerminate();
      return false;
   }

   return true;
}

bool AsteroidApp::initVulkEase()
{

#ifdef DEBUG
   bool vsync = true;
#else
   bool vsync = false;
#endif

   if (veCreateContext("VulkEase Asteroid Demo", nullptr, 0, &context_) != VE_SUCCESS || !context_)
   {
      std::fprintf(stderr, "Failed to create VulkEase context\n");
      return false;
   }

   // Create device (VK_NULL_HANDLE = auto-select best GPU)
   if (veCreateDevice(context_, VK_NULL_HANDLE, nullptr, 0, &device_) != VE_SUCCESS || !device_)
   {
      std::fprintf(stderr, "Failed to create VulkEase device\n");
      return false;
   }

   uint32_t width = kInitialWidth;
   uint32_t height = kInitialHeight;
   glfwGetFramebufferSize(window_, reinterpret_cast<int *>(&width), reinterpret_cast<int *>(&height));

   // Set up swapchain descriptor with platform-specific surface
   VESwapchainDesc swapchainDesc{};
   swapchainDesc.width = width;
   swapchainDesc.height = height;
   swapchainDesc.colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
   swapchainDesc.vsync = vsync;
   swapchainDesc.debugName = "AsteroidSwapchain";

#if defined(_WIN32)
   swapchainDesc.surface.type = VE_SURFACE_TYPE_WIN32;
   swapchainDesc.surface.win32.hwnd = glfwGetWin32Window(window_);
#elif defined(__linux__)
   swapchainDesc.surface.type = VE_SURFACE_TYPE_XLIB;
   swapchainDesc.surface.xlib.display = glfwGetX11Display();
   swapchainDesc.surface.xlib.window = glfwGetX11Window(window_);
#elif defined(__APPLE__)
   swapchainDesc.surface.type = VE_SURFACE_TYPE_COCOA;
   swapchainDesc.surface.cocoa.window = glfwGetCocoaWindow(window_);
#else
#error "Unsupported platform"
#endif

   if (veCreateSwapchain(device_, &swapchainDesc, &swapchain_) != VE_SUCCESS || !swapchain_)
   {
      std::fprintf(stderr, "Failed to create VulkEase swapchain\n");
      return false;
   }

   swapchainFormat_ = veGetSwapchainFormat(swapchain_);

   // Create render target for offscreen rendering (with depth buffer)
   int rtWidth, rtHeight;
   glfwGetFramebufferSize(window_, &rtWidth, &rtHeight);

   VERenderTargetDesc rtDesc{};
   rtDesc.width = static_cast<uint32_t>(rtWidth);
   rtDesc.height = static_cast<uint32_t>(rtHeight);
   rtDesc.colorFormat = swapchainFormat_;
   rtDesc.depthFormat = VK_FORMAT_D32_SFLOAT;
   rtDesc.sampleCount = 1;
   rtDesc.hasResolveTarget = false;
   rtDesc.debugName = "AsteroidRenderTarget";

   if (veCreateRenderTarget(device_, &rtDesc, &renderTarget_) != VE_SUCCESS || !renderTarget_)
   {
      std::fprintf(stderr, "Failed to create render target\n");
      return false;
   }

   // Build rendering info once (reused every frame)
   renderingInfo_ = veCreateRenderingInfo(static_cast<uint32_t>(rtWidth), static_cast<uint32_t>(rtHeight));
   VETextureIndex rtColor = veGetRenderTargetColorTexture(renderTarget_);
   (void)veRenderingAddColorAttachment(&renderingInfo_,
                                 rtColor,
                                  VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  VEColor{0.01f, 0.01f, 0.015f, 1.0f}); // Dark space background
   VETextureIndex rtDepth = veGetRenderTargetDepthTexture(renderTarget_);
   (void)veRenderingSetDepthAttachment(&renderingInfo_,
                                 rtDepth,
                                  VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f);

   return true;
}

void AsteroidApp::shutdownVulkEase()
{
   if (device_)
   {
      veDeviceWaitIdle(device_);
   }

   destroyAssets();

   if (renderTarget_)
   {
      (void)veDestroyRenderTarget(renderTarget_);
      renderTarget_ = nullptr;
   }

   if (swapchain_)
   {
      (void)veDestroySwapchain(swapchain_);
      swapchain_ = nullptr;
   }

   if (device_)
   {
      (void)veDestroyDevice(device_);
      device_ = nullptr;
   }

   if (context_)
   {
      (void)veDestroyContext(context_);
      context_ = nullptr;
   }
}

void AsteroidApp::destroyWindow()
{
   if (window_)
   {
      glfwDestroyWindow(window_);
      window_ = nullptr;
   }
   glfwTerminate();
}

bool AsteroidApp::createAssets()
{
   if (veLoadShaderFromFile(device_, kVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT, "main", "AsteroidVertexShader",
                            &vertexShader_) != VE_SUCCESS ||
       !vertexShader_)
   {
      std::fprintf(stderr, "Failed to load vertex shader\n");
      return false;
   }

   if (veLoadShaderFromFile(device_, kFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT, "main",
                            "AsteroidFragmentShader", &fragmentShader_) != VE_SUCCESS ||
       !fragmentShader_)
   {
      std::fprintf(stderr, "Failed to load fragment shader\n");
      return false;
   }

   // Create graphics pipeline with shaders
   VEGraphicsPipelineDesc pipelineDesc = veDefaultGraphicsPipelineDesc();
   pipelineDesc.vertexShader = vertexShader_;
   pipelineDesc.fragmentShader = fragmentShader_;
   pipelineDesc.debugName = "AsteroidPipeline";

   if (veCreateGraphicsPipeline(device_, &pipelineDesc, &pipeline_) != VE_SUCCESS || !pipeline_)
   {
      std::fprintf(stderr, "Failed to create graphics pipeline\n");
      return false;
   }

   const auto vertices = makeIcosahedronVertices();
   const auto indices = makeIcosahedronIndices();

   if (veCreateVertexBuffer(device_, vertices.data(), static_cast<uint64_t>(vertices.size() * sizeof(Vertex)),
                            "AsteroidVertices", &vertexBuffer_) != VE_SUCCESS ||
       vertexBuffer_ == VE_INVALID_ADDRESS)
   {
      std::fprintf(stderr, "Failed to create vertex buffer\n");
      return false;
   }

   if (veCreateIndexBuffer(device_, indices.data(),
                           static_cast<uint64_t>(indices.size() * sizeof(uint16_t)), "AsteroidIndices", &indexBuffer_) !=
           VE_SUCCESS ||
       indexBuffer_ == VE_INVALID_ADDRESS)
   {
      std::fprintf(stderr, "Failed to create index buffer\n");
      return false;
   }

   indexCount_ = static_cast<uint32_t>(indices.size());

   instanceCount_ = kAsteroidCount;
   cpuInstances_.resize(instanceCount_);
   for (auto &instance : cpuInstances_)
   {
      std::memset(&instance, 0, sizeof(AsteroidInstance));
      instance.model[0] = instance.model[5] = instance.model[10] = instance.model[15] = 1.0f;
      instance.color[3] = 1.0f;
   }

   VEBufferDesc instanceDesc{};
   instanceDesc.size = static_cast<uint64_t>(cpuInstances_.size() * sizeof(AsteroidInstance));
   instanceDesc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   instanceDesc.persistentlyMapped = false;
   instanceDesc.debugName = "AsteroidInstances";

   if (veCreateBuffer(device_, &instanceDesc, &instanceBuffer_) != VE_SUCCESS || instanceBuffer_ == VE_INVALID_ADDRESS)
   {
      std::fprintf(stderr, "Failed to create instance buffer\n");
      return false;
   }

   VEBufferDesc cameraDesc{};
   cameraDesc.size = sizeof(float) * 20; // 16 for VP, 4 for camera position.
   cameraDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
   cameraDesc.persistentlyMapped = false;
   cameraDesc.debugName = "AsteroidCamera";

   if (veCreateBuffer(device_, &cameraDesc, &cameraBuffer_) != VE_SUCCESS || cameraBuffer_ == VE_INVALID_ADDRESS)
   {
      std::fprintf(stderr, "Failed to create camera buffer\n");
      return false;
   }

   // Upload initial data.
   if (veUpdateBuffer(device_, instanceBuffer_, cpuInstances_.data(),
                      static_cast<uint64_t>(cpuInstances_.size() * sizeof(AsteroidInstance)), 0) != VE_SUCCESS)
   {
      std::fprintf(stderr, "Failed to upload instance data\n");
      return false;
   }

   chunks_.clear();
   for (uint32_t first = 0; first < instanceCount_; first += kChunkSize)
   {
      Chunk chunk{};
      chunk.firstInstance = first;
      chunk.count = std::min(kChunkSize, instanceCount_ - first);
      chunks_.push_back(chunk);
   }

   return true;
}

void AsteroidApp::destroyAssets()
{
   if (device_)
   {
      if (cameraBuffer_ != VE_INVALID_ADDRESS)
      {
         (void)veDestroyBuffer(device_, cameraBuffer_);
         cameraBuffer_ = VE_INVALID_ADDRESS;
      }
      if (instanceBuffer_ != VE_INVALID_ADDRESS)
      {
         (void)veDestroyBuffer(device_, instanceBuffer_);
         instanceBuffer_ = VE_INVALID_ADDRESS;
      }
      if (indexBuffer_ != VE_INVALID_ADDRESS)
      {
         (void)veDestroyBuffer(device_, indexBuffer_);
         indexBuffer_ = VE_INVALID_ADDRESS;
      }
      if (vertexBuffer_ != VE_INVALID_ADDRESS)
      {
         (void)veDestroyBuffer(device_, vertexBuffer_);
         vertexBuffer_ = VE_INVALID_ADDRESS;
      }
      if (pipeline_)
      {
         (void)veDestroyGraphicsPipeline(pipeline_);
         pipeline_ = nullptr;
      }
      if (fragmentShader_)
      {
         (void)veDestroyShader(fragmentShader_);
         fragmentShader_ = nullptr;
      }
      if (vertexShader_)
      {
         (void)veDestroyShader(vertexShader_);
         vertexShader_ = nullptr;
      }
   }
}

void AsteroidApp::mainLoop()
{
   auto lastTime = std::chrono::high_resolution_clock::now();

   while (!glfwWindowShouldClose(window_))
   {
      auto frameStart = std::chrono::high_resolution_clock::now();

      glfwPollEvents();
      handleInput();

      double streamingMs = 0.0;
      drainSimulationUpdates(streamingMs);

      VkExtent2D extent = veGetSwapchainSize(swapchain_);
      uint32_t width = extent.width;
      uint32_t height = extent.height;
      if (width == 0 || height == 0)
      {
         continue;
      }

      std::vector<RecordedChunk> recorded;
      double recordMs = 0.0;
      recordChunks(width, height, recorded, recordMs);

      double submitMs = 0.0;
      submitFrame(recorded, submitMs);

      auto frameEnd = std::chrono::high_resolution_clock::now();
      double frameMs = toMilliseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(frameEnd - frameStart));

      double deltaSeconds =
          std::chrono::duration_cast<std::chrono::duration<double>>(frameEnd - lastTime).count();
      lastTime = frameEnd;
      elapsedTime_ += deltaSeconds;

      collectMetrics(frameMs, recordMs, submitMs, streamingMs);
      metricsReporter_.tick(deltaSeconds);

      ++frameIndex_;
   }
}

void AsteroidApp::handleInput()
{
   if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
   {
      glfwSetWindowShouldClose(window_, GLFW_TRUE);
   }

   static bool toggleKeyDown = false;
   int state = glfwGetKey(window_, GLFW_KEY_M);
   if (state == GLFW_PRESS && !toggleKeyDown)
   {
      multithreaded_ = !multithreaded_;
      std::cout << "[Asteroids] Multithreaded rendering "
                << (multithreaded_ ? "enabled" : "disabled") << "\n";
      toggleKeyDown = true;
   }
   else if (state == GLFW_RELEASE)
   {
      toggleKeyDown = false;
   }
}

void AsteroidApp::drainSimulationUpdates(double &outCpuMs)
{
   auto start = std::chrono::high_resolution_clock::now();

   ChunkUpdate update;
   bool any = false;
   while (simulation_.popUpdate(update))
   {
      any = true;
      if (update.startIndex >= cpuInstances_.size())
      {
         continue;
      }

      const uint64_t offset = static_cast<uint64_t>(update.startIndex) * sizeof(AsteroidInstance);
      const uint64_t size =
          static_cast<uint64_t>(update.instances.size() * sizeof(AsteroidInstance));

      const size_t copyCount = std::min<std::size_t>(update.instances.size(), cpuInstances_.size() - update.startIndex);
      std::memcpy(cpuInstances_.data() + update.startIndex, update.instances.data(),
                  copyCount * sizeof(AsteroidInstance));

      if (size > 0)
      {
         VEResult result = veUpdateBuffer(device_, instanceBuffer_, update.instances.data(), size, offset);
         if (result != VE_SUCCESS)
         {
            std::fprintf(stderr, "Instance buffer update failed\n");
         }
      }
   }

   if (any)
   {
      auto end = std::chrono::high_resolution_clock::now();
      outCpuMs = toMilliseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
   }
   else
   {
      outCpuMs = 0.0;
   }
}

void AsteroidApp::recordChunks(uint32_t width, uint32_t height, std::vector<RecordedChunk> &out, double &recordCpuMs)
{
   auto start = std::chrono::high_resolution_clock::now();

   out.clear();
   out.reserve(chunks_.size());

   auto recordChunk = [this, width, height](uint32_t chunkIndex, RecordedChunk &outChunk) -> bool {
      // Set up secondary command buffer descriptor.
      // Note: We specify formats directly here rather than using vePopulateSecondaryDescFromRenderingInfo
      // because secondary buffers are recorded in parallel BEFORE we acquire the swapchain image.
      // The formats are known constants (swapchainFormat_, VK_FORMAT_D32_SFLOAT).
      VESecondaryCommandBufferDesc desc{};
      desc.beginRecording = true;
      desc.usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
      desc.colorAttachmentCount = 1;
      desc.colorAttachmentFormats[0] = swapchainFormat_;
      desc.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
      desc.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
      desc.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      desc.viewMask = 0;
      desc.occlusionQueryEnable = false;
      desc.occlusionQueryFlags = 0;
      // Render area for veApplyRenderState defaults
      desc.renderAreaX = 0;
      desc.renderAreaY = 0;
      desc.renderAreaWidth = width;
      desc.renderAreaHeight = height;

      VECommandBuffer *cmd = nullptr;
      if (veBeginSecondaryCommandBuffer(device_, &desc, &cmd) != VE_SUCCESS || !cmd)
      {
         fatal("Failed to begin secondary command buffer (no submit)");
      }

      // With VK_EXT_shader_object, dynamic state is NOT inherited by secondary command buffers.
      // Each secondary must set all required state before drawing.
      veApplyGraphicsState(cmd, pipeline_, nullptr, nullptr, nullptr);

      VEGraphicsPushConstants push = VE_INIT_GRAPHICS_PUSH_CONSTANTS();
      push.vertexBuffer = vertexBuffer_;
      push.indexBuffer = indexBuffer_;
      push.uniformBuffers[0] = cameraBuffer_;
      push.uniformBuffers[1] = instanceBuffer_;
      push.activeUniformCount = 2;

      vePushConstants(cmd, &push, sizeof(push), 0);
      veBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);

      const Chunk &chunk = chunks_[chunkIndex];
      veDrawIndexed(cmd, indexCount_, chunk.count, 0, 0, chunk.firstInstance);

      if (veEndCommandBuffer(cmd) != VE_SUCCESS)
      {
         fatal("Failed to end secondary command buffer (no submit)");
      }

      outChunk.chunkIndex = chunkIndex;
      outChunk.cmd = cmd;
      return true;
   };

   if (multithreaded_ && workerPool_)
   {
      std::mutex resultMutex;
      std::condition_variable cv;
      std::atomic<uint32_t> remaining(static_cast<uint32_t>(chunks_.size()));

      for (uint32_t i = 0; i < chunks_.size(); ++i)
      {
         workerPool_->enqueue([&, i]() {
            RecordedChunk chunk{};
            if (recordChunk(i, chunk))
            {
               std::lock_guard<std::mutex> lock(resultMutex);
               out.push_back(chunk);
            }
            if (remaining.fetch_sub(1) == 1)
            {
               cv.notify_one();
            }
         });
      }

      std::unique_lock<std::mutex> lock(resultMutex);
      cv.wait(lock, [&remaining]() { return remaining.load() == 0; });
   }
   else
   {
      for (uint32_t i = 0; i < chunks_.size(); ++i)
      {
         RecordedChunk chunk{};
         if (recordChunk(i, chunk))
         {
            out.push_back(chunk);
         }
      }
   }

   std::sort(out.begin(), out.end(), [](const RecordedChunk &a, const RecordedChunk &b) {
      return a.chunkIndex < b.chunkIndex;
   });

   auto end = std::chrono::high_resolution_clock::now();
   recordCpuMs = toMilliseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
}

void AsteroidApp::submitFrame(const std::vector<RecordedChunk> &recorded, double &submitCpuMs)
{
   auto start = std::chrono::high_resolution_clock::now();

   // Get render target size for camera projection
   VkExtent2D extent = veGetRenderTargetSize(renderTarget_);
   uint32_t width = extent.width;
   uint32_t height = extent.height;

   VECommandBuffer *primary = nullptr;
   if (veBeginCommandBuffer(device_, &primary) != VE_SUCCESS || !primary)
   {
      fatal("Failed to begin primary command buffer");
   }

   // Begin rendering (automatically handles texture transitions)
   veBeginRendering(primary, &renderingInfo_);

   // Update camera buffer once per frame before executing secondary buffers.
   // Use actual elapsed time so animation speed is framerate-independent.
   const float timeSeconds = static_cast<float>(elapsedTime_);
   std::array<float, 3> eye{std::cos(timeSeconds * 0.25f) * kCameraRadius, 15.0f,
                            std::sin(timeSeconds * 0.25f) * kCameraRadius};
   std::array<float, 3> target{0.0f, 0.0f, 0.0f};
   std::array<float, 3> up{0.0f, 1.0f, 0.0f};

   float view[16];
   computeLookAt(view, eye, target, up);
   float proj[16];
   computePerspective(proj, kPi / 3.0f, static_cast<float>(width) / static_cast<float>(height),
                      0.1f, 500.0f);
   float viewProj[16];
   multiplyMatrix(proj, view, viewProj);

   struct CameraData
   {
      float viewProj[16];
      float cameraPos[4];
   } cameraData{};
   std::memcpy(cameraData.viewProj, viewProj, sizeof(viewProj));
   cameraData.cameraPos[0] = eye[0];
   cameraData.cameraPos[1] = eye[1];
   cameraData.cameraPos[2] = eye[2];
   cameraData.cameraPos[3] = 1.0f;

   veUpdateBuffer(device_, cameraBuffer_, &cameraData, sizeof(CameraData), 0);

   // Set render state once in the primary buffer - this is inherited by all secondary buffers.
   // Note: With VK_EXT_shader_object, dynamic state is NOT inherited by secondary command buffers,
   // so each secondary must also set state. We set it here for reference/documentation.
   veBindGraphicsPipeline(primary, pipeline_);
   veApplyGraphicsState(primary, pipeline_, nullptr, nullptr, nullptr);

   if (!recorded.empty())
   {
      std::vector<VECommandBuffer *> secondaryCmds;
      secondaryCmds.reserve(recorded.size());
      for (const RecordedChunk &chunk : recorded)
      {
         secondaryCmds.push_back(chunk.cmd);
      }
      VEResult execResult =
          veExecuteSecondaryCommandBuffers(primary, static_cast<uint32_t>(secondaryCmds.size()),
                                           secondaryCmds.data(), true);  // Release secondary command buffers
      if (execResult != VE_SUCCESS)
      {
         fatal("Failed to execute secondary command buffers");
      }
   }

   veEndRendering(primary);

   // Blit render target to swapchain (acquires swapchain image automatically)
   VEResult blitResult = veBlitToSwapchain(primary, renderTarget_, swapchain_, VK_FILTER_LINEAR);
   if (blitResult == VE_ERROR_SWAPCHAIN_OUT_OF_DATE)
   {
      int framebufferWidth = 0;
      int framebufferHeight = 0;
      glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
      if (framebufferWidth > 0 && framebufferHeight > 0)
      {
         veResizeSwapchain(swapchain_, static_cast<uint32_t>(framebufferWidth),
                           static_cast<uint32_t>(framebufferHeight));
         veResizeRenderTarget(renderTarget_, static_cast<uint32_t>(framebufferWidth),
                              static_cast<uint32_t>(framebufferHeight));
         
         // Rebuild rendering info with new size and texture handles
         renderingInfo_ = veCreateRenderingInfo(static_cast<uint32_t>(framebufferWidth),
                                               static_cast<uint32_t>(framebufferHeight));
         VETextureIndex rtColor = veGetRenderTargetColorTexture(renderTarget_);
         (void)veRenderingAddColorAttachment(&renderingInfo_,
                                        rtColor,
                                        VK_ATTACHMENT_LOAD_OP_CLEAR,
                                        VEColor{0.01f, 0.01f, 0.015f, 1.0f});
         VETextureIndex rtDepth = veGetRenderTargetDepthTexture(renderTarget_);
         (void)veRenderingSetDepthAttachment(&renderingInfo_,
                                        rtDepth,
                                        VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f);
      }
      // Release command buffer manually on early return
      veReleaseCommandBuffer(primary);
      auto end = std::chrono::high_resolution_clock::now();
      submitCpuMs = toMilliseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
      return;
   }
   
   // Present (releases primary command buffer)
   VEResult presentResult = vePresentImage(swapchain_, primary, true);
   if (presentResult != VE_SUCCESS)
   {
      fatal("Failed to present image");
   }

   auto end = std::chrono::high_resolution_clock::now();
   submitCpuMs = toMilliseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
}

void AsteroidApp::collectMetrics(double frameCpuMs, double recordCpuMs, double submitCpuMs, double streamingCpuMs)
{
   MetricsSample sample;
   sample.frameIndex = frameIndex_;
   sample.frameCpuMs = frameCpuMs;
   sample.recordCpuMs = recordCpuMs;
   sample.submitCpuMs = submitCpuMs;
   sample.streamingCpuMs = streamingCpuMs;
   sample.activeWorkers = multithreaded_ && workerPool_ ? static_cast<uint32_t>(workerPool_->size()) : 1;
   sample.chunkCount = static_cast<uint32_t>(chunks_.size());
   sample.instanceCount = instanceCount_;
   sample.multithreaded = multithreaded_;
   sample.fps = frameCpuMs > 0.0 ? 1000.0 / frameCpuMs : 0.0;

   metricsCollector_.submit(sample);
}

void AsteroidApp::updateWindowTitle(const MetricsSummary &summary)
{
   if (!window_)
   {
      return;
   }

   std::ostringstream oss;
   oss << "VulkEase Asteroids | "
       << (summary.latest.multithreaded ? "MT" : "ST")
       << " | FPS: " << std::fixed << std::setprecision(1) << summary.latest.fps
       << " | Frame(ms): " << std::setprecision(2) << summary.latest.frameCpuMs
       << " | Record(ms): " << summary.latest.recordCpuMs;

   glfwSetWindowTitle(window_, oss.str().c_str());
}

void AsteroidApp::logToConsole(const MetricsSummary &summary)
{
   std::cout << std::fixed << std::setprecision(2)
             << "[Metrics] frame=" << summary.latest.frameIndex << " fps=" << summary.latest.fps
             << " frame_ms=" << summary.latest.frameCpuMs << " record_ms=" << summary.latest.recordCpuMs
             << " submit_ms=" << summary.latest.submitCpuMs << " stream_ms=" << summary.latest.streamingCpuMs
             << " workers=" << summary.latest.activeWorkers
             << " mode=" << (summary.latest.multithreaded ? "multi" : "single") << "\n";
}

void AsteroidApp::appendCsv(const MetricsSummary &summary)
{
   if (!csvFile_)
   {
      return;
   }

   std::fprintf(csvFile_.get(), "%llu,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%u,%u,%s\n",
                static_cast<unsigned long long>(summary.latest.frameIndex), summary.latest.frameCpuMs,
                summary.latest.recordCpuMs, summary.latest.submitCpuMs, summary.latest.streamingCpuMs,
                summary.latest.fps, summary.latest.activeWorkers, summary.latest.chunkCount,
                summary.latest.instanceCount, summary.latest.multithreaded ? "true" : "false");
   std::fflush(csvFile_.get());
}


