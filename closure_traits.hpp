#pragma once
// Code from
// https://stackoverflow.com/a/72694693
// Luka Govedič

#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
#endif

namespace requirecpp {

#ifdef __GNUG__
template<typename T>
static std::string type_pretty() {

    int status;

    std::unique_ptr<char, decltype(&std::free)> res {
        abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status), std::free
    };

    return (status==0) ? res.get() : typeid(T).name();
}

#else

// demangle only for g++
template<typename T>
std::string type_pretty() {
    return typeid(T).name();
}

#endif

template <typename>
struct closure_traits;

template <typename FunctionT>  // overloaded operator () (e.g. std::function)
struct closure_traits
    : closure_traits<
          decltype(&std::remove_reference_t<FunctionT>::operator())> {};

template <typename ReturnTypeT, typename... Args>  // Free functions
struct closure_traits<ReturnTypeT(Args...)> {
  using arguments = std::tuple<Args...>;

  template<template <typename ...TA> typename T>
  using unpack_arguments_to = T<Args...>;

  static constexpr std::size_t arity = std::tuple_size<arguments>::value;

  template <std::size_t N>
  using argument_type = typename std::tuple_element<N, arguments>::type;

  using return_type = ReturnTypeT;
};

template <typename ReturnTypeT, typename... Args>  // Function pointers
struct closure_traits<ReturnTypeT (*)(Args...)>
    : closure_traits<ReturnTypeT(Args...)> {};

// member functions
template <typename ReturnTypeT, typename ClassTypeT, typename... Args>
struct closure_traits<ReturnTypeT (ClassTypeT::*)(Args...)>
    : closure_traits<ReturnTypeT(Args...)> {
  using class_type = ClassTypeT;
};

// const member functions (and lambda's operator() gets redirected here)
template <typename ReturnTypeT, typename ClassTypeT, typename... Args>
struct closure_traits<ReturnTypeT (ClassTypeT::*)(Args...) const>
    : closure_traits<ReturnTypeT (ClassTypeT::*)(Args...)> {};

// helper to unpack tuple "arguments"
template <typename... Args>
struct unpack_tuple_to {
  template <typename T>
  using type = T;
};
template <typename... Args>
struct unpack_tuple_to<std::tuple<Args...>> {
  template <template <typename...> typename T>
  using type = T<Args...>;
};

// check if a type is std::shared_ptr<T>
template <template <typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

static_assert(is_specialization_of<std::shared_ptr, std::shared_ptr<int>>{});


}
