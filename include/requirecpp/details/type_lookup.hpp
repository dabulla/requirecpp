#pragma once
#include <memory>
#include <type_traits>

namespace {

// check if a type is std::shared_ptr<T>
template <template <typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

static_assert(is_specialization_of<std::shared_ptr, std::shared_ptr<int>>{});

template <typename T>
struct DeclvalHelper {
  using type = T;
};
}  // namespace

namespace requirecpp::details {

template <typename T>
static constexpr auto lookup_type() {
  if constexpr (is_specialization_of<std::shared_ptr, std::decay_t<T>>::value) {
    return DeclvalHelper<
        std::decay_t<typename std::decay_t<T>::element_type>>();
  } else if constexpr (std::is_pointer<std::decay_t<T>>()) {
    return DeclvalHelper<
        std::decay_t<std::remove_pointer_t<std::decay_t<T>>>>();
  } else {
    return DeclvalHelper<std::decay_t<T>>();
  }
}

template <typename T>
using LookupType =
    typename std::invoke_result_t<decltype(lookup_type<T>)>::type;

}  // namespace requirecpp::details
