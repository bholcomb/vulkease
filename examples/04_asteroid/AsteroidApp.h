#pragma once

#include "Metrics.h"
#include "Simulation.h"
#include "ThreadPool.h"

#include <GLFW/glfw3.h>
#include <cstdint>
#include <string>
#include <vector>

extern "C"
{
#include <vulkease.h>
}

struct CommandLineOptions
{
   bool enableCsv{false};
   std::string csvPath{"asteroid_metrics.csv"};
   bool startMultithreaded{true};
};

class AsteroidApp
{
public:
   AsteroidApp();
   ~AsteroidApp();

   bool run(int argc, char **argv);

private:
   struct Chunk
   {
      uint32_t firstInstance{0};
      uint32_t count{0};
   };

   struct RecordedChunk
   {
      uint32_t chunkIndex{0};
      VECommandBuffer *cmd{nullptr};
   };


   bool parseCommandLine(int argc, char **argv, CommandLineOptions &outOptions);

   bool initWindow();
   bool initVulkEase();
   void shutdownVulkEase();
   void destroyWindow();

   bool createAssets();
   void destroyAssets();
   bool recreateDepthBuffer(uint32_t width, uint32_t height);

   void mainLoop();
   void handleInput();

   void drainSimulationUpdates(double &outCpuMs);

   void recordChunks(uint32_t width, uint32_t height, std::vector<RecordedChunk> &out,
                     double &recordCpuMs);

   void submitFrame(const std::vector<RecordedChunk> &recorded, double &submitCpuMs);
   void collectMetrics(double frameCpuMs, double recordCpuMs, double submitCpuMs, double streamingCpuMs);

   void updateWindowTitle(const MetricsSummary &summary);
   void logToConsole(const MetricsSummary &summary);
   void appendCsv(const MetricsSummary &summary);

   GLFWwindow *window_{nullptr};
   VEContext *context_{nullptr};
   VEDevice *device_{nullptr};
   VESwapchain *swapchain_{nullptr};
   VkFormat swapchainFormat_{VK_FORMAT_UNDEFINED};

   VEShader *vertexShader_{nullptr};
   VEShader *fragmentShader_{nullptr};
   VEShaderConfig *shaderConfig_{nullptr};
   VERenderConfig *renderConfig_{nullptr};

   VEBufferAddress vertexBuffer_{VE_INVALID_ADDRESS};
   VEBufferAddress indexBuffer_{VE_INVALID_ADDRESS};
   VEBufferAddress instanceBuffer_{VE_INVALID_ADDRESS};
   VEBufferAddress cameraBuffer_{VE_INVALID_ADDRESS};
   VETextureIndex depthTexture_{VE_INVALID_TEXTURE_INDEX};
   uint32_t depthWidth_{0};
   uint32_t depthHeight_{0};

   uint32_t indexCount_{0};
   uint32_t instanceCount_{0};
   std::vector<AsteroidInstance> cpuInstances_;
   std::vector<Chunk> chunks_;

   SimulationSystem simulation_;
   std::unique_ptr<ThreadPool> workerPool_;
   bool multithreaded_{true};

   MetricsCollector metricsCollector_;
   MetricsReporter metricsReporter_;
   CommandLineOptions options_;

   double lastTitleUpdate_{0.0};
   double elapsedTime_{0.0};  // Total elapsed time in seconds
   uint64_t frameIndex_{0};
   std::unique_ptr<FILE, int (*)(FILE *)> csvFile_;


};


