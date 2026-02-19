#include "Simulation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <utility>

namespace
{
constexpr float PI = 3.14159265359f;

void makeRotation(float angle, float axisX, float axisY, float axisZ, float out[16])
{
   const float norm = std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
   const float x = axisX / norm;
   const float y = axisY / norm;
   const float z = axisZ / norm;

   const float c = std::cos(angle);
   const float s = std::sin(angle);
   const float t = 1.0f - c;

   out[0] = c + x * x * t;
   out[1] = x * y * t + z * s;
   out[2] = x * z * t - y * s;
   out[3] = 0.0f;

   out[4] = x * y * t - z * s;
   out[5] = c + y * y * t;
   out[6] = y * z * t + x * s;
   out[7] = 0.0f;

   out[8] = x * z * t + y * s;
   out[9] = y * z * t - x * s;
   out[10] = c + z * z * t;
   out[11] = 0.0f;

   out[12] = 0.0f;
   out[13] = 0.0f;
   out[14] = 0.0f;
   out[15] = 1.0f;
}

void applyTransform(AsteroidInstance &instance, float translateX, float translateY, float translateZ, float scale)
{
   instance.model[12] = translateX;
   instance.model[13] = translateY;
   instance.model[14] = translateZ;

   // Apply uniform scale by scaling basis vectors.
   for (int row = 0; row < 3; ++row)
   {
      for (int col = 0; col < 3; ++col)
      {
         instance.model[row * 4 + col] *= scale;
      }
   }
}

} // namespace

SimulationSystem::SimulationSystem()
    : rng_(std::random_device{}()), uniform01_(0.0f, 1.0f), speedDist_(0.2f, 1.6f), hueDist_(0.0f, 1.0f)
{
}

SimulationSystem::~SimulationSystem() { stop(); }

void SimulationSystem::initialize(uint32_t asteroidCount, uint32_t chunkSize)
{
   asteroidCount_ = asteroidCount;
   chunkSize_ = std::max<uint32_t>(1, chunkSize);
   chunkCount_ = (asteroidCount_ + chunkSize_ - 1) / chunkSize_;

   std::lock_guard<std::mutex> lock(stateMutex_);
   state_.resize(asteroidCount_);
   for (uint32_t i = 0; i < asteroidCount_; ++i)
   {
      AsteroidInstance instance{};
      makeRotation(uniform01_(rng_) * 2.0f * PI, uniform01_(rng_), uniform01_(rng_), uniform01_(rng_), instance.model);
      const float radius = 25.0f + uniform01_(rng_) * 25.0f;
      const float angle = static_cast<float>(i) / static_cast<float>(asteroidCount_) * 2.0f * PI;
      const float height = (uniform01_(rng_) - 0.5f) * 10.0f;
      applyTransform(instance, std::cos(angle) * radius, height, std::sin(angle) * radius, 0.5f + uniform01_(rng_));

      instance.color[0] = 0.5f + 0.5f * uniform01_(rng_);
      instance.color[1] = 0.4f + 0.4f * uniform01_(rng_);
      instance.color[2] = 0.3f + 0.4f * uniform01_(rng_);
      instance.color[3] = 1.0f;

      state_[i] = instance;
   }
}

void SimulationSystem::start()
{
   if (running_.exchange(true))
   {
      return;
   }
   worker_ = std::thread([this]() { run(); });
}

void SimulationSystem::stop()
{
   if (!running_.exchange(false))
   {
      return;
   }
   if (worker_.joinable())
   {
      worker_.join();
   }
}

bool SimulationSystem::popUpdate(ChunkUpdate &outUpdate)
{
   std::lock_guard<std::mutex> lock(pendingMutex_);
   if (pending_.empty())
   {
      return false;
   }

   outUpdate = std::move(pending_.back());
   pending_.pop_back();
   return true;
}

void SimulationSystem::copyState(std::vector<AsteroidInstance> &outState) const
{
   std::lock_guard<std::mutex> lock(stateMutex_);
   outState = state_;
}

void SimulationSystem::run()
{
   using namespace std::chrono_literals;

   std::uniform_int_distribution<uint32_t> chunkPicker(0, chunkCount_ == 0 ? 0 : chunkCount_ - 1);

   while (running_)
   {
      const uint32_t updatesThisTick = std::max<uint32_t>(1, static_cast<uint32_t>(speedDist_(rng_)));
      for (uint32_t i = 0; i < updatesThisTick; ++i)
      {
         uint32_t chunk = chunkPicker(rng_);
         updateChunk(chunk);
      }

      std::this_thread::sleep_for(16ms);
   }
}

void SimulationSystem::updateChunk(uint32_t chunkIndex)
{
   if (chunkCount_ == 0)
   {
      return;
   }

   const uint32_t start = chunkIndex * chunkSize_;
   const uint32_t end = std::min(start + chunkSize_, asteroidCount_);

   ChunkUpdate update;
   update.startIndex = start;
   update.instances.reserve(end - start);

   std::lock_guard<std::mutex> lock(stateMutex_);

   for (uint32_t idx = start; idx < end; ++idx)
   {
      AsteroidInstance instance = state_[idx];

      const float jitterAngle = (uniform01_(rng_) - 0.5f) * 0.1f;
      float rotation[16];
      makeRotation(jitterAngle, uniform01_(rng_), uniform01_(rng_), uniform01_(rng_), rotation);

      // Multiply rotation * current model to spin asteroid around its local axis.
      float rotated[16]{};
      for (int row = 0; row < 4; ++row)
      {
         for (int col = 0; col < 4; ++col)
         {
            rotated[row * 4 + col] = rotation[row * 4 + 0] * instance.model[0 * 4 + col] +
                                     rotation[row * 4 + 1] * instance.model[1 * 4 + col] +
                                     rotation[row * 4 + 2] * instance.model[2 * 4 + col] +
                                     rotation[row * 4 + 3] * instance.model[3 * 4 + col];
         }
      }
      std::memcpy(instance.model, rotated, sizeof(rotated));

      // Apply small orbital drift.
      const float drift = (uniform01_(rng_) - 0.5f) * 0.15f;
      instance.model[12] += drift * instance.model[0];
      instance.model[13] += (uniform01_(rng_) - 0.5f) * 0.05f;
      instance.model[14] += drift * instance.model[8];

      // Tint color slightly to visualise streaming updates.
      const float tint = 0.02f * (uniform01_(rng_) - 0.5f);
      instance.color[0] = std::clamp(instance.color[0] + tint, 0.2f, 1.0f);
      instance.color[1] = std::clamp(instance.color[1] + tint, 0.2f, 1.0f);
      instance.color[2] = std::clamp(instance.color[2] + tint, 0.2f, 1.0f);

      state_[idx] = instance;
      update.instances.push_back(instance);
   }

   {
      std::lock_guard<std::mutex> lock(pendingMutex_);
      pending_.push_back(std::move(update));
   }
}


