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
  std::string getText() const { return "Hello World!"; }
};

void testcase1() {
  requirecpp::Context dependencies;

  dependencies.require([](std::shared_ptr<Printer> printer,
                          std::shared_ptr<HelloWorld> helloWorld) {
    std::cout << "shared_ptr" << std::endl;
    printer->print(helloWorld->getText());
  });
  dependencies.require([](const std::shared_ptr<Printer> printer,
                          const std::shared_ptr<HelloWorld> helloWorld) {
    std::cout << "const shared_ptr" << std::endl;
    printer->print(helloWorld->getText());
  });
  //  dependencies.require([](std::shared_ptr<Printer>& printer,
  //                          std::shared_ptr<HelloWorld>& helloWorld) {
  //    std::cout << "shared_ptr&" << std::endl;
  //    printer->print(helloWorld->getText());
  //  });
  //  dependencies.require([](const std::shared_ptr<Printer>& printer,
  //                          const std::shared_ptr<HelloWorld>& helloWorld) {
  //    std::cout << "const shared_ptr&" << std::endl;
  //    printer->print(helloWorld->getText());
  //  });
  dependencies.require(
      [](const Printer* printer, const HelloWorld* helloWorld) {
        std::cout << "const *" << std::endl;
        printer->print(helloWorld->getText());
      });
  dependencies.require([](Printer* printer, HelloWorld* helloWorld) {
    std::cout << "*" << std::endl;
    printer->print(helloWorld->getText());
  });
  dependencies.require(
      [](const Printer& printer, const HelloWorld& helloWorld) {
        std::cout << "const &" << std::endl;
        printer.print(helloWorld.getText());
      });
  dependencies.require([](Printer& printer, HelloWorld& helloWorld) {
    std::cout << "&" << std::endl;
    printer.print(helloWorld.getText());
  });

  dependencies.emplace<Printer>();
  dependencies.emplace<HelloWorld>();
}
