#pragma once

#include <assert.h>
#include <stdint.h>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_set>
#include <utility>
#include <vector>
#include "closure_traits.hpp"
#include "decorators.hpp"

// todo: simplify by replace some mutexes by synchronized_value, semaphore

// Rules for DependencyReactor: In the destructor, own pending require calls
// will throw. Destructor will wait for ComponentReferences to unlock

namespace requirecpp {

template <class F>
class finally {
 public:
  explicit finally(F&& action) noexcept : m_action(std::forward<F>(action)) {}
  finally(const finally&) = delete;
  finally(finally&&) = delete;
  finally& operator=(const finally&) = delete;
  finally& operator=(finally&&) = delete;
  ~finally() { m_action(); }

 private:
  F m_action;
};

// template <typename T>
// class UsageGuard {
//   T& m_t;
//   std::shared_lock<std::shared_mutex> m_lock;

// public:
//  using type = T;
//  operator T() { return m_t; }
//  operator T() const { return m_t; }
//  T* operator->() { return &m_t; }
//  T* operator->() const { return &m_t; }
//  T& operator*() { return m_t; }
//  T& operator*() const { return m_t; }
//  UsageGuard(T& t, std::shared_lock<std::shared_mutex>&& lock)
//      : m_t{t}, m_lock{std::move(lock)} {}
//  ~UsageGuard() {}
//  UsageGuard(const UsageGuard& other) = delete;             // no copy
//  UsageGuard& operator=(const UsageGuard& other) = delete;  // no copy assign
//  UsageGuard(UsageGuard&& other) noexcept
//      : m_t{other.m_t}, m_lock{std::move(other.m_lock)} {}
//  UsageGuard& operator=(UsageGuard&& other) noexcept {
//    if (this != &other) {
//      m_t = std::move(other.m_t);
//      m_lock = std::move(other.m_lock);
//    }
//    return *this;
//  }
//};

class Context final {
 private:
  //    template<template <typename ...Tail> typename tuple, typename ...Tail>
  //    struct Satisfied
  //    {
  //        static bool satisfied(const Context* ctx) { return true; }
  //    };
  template <typename... Tail>
  struct Satisfied {
    static bool satisfied(const Context* ctx) { return true; }
  };

  template <typename T, typename... Tail>
  struct Satisfied<T, Tail...> {
    static bool satisfied(const Context* ctx) {
      return ctx->exists<T>() && Satisfied<Tail...>::satisfied(ctx);
    }
  };

  //  template <typename T>
  //  struct Get {
  //    static std::shared_ptr<T> get(const Context* ctx) {
  //      const auto& objects = Context::s_objects<typename std::decay_t<T>>;
  //      auto iter = objects.find(ctx);
  //      if (iter == end(objects)) {
  //        throw std::logic_error{
  //            "Tried to access object that is not yet available"};
  //      }
  //      return iter->get();
  //    }
  //  };
  //    template <typename T, auto ...state>
  //    struct Get<decorator::StateDecoration<T, state...>>
  //    {
  //        // just throw if component does not exist
  //        static UsageGuard<T>& get()
  //        {
  //            T* &comp = ContextAssociated<Context>::template components<T>;
  //            if(nullptr == comp) throw std::runtime_error{"Component
  //            Unknown"}; return *comp;
  //        }
  //    };

  template <typename T>
  struct LookupDeducer {
    using type = std::decay_t<T>;
  };
  template <typename T>
  struct LookupDeducer<std::shared_ptr<T>> {
    using type = std::decay_t<T>;
  };
  template <typename T>
  struct LookupDeducer<T*> {
    using type = std::decay_t<T>;
  };

  template <typename T>
  struct DepConverter {
    static T convert(std::shared_ptr<std::remove_reference_t<T>>&& from) {
      return *from;
    }
  };
  template <typename T>
  struct DepConverter<std::shared_ptr<T>> {
    static std::shared_ptr<T> convert(
        std::shared_ptr<std::remove_reference_t<T>>&& from) {
      return from;
    }
  };
  template <typename T>
  struct DepConverter<T*> {
    static T* convert(std::shared_ptr<std::remove_reference_t<T>>&& from) {
      return from.get();
    }
  };
  template <typename... Deps>
  struct CallHelper {
    template <typename Callback>
    static void invoke(Context* ctx, Callback&& callback) {
      callback(DepConverter<Deps>::convert(
          ctx->require<typename LookupDeducer<Deps>::type>())...);
    }
  };
  template <typename... Deps>
  struct DebugInfo {
    static std::unordered_map<std::string, bool> list(const Context* ctx) {
      return {};
    }
  };
  template <typename Dep, typename... Deps>
  struct DebugInfo<Dep, Deps...> {
    static std::unordered_map<std::string, bool> list(const Context* ctx) {
      auto set1 = DebugInfo<Dep>::list(ctx);
      const auto set2 = DebugInfo<Deps...>::list(ctx);
      set1.insert(set2.cbegin(), set2.cend());
      return set1;
    }
  };

  class RequirementCallback {
   public:
    template <typename Callback>
    RequirementCallback(Callback&& callback) {
      m_satisfied = [](const Context* ctx) -> bool {
        return unpack_tuple_to<typename closure_traits<Callback>::arguments>::
            template type<Satisfied>::satisfied(ctx);
      };
      m_callback = [callback](Context* ctx) mutable -> void {
        unpack_tuple_to<typename closure_traits<Callback>::arguments>::
            template type<CallHelper>::invoke(ctx, callback);
      };
      m_debug =
          [](const Context* ctx) -> std::unordered_map<std::string, bool> {
        return unpack_tuple_to<typename closure_traits<Callback>::arguments>::
            template type<DebugInfo>::list(ctx);
      };
    }

    bool satisfied(const Context* ctx) { return m_satisfied(ctx); }
    void call(Context* ctx) { return m_callback(ctx); }
    std::unordered_map<std::string, bool> listDependecies(const Context* ctx) {
      return m_debug(ctx);
    }

   private:
    std::function<bool(const Context*)> m_satisfied;
    std::function<void(Context*)> m_callback;
    std::function<std::unordered_map<std::string, bool>(const Context*)>
        m_debug;
  };

 public:
  Context() = default;
  Context(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;
  ~Context() = default;

  template <typename T, typename... Args>
  void emplace(Args&&... args) {
    using type = LookupDeducer<T>::type;
    std::scoped_lock lk{m_mutex};
    auto& objects = Context::s_objects<typename std::decay_t<type>>;
    auto iter = objects.find(this);
    if (iter == end(objects)) {
      bool success;
      std::tie(iter, success) = objects.try_emplace(
          this, std::make_shared<Context::TrackableObject<T>>(
                    std::make_shared<T>(std::forward<Args>(args)...)));
    }
    iter->second->set(std::make_shared<T>(std::forward<Args>(args)...));
    m_pending.erase(
        std::remove_if(begin(m_pending), end(m_pending), [&](auto& cb) {
          bool satisfied = cb.satisfied(this);
          if (satisfied) {
            std::cout << "Calling dep" << m_pending.size() << std::endl;
            cb.call(this);
          }
          return satisfied;
        }));
  }

  template <typename T, typename Fn>
  void provide(Fn&& callback) {}

  // does not manage the objects lifetime
  template <typename T>
  void publish(const T& obj);

  //    template<typename ...Args>
  //    void push(const Args&& ...args);

  //    template<typename T, typename ...Args>
  //    void emplace(const Args&& ...args);

  template <typename T>
  std::shared_ptr<T> require() {
    std::unique_lock lk{m_mutex};
    std::shared_ptr<TrackableObject<T>> copy = lookup_or_create<T>();
    lk.unlock();
    return copy->get();
  }

  template <typename Callback>
  void require(Callback&& callback) {
    std::cout << "req callback " << m_pending.size() << std::endl;
    RequirementCallback cb{callback};
    std::scoped_lock lk{m_mutex};
    if (cb.satisfied(this)) {
      std::cout << "req satisfied " << m_pending.size() << std::endl;
      cb.call(this);
    } else {
      m_pending.emplace_back(std::move(cb));
    }
  }

 private:
  // if there are pending get() calls, make sure the object is not destroyed,
  // call fail() first and ensure get() requests returned.
  template <typename T>
  class TrackableObject {
   public:
    TrackableObject(std::shared_ptr<T>&& obj = nullptr)
        : m_object{std::move(obj)} {}
    TrackableObject(const TrackableObject&) = delete;
    TrackableObject(TrackableObject&&) = delete;
    TrackableObject& operator=(const TrackableObject&) = delete;
    TrackableObject& operator=(TrackableObject&&) = delete;
    ~TrackableObject() { assert(m_pending == 0); }

    void set(const std::shared_ptr<T>& obj) {
      std::unique_lock lk{m_mutex};
      m_object = obj;
      lk.unlock();
      m_cv.notify_all();
    }
    // blocking
    std::shared_ptr<T> get() {
      std::unique_lock lk{m_mutex};
      if (!m_shutdown && m_object) {
        return m_object;
      } else {
        m_pending++;
        finally final_action{[&] { m_pending--; }};
        m_cv.wait(lk, [&] { return m_shutdown || m_object != nullptr; });
        if (m_shutdown)
          throw std::runtime_error{"Could not get object"};
      }
    }

    void fail() {
      {
        std::scoped_lock lk{m_mutex};
        m_shutdown = true;
      }
      m_cv.notify_all();
    }

   private:
    uint32_t m_pending{0};

    std::shared_ptr<T> m_object;
    bool m_shutdown{false};
    std::mutex m_mutex;
    std::condition_variable m_cv;
  };
  std::recursive_mutex m_mutex;

  template <typename T>
  using LookupType = std::conditional<
      is_specialization_of<std::shared_ptr, std::decay_t<T>>::type,
      typename T::element_type,
      std::decay_t<T>>;
  // lookup, decays type, handles shared_ptr
  template <typename T>
  constexpr auto& get_objects() {
    return Context::s_objects<LookupType<T>>;
  }
  template <typename T>
  bool exists() {
    const auto& objects = get_objects<T>();
    const auto& iter = objects.find(this);
    return iter != end(objects) && iter->second != nullptr &&
           iter->second->get() != nullptr;
  }
  template <typename T>
  std::shared_ptr<TrackableObject<T>> lookup_or_create() {
    const auto& objects = get_objects<T>();
    auto iter = objects.find(this);
    if (iter == end(objects)) {
      bool success;
      std::tie(iter, success) = objects.try_emplace(
          this, std::make_shared<Context::TrackableObject<LookupType<T>>>());
    }
    return iter->second;
  }
  template <typename T>
  static std::unordered_map<const Context*, std::shared_ptr<TrackableObject<T>>>
      s_objects;

  std::vector<RequirementCallback> m_pending;
};
template <typename T>
std::unordered_map<const Context*, std::shared_ptr<Context::TrackableObject<T>>>
    Context::s_objects;
}  // namespace requirecpp
