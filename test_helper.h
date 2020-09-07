#ifndef TP__TEST_HELPER_H_
#define TP__TEST_HELPER_H_

#include <chrono>
#include <functional>
#include <iostream>
#include <cassert>

class TestHelper {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;
  using Duration = Clock::duration;

  using WrappedTest = std::function<void()>;
  using PostCondition = std::function<void(const std::string&)>;
  using Reseter = std::function<void()>;

  template<typename DurationCastType = std::chrono::milliseconds>
  static void timedTest(const std::string& name,
                        const WrappedTest& test,
                        const PostCondition& post_condition,
                        const std::string& duration_suffix = "ms");

  template<typename DurationCastType = std::chrono::milliseconds>
  static void timedTestGroup(const std::vector<std::string>& names,
                             const std::vector<WrappedTest>& tests,
                             const Reseter& reseter,
                             const PostCondition& post_condition,
                             const std::string& duration_suffix = "ms");
};
template<typename DurationCastType>
void TestHelper::timedTest(const std::string& name,
                           const TestHelper::WrappedTest& test,
                           const TestHelper::PostCondition& post_condition,
                           const std::string& duration_suffix) {
  const auto start = Clock::now();
  test();
  const auto end = Clock::now();
  post_condition(name);
  std::cout << name << " : " << std::chrono::duration_cast<DurationCastType>(end - start).count() << duration_suffix << "\n";
}

template<typename DurationCastType>
void TestHelper::timedTestGroup(const std::vector<std::string>& names,
                                const std::vector<WrappedTest>& tests,
                                const Reseter& reseter,
                                const TestHelper::PostCondition& post_condition,
                                const std::string& duration_suffix) {
  assert(names.size() == tests.size() && "Can't test group with mismatching names and tests sizes.");

  for (auto i = 0; i < tests.size(); ++i) {
    timedTest(names[i], tests[i], post_condition, duration_suffix);
    reseter();
  }
}

#endif //TP__TEST_HELPER_H_
