#include <iostream>
#include <thread>
#include "requirecpp/requirecpp.hpp"

using namespace requirecpp;
using namespace std::chrono_literals;

// example with objects aware of requirecpp
// no wiring required outside of components, components manage themselfes

class Egg;

class Chicken {
 public:
  Chicken(requirecpp::Context& ctx, const std::string& name) : m_name{name} {
    ctx.require([this, &ctx](const Egg& egg) { hatched_from(egg); },
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
    return 0;
  } catch (const std::exception& e) {
    std::cout << "Exception: " << e.what() << std::endl << std::endl;
  }
}
