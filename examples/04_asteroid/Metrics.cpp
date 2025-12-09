#include "Metrics.h"

#include <algorithm>
#include <cstdio>
#include <numeric>

MetricsCollector::MetricsCollector(std::size_t capacity) : capacity_(std::max<std::size_t>(1, capacity))
{
   buffer_.resize(capacity_);
}

void MetricsCollector::submit(const MetricsSample &sample)
{
   std::lock_guard<std::mutex> lock(mutex_);
   buffer_[index_] = sample;
   latest_ = sample;
   index_ = (index_ + 1) % capacity_;
   count_ = std::min<std::size_t>(count_ + 1, capacity_);
}

MetricsSummary MetricsCollector::summary() const
{
   std::lock_guard<std::mutex> lock(mutex_);

   MetricsSample avg{};
   if (count_ > 0)
   {
      auto accumulate = [this](auto accessor) {
         double total = 0.0;
         for (std::size_t i = 0; i < count_; ++i)
         {
            total += accessor(buffer_[i]);
         }
         return total / static_cast<double>(count_);
      };

      avg.frameCpuMs = accumulate([](const MetricsSample &s) { return s.frameCpuMs; });
      avg.recordCpuMs = accumulate([](const MetricsSample &s) { return s.recordCpuMs; });
      avg.submitCpuMs = accumulate([](const MetricsSample &s) { return s.submitCpuMs; });
      avg.streamingCpuMs = accumulate([](const MetricsSample &s) { return s.streamingCpuMs; });
      avg.fps = accumulate([](const MetricsSample &s) { return s.fps; });
      avg.activeWorkers = static_cast<uint32_t>(
          accumulate([](const MetricsSample &s) { return static_cast<double>(s.activeWorkers); }));
      avg.chunkCount =
          static_cast<uint32_t>(accumulate([](const MetricsSample &s) { return static_cast<double>(s.chunkCount); }));
      avg.instanceCount = static_cast<uint32_t>(
          accumulate([](const MetricsSample &s) { return static_cast<double>(s.instanceCount); }));
   }

   avg.multithreaded = latest_.multithreaded;

   return MetricsSummary{latest_, avg};
}

MetricsReporter::MetricsReporter(MetricsCollector &collector) : collector_(collector) {}

void MetricsReporter::addSink(SinkConfig config)
{
   if (!config.callback || config.intervalSeconds <= 0.0)
   {
      return;
   }
   sinks_.push_back(SinkState{std::move(config), 0.0});
}

void MetricsReporter::tick(double deltaSeconds)
{
   MetricsSummary summary = collector_.summary();

   for (auto &sink : sinks_)
   {
      sink.accumulated += deltaSeconds;
      if (sink.accumulated >= sink.config.intervalSeconds)
      {
         sink.config.callback(summary);
         sink.accumulated = 0.0;
      }
   }
}

void MetricsReporter::flush()
{
   MetricsSummary summary = collector_.summary();
   for (auto &sink : sinks_)
   {
      sink.config.callback(summary);
      sink.accumulated = 0.0;
   }
}

double toMilliseconds(std::chrono::nanoseconds duration)
{
   return static_cast<double>(duration.count()) / 1'000'000.0;
}


