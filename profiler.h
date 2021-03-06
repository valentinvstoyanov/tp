#ifndef TP__PROFILER_H_
#define TP__PROFILER_H_

#include <unordered_map>
#include <thread>
#include <mutex>
#include <iostream>

class Profiler {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;
  using Duration = Clock::duration;

  struct ThreadInfo {
    Duration lock_duration = Duration::zero();
    Duration wait_duration = Duration::zero();
    Duration tasks_duration = Duration::zero();

    std::size_t completed_tasks_count = 0;

    unsigned locks_count = 0;
    unsigned unlocks_count = 0;


    friend std::ostream& operator<<(std::ostream& os, const ThreadInfo& thread_info) {
      return os << "\tLock time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(thread_info.lock_duration).count()
                << "\n\tLocks count: " << thread_info.locks_count
                << "\n\tUnlocks count: " << thread_info.unlocks_count
                << "\n\tLocks - Unlocks diff: " << thread_info.locks_count - thread_info.unlocks_count
                << "\n\tCompleted tasks count: " << thread_info.completed_tasks_count
                << "\n\tTasks time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(thread_info.tasks_duration).count()
                << "\n\tAverage task time: " << (thread_info.completed_tasks_count == 0 ? 0 : std::chrono::duration_cast<std::chrono::nanoseconds>(thread_info.tasks_duration / thread_info.completed_tasks_count).count());
    }
  };

  Profiler() = default;

  Profiler(Profiler&&);
  Profiler& operator=(Profiler&&);

  Profiler(const Profiler&) = delete;
  Profiler& operator=(const Profiler&) = delete;

  void logLock();
  void logUnlock();

  void logWait(Duration);
  void logTask(Duration);

  friend std::ostream& operator<<(std::ostream& os, const Profiler& profiler);
 private:
  std::unordered_map<std::thread::id, ThreadInfo> thread_info_map;
  mutable std::mutex mutex;
};

Profiler::Profiler(Profiler&& other) {
  std::lock_guard<std::mutex> lock(other.mutex);
  thread_info_map = std::move(other.thread_info_map);
}

Profiler& Profiler::operator=(Profiler&& other) {
  if (this != &other) {
    std::lock(mutex, other.mutex);
    std::lock_guard<std::mutex> this_lock(mutex, std::adopt_lock);
    std::lock_guard<std::mutex> other_lock(other.mutex, std::adopt_lock);
    thread_info_map = std::move(other.thread_info_map);
  }
  return *this;
}

void Profiler::logLock() {
  auto duration = Clock::now().time_since_epoch();
  std::lock_guard<std::mutex> lock(mutex);
  auto& thread_info = thread_info_map[std::this_thread::get_id()];
  thread_info.lock_duration -= duration;
  thread_info.locks_count += 1;
}

void Profiler::logUnlock() {
  auto duration = Clock::now().time_since_epoch();
  std::lock_guard<std::mutex> lock(mutex);
  auto& thread_info = thread_info_map[std::this_thread::get_id()];
  thread_info.lock_duration += duration;
  thread_info.unlocks_count += 1;
}

std::ostream& operator<<(std::ostream& os, const Profiler& profiler) {
  std::lock_guard<std::mutex> lock(profiler.mutex);
  for (auto& p: profiler.thread_info_map) {
    os << "Thread id : " << p.first << "\n" << p.second << "\n";
  }
  return os;
}

void Profiler::logWait(Duration duration) {
  std::lock_guard<std::mutex> lock(mutex);
  auto& thread_info = thread_info_map[std::this_thread::get_id()];
  thread_info.wait_duration += duration;
}

void Profiler::logTask(Duration duration) {
  std::lock_guard<std::mutex> lock(mutex);
  auto& thread_info = thread_info_map[std::this_thread::get_id()];
  thread_info.tasks_duration += duration;
  thread_info.completed_tasks_count += 1;
}

#endif //TP__PROFILER_H_
