#include <iostream>
#include <vector>
#include "thread_pool.h"

using namespace std::chrono_literals;

std::chrono::high_resolution_clock::duration forEachTestWithoutThreadPool(std::vector<int>& v,
                                                                          const std::function<void(int&)>& f) {
  auto start = std::chrono::high_resolution_clock::now();
  for (auto& x:v)
    f(x);
  auto end = std::chrono::high_resolution_clock::now();
  return end - start;
}

std::chrono::high_resolution_clock::duration forEachTestWithThreadPool(std::vector<int>& v,
                                                                       const std::function<void(int&)>& f) {
  auto start = std::chrono::high_resolution_clock::now();
  {
    ThreadPool thread_pool(2, DestructionPolicy::WAIT_ALL);
    thread_pool.forEach(v.begin(), v.end(), f);
  }
  auto end = std::chrono::high_resolution_clock::now();
  return end - start;
}

void forEachTest() {
  std::vector<int> v(100000, 1);
  auto f = [](int& x) {
    std::this_thread::sleep_for(1us);
    x *= 3;
  };

  auto seq_d = std::chrono::duration_cast<std::chrono::milliseconds>(forEachTestWithoutThreadPool(v, f)).count();
  std::cout << "forEach without thread pool took : " << seq_d << "\n";
  std::for_each(v.begin(), v.end(), [](int x) { assert(x == 3 && "x should be 3");});

  std::fill(v.begin(), v.end(), 1);

  auto par_d = std::chrono::duration_cast<std::chrono::milliseconds>(forEachTestWithThreadPool(v, f)).count();
  std::cout << "forEach with thread pool took : " << par_d << "\n";
  std::for_each(v.begin(), v.end(), [](int x) { assert(x == 3 && "x should be 3");});
}

int main() {
   forEachTest();

//  std::vector<int> v(1000, 1);
//
//  {
//    ThreadPool thread_pool(4, DestructionPolicy::WAIT_ALL);
//
//    for (auto i = 0; i < 10; ++i) {
//      thread_pool.add([i] {
//        std::this_thread::sleep_for(std::chrono::seconds((i * i + 3) / 10));
//        std::cout << i << '\n';
//      });
//    }
//
//    thread_pool.forEach(v.begin(), v.end(), [](int& x) {
//      std::this_thread::sleep_for(100us);
//      x *= 3;
//    });
//
//    //std::this_thread::sleep_for(3s);
//    //thread_pool.waitTasks();
//  }
//
//  for (auto x:v) {
//    assert(x == 3 && "Every vector element should be 3.");
//  }
//  std::cout << "\n";

  return 0;
}
