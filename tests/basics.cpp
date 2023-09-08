#include <iostream>
#include "requirecpp/requirecpp.hpp"

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
      [](const Printer& printer, const HelloWorld& helloWorld) {
        printer.print(helloWorld.get_text());
      },
      "print hello world");

  context.emplace<Printer>();
  context.emplace<HelloWorld>();
}
