#ifndef TP__THREAD_POOL_H_
#define TP__THREAD_POOL_H_

#include <queue>
#include <mutex>
#include <thread>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <random>
#include <utility>
#include "join_threads.h"
#include "destruction_policy.h"
#include "thread_safe_queue.h"
#include "worker.h"

class ThreadPool {
 public:
  using Task = std::function<void()>;

  explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency(),
                      DestructionPolicy destruction_policy = DestructionPolicy::WAIT_CURRENT);

#ifndef NDEBUG
  explicit ThreadPool(const std::shared_ptr<Profiler>&,
                      std::size_t thread_count = std::thread::hardware_concurrency(),
                      DestructionPolicy destruction_policy = DestructionPolicy::WAIT_CURRENT);
#endif

  ~ThreadPool();

  void add(Task task);
  void clearTasks();
  void waitTasks();

  template<typename InputIt, typename UnaryFunction>
  void forEach(InputIt first, InputIt last, UnaryFunction f);
 private:
  void terminate();

  std::vector<Worker<Task>> workers;
  std::atomic_bool terminated;
  std::atomic_bool waiting;
  std::atomic_size_t idle_workers_count;
  DestructionPolicy destruction_policy;

  std::random_device random_device;
  std::mt19937 engine;
  std::uniform_int_distribution<std::size_t> distribution;

#ifndef NDEBUG
  std::shared_ptr<Profiler> profiler;
#endif
};

ThreadPool::ThreadPool(std::size_t thread_count, DestructionPolicy destruction_policy)
    : terminated(false),
      waiting(false),
      destruction_policy(destruction_policy),
      idle_workers_count(0),
      engine(random_device()) {
  assert(thread_count > 0 && "The supplied thread count value cannot be 0");

  distribution = std::uniform_int_distribution<std::size_t>(0, thread_count - 1);

  workers.reserve(thread_count);
  try {
    for (auto i = 0; i < thread_count; ++i) {
      workers.emplace_back(
          [this](Task& task) {
            if (terminated) {
              return false;
            }

            auto starting_index = distribution(engine);

            for (auto i = 0; i < workers.size() - 1; ++i) {
              if (workers[(starting_index + i) % workers.size()].trySteal(task)) {
                return true;
              }
            }

            return false;
          },
          [this] {
            idle_workers_count++;

            while (waiting) {
              std::this_thread::yield();//sleep_for(std::chrono::microseconds (100));
            }
          }
      );
    }
  } catch (...) {
    terminate();
    throw;
  }
}

#ifndef NDEBUG
ThreadPool::ThreadPool(const std::shared_ptr<Profiler>& profiler_ptr,
                       std::size_t thread_count,
                       DestructionPolicy destruction_policy)
    : profiler(profiler_ptr),
      terminated(false),
      waiting(false),
      destruction_policy(destruction_policy),
      idle_workers_count(0),
      engine(random_device()) {
  assert(thread_count > 0 && "The supplied thread count value cannot be 0");

  distribution = std::uniform_int_distribution<std::size_t>(0, thread_count - 1);

  workers.reserve(thread_count);
  try {
    for (auto i = 0; i < thread_count; ++i) {
      workers.emplace_back(
          [this](Task& task) {
            if (terminated) {
              return false;
            }

            auto starting_index = distribution(engine);

            for (auto i = 0; i < workers.size() - 1; ++i) {
              if (workers[(starting_index + i) % workers.size()].trySteal(task)) {
                return true;
              }
            }

            return false;
          },
          [this] {
            idle_workers_count++;

            while (waiting) {
              std::this_thread::yield();//sleep_for(std::chrono::microseconds (100));
            }
          },
          profiler_ptr
      );
    }
  } catch (...) {
    terminate();
    throw;
  }
}
#endif

ThreadPool::~ThreadPool() {
  if (destruction_policy == DestructionPolicy::WAIT_CURRENT) {
    terminate();
  } else if (destruction_policy == DestructionPolicy::WAIT_ALL) {
    waitTasks();
    terminate();
  }
}

void ThreadPool::add(ThreadPool::Task task) {
  workers[distribution(engine)].add(std::move(task));
}

void ThreadPool::clearTasks() {
  for (auto& worker: workers) {
    worker.clearTasks();
  }
}

void ThreadPool::waitTasks() {
  waiting = true;
  for (auto& worker: workers) {
    worker.setWait(true);
  }

  while (idle_workers_count != workers.size()) {
    std::this_thread::yield();//sleep_for(std::chrono::milliseconds(50));
  }

  for (auto& worker: workers) {
    worker.setWait(false);
  }
  waiting = false;
}

template<typename InputIt, typename UnaryFunction>
void ThreadPool::forEach(InputIt first, InputIt last, UnaryFunction f) {
  auto tasks_count = std::distance(first, last);
  auto tasks_per_worker_count = tasks_count / workers.size();
  auto remaining_tasks_count = tasks_count % workers.size();

  for (auto& worker: workers) {
    worker.add([first, &f, tasks_per_worker_count] { std::for_each(first, first + tasks_per_worker_count, f); });
    first += tasks_per_worker_count;
  }

  if (remaining_tasks_count > 0) {
    add([first, last, &f] { std::for_each(first, last, f); });
  }
}

void ThreadPool::terminate() {
  terminated = true;

  for (auto& worker: workers) {
    worker.terminate();
  }
}

#endif //TP__THREAD_POOL_H_
