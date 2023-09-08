#include <atomic>
#include <iostream>
#include <thread>
#include "requirecpp/requirecpp.hpp"

using namespace std::chrono_literals;

class Clazz1 {};

class Clazz2 {};
class Clazz3 {};

int main() {
  std::weak_ptr<Clazz1> c1;
  std::weak_ptr<Clazz2> c2;
  {
    requirecpp::Context context;
    bool executed = false;
    context.require(
        [&](std::shared_ptr<Clazz1> clazz1, std::shared_ptr<Clazz2> clazz2) {
          executed = true;
          c1 = clazz1;
          c2 = clazz2;
        },
        "test");
    context.emplace<Clazz1>();
    context.remove<Clazz1>();
    context.emplace<Clazz2>();
    assert(!executed);
    context.emplace<Clazz1>();
    assert(executed);
    assert(!c1.expired());
    assert(!c2.expired());
    context.require(
        [&](Clazz1* clazz1, Clazz2* clazz2, Clazz3*) { assert(false); },
        "test2");
  }
  assert(c1.expired());
  assert(c2.expired());
}
