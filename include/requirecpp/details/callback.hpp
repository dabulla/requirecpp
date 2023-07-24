#pragma once
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include "requirecpp/details/closure_traits.hpp"
#include "requirecpp/details/pretty_type.hpp"
#include "requirecpp/details/type_lookup.hpp"
#include "requirecpp/requirecpp.hpp"

namespace requirecpp::details {

using requirecpp::Context;

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
      closure_traits<Fn>::template unpack_arguments_to<CallHelper>::invoke(ctx,
                                                                           cb);
    };
    m_debug = [](const Context* ctx) -> std::unordered_map<std::string, bool> {
      return closure_traits<Fn>::template unpack_arguments_to<DebugInfo>::list(
          ctx);
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
      ss << dep << (satisfied ? "" : " [missing]");
    }
    ss << ")";
    return ss.str();
  }

 private:
  std::unique_ptr<std::atomic_flag> m_called;
  std::function<bool(const Context*)> m_satisfied;
  std::function<void(Context*)> m_callback;
  std::function<std::unordered_map<std::string, bool>(const Context*)> m_debug;
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
          /* type_pretty<Dep>() + " -> " + */ PrettyType<
              LookupType<Dep>>::name(),
          satisfied);
      auto set = DebugInfo<Deps...>::list(ctx);
      set.emplace(pair);
      return set;
    }
  };
};

}  // namespace requirecpp::details
