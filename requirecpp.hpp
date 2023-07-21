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
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>
#include "closure_traits.hpp"
#include "decorators.hpp"

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

class Context final {
 private:
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
      callback(DepConverter<Deps>::convert(ctx->require<Deps>())...);
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
    std::scoped_lock lk{m_mutex};
    std::shared_ptr<TrackableObject<T>> copy = lookup_emplace<T>();
    copy->set(std::make_shared<T>(std::forward<Args>(args)...));
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
  static constexpr auto lookup_type() {
    if constexpr (is_specialization_of<std::shared_ptr,
                                       std::decay_t<T>>::value) {
      return std::decay_t<typename T::element_type>();
    } else if constexpr (std::is_pointer<std::decay_t<T>>()) {
      return std::decay_t<std::remove_pointer_t<std::decay_t<T>>>();
    } else {
      return std::decay_t<T>();
    }
  }
  template <typename T>
  using LookupType = std::invoke_result_t<decltype(lookup_type<T>)>;

  template <typename T>
  std::shared_ptr<LookupType<T>> require() {
    std::unique_lock lk{m_mutex};
    std::shared_ptr<TrackableObject<LookupType<T>>> copy =
        lookup_or_create<T>();
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

  // lookup, decays type, handles shared_ptr
  template <typename T>
  constexpr auto& get_objects() {
    return Context::s_objects<LookupType<T>>;
  }
  template <typename T>
  constexpr auto& get_objects() const {
    return Context::s_objects<LookupType<T>>;
  }
  template <typename T>
  bool exists() const {
    const auto& objects = get_objects<T>();
    const auto& iter = objects.find(this);
    return iter != end(objects) && iter->second != nullptr &&
           iter->second->get() != nullptr;
  }
  template <typename T>
  std::shared_ptr<TrackableObject<LookupType<T>>> lookup_or_create() {
    auto& objects = get_objects<T>();
    auto iter = objects.find(this);
    if (iter == end(objects)) {
      bool success;
      std::tie(iter, success) = objects.try_emplace(
          this, std::make_shared<Context::TrackableObject<LookupType<T>>>());
    }
    return iter->second;
  }
  template <typename T, typename... Args>
  std::shared_ptr<TrackableObject<LookupType<T>>> lookup_emplace(Args... args) {
    auto& objects = get_objects<T>();
    auto iter = objects.find(this);
    if (iter != end(objects)) {
      throw std::invalid_argument{"Object type already registered"};
    }
    auto p = std::make_shared<TrackableObject<LookupType<T>>>(
        std::make_shared<LookupType<T>>(std::forward<Args>(args)...));
    auto [obj, suc] = objects.emplace(this, p);
    return obj->second;
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
