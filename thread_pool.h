#ifndef TP__THREAD_POOL_H_
#define TP__THREAD_POOL_H_

#include <queue>
#include <mutex>
#include <thread>
#include <cassert>
#include <condition_variable>
#include "join_threads.h"
#include "destruction_policy.h"

class ThreadPool {
 public:
	using Task = std::function<void()>;

	explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency(),
											DestructionPolicy destruction_policy = DestructionPolicy::WAIT_CURRENT);
	~ThreadPool();

	void add(Task task);
	void clearTasks();

	template<typename InputIt, typename UnaryFunction>
	void forEach(InputIt first, InputIt last, UnaryFunction f);
 private:
	void workerFunction();

	std::queue<Task> tasks;
	std::vector<std::thread> workers;
	JoinThreads worker_joiner;

	std::mutex mutex;
	std::condition_variable tasks_event;
	bool done;

	DestructionPolicy destruction_policy;
};

ThreadPool::ThreadPool(std::size_t thread_count, DestructionPolicy destruction_policy)
		: done(false), destruction_policy(destruction_policy), worker_joiner(workers) {
	assert(thread_count > 0 && "The supplied thread count value cannot be 0");

	workers.reserve(thread_count);
	try {
		for (auto i = 0; i < thread_count; ++i) {
			workers.emplace_back(&ThreadPool::workerFunction, this);
		}
	} catch (...) {
		done = true;
		throw;
	}
}

ThreadPool::~ThreadPool() {
	{
		std::lock_guard<std::mutex> lock(mutex);
		done = true;
	}
	tasks_event.notify_all();
}

void ThreadPool::add(ThreadPool::Task task) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		tasks.push(std::move(task));
	}
	tasks_event.notify_one();
}

void ThreadPool::clearTasks() {
	std::lock_guard<std::mutex> lock(mutex);
	while (!tasks.empty()) {
		tasks.pop();
	}
}

template<typename InputIt, typename UnaryFunction>
void ThreadPool::forEach(InputIt first, InputIt last, UnaryFunction f) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (; first != last; ++first) {
			auto& v = *first;
			tasks.push([&f, &v] { f(v); });
		}
	}
	tasks_event.notify_all();
}

void ThreadPool::workerFunction() {
	while (true) {
		std::unique_lock<std::mutex> lock(mutex);
		tasks_event.wait(lock, [this] {
			return done || !tasks.empty();
		});

		if (done && (destruction_policy == DestructionPolicy::WAIT_CURRENT
				|| (destruction_policy == DestructionPolicy::WAIT_ALL && tasks.empty()))) {
			break;
		}

		auto task = std::move(tasks.front());
		tasks.pop();
		lock.unlock();
		task();
	}
}

#endif //TP__THREAD_POOL_H_
