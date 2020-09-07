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
#include "destruction_policy.h"
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
            if (workers.empty() || terminated) {
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
            if (workers.empty() || terminated) {
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
    std::this_thread::yield();
  }

  for (auto& worker: workers) {
    worker.setWait(false);
  }
  waiting = false;
}

template<typename InputIt, typename UnaryFunction>
void ThreadPool::forEach(InputIt first, InputIt last, UnaryFunction f) {
  const auto tasks_count = std::distance(first, last);
//  if (tasks_count == 1) {
//    add([first, f] { f(*first); });
//    return;
//  }

  const auto tasks_per_worker = tasks_count / workers.size();
  if (tasks_per_worker > 0) {
    for (auto& worker : workers) {
      worker.add([this, first, tasks_per_worker, f] { forEach(first, first + tasks_per_worker, f); });
      std::advance(first, tasks_per_worker);
    }
  }

  const auto remaining_tasks_count = tasks_count % workers.size();
  if (remaining_tasks_count > 0) {
    if (remaining_tasks_count == 1) {
      add([first, f] { f(*first); });
    } else {
      const auto half = remaining_tasks_count / 2;
      add([this, first, half, f] { forEach(first, first + half, f); });
      std::advance(first, half);
      add([this, first, last, f] { forEach(first, last, f); });
    }
  }
}

void ThreadPool::terminate() {
  terminated = true;

  for (auto& worker: workers) {
    worker.terminate();
  }
}

#endif //TP__THREAD_POOL_H_
