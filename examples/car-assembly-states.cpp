#include <iostream>
#include <memory>
#include "requirecpp/decorators.hpp"
#include "requirecpp/requirecpp.hpp"

using requirecpp::decorator::Tagged;

// parts
class Motor {};

class Wheels {
 public:
  virtual std::string get_type() const = 0;
};

class SummerTires : public Wheels {
  std::string get_type() const override { return "Summer"; }
};

class WinterTires : public Wheels {
  std::string get_type() const override { return "Winter"; };
};

// interiour
class SteeringWheel {};
class Seats {};

struct Car {
  std::shared_ptr<Motor> motor;
  std::shared_ptr<Wheels> wheels;
  std::shared_ptr<SteeringWheel> steering_wheel;
  std::shared_ptr<Seats> seats;
  std::string color;
};

enum class CarState {
  ASSEMBLED,
  PAINTED,
  INTERIOR_ASSEMBLED,
  QUALITY_CONTROL_PASSED
};

class CarFactory {
 public:
  explicit CarFactory(const std::function<void(Car&)>& on_finished)
      : m_finished{on_finished} {
    m_ctx.require(
        [this](std::shared_ptr<Wheels> wheels, std::shared_ptr<Motor> motor) {
          m_car.motor = motor;
          m_car.wheels = wheels;
          m_ctx.emplace<Tagged<Car, CarState::ASSEMBLED>>();
        },
        "outside");
    m_ctx.require(
        [this](std::shared_ptr<SteeringWheel> sw,
               std::shared_ptr<Seats> seats) {
          m_car.steering_wheel = sw;
          m_car.seats = seats;
          m_ctx.emplace<Tagged<Car, CarState::INTERIOR_ASSEMBLED>>();
        },
        "interior");
    m_ctx.require([this](Tagged<Car, CarState::ASSEMBLED>&,
                         Tagged<Car, CarState::INTERIOR_ASSEMBLED>&,
                         Tagged<Car, CarState::PAINTED>&) { m_ready = true; },
                  "quality_control");
    m_ctx.require(
        [this](Tagged<Car, CarState::QUALITY_CONTROL_PASSED>&) {
          m_finished(m_car);
        },

        "finish");
  }

  void paint(std::string_view color) {
    std::cout << "paint " << color << std::endl;
    m_car.color = color;
    m_ctx.emplace<Tagged<Car, CarState::PAINTED>>();
  }

  void deliver_wheels(const std::shared_ptr<Wheels>& wheels) {
    std::cout << "add wheels" << std::endl;
    m_ctx.push(wheels);
  }
  void deliver_motor(const std::shared_ptr<Motor>& motor) {
    std::cout << "add motor" << std::endl;
    m_ctx.push(motor);
  }
  void install_steeringwheel() {
    std::cout << "add steeringwheel" << std::endl;
    m_ctx.emplace<SteeringWheel>();
  }
  void install_seats() {
    std::cout << "add seats" << std::endl;
    m_ctx.emplace<Seats>();
  }

  bool pull_car() {
    // make sure car has wheels while pulling
    auto wheels = m_ctx.get<Wheels>()->optional();
    if (wheels) {
      std::cout << "pulling car to different location" << std::endl;
    } else {
      std::cout << "car cannot be pulled, no wheels!" << std::endl;
    }
    return wheels != nullptr;
  }

  bool inspect() {
    std::cout << "inspection ";
    if (m_ready) {
      std::cout << "passed!" << std::endl;
      m_ctx.emplace<Tagged<Car, CarState::QUALITY_CONTROL_PASSED>>();
      return true;
    }
    std::cout << "failed!" << std::endl;
    return false;
  }

  void print_missing() { m_ctx.print_pending(); }

 private:
  std::function<void(Car&)> m_finished;
  requirecpp::Context m_ctx;
  Car m_car;
  bool m_ready{false};
};

int main() {
  CarFactory factory{[](Car& car) {
    std::cout << "Car finished! Tires: " << car.wheels->get_type()
              << ", Color: " << car.color << std::endl;
  }};

  factory.pull_car();
  factory.deliver_wheels(std::make_shared<SummerTires>());
  factory.pull_car();
  factory.deliver_motor(std::make_shared<Motor>());
  factory.install_seats();
  factory.paint("red");

  if (!factory.inspect()) {
    std::cout << "missing steps:" << std::endl;
    factory.print_missing();
  }
  factory.install_steeringwheel();
  factory.inspect();

  return 0;
}
