#ifndef TP__STEALING_QUEUE_H_
#define TP__STEALING_QUEUE_H_

#include <mutex>
#include <deque>
#include <condition_variable>

template<typename T>
class StealingQueue {
 public:
  StealingQueue() = default;

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
  mutable std::mutex mutex;
  std::deque<T> deque;
  std::condition_variable event;
};

template<typename T>
void StealingQueue<T>::push(T val) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    deque.push_front(std::move(val));
  }
  event.notify_one();
}

template<typename T>
bool StealingQueue<T>::empty() const {
  std::lock_guard<std::mutex> lock(mutex);
  return deque.empty();
}

template<typename T>
bool StealingQueue<T>::tryPop(T& val) {
  std::lock_guard<std::mutex> lock(mutex);
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
  std::unique_lock<std::mutex> lock(mutex);
  event.wait(lock, [this, &wait_pred] { return wait_pred(deque.empty()); });
  if (pop_pred(deque.empty())) {
    val = std::move(deque.front());
    deque.pop_front();
    return true;
  }

  return false;
}

template<typename T>
bool StealingQueue<T>::trySteal(T& val) {
  std::lock_guard<std::mutex> lock(mutex);
  if (deque.empty()) {
    return false;
  }
  val = std::move(deque.back());
  deque.pop_back();
  return true;
}

template<typename T>
void StealingQueue<T>::clear() {
  std::lock_guard<std::mutex> lock(mutex);
  deque.clear();
}

template<typename T>
StealingQueue<T>::StealingQueue(StealingQueue&& other) {
  std::lock_guard<std::mutex> lock(other.mutex);
  deque = std::move(other.deque);
}

template<typename T>
StealingQueue<T>& StealingQueue<T>::operator=(StealingQueue&& other) {
  if (this == &other) {
    return *this;
  }

  {
    std::lock(mutex, other.mutex);
    std::lock_guard<std::mutex> this_lock(mutex, std::adopt_lock);
    std::lock_guard<std::mutex> other_lock(other.mutex, std::adopt_lock);
    deque = std::move(other.deque);
  }
  event.notify_all();
  return *this;
}

template<typename T>
void StealingQueue<T>::notify() {
  event.notify_all();
}

#endif //TP__STEALING_QUEUE_H_
