cmake_minimum_required(VERSION 3.15)
project(tp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS -pthread)

add_executable(tp main.cpp join_threads.h thread_pool.h destruction_policy.h)