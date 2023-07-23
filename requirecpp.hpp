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

class Context final {
 private:
  class Callback {
   public:
    Callback(const Callback&) = delete;
    Callback(Callback&&) = default;
    Callback& operator=(const Callback&) = delete;
    Callback& operator=(Callback&&) = default;
    ~Callback() = default;
    template <typename Fn>
    Callback(Fn&& callback, const std::string& name)
        : m_called{std::make_unique<std::atomic_flag>()}, m_name{name} {
      m_satisfied = [](const Context* ctx) -> bool {
        return closure_traits<Fn>::template unpack_arguments_to<
            Satisfied>::satisfied(ctx);
      };
      m_callback = [cb = std::forward<Fn>(callback)](Context* ctx) -> void {
        closure_traits<Fn>::template unpack_arguments_to<CallHelper>::invoke(
            ctx, cb);
      };
      m_debug =
          [](const Context* ctx) -> std::unordered_map<std::string, bool> {
        return closure_traits<Fn>::template unpack_arguments_to<
            DebugInfo>::list(ctx);
      };
    }

    bool satisfied(const Context* ctx) { return m_satisfied(ctx); }
    void call(Context* ctx) {
      if (!m_called->test_and_set())
        m_callback(ctx);
    }
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
    std::unique_ptr<std::atomic_flag> m_called;
    std::function<bool(const Context*)> m_satisfied;
    std::function<void(Context*)> m_callback;
    std::function<std::unordered_map<std::string, bool>(const Context*)>
        m_debug;
    std::string m_name;

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
  };

 public:
  Context() = default;
  Context(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;
  ~Context() = default;

  template <typename T, typename... Args>
  std::shared_ptr<T> emplace(Args&&... args) {
    std::scoped_lock lk{m_mutex};
    // todo prevent/handle overwrite
    auto p = lookup_emplace<T>(std::forward<Args>(args)...);
    check_pending();
    return p;
  }
  template <typename T>
  void push(std::shared_ptr<T>&& p) {
    std::scoped_lock lk{m_mutex};
    // todo prevent/handle overwrite
    lookup_push(p);
    check_pending();
  }

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
    return copy->blocking_get();
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

  template <typename Fn>
  void require(Fn&& callback, const std::string& name = "unnamed") {
    Callback cb{callback, name};
    std::scoped_lock lk{m_mutex};
    if (cb.satisfied(this)) {
      cb.call(this);
    } else {
      // std::cout << "add pending: " << cb.declaration(this) << std::endl;
      m_pending.emplace_back(std::move(cb));
    }
  }

  std::vector<std::string> list_pending(bool deps = true) const {
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
  void print_pending(bool deps = true) const {
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
    ~TrackableObject() = default;

    void set(const std::shared_ptr<T>& obj) {
      std::unique_lock lk{m_mutex};
      m_object = obj;
      lk.unlock();
      m_cv.notify_all();
    }
    // blocking
    std::shared_ptr<T> blocking_get() {
      std::unique_lock lk{m_mutex};
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

    bool has_value() const { return m_object != nullptr; }

   private:
    std::shared_ptr<T> m_object;
    bool m_shutdown{false};
    std::mutex m_mutex;
    std::condition_variable m_cv;
  };
  mutable std::recursive_mutex m_mutex;

  void check_pending() {
    std::deque<Callback> cbs;
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
    std::shared_lock objects_lk{Context::s_objects_mutex<LookupType<T>>};
    const auto& objects = get_objects<T>();
    const auto& iter = objects.find(this);
    return iter != end(objects) && iter->second != nullptr &&
           iter->second->has_value();
  }
  template <typename T>
  std::shared_ptr<TrackableObject<LookupType<T>>> lookup_or_create() {
    std::scoped_lock objects_lk{Context::s_objects_mutex<LookupType<T>>};
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
  std::shared_ptr<T> lookup_emplace(Args&&... args) {
    static_assert(std::is_same<LookupType<T>, T>::value,
                  "emplace type must be same as lookup type");
    std::scoped_lock objects_lk{Context::s_objects_mutex<T>};
    auto& objects = get_objects<T>();
    auto iter = objects.find(this);
    auto obj_ptr = std::make_shared<T>(std::forward<Args>(args)...);
    if (iter == end(objects)) {
      objects.try_emplace(this, std::make_shared<TrackableObject<T>>(obj_ptr));
    } else {
      iter->second->set(obj_ptr);
    }
    return obj_ptr;
  }
  template <typename T>
  void lookup_push(std::shared_ptr<T> obj_ptr) {
    static_assert(std::is_same<LookupType<T>, T>::value,
                  "emplace type must be same as lookup type");
    std::scoped_lock objects_lk{Context::s_objects_mutex<T>};
    auto& objects = get_objects<T>();
    auto iter = objects.find(this);
    if (iter == end(objects)) {
      objects.try_emplace(this, std::make_shared<TrackableObject<T>>(obj_ptr));
    } else {
      iter->second->set(obj_ptr);
    }
  }
  template <typename T>
  static std::unordered_map<const Context*, std::shared_ptr<TrackableObject<T>>>
      s_objects;
  template <typename T>
  static std::shared_mutex s_objects_mutex;

  std::deque<Callback> m_pending;
};
template <typename T>
std::unordered_map<const Context*, std::shared_ptr<Context::TrackableObject<T>>>
    Context::s_objects;

template <typename T>
std::shared_mutex Context::s_objects_mutex;
}  // namespace requirecpp
