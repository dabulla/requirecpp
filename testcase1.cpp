#include <atomic>
#include <iostream>
#include <thread>
#include "requirecpp.hpp"
#include "testcases.h"

using namespace std::chrono_literals;

class Printer {
 public:
  void print(const std::string& text) const {
    std::cout << "Printer prints: " << text << std::endl;
  }
};

class HelloWorld {
 public:
  std::string get_text() const { return "Hello World!"; }
};

void testcase1() {
  requirecpp::Context context;

  context.require(
      [](std::shared_ptr<Printer> printer,
         std::shared_ptr<HelloWorld> helloWorld) {
        std::cout << "Call: shared_ptr: ";
        printer->print(helloWorld->get_text());
      },
      "fn_shared_ptr");
  context.require(
      [](const std::shared_ptr<Printer> printer,
         const std::shared_ptr<HelloWorld> helloWorld) {
        std::cout << "Call: const shared_ptr: ";
        printer->print(helloWorld->get_text());
      },
      "fn_const_shared_ptr");
  context.require(
      [](std::shared_ptr<Printer>& printer,
         std::shared_ptr<HelloWorld>& helloWorld) {
        std::cout << "Call: shared_ptr&: ";
        printer->print(helloWorld->get_text());
      },
      "fn_shared_ptr_ref");
  context.require(
      [](const std::shared_ptr<Printer>& printer,
         const std::shared_ptr<HelloWorld>& helloWorld) {
        std::cout << "Call: const shared_ptr&: ";
        printer->print(helloWorld->get_text());
      },
      "fn_const_shared_ptr_ref");
  context.require(
      [](Printer* printer, HelloWorld* helloWorld) {
        std::cout << "Call: *: ";
        printer->print(helloWorld->get_text());
      },
      "fn_ptr");
  context.require(
      [](const Printer* printer, const HelloWorld* helloWorld) {
        std::cout << "Call: const *: ";
        printer->print(helloWorld->get_text());
      },
      "fn_const_ptr");
  context.require(
      [](Printer& printer, HelloWorld& helloWorld) {
        std::cout << "Call: &: ";
        printer.print(helloWorld.get_text());
      },
      "fn_ref");
  context.require(
      [](const Printer& printer, const HelloWorld& helloWorld) {
        std::cout << "Call: const &: ";
        printer.print(helloWorld.get_text());
      },
      "fn_const_ref");

  const auto list_pening = [&] {
    for (const auto& str : context.list_pending()) {
      std::cout << str << std::endl;
    }
  };

  // list_pening();
  context.emplace<Printer>();
  context.emplace<HelloWorld>();
  // list_pening();
}
