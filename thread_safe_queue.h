#ifndef TP__THREAD_SAFE_QUEUE_H_
#define TP__THREAD_SAFE_QUEUE_H_

#include <condition_variable>
#include <memory>
#include <queue>

template<typename T>
class ThreadSafeQueue {
 public:
  ThreadSafeQueue() = default;

  void push(T val);
  template<typename InputIt>
  void pushAll(InputIt first, InputIt last);

  void waitAndPop(T& val);
  bool tryPop(T& val);

  bool empty() const;
 private:
  mutable std::mutex mutex;
  std::queue<T> queue;
  std::condition_variable event;
};

template<typename T>
void ThreadSafeQueue<T>::push(T val) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push(std::move(val));
  }
  event.notify_one();
}

template<typename T>
template<typename InputIt>
void ThreadSafeQueue<T>::pushAll(InputIt first, InputIt last) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (; first != last; ++first) {
      queue.push(*first);
    }
  }
  event.notify_all();
}

template<typename T>
void ThreadSafeQueue<T>::waitAndPop(T& val) {
  std::unique_lock<std::mutex> lock(mutex);
  event.wait(lock, [this] { return !queue.empty(); });
  val = std::move(queue.front());
  queue.pop();
}

template<typename T>
bool ThreadSafeQueue<T>::tryPop(T& val) {
  std::lock_guard<std::mutex> lock(mutex);
  if (queue.empty())
    return false;
  val = std::move(queue.front());
  queue.pop();
  return true;
}

template<typename T>
bool ThreadSafeQueue<T>::empty() const {
  std::lock_guard<std::mutex> lock(mutex);
  return queue.empty();
}

#endif //TP__THREAD_SAFE_QUEUE_H_
