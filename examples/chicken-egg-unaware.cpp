#include <iostream>
#include <thread>
#include "requirecpp/requirecpp.hpp"

using namespace requirecpp;
using namespace std::chrono_literals;

// example with objects unaware of requirecpp
// users codebase of valuable components is not infected with requirecpp

class Egg;

class Chicken {
 public:
  Chicken(const std::string& name) : m_name{name} {}
  void hatched_from(const Egg& egg) const;
  std::string get_name() const { return m_name; }

 private:
  std::string m_name;
};

class Egg {
 public:
  Egg(const std::string& label) : m_label{label} {}
  void laid_by(const Chicken& chicken) const;
  std::string get_label() const { return m_label; }

 private:
  std::string m_label;
};

void Chicken::hatched_from(const Egg& egg) const {
  std::cout << get_name() << " hatched from " << egg.get_label() << std::endl;
}

void Egg::laid_by(const Chicken& chicken) const {
  std::cout << get_label() << " laid by " << chicken.get_name() << std::endl;
}

int main() {
  try {
    requirecpp::Context ctx;
    ctx.require([](const Chicken& chicken,
                   const Egg& egg) { chicken.hatched_from(egg); },
                "hatched_from");
    ctx.require(
        [](const Chicken& chicken, const Egg& egg) { egg.laid_by(chicken); },
        "laid_by");
    ctx.emplace<Chicken>("Chuck");
    ctx.emplace<Egg>("Egg3000");

    return 0;
  } catch (const std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl << std::endl;
  }
}
