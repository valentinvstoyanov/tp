cmake_minimum_required(VERSION 3.15)
project(tp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-pthread -fopenmp")

add_executable(tp main.cpp thread_pool.h destruction_policy.h worker.h stealing_queue.h profiler.h profiled_mutex.h)