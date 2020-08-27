#include <iostream>
#include "thread_pool.h"

using namespace std::chrono_literals;

std::chrono::high_resolution_clock::duration forEachTestWithoutThreadPool(std::vector<int>& v, const std::function<void(int&)>& f) {
	auto start = std::chrono::high_resolution_clock::now();
	for (auto& x:v)
		f(x);
	auto end = std::chrono::high_resolution_clock::now();
	return end - start;
}

std::chrono::high_resolution_clock::duration forEachTestWithThreadPool(std::vector<int>& v, const std::function<void(int&)>& f) {
	auto start = std::chrono::high_resolution_clock::now();
	{
		ThreadPool thread_pool(2, DestructionPolicy::WAIT_ALL);
		thread_pool.forEach(v.begin(), v.end(), f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	return end - start;
}

void forEachTest() {
	std::vector<int> v(100, 1);
	auto f = [] (int& x) {
		std::this_thread::sleep_for(1us);
		x *= 3;
	};

	auto seq_d = std::chrono::duration_cast<std::chrono::milliseconds>(forEachTestWithoutThreadPool(v, f)).count();
	std::cout << "forEach without thread pool took : " << seq_d << "\n";

	std::fill(v.begin(), v.end(), 1);

	auto par_d = std::chrono::duration_cast<std::chrono::milliseconds>(forEachTestWithThreadPool(v, f)).count();
	std::cout << "forEach with thread pool took : " << par_d << "\n";
}

int main() {
	forEachTest();

//	std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
//
//	{
//		ThreadPool thread_pool(4, DestructionPolicy::WAIT_ALL);
//
//		for (auto i = 0; i < 10; ++i) {
//			thread_pool.add([i] {
//				std::this_thread::sleep_for(std::chrono::seconds((i * i + 1) / 10));
//				std::cout << i << '\n';
//			});
//		}
//
//		thread_pool.forEach(v.begin(), v.end(), [](int& x) {
//			x *= 3;
//		});
//	}
//
//	for (auto& x:v) {
//		std::cout << x << " ";
//	}
//	std::cout << "\n";

	return 0;
}
