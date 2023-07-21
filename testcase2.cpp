// #include <atomic>
// #include <iostream>
// #include <thread>
// #include "requirecpp.hpp"
// #include "testcases.h"

// using namespace std::chrono_literals;

//// Token for require context
// class TestCaseA {};

// class SlowComponent {
//   std::atomic<int> m_counter{0};

// public:
//  void slowOperation() {
//    std::this_thread::sleep_for(100ms);
//    ++m_counter;
//  }
//};

// class UseSlowComponent {
//   requirecpp::Context& m_deps;

// public:
//  UseSlowComponent(requirecpp::Context& ctx) : m_deps{ctx} {
//    m_deps.require([this](SlowComponent& comp) { comp.slowOperation(); });
//  }
//  void use() {
//    m_deps.require([](SlowComponent& comp) { comp.slowOperation(); });
//  }
//};

// class RegisterSlowComponent {
//   requirecpp::Context m_deps;

// public:
//  RegisterSlowComponent() { m_deps.emplace<SlowComponent>(); }
//};

// class CreatorObject {
//   requirecpp::Context m_deps;

// public:
//  CreatorObject() {
//    std::this_thread::sleep_for(10ms);
//    m_deps.emplace<RegisterSlowComponent>();
//    m_deps.emplace<UseSlowComponent>();
//    std::cout << "Dest" << std::endl;
//  }
//};

// class TestThreadType {};
// void testcase2() {
//   requirecpp::Context dependencies;
//   std::thread t{[&dependencies]() {
//     try {
//       // sync, blocking version
//       auto user = dependencies.require<UseSlowComponent>();
//       user->use();
//       std::cout << "cleanup TestcaseA" << std::endl;
//     } catch (const std::exception& e) {
//       std::cout << "Exception in Thread: " << e.what() << std::endl
//                 << std::endl;
//     }
//   }};
//   try {
//     // require thread to be started
//     CreatorObject c{};
//     auto thread = dependencies.require<TestThreadType>();
//     // thread.unlock();
//     //  require thread finish all require() calls
//     std::cout << "require finish" << std::endl;
//     dependencies.require<TestThreadType>();  //< fails if not all require
//                                              // calls have been made,
//     std::cout << "require Finished" << std::endl;
//   } catch (const std::exception& e) {
//     std::cout << "Exception: " << e.what() << std::endl << std::endl;
//   }
//   if (t.joinable()) {
//     std::cout << "Join ";
//     t.join();
//   }
//   std::cout << "Fin" << std::endl;
// }
