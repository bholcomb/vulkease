#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct MetricsSample
{
   uint64_t frameIndex{0};
   double frameCpuMs{0.0};
   double recordCpuMs{0.0};
   double submitCpuMs{0.0};
   double streamingCpuMs{0.0};
   double fps{0.0};
   uint32_t activeWorkers{0};
   uint32_t chunkCount{0};
   uint32_t instanceCount{0};
   bool multithreaded{true};
};

struct MetricsSummary
{
   MetricsSample latest;
   MetricsSample average;
};

class MetricsCollector
{
public:
   explicit MetricsCollector(std::size_t capacity = 240);

   void submit(const MetricsSample &sample);

   MetricsSummary summary() const;

private:
   const std::size_t capacity_;
   mutable std::mutex mutex_;
   std::vector<MetricsSample> buffer_;
   std::size_t index_{0};
   std::size_t count_{0};
   MetricsSample latest_;
};

class MetricsReporter
{
public:
   using SinkCallback = std::function<void(const MetricsSummary &)>;

   struct SinkConfig
   {
      std::string name;
      double intervalSeconds{1.0};
      SinkCallback callback;
   };

   explicit MetricsReporter(MetricsCollector &collector);

   void addSink(SinkConfig config);

   /// Advance timers and emit sinks that reach their interval.
   void tick(double deltaSeconds);

   /// Forces all sinks to emit immediately.
   void flush();

private:
   struct SinkState
   {
      SinkConfig config;
      double accumulated{0.0};
   };

   MetricsCollector &collector_;
   std::vector<SinkState> sinks_;
};

double toMilliseconds(std::chrono::nanoseconds duration);


