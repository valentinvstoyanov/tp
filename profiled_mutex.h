#ifndef TP__PROFILED_MUTEX_H_
#define TP__PROFILED_MUTEX_H_

#include <utility>

#include "profiler.h"

class ProfiledMutex {
 public:
  explicit ProfiledMutex(std::shared_ptr<Profiler> profiler = nullptr);

  ProfiledMutex(ProfiledMutex&&);
  ProfiledMutex& operator=(ProfiledMutex&&);

  ProfiledMutex(const ProfiledMutex&) = delete;
  ProfiledMutex& operator=(const ProfiledMutex&) = delete;

  void lock();
  void unlock();
 private:
  std::shared_ptr<Profiler> profiler;
  std::mutex mutex;
};

ProfiledMutex::ProfiledMutex(std::shared_ptr<Profiler> profiler_ptr) : profiler(std::move(profiler_ptr)) {
}

ProfiledMutex::ProfiledMutex(ProfiledMutex&& other) {
  std::lock_guard<std::mutex> lock(other.mutex);
  profiler = std::move(other.profiler);
}

ProfiledMutex& ProfiledMutex::operator=(ProfiledMutex&& other) {
  if (this != &other) {
    std::lock(mutex, other.mutex);
    std::lock_guard<std::mutex> this_lock(mutex, std::adopt_lock);
    std::lock_guard<std::mutex> other_lock(other.mutex, std::adopt_lock);
    profiler = std::move(other.profiler);
  }
  return *this;
}

void ProfiledMutex::lock() {
  mutex.lock();
  if (profiler)
    profiler->logLock();
}

void ProfiledMutex::unlock() {
  mutex.unlock();
  if (profiler)
    profiler->logUnlock();
}

#endif //TP__PROFILED_MUTEX_H_
