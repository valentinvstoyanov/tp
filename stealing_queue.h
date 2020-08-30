#ifndef TP__STEALING_QUEUE_H_
#define TP__STEALING_QUEUE_H_

#include <mutex>
#include <deque>
#include <condition_variable>

#ifndef NDEBUG
#include "profiler.h"
#include "profiled_mutex.h"
#endif

template<typename T>
class StealingQueue {
 public:
  StealingQueue() = default;

#ifndef NDEBUG
  explicit StealingQueue(const std::shared_ptr<Profiler>&);
#endif

  StealingQueue(const StealingQueue&) = delete;
  StealingQueue& operator=(const StealingQueue&) = delete;

  StealingQueue(StealingQueue&&);
  StealingQueue& operator=(StealingQueue&&);

  void push(T val);

  bool empty() const;

  template<typename WaitPred, typename PopPred>
  bool waitAndPopIf(T& val, const WaitPred&, const PopPred&);
  bool tryPop(T& val);
  bool trySteal(T& val);

  void clear();

  void notify();
 private:
#ifndef NDEBUG
  using MutexType = ProfiledMutex;
  using CondVarType = std::condition_variable_any;

  std::shared_ptr<Profiler> profiler;
#else
  using MutexType = std::mutex;
  using CondVarType = std::condition_variable;
#endif

  mutable MutexType mutex;
  std::deque<T> deque;
  CondVarType event;
};

#ifndef NDEBUG
template<typename T>
StealingQueue<T>::StealingQueue(const std::shared_ptr<Profiler>& profiler_ptr): profiler(profiler_ptr), mutex(profiler_ptr) {
}
#endif

template<typename T>
StealingQueue<T>::StealingQueue(StealingQueue&& other) {
  std::lock_guard<MutexType> lock(other.mutex);
  deque = std::move(other.deque);
#ifndef NDEBUG
  profiler = std::move(other.profiler);
#endif
}

template<typename T>
StealingQueue<T>& StealingQueue<T>::operator=(StealingQueue&& other) {
  if (this == &other) {
    return *this;
  }

  {
    std::lock(mutex, other.mutex);
    std::lock_guard<MutexType> this_lock(mutex, std::adopt_lock);
    std::lock_guard<MutexType> other_lock(other.mutex, std::adopt_lock);
    deque = std::move(other.deque);
#ifndef NDEBUG
    profiler = std::move(other.profiler);
#endif
  }
  event.notify_all();
  return *this;
}


template<typename T>
void StealingQueue<T>::push(T val) {
  {
    std::lock_guard<MutexType> lock(mutex);
    deque.push_front(std::move(val));
  }
  event.notify_one();
}

template<typename T>
bool StealingQueue<T>::empty() const {
  std::lock_guard<MutexType> lock(mutex);
  return deque.empty();
}

template<typename T>
bool StealingQueue<T>::tryPop(T& val) {
  std::lock_guard<MutexType> lock(mutex);
  if (deque.empty()) {
    return false;
  }
  val = std::move(deque.front());
  deque.pop_front();
  return true;
}

template<typename T>
template<typename WaitPred, typename PopPred>
bool StealingQueue<T>::waitAndPopIf(T& val, const WaitPred& wait_pred, const PopPred& pop_pred) {
  std::unique_lock<MutexType> lock(mutex);
#ifndef NDEBUG
  const auto start = Profiler::Clock::now();
  event.wait<std::unique_lock<MutexType>>(lock, [this, &wait_pred] { return wait_pred(deque.empty()); });
  const auto end = Profiler::Clock::now();
  if (profiler) {
    profiler->logWait(end - start);
  }
#else
  event.wait(lock, [this, &wait_pred] { return wait_pred(deque.empty()); });
#endif

  if (pop_pred(deque.empty())) {
    val = std::move(deque.front());
    deque.pop_front();
    return true;
  }

  return false;
}

template<typename T>
bool StealingQueue<T>::trySteal(T& val) {
  std::lock_guard<MutexType> lock(mutex);
  if (deque.empty()) {
    return false;
  }
  val = std::move(deque.back());
  deque.pop_back();
  return true;
}

template<typename T>
void StealingQueue<T>::clear() {
  std::lock_guard<MutexType> lock(mutex);
  deque.clear();
}

template<typename T>
void StealingQueue<T>::notify() {
  event.notify_all();
}

#endif //TP__STEALING_QUEUE_H_
