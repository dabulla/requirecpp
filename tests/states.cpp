#include <iostream>
#include <thread>
#include "requirecpp/decorators.hpp"
#include "requirecpp/requirecpp.hpp"

using namespace requirecpp;
using namespace std::chrono_literals;

// example with objects aware of requirecpp

class Egg;

class Chicken {
 public:
  Chicken(requirecpp::Context& ctx, const std::string& name) : m_name{name} {
    ctx.require(
        [this, &ctx](const Egg& egg) {
          hatched_from(egg);
          ctx.emplace<decorator::Tagged<Chicken, 0>>();
        },
        "hatched_from");
  }
  void hatched_from(const Egg& egg) const;
  std::string get_name() const { return m_name; }

 private:
  std::string m_name;
};

class Egg {
 public:
  Egg(requirecpp::Context& ctx, const std::string& label) : m_label{label} {
    ctx.require([this](const Chicken& chicken) { laid_by(chicken); },
                "laid_by");
  }
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
    auto chicken = ctx.emplace<Chicken>(ctx, "Chuck");
    auto egg = ctx.emplace<Egg>(ctx, "Egg3000");
    ctx.require(
        [](const decorator::Tagged<Chicken, 2>& o) {
          std::cout << "chicken is two years old " << o.object->get_name()
                    << std::endl;
        },
        "state 2");
    ctx.require(
        [](const decorator::Tagged<Chicken, 0>&) {
          std::cout << "chicken was born" << std::endl;
        },
        "state 0");
    ctx.emplace<decorator::Tagged<Chicken, 1>>(chicken);
    ctx.require(
        [](const decorator::Tagged<Chicken, 1>&) {
          std::cout << "chicken is one year old" << std::endl;
        },
        "state 1");
    ctx.emplace<decorator::Tagged<Chicken, 2>>(chicken);
    ctx.emplace<decorator::Tagged<Chicken, 3>>(chicken);
    ctx.require(
        [](const decorator::Tagged<Chicken, 3>&) {
          std::cout << "chicken is three years old" << std::endl;
        },
        "state 3");
    return 0;
  } catch (const std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl << std::endl;
  }
}
