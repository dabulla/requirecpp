#pragma once

#include <stdint.h>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include "requirecpp/details/type_lookup.hpp"

namespace requirecpp::details {
class Callback;
template <typename T>
class TrackableObject;
}  // namespace requirecpp::details

namespace requirecpp {

using details::LookupType;

class Context final {
 public:
  Context();
  Context(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;
  ~Context();

  template <typename T, typename... Args>
  std::shared_ptr<T> emplace(Args&&... args);
  template <typename T>
  void push(const std::shared_ptr<T>& p);

  template <typename Fn>
  void require(Fn&& callback, const std::string& name = "unnamed");

  template <typename T>
  std::shared_ptr<details::TrackableObject<LookupType<T>>> get();

  template <typename T>
  std::shared_ptr<LookupType<T>> remove();

  template <typename T>
  bool exists() const;

  std::vector<std::string> list_pending(bool deps = true) const;
  void print_pending(bool deps = true) const;

 private:
  struct details_callbacks;
  void check_pending();

  template <typename T>
  std::shared_ptr<LookupType<T>> lookup_remove();

  template <typename T>
  std::shared_ptr<details::TrackableObject<LookupType<T>>> lookup_set_create(
      std::shared_ptr<T> obj_ptr = nullptr);
  std::unique_ptr<details_callbacks> m_details;
  mutable std::recursive_mutex m_mutex;

  std::deque<std::function<void()>> m_destructors;

  template <typename T>
  static std::unordered_map<const Context*,
                            std::shared_ptr<details::TrackableObject<T>>>
      s_objects;
  template <typename T>
  static std::shared_mutex s_objects_mutex;

  friend class details::Callback;
};
}  // namespace requirecpp

#include "requirecpp/details/requirecpp.ipp"
