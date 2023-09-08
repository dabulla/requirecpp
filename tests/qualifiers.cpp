#include <iostream>
#include "requirecpp/requirecpp.hpp"

using namespace std::chrono_literals;

class Printer {
 public:
  Printer() = default;
  Printer(const Printer&) = delete;
  Printer(Printer&&) = delete;
  Printer& operator=(const Printer&) = delete;
  Printer& operator=(Printer&&) = delete;
  void print(const std::string& text) const {
    std::cout << "Printer prints: " << text << std::endl;
  }
};

class HelloWorld {
 public:
  std::string get_text() const { return "Hello World!"; }
};

int main() {
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

  context.emplace<Printer>();
  context.emplace<HelloWorld>();
}
