#ifndef TP__JOIN_THREADS_H_
#define TP__JOIN_THREADS_H_

#include <vector>
#include <thread>

class JoinThreads {
 public:
	explicit JoinThreads(std::vector<std::thread>&);
	~JoinThreads();

 private:
	std::vector<std::thread>& threads;
};

JoinThreads::JoinThreads(std::vector<std::thread>& threads) : threads(threads) {
}

JoinThreads::~JoinThreads() {
	for (auto& thread : threads) {
		if (thread.joinable())
			thread.join();
	}
}

#endif //TP__JOIN_THREADS_H_
