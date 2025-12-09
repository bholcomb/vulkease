#include "ThreadPool.h"

#include <cassert>
#include <stdexcept>

ThreadPool::ThreadPool(std::size_t threadCount)
{
   if (threadCount == 0)
   {
      threadCount = 1;
   }

   try
   {
      workers_.reserve(threadCount);
      for (std::size_t i = 0; i < threadCount; ++i)
      {
         workers_.emplace_back([this]() { workerLoop(); });
      }
   }
   catch (...)
   {
      shutdown_ = true;
      cvJobs_.notify_all();
      for (auto &worker : workers_)
      {
         if (worker.joinable())
         {
            worker.join();
         }
      }
      throw;
   }
}

ThreadPool::~ThreadPool()
{
   {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
   }

   cvJobs_.notify_all();

   for (auto &worker : workers_)
   {
      if (worker.joinable())
      {
         worker.join();
      }
   }
}

void ThreadPool::enqueue(std::function<void()> job)
{
   {
      std::lock_guard<std::mutex> lock(mutex_);
      if (shutdown_)
      {
         throw std::runtime_error("ThreadPool is shutting down");
      }
      jobs_.push(std::move(job));
   }
   cvJobs_.notify_one();
}

void ThreadPool::waitIdle()
{
   std::unique_lock<std::mutex> lock(mutex_);
   cvIdle_.wait(lock, [this]() { return jobs_.empty() && activeWorkers_ == 0; });
}

void ThreadPool::workerLoop()
{
   while (true)
   {
      std::function<void()> job;
      {
         std::unique_lock<std::mutex> lock(mutex_);
         cvJobs_.wait(lock, [this]() { return shutdown_ || !jobs_.empty(); });

         if (shutdown_ && jobs_.empty())
         {
            return;
         }

         job = std::move(jobs_.front());
         jobs_.pop();
         ++activeWorkers_;
      }

      try
      {
         job();
      }
      catch (...)
      {
         // Swallow exceptions to keep other workers alive.
      }

      {
         std::lock_guard<std::mutex> lock(mutex_);
         assert(activeWorkers_ > 0);
         --activeWorkers_;
         if (jobs_.empty() && activeWorkers_ == 0)
         {
            cvIdle_.notify_all();
         }
      }
   }
}


