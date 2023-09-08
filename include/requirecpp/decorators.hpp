#pragma once

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>

namespace requirecpp::decorator {

template <typename T, auto... State>
struct Tagged {
  std::shared_ptr<T> object;
};

// TODO locking! object must not be in use!
template <typename T>
class Restartable {
 public:
  template <typename... Args>
  Restartable(Args&&... args)
      : m_object{std::make_unique<T>(std::forward<Args>(args)...)} {}
  // Restartable(std::unique_ptr<T> object) : m_object{std::move(object)} {}
  template <typename Fn>
  void on_start(Fn&& fn) {
    if (m_object) {
      fn();
    }
    m_funcs_start.emplace(std::forward<Fn>(fn));
  }
  template <typename Fn>
  void before_stop(Fn&& fn) {
    m_funcs_stop.emplace(std::forward<Fn>(fn));
  }
  T& get_object() { return m_object; }
  const T& get_object() const { return m_object; }

  void reset() {
    if (m_object) {
      std::ranges::for_each(m_funcs_stop, [](auto& fn) { fn(); });
    }
    m_object.reset();
  }

  template <typename... Args>
  void recreate(Args&&... args) {
    reset();
    m_object = std::make_unique<T>(std::forward<Args>(args)...);
    std::ranges::for_each(m_funcs_start, [](auto& fn) { fn(); });
  }

 private:
  std::unique_ptr<T> m_object;
  std::deque<std::function<void()>> m_funcs_start;
  std::deque<std::function<void()>> m_funcs_stop;
};
}  // namespace requirecpp::decorator
