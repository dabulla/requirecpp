#pragma once

#include <assert.h>
#include <stdint.h>
#include "requirecpp/details/callback.hpp"
#include "requirecpp/details/trackable_object.hpp"
#include "requirecpp/requirecpp.hpp"

namespace requirecpp {

Context::~Context() {
  for (auto dtor : m_destructors) {
    dtor();
  }
}

template <typename T, typename... Args>
std::shared_ptr<T> Context::emplace(Args&&... args) {
  std::scoped_lock lk{m_mutex};
  // todo prevent/handle overwrite
  auto p = std::make_shared<T>(std::forward<Args>(args)...);
  lookup_set_create<LookupType<T>>(p);
  check_pending();
  return p;
}

template <typename T>
void Context::push(const std::shared_ptr<T>& p) {
  std::scoped_lock lk{m_mutex};
  // todo prevent/handle overwrite
  lookup_set_create<LookupType<T>>(p);
  check_pending();
}

template <typename Fn>
void Context::require(Fn&& callback, const std::string& name) {
  details::Callback cb{callback, name};
  std::scoped_lock lk{m_mutex};
  if (cb.satisfied(this)) {
    cb.call(this);
  } else {
    // std::cout << "add pending: " << cb.declaration(this) << std::endl;
    m_pending.emplace_back(std::move(cb));
  }
}

std::vector<std::string> Context::list_pending(bool deps) const {
  std::vector<std::string> ret;
  std::scoped_lock lk{m_mutex};
  for (const auto& cb : m_pending) {
    if (deps) {
      ret.emplace_back(cb.declaration(this));
    } else {
      ret.emplace_back(cb.get_name());
    }
  }
  return ret;
}
void Context::print_pending(bool deps) const {
  std::scoped_lock lk{m_mutex};
  if (m_pending.empty())
    std::cout << "No pending requirements." << std::endl;
  else {
    auto v = list_pending(deps);
    for (const auto& str : v) {
      std::cout << str << std::endl;
    }
  }
}

template <typename T>
std::shared_ptr<details::TrackableObject<LookupType<T>>> Context::get() {
  std::unique_lock lk{m_mutex};
  return lookup_set_create<LookupType<T>>();
}

template <typename T>
std::shared_ptr<LookupType<T>> Context::remove() {
  std::unique_lock lk{m_mutex};
  auto object = lookup_remove<T>();
  lk.unlock();
  return object;
}

void Context::check_pending() {
  std::deque<details::Callback> cbs;
  std::erase_if(m_pending, [&](auto& cb) {
    bool satisfied = cb.satisfied(this);
    if (satisfied) {
      cbs.emplace_back(std::move(cb));
    }
    return satisfied;
  });
  for (auto& cb : cbs) {
    // todo cbs can remove objects and may cannot execute. move back to
    // m_pending? recurse?
    cb.call(this);
  }
}

template <typename T>
bool Context::exists() const {
  std::shared_lock objects_lk{Context::s_objects_mutex<LookupType<T>>};
  const auto& objects = Context::s_objects<LookupType<T>>;
  const auto& iter = objects.find(this);
  return iter != end(objects) && iter->second != nullptr &&
         iter->second->has_value();
}

template <typename T>
std::shared_ptr<LookupType<T>> Context::lookup_remove() {
  static_assert(std::is_same<LookupType<T>, T>::value,
                "emplace type must be same as lookup type");
  std::scoped_lock objects_lk{Context::s_objects_mutex<LookupType<T>>};
  auto& objects = Context::s_objects<LookupType<T>>;
  auto iter = objects.find(this);
  if (iter != end(objects)) {
    auto trackable = iter->second;
    auto p = trackable->optional();
    trackable->fail();
    objects.erase(iter);
    return p;
  }
  return nullptr;
}

// retrieve, set or create empty trackable object
template <typename T>
std::shared_ptr<details::TrackableObject<LookupType<T>>>
Context::lookup_set_create(std::shared_ptr<T> obj_ptr) {
  static_assert(std::is_same<LookupType<T>, T>::value,
                "emplace type must be same as lookup type");
  std::scoped_lock objects_lk{Context::s_objects_mutex<T>};
  auto& objects = Context::s_objects<LookupType<T>>;
  auto iter = objects.find(this);
  if (iter == end(objects)) {
    bool success;
    std::tie(iter, success) = objects.try_emplace(
        this, std::make_shared<details::TrackableObject<T>>(obj_ptr));
    m_destructors.emplace_back([this] { remove<T>(); });
  } else if (obj_ptr != nullptr) {
    iter->second->set(obj_ptr);
  }
  return iter->second;
}

template <typename T>
std::unordered_map<const Context*, std::shared_ptr<details::TrackableObject<T>>>
    Context::s_objects;

template <typename T>
std::shared_mutex Context::s_objects_mutex;
}  // namespace requirecpp
