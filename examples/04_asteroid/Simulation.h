#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

struct AsteroidInstance
{
   float model[16];
   float color[4];
};

struct ChunkUpdate
{
   uint32_t startIndex{0};
   std::vector<AsteroidInstance> instances;
};

/**
 * @brief Background simulation that generates chunked asteroid updates.
 *
 * The simulation maintains its own copy of the asteroid field. At a configurable
 * cadence it mutates one or more chunks and publishes them for the render thread
 * to consume. This mimics streaming fresh data from a network or disk source.
 */
class SimulationSystem
{
public:
   SimulationSystem();
   ~SimulationSystem();

   void initialize(uint32_t asteroidCount, uint32_t chunkSize);
   void start();
   void stop();

   /// Returns true if there are pending chunk updates.
   bool popUpdate(ChunkUpdate &outUpdate);

   /// Copies the current authoritative state into the provided vector.
   void copyState(std::vector<AsteroidInstance> &outState) const;

private:
   void run();

   void randomizeChunk(uint32_t chunkIndex);
   void updateChunk(uint32_t chunkIndex);

   uint32_t asteroidCount_{0};
   uint32_t chunkSize_{0};
   uint32_t chunkCount_{0};

   std::vector<AsteroidInstance> state_;

   mutable std::mutex stateMutex_;
   mutable std::mutex pendingMutex_;
   std::vector<ChunkUpdate> pending_;

   std::mt19937 rng_;
   std::uniform_real_distribution<float> uniform01_;
   std::uniform_real_distribution<float> speedDist_;
   std::uniform_real_distribution<float> hueDist_;

   std::atomic<bool> running_{false};
   std::thread worker_;
};


