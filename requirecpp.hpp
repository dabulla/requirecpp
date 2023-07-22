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
#include <sstream>
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

  template <typename... Deps>
  struct CallHelper {
    template <typename Callback>
    static void invoke(Context* ctx, Callback&& callback) {
      callback(convert_dep<Deps>(ctx->require<Deps>())...);
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
      bool satisfied = Satisfied<Dep>::satisfied(ctx);
      auto pair = std::make_pair(
          type_pretty<Dep>() + " -> " + type_pretty<LookupType<Dep>>(),
          satisfied);
      auto set = DebugInfo<Deps...>::list(ctx);
      set.emplace(pair);
      return set;
    }
  };

  class RequirementCallback {
   public:
    template <typename Callback>
    RequirementCallback(Callback&& callback, const std::string& name)
        : m_name{name} {
      auto tmp = [name](const Context* ctx) {
        auto list = closure_traits<Callback>::template unpack_arguments_to<
            DebugInfo>::list(ctx);
        for (const auto& [dep, satisfied] : list) {
          std::cout << name << "{" << dep << ": "
                    << (satisfied ? "    " : "not ") << "satisfied}"
                    << std::endl;
        }
      };
      m_satisfied = [tmp](const Context* ctx) -> bool {
        // tmp(ctx);
        return closure_traits<Callback>::template unpack_arguments_to<
            Satisfied>::satisfied(ctx);
      };
      m_callback = [callback, tmp](Context* ctx) -> void {
        // tmp(ctx);
        closure_traits<Callback>::template unpack_arguments_to<
            CallHelper>::invoke(ctx, callback);
      };
      m_debug =
          [](const Context* ctx) -> std::unordered_map<std::string, bool> {
        return closure_traits<Callback>::template unpack_arguments_to<
            DebugInfo>::list(ctx);
      };
    }

    bool satisfied(const Context* ctx) { return m_satisfied(ctx); }
    void call(Context* ctx) { return m_callback(ctx); }
    std::unordered_map<std::string, bool> list_dependecies(
        const Context* ctx) const {
      return m_debug(ctx);
    }
    void print_dependencies(const Context* ctx) const {
      for (const auto& [name, satisfied] : list_dependecies(ctx)) {
        std::cout << "{" << name << ": " << (satisfied ? "    " : "not ")
                  << "satisfied}" << std::endl;
      }
    }

    const std::string& get_name() const { return m_name; }

    std::string declaration(const Context* ctx) const {
      std::stringstream ss;
      ss << get_name() << "(";
      bool first = true;
      for (const auto& [dep, satisfied] : list_dependecies(ctx)) {
        if (first)
          first = false;
        else
          ss << ", ";
        ss << dep << (satisfied ? " yes" : " no");
      }
      ss << ")";
      return ss.str();
    }

   private:
    std::function<bool(const Context*)> m_satisfied;
    std::function<void(Context*)> m_callback;
    std::function<std::unordered_map<std::string, bool>(const Context*)>
        m_debug;
    std::string m_name;
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
    // todo prevent/handle overwrite
    std::shared_ptr<TrackableObject<T>> copy =
        lookup_emplace<T>(std::forward<Args>(args)...);
    std::cout << "Size START: " << m_pending.size() << std::endl;
    m_pending.erase(std::remove_if(begin(m_pending), end(m_pending),
                                   [&](auto& cb) {
                                     bool satisfied = cb.satisfied(this);
                                     if (satisfied) {
                                       cb.call(this);
                                     }
                                     return satisfied;
                                   }),
                    end(m_pending));
    std::cout << "Size END: " << m_pending.size() << std::endl;
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
  struct DeclvalHelper {
    using type = T;
  };

  template <typename T>
  static constexpr auto lookup_type() {
    if constexpr (is_specialization_of<std::shared_ptr,
                                       std::decay_t<T>>::value) {
      return DeclvalHelper<
          std::decay_t<typename std::decay_t<T>::element_type>>();
    } else if constexpr (std::is_pointer<std::decay_t<T>>()) {
      return DeclvalHelper<
          std::decay_t<std::remove_pointer_t<std::decay_t<T>>>>();
    } else {
      return DeclvalHelper<std::decay_t<T>>();
    }
  }
  // template <typename T>
  // using LookupType = lookup_type<T>()::type;
  template <typename T>
  using LookupType =
      typename std::invoke_result_t<decltype(lookup_type<T>)>::type;

  template <typename T>
  std::shared_ptr<LookupType<T>> require() {
    std::unique_lock lk{m_mutex};
    std::shared_ptr<TrackableObject<LookupType<T>>> copy =
        lookup_or_create<T>();
    lk.unlock();
    return copy->get();
  }

  template <typename T>
  static constexpr T convert_dep(std::shared_ptr<LookupType<T>>&& sp) {
    if constexpr (is_specialization_of<std::shared_ptr,
                                       std::decay_t<T>>::value) {
      return sp;
    } else if constexpr (std::is_pointer<std::decay_t<T>>()) {
      return sp.get();
    } else {
      return *sp.get();
    }
  }

  template <typename Callback>
  void require(Callback&& callback, const std::string& name = "") {
    RequirementCallback cb{callback, name};
    std::scoped_lock lk{m_mutex};
    if (cb.satisfied(this)) {
      cb.call(this);
    } else {
      std::cout << "pending: " << cb.declaration(this) << std::endl;
      m_pending.emplace_back(std::move(cb));
    }
  }

  std::vector<std::string> list_pending(bool deps = true) const {
    std::vector<std::string> ret;
    for (const auto& cb : m_pending) {
      if (deps) {
        ret.emplace_back(cb.declaration(this));
      } else {
        ret.emplace_back(cb.get_name());
      }
    }
    return ret;
  }

 private:
  // if there are pending get() calls, make sure the object is not destroyed,
  // call fail() first and ensure get() requests returned.
  template <typename T>
  class TrackableObject {
   public:
    TrackableObject(std::shared_ptr<T> obj) : m_object{std::move(obj)} {}
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
      m_pending++;
      finally final_action{[&] { m_pending--; }};
      m_cv.wait(lk, [&] { return m_shutdown || m_object != nullptr; });
      if (m_shutdown)
        throw std::runtime_error{"Could not get object"};
      return m_object;
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
      auto p = std::make_shared<TrackableObject<LookupType<T>>>(nullptr);
      bool success;
      std::tie(iter, success) = objects.try_emplace(this, p);
    }
    return iter->second;
  }
  template <typename T, typename... Args>
  std::shared_ptr<TrackableObject<LookupType<T>>> lookup_emplace(
      Args&&... args) {
    auto& objects = get_objects<T>();
    auto iter = objects.find(this);
    auto obj_ptr = std::make_shared<LookupType<T>>(std::forward<Args>(args)...);
    if (iter == end(objects)) {
      auto p = std::make_shared<TrackableObject<LookupType<T>>>(obj_ptr);
      objects.try_emplace(this, p);
      return p;
    } else {
      iter->second->set(obj_ptr);
      return iter->second;
    }
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
