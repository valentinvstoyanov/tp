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

std::chrono::high_resolution_clock::duration forEachTestWithThreadPoolWithCreation(std::vector<int>& v,
                                                                                   const std::function<void(int&)>& f,
                                                                                   std::size_t thread_count = std::thread::hardware_concurrency()) {
  auto start = std::chrono::high_resolution_clock::now();
  {
    ThreadPool thread_pool(thread_count, DestructionPolicy::WAIT_ALL);
    thread_pool.forEach(v.begin(), v.end(), f);
  }
  auto end = std::chrono::high_resolution_clock::now();
  return end - start;
}

std::chrono::high_resolution_clock::duration forEachTestWithThreadPoolWithoutCreation(std::vector<int>& v,
                                                                                      const std::function<void(int&)>& f,
                                                                                      ThreadPool& thread_pool) {
  auto start = std::chrono::high_resolution_clock::now();
  thread_pool.forEach(v.begin(), v.end(), f);
  thread_pool.waitTasks();
  auto end = std::chrono::high_resolution_clock::now();
  return end - start;
}

std::chrono::high_resolution_clock::duration forEachTestWithOpenMP(std::size_t thread_count,
                                                                   std::vector<int>& v,
                                                                   const std::function<void(int&)>& f) {
  auto start = std::chrono::high_resolution_clock::now();
#pragma omp parallel for num_threads(thread_count)
  for (auto it = v.begin(); it != v.end(); ++it) {
    f(*it);
  }
  auto end = std::chrono::high_resolution_clock::now();
  return end - start;
}

void forEachTest(std::size_t thread_count = std::thread::hardware_concurrency()) {
  constexpr size_t vector_size = 1000000;
  constexpr auto sleep_duration = 10us;
  constexpr int val = 1;
  constexpr int mult = 3;
  constexpr int prod = val * mult;
  using duration_cast_type = std::chrono::seconds;

  std::vector<int> v(vector_size, val);
  auto f = [mult, &sleep_duration](int& x) {
    std::this_thread::sleep_for(sleep_duration);
    x *= mult;
  };

//  const auto seq_d = std::chrono::duration_cast<duration_cast_type>(forEachTestWithoutThreadPool(v, f)).count();
//  std::cout << "forEach without thread pool took : " << seq_d << "\n";
//  std::for_each(v.begin(), v.end(), [](int x) { assert(x == prod && "forEachTest without thread pool assertion failed."); });
//  std::fill(v.begin(), v.end(), val);
  std::cout << "Thread count: " << thread_count << "\n";

  const auto par_w_tp_d =
      std::chrono::duration_cast<duration_cast_type>(forEachTestWithThreadPoolWithCreation(v, f, thread_count)).count();
  std::cout << "forEach with thread pool with creation and destruction took : " << par_w_tp_d << "\n";
  std::for_each(v.begin(),
                v.end(),
                [](int x) {
                  assert(x == prod && "forEach with thread pool with creation and destruction assertion failed.");
                });
  std::fill(v.begin(), v.end(), val);

  ThreadPool thread_pool(thread_count);
  const auto par_wo_tp_d = std::chrono::duration_cast<duration_cast_type>(forEachTestWithThreadPoolWithoutCreation(v,
                                                                                                                   f,
                                                                                                                   thread_pool)).count();
  std::cout << "forEach with thread pool without creation and destruction took : " << par_wo_tp_d << "\n";
  std::for_each(v.begin(), v.end(), [](int x) { assert(x == prod && "forEach with thread pool without creation and destruction assertion failed."); });
  std::fill(v.begin(), v.end(), val);

  const auto open_mp_d = std::chrono::duration_cast<duration_cast_type>(forEachTestWithOpenMP(thread_count, v, f)).count();
  std::cout << "forEach with OpenMP took : " << open_mp_d << "\n";
  std::for_each(v.begin(), v.end(),[](int x) { assert(x == prod && "forEach with OpenMP assertion failed.");});
}

void taskTest(std::size_t thread_count = std::thread::hardware_concurrency()) {
  constexpr auto tasks_count = 100000;
  constexpr auto sleep_duration = 10us;
  using duration_cast_type = std::chrono::milliseconds;

  auto f = [&sleep_duration] { std::this_thread::sleep_for(sleep_duration); };

  auto start = std::chrono::high_resolution_clock::now();
  {
    ThreadPool thread_pool(thread_count, DestructionPolicy::WAIT_ALL);
    for (auto i = 0; i < tasks_count; ++i) {
      thread_pool.add(f);
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "TP : " << std::chrono::duration_cast<duration_cast_type>(end - start).count() << "\n";


   start = std::chrono::high_resolution_clock::now();
#pragma omp parallel for num_threads(thread_count)
  for (auto i = 0; i < tasks_count; ++i) {
    f();
  }
  end = std::chrono::high_resolution_clock::now();
  std::cout << "OpenMP : " << std::chrono::duration_cast<duration_cast_type>(end - start).count() << "\n";
}

int main() {
  std::cout << "hardware_concurrency: " << std::thread::hardware_concurrency() << "\n";
  forEachTest(4);

  //auto profiler = std::make_shared<Profiler>();
  //ThreadPool thread_pool(profiler, 3, DestructionPolicy::WAIT_CURRENT);
//  ThreadPool thread_pool(3, DestructionPolicy::WAIT_CURRENT);
//  for (auto i = 0; i < 10; ++i) {
//    thread_pool.add([i] {
//      std::this_thread::sleep_for(std::chrono::seconds((i * i) / 10));
//      std::cout << i << '\n';
//    });
//  }
//  thread_pool.waitTasks();

  //std::cout << *profiler << std::endl;

//  std::vector<int> v(70000, 1);
//
//  {
//    ThreadPool thread_pool(5, DestructionPolicy::WAIT_ALL);
//
////    for (auto i = 0; i < 10; ++i) {
////      thread_pool.add([i] {
////        std::this_thread::sleep_for(std::chrono::seconds((i * i + 3) / 10));
////        std::cout << i << '\n';
////      });
////    }
//
//    thread_pool.forEach(v.begin(), v.end(), [](int& x) {
//      //std::this_thread::sleep_for(10ms);
//      x *= 3;
//    });
//
//    //std::this_thread::sleep_for(3s);
//    //thread_pool.waitTasks();
//  }
//
//  for (auto& x:v) {
//    std::cout << x << " ";
//    assert(x == 3 && "Every vector element should be 3.");
//  }
//  std::cout << "\n";

  return 0;
}
