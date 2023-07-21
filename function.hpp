#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>

namespace requirecpp {

template <typename>
class function;

template <typename FunctionT>  // overloaded operator () (e.g. std::function)
class function
    : public function<
          decltype(&std::remove_reference_t<FunctionT>::operator())> {};

template <typename ReturnTypeT, typename... Args>  // Free functions
class function<ReturnTypeT(Args...)> {
  using T = ReturnTypeT(Args...);

 public:
  explicit function(T&& fn) : m_fn{std::forward<T>(fn)} {}
  using arguments = std::tuple<Args...>;

  static constexpr std::size_t arity = std::tuple_size<arguments>::value;

  template <std::size_t N>
  using argument_type = typename std::tuple_element<N, arguments>::type;

  using return_type = ReturnTypeT;

 private:
  std::function<ReturnTypeT(Args...)> m_fn;
};

template <typename ReturnTypeT, typename... Args>  // Function pointers
class function<ReturnTypeT (*)(Args...)>
    : public function<ReturnTypeT(Args...)> {};

// member functions
template <typename ReturnTypeT, typename ClassTypeT, typename... Args>
class function<ReturnTypeT (ClassTypeT::*)(Args...)>
    : public function<ReturnTypeT(Args...)> {
  using class_type = ClassTypeT;
};

// const member functions (and lambda's operator() gets redirected here)
template <typename ReturnTypeT, typename ClassTypeT, typename... Args>
class function<ReturnTypeT (ClassTypeT::*)(Args...) const>
    : public function<ReturnTypeT (ClassTypeT::*)(Args...)> {};

template <typename... Args>
class unpack_tuple_to {
  template <typename T>
  using type = T;
};
template <typename... Args>
class unpack_tuple_to<std::tuple<Args...>> {
  template <template <typename...> typename T>
  using type = T<Args...>;
};

}  // namespace requirecpp
