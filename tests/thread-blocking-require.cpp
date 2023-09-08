#include <barrier>
#include <iostream>
#include <thread>
#include "requirecpp/requirecpp.hpp"

class Clazz1 {};
class Clazz2 {};
class Clazz3 {};

int main() {
  {
    requirecpp::Context context;
    std::jthread thread1{[&] {
      std::cout << "thread1: " << *context.get<std::string>()->require()
                << std::endl;
    }};
    context.emplace<std::string>("Hello");
  }
  {
    std::barrier barrier1{2};
    std::barrier barrier2{2};
    auto context = std::make_shared<requirecpp::Context>();
    std::jthread thread1{[&] {
      try {
        auto obj1 = context->get<Clazz1>()->require();
      } catch (const std::exception& e) {
        std::cout << "thread2: Requirement clazz1 unsatisfied: " << e.what()
                  << std::endl;
      }
      try {
        auto obj2 = context->get<Clazz2>();
        barrier1.arrive_and_wait();
        obj2->require();
      } catch (const std::exception& e) {
        std::cout << "thread2: Requirement clazz2 unsatisfied: " << e.what()
                  << std::endl;
      }
    }};
    context->get<Clazz1>()->fail();

    barrier1.arrive_and_wait();
    // it is safe to destruct context, while a requirement is blocking
    context.reset();
  }
}
