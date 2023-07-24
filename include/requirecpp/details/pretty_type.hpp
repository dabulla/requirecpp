#pragma once

#include "requirecpp/decorators.hpp"
#ifdef __GNUG__
#include <cxxabi.h>
#include <cstdlib>
#include <memory>
#include <sstream>

namespace {
template <auto V>
constexpr auto valuename() {
  // Note: this might be compiler or version specific.
  constexpr std::string_view fn{__PRETTY_FUNCTION__};
  constexpr auto value_begin = 55;
  constexpr auto value_end = 1;
  return fn.substr(value_begin, fn.length() - value_begin - value_end);
}
}  // namespace
#endif

namespace requirecpp::details {
#ifdef __GNUG__

template <auto State>
struct PrettyValue {
  static std::string name() {
    int status;
    std::unique_ptr<char, decltype(&std::free)> res{
        abi::__cxa_demangle(typeid(State).name(), nullptr, nullptr, &status),
        std::free};
    return (status == 0) ? std::string{valuename<State>()}
                         : typeid(State).name();
  }
};

template <typename T>
struct PrettyType {
  static std::string name() {
    int status;

    std::unique_ptr<char, decltype(&std::free)> res{
        abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status),
        std::free};

    return (status == 0) ? res.get() : typeid(T).name();
  }
};

template <typename T, auto... State>
struct PrettyType<requirecpp::decorator::Tagged<T, State...>> {
  static std::string name() {
    int status;
    std::unique_ptr<char, decltype(&std::free)> res{
        abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status),
        std::free};

    if (status == 0) {
      std::ostringstream oss;
      oss << res.get() << "<";
      using std::to_string;
      (oss << ... << PrettyValue<State>::name());
      oss << ">";
      return oss.str();
    } else {
      return typeid(T).name();
    }
  }
};

#else

// demangle only for g++
template <typename T>
std::string type_pretty() {
  return typeid(T).name();
}

#endif
}  // namespace requirecpp::details
