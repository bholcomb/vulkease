#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/**
 * @brief Simple C++17 thread pool for scheduling lightweight jobs.
 *
 * The pool owns a fixed set of worker threads. Jobs are FIFO scheduled.
 * waitIdle() blocks until all queued jobs finish. Destroying the pool joins all
 * workers and drains outstanding jobs.
 */
class ThreadPool
{
public:
   explicit ThreadPool(std::size_t threadCount);
   ~ThreadPool();

   ThreadPool(const ThreadPool &) = delete;
   ThreadPool &operator=(const ThreadPool &) = delete;

   /// Schedule a job for execution.
   void enqueue(std::function<void()> job);

   /// Block until the queue is empty and no worker is currently executing.
   void waitIdle();

   std::size_t size() const noexcept { return workers_.size(); }

private:
   void workerLoop();

   std::vector<std::thread> workers_;
   std::queue<std::function<void()>> jobs_;
   mutable std::mutex mutex_;
   std::condition_variable cvJobs_;
   std::condition_variable cvIdle_;
   bool shutdown_{false};
   std::size_t activeWorkers_{0};
};


