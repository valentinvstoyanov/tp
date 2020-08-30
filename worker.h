#ifndef TP__WORKER_H_
#define TP__WORKER_H_

#include <thread>
#include <functional>
#include <atomic>
#include "stealing_queue.h"

template<typename Task>
class Worker {
 public:
  using StealCallback = std::function<bool(Task&)>;
  using IdleCallback = std::function<void()>;

  Worker(StealCallback, IdleCallback);

#ifndef NDEBUG
  Worker(StealCallback, IdleCallback, const std::shared_ptr<Profiler>&);
#endif

  ~Worker();

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  Worker(Worker&&);
  Worker& operator=(Worker&&) = default;

  void add(Task task);
  void clearTasks();
  bool trySteal(Task& task);

  void terminate();
  void setWait(bool);

 private:
  void workerFunction();

  StealingQueue<Task> queue;
  StealCallback steal_callback;
  IdleCallback idle_callback;
  std::atomic_bool terminated;
  std::atomic_bool waiting;
  std::thread thread;

#ifndef NDEBUG
  std::shared_ptr<Profiler> profiler;
#endif
};

template<typename Task>
Worker<Task>::Worker(StealCallback steal_callback, IdleCallback idle_callback)
    : terminated(false),
      waiting(false),
      steal_callback(std::move(steal_callback)),
      idle_callback(std::move(idle_callback)),
      thread(&Worker::workerFunction, this) {
}

#ifndef NDEBUG
template<typename Task>
Worker<Task>::Worker(StealCallback steal_callback, IdleCallback idle_callback, const std::shared_ptr<Profiler>& profiler_ptr)
    : profiler(profiler_ptr),
      queue(profiler_ptr),
      terminated(false),
      waiting(false),
      steal_callback(std::move(steal_callback)),
      idle_callback(std::move(idle_callback)),
      thread(&Worker::workerFunction, this) {
}
#endif

/*profiler(profiler_ptr),
      queue(profiler_ptr),
      terminated(false),
      waiting(false),
      steal_callback(std::move(steal_callback)),
      idle_callback(std::move(idle_callback)),
      thread(&Worker::workerFunction, this)*/

template<typename Task>
Worker<Task>::Worker(Worker&& other)
    : queue(std::move(other.queue)),
      steal_callback(std::move(other.steal_callback)),
      idle_callback(std::move(other.idle_callback)),
      terminated(other.terminated.load()),
      waiting(other.waiting.load()),
      thread(std::move(other.thread)) {
#ifndef NDEBUG
  profiler = std::move(other.profiler);
#endif
}

template<typename Task>
Worker<Task>::~Worker() {
  terminate();
}

template<typename Task>
void Worker<Task>::add(Task task) {
  queue.push(std::move(task));
}

template<typename Task>
void Worker<Task>::clearTasks() {
  queue.clear();
}

template<typename Task>
bool Worker<Task>::trySteal(Task& task) {
  return queue.trySteal(task);
}

template<typename Task>
void Worker<Task>::workerFunction() {
  while (!terminated) {
    Task task;
    if (queue.tryPop(task) ||
        steal_callback(task) ||
        queue.waitAndPopIf(task,
                           [this](bool empty) { return waiting || terminated || !empty; },
                           [this](bool empty) { return !empty && !terminated; })) {
      if (!terminated) {
#ifndef NDEBUG
        const auto start = Profiler::Clock::now();
#endif
        task();
#ifndef NDEBUG
        const auto end = Profiler::Clock::now();
        if (profiler) {
          profiler->logTask(end - start);
        }
#endif
      }
    } else if (waiting) {
      idle_callback();
    }
  }
}

template<typename Task>
void Worker<Task>::terminate() {
  terminated = true;
  queue.notify();
  if (thread.joinable()) {
    thread.join();
  }
}

template<typename Task>
void Worker<Task>::setWait(bool w) {
  waiting = w;
  if (w) {
    queue.notify();
  }
}

#endif //TP__WORKER_H_
